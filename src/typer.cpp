#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unistd.h>
#include <vector>

#include <nlohmann/json.hpp>

#include "typer.hpp"

using json = nlohmann::json;

// -------- Вспомогательные функции для состояния --------
static std::string statePath()
{
    const char* home = getenv("HOME");
    return std::string(home ? home : ".") + "/.typer_sessions.json";
}

// -------- Декодирование UTF-8 --------
void Typer::decodeUtf8(
    const std::string& utf8,
    std::u32string& outText,
    std::vector<size_t>& outByteOffsets
)
{
    outText.clear();
    outByteOffsets.clear();
    outByteOffsets.reserve(utf8.size() + 1);

    size_t i = 0;
    while (i < utf8.size())
    {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        size_t len;
        if ((c & 0x80) == 0)
            len = 1;
        else if ((c & 0xE0) == 0xC0)
            len = 2;
        else if ((c & 0xF0) == 0xE0)
            len = 3;
        else if ((c & 0xF8) == 0xF0)
            len = 4;
        else
        {
            // Некорректный байт, пропускаем
            ++i;
            continue;
        }

        if (i + len > utf8.size())
            break;

        // Декодируем
        char32_t cp = 0;
        if (len == 1)
            cp = c;
        else if (len == 2)
            cp = ((c & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
        else if (len == 3)
            cp = ((c & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
        else if (len == 4)
            cp = ((c & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) | ((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);

        outByteOffsets.push_back(i);
        outText.push_back(cp);
        i += len;
    }
    outByteOffsets.push_back(utf8.size()); // смещение конца
}

// -------- Преобразование char32_t в UTF-8 --------
std::string Typer::utf8FromChar32(char32_t cp)
{
    std::string result;
    if (cp <= 0x7F)
    {
        result.push_back(static_cast<char>(cp));
    }
    else if (cp <= 0x7FF)
    {
        result.push_back(0xC0 | ((cp >> 6) & 0x1F));
        result.push_back(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0xFFFF)
    {
        result.push_back(0xE0 | ((cp >> 12) & 0x0F));
        result.push_back(0x80 | ((cp >> 6) & 0x3F));
        result.push_back(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0x10FFFF)
    {
        result.push_back(0xF0 | ((cp >> 18) & 0x07));
        result.push_back(0x80 | ((cp >> 12) & 0x3F));
        result.push_back(0x80 | ((cp >> 6) & 0x3F));
        result.push_back(0x80 | (cp & 0x3F));
    }
    return result;
}

// -------- Загрузка/сохранение прогресса --------
void Typer::loadState(const std::string& filepath, size_t& offset)
{
    std::ifstream file(statePath());
    if (file.is_open())
    {
        try
        {
            json stateJson;
            file >> stateJson;
            if (stateJson.contains(filepath))
                offset = stateJson[filepath].get<size_t>();
        }
        catch (...)
        {
        }
    }
}

void Typer::saveState(const std::string& filepath, size_t offset) const
{
    json stateJson;
    std::ifstream in(statePath());
    if (in.is_open())
    {
        try
        {
            in >> stateJson;
        }
        catch (...)
        {
        }
    }
    stateJson[filepath] = offset;
    std::ofstream out(statePath());
    if (out.is_open())
    {
        out << stateJson.dump(4);
        out.flush(); // убедимся, что данные записаны
    }
}

// -------- Отрисовка с буферизацией (без мерцания) --------
void Typer::draw(size_t pos, bool error)
{
    const int rows = term.getRows();
    const int cols = term.getCols();

    // Определяем начало видимой области: начало строки, содержащей pos
    size_t start = pos;
    while (start > 0 && m_text[start - 1] != U'\n')
        --start;

    // Вычисляем координаты курсора для позиции pos
    size_t cursorRow = 0;
    size_t cursorCol = 0;
    {
        size_t row = 0;
        size_t col = 0;
        for (size_t i = start; i < pos && i < m_text.size(); ++i)
        {
            char32_t ch = m_text[i];
            if (ch == U'\n')
            {
                ++row;
                col = 0;
            }
            else
            {
                ++col;
                if (col >= static_cast<size_t>(cols))
                {
                    ++row;
                    col = 0;
                }
            }
        }
        cursorRow = row;
        cursorCol = col;
    }

    // Подготовим выходной буфер
    std::string output;
    output.reserve(4096); // примерный размер

    // Скрываем курсор на время обновления
    output += "\033[?25l";
    // Очищаем экран
    output += "\033[2J\033[H";

    // --- Отрисовка текста с цветами ---
    size_t idx = start;
    size_t row = 0;
    size_t col = 0;

    while (row < static_cast<size_t>(rows) && idx < m_text.size())
    {
        char32_t ch = m_text[idx];

        // Определяем цвет
        bool isBefore = (idx < pos);
        bool isCurrent = (idx == pos);

        if (isBefore)
            output += "\033[32m"; // зелёный
        else if (isCurrent && error)
            output += "\033[41;37m"; // красный фон, белый текст
        else
            output += "\033[0m"; // обычный (сброс)

        // Добавляем символ (в UTF-8)
        size_t byteStart = m_byteOffsets[idx];
        size_t byteEnd = m_byteOffsets[idx + 1];
        output.append(m_utf8, byteStart, byteEnd - byteStart);

        // Сбрасываем цвет после каждого специального
        if (isBefore || (isCurrent && error))
            output += "\033[0m";

        // Обрабатываем перенос строки или по ширине
        if (ch == U'\n')
        {
            ++row;
            col = 0;
        }
        else
        {
            ++col;
            if (col >= static_cast<size_t>(cols))
            {
                ++row;
                col = 0;
            }
        }

        ++idx;
    }

    // После цикла устанавливаем курсор в позицию ввода
    int targetRow = static_cast<int>(cursorRow + 1);
    int targetCol = static_cast<int>(cursorCol + 1);
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%d;%dH", targetRow, targetCol);
    output += buf;

    // Показываем курсор
    output += "\033[?25h";

    // Один вызов для всей отрисовки
    write(STDOUT_FILENO, output.data(), output.size());
}

// -------- Вывод статистики (расширенная) --------
void Typer::printStats() const
{
    if (!m_started)
    {
        std::cout << "Нет данных для статистики (ни одного символа не введено).\n";
        return;
    }

    const auto endTime = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(endTime - m_startTime).count();
    const double minutes = elapsed / 60.0;

    const size_t total = m_correctChars + m_errors;
    const double accuracy = (total > 0) ? (static_cast<double>(m_correctChars) / total * 100.0) : 0.0;

    const double wpm = (minutes > 0) ? (m_correctChars / 5.0 / minutes) : 0.0;

    std::cout
        << "\n========= СТАТИСТИКА =========\n"
        << "  Время работы:     " << elapsed << " сек.\n"
        << "  Правильных симв.: " << m_correctChars << "\n"
        << "  Ошибок:           " << m_errors << "\n"
        << "  Точность:         " << std::fixed << std::setprecision(1) << accuracy << "%\n"
        << "  Скорость (WPM):   " << std::fixed << std::setprecision(1) << wpm << "\n";

    // Расширенная статистика
    std::cout << "\n--- Расширенная статистика ---\n";
    std::cout << "  Всего нажатий клавиш: " << m_totalKeystrokes << "\n";
    std::cout << "  Нажатий Backspace:    " << m_backspaces << "\n";
    std::cout << "  Макс. последовательность без ошибок: " << m_maxConsecutiveCorrect << " симв.\n";

    // Среднее время между нажатиями
    if (m_timeIntervalsCount > 0)
    {
        double avgMs = static_cast<double>(m_totalTimeBetweenKeys.count()) / m_timeIntervalsCount;
        std::cout << "  Среднее время между нажатиями: " << std::fixed << std::setprecision(1) << avgMs << " мс\n";
    }
    if (m_correctIntervals > 0)
    {
        double avgCorrectMs = static_cast<double>(m_correctTimeSum.count()) / m_correctIntervals;
        std::cout << "  Среднее время для правильных:  " << std::fixed << std::setprecision(1) << avgCorrectMs << " мс\n";
    }
    if (m_errorIntervals > 0)
    {
        double avgErrorMs = static_cast<double>(m_errorTimeSum.count()) / m_errorIntervals;
        std::cout << "  Среднее время для ошибочных:   " << std::fixed << std::setprecision(1) << avgErrorMs << " мс\n";
    }

    // Топ-5 ошибочных символов
    if (!m_charErrors.empty())
    {
        std::vector<std::pair<char32_t, size_t>> errorsVec(m_charErrors.begin(), m_charErrors.end());
        std::sort(errorsVec.begin(), errorsVec.end(), [](const auto& a, const auto& b)
                  { return a.second > b.second; });

        std::cout << "\n  Топ-5 символов с наибольшим числом ошибок:\n";
        size_t count = 0;
        for (const auto& p : errorsVec)
        {
            if (count >= 5)
                break;
            char32_t ch = p.first;
            size_t errs = p.second;
            size_t totalPresses = m_charPresses.count(ch) ? m_charPresses.at(ch) : 0;
            double errPercent = (totalPresses > 0) ? (100.0 * errs / totalPresses) : 0.0;
            std::string chStr = utf8FromChar32(ch);
            // если символ управляющий (например, табуляция) - заменим
            if (ch < 0x20)
            {
                chStr = "\\x" + std::to_string(static_cast<int>(ch));
            }
            std::cout << "    '" << chStr << "' — ошибок: " << errs
                      << " (из " << totalPresses << " нажатий, "
                      << std::fixed << std::setprecision(1) << errPercent << "%)\n";
            ++count;
        }
    }

    std::cout << "===============================\n";
}

// -------- Основной цикл --------
void Typer::run(
    const std::string& filepath,
    const std::string& utf8Content,
    size_t& offset,
    bool reset
)
{
    // Декодируем содержимое файла
    decodeUtf8(utf8Content, m_text, m_byteOffsets);
    m_utf8 = utf8Content;

    if (!reset)
        loadState(filepath, offset);
    else
        offset = 0;

    // Сброс статистики
    m_totalChars = 0;
    m_correctChars = 0;
    m_errors = 0;
    m_started = false;

    // Сброс расширенной статистики
    m_charPresses.clear();
    m_charErrors.clear();
    m_charCorrect.clear();
    m_backspaces = 0;
    m_totalKeystrokes = 0;
    m_consecutiveCorrect = 0;
    m_maxConsecutiveCorrect = 0;
    m_totalTimeBetweenKeys = std::chrono::milliseconds { 0 };
    m_timeIntervalsCount = 0;
    m_correctTimeSum = std::chrono::milliseconds { 0 };
    m_correctIntervals = 0;
    m_errorTimeSum = std::chrono::milliseconds { 0 };
    m_errorIntervals = 0;
    m_lastKeyTime = std::chrono::steady_clock::now(); // инициализируем, но интервал не будет учтен до второго нажатия

    term.enableRaw();
    term.clearScreen();

    size_t pos = offset;
    bool error = false;

    while (pos < m_text.size())
    {
        draw(pos, error);
        char32_t ch = term.getChar();

        if (ch == 27) // ESC
            break;

        // Измеряем время с предыдущего нажатия
        auto now = std::chrono::steady_clock::now();
        std::chrono::milliseconds diff(0);
        if (m_totalKeystrokes > 0)
        {
            diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastKeyTime);
            m_totalTimeBetweenKeys += diff;
            m_timeIntervalsCount++;
        }
        m_lastKeyTime = now;

        if (ch == 127 || ch == '\b')
        {
            ++m_totalKeystrokes;
            ++m_backspaces;
            // diff добавляется только к общей сумме, мы уже добавили
            if (pos > offset)
            {
                if (error)
                {
                    error = false;
                }
                else
                {
                    --pos;
                }
            }
            continue;
        }

        // --- Обычный ввод символа ---
        ++m_totalKeystrokes;

        if (!m_started)
        {
            m_startTime = std::chrono::steady_clock::now();
            m_started = true;
        }

        ++m_totalChars;
        if (pos < m_text.size() && ch == m_text[pos])
        {
            // Правильный символ
            ++m_correctChars;
            m_charCorrect[ch]++;
            m_charPresses[ch]++;
            error = false;
            ++pos;
            ++m_consecutiveCorrect;
            if (m_consecutiveCorrect > m_maxConsecutiveCorrect)
                m_maxConsecutiveCorrect = m_consecutiveCorrect;
            // Добавляем интервал к правильным
            if (diff.count() > 0)
            {
                m_correctTimeSum += diff;
                m_correctIntervals++;
            }
        }
        else
        {
            // Ошибка
            ++m_errors;
            m_charErrors[ch]++;
            m_charPresses[ch]++;
            error = true;
            m_consecutiveCorrect = 0; // сбрасываем последовательность
            // Добавляем интервал к ошибочным
            if (diff.count() > 0)
            {
                m_errorTimeSum += diff;
                m_errorIntervals++;
            }
        }
    }

    // Сохраняем прогресс
    saveState(filepath, pos);
    offset = pos;

    term.disableRaw();
    term.clearScreen();
    std::cout << "\033[H"; // курсор в левый верхний угол

    std::cout << "Прогресс сохранён. Остановились на позиции " << pos << "\n\n";
    printStats();
    std::cout.flush();
}
