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

// -------- Декодирование/кодирование UTF-8 --------
std::u32string Typer::decodeUtf8(const std::string& utf8)
{
    std::u32string out;
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
            ++i;
            continue;
        } // invalid, skip

        if (i + len > utf8.size())
            break;

        char32_t cp = 0;
        if (len == 1)
            cp = c;
        else if (len == 2)
            cp = ((c & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
        else if (len == 3)
            cp = ((c & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
        else if (len == 4)
            cp = ((c & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) | ((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);

        out.push_back(cp);
        i += len;
    }
    return out;
}

std::string Typer::encodeUtf8(const std::u32string& u32str)
{
    std::string out;
    for (char32_t cp : u32str)
    {
        if (cp <= 0x7F)
        {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp <= 0x7FF)
        {
            out.push_back(0xC0 | ((cp >> 6) & 0x1F));
            out.push_back(0x80 | (cp & 0x3F));
        }
        else if (cp <= 0xFFFF)
        {
            out.push_back(0xE0 | ((cp >> 12) & 0x0F));
            out.push_back(0x80 | ((cp >> 6) & 0x3F));
            out.push_back(0x80 | (cp & 0x3F));
        }
        else if (cp <= 0x10FFFF)
        {
            out.push_back(0xF0 | ((cp >> 18) & 0x07));
            out.push_back(0x80 | ((cp >> 12) & 0x3F));
            out.push_back(0x80 | ((cp >> 6) & 0x3F));
            out.push_back(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

// -------- Загрузка/сохранение прогресса --------
void Typer::loadState(const std::string& filepath, std::u32string& input)
{
    std::ifstream file(statePath());
    if (file.is_open())
    {
        try
        {
            json stateJson;
            file >> stateJson;
            if (stateJson.contains(filepath))
            {
                const auto& entry = stateJson[filepath];
                if (entry.is_string())
                {
                    std::string utf8Input = entry.get<std::string>();
                    input = decodeUtf8(utf8Input);
                }
            }
        }
        catch (...)
        {
        }
    }
}

void Typer::saveState(const std::string& filepath, const std::u32string& input) const
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
    stateJson[filepath] = encodeUtf8(input);
    std::ofstream out(statePath());
    if (out.is_open())
    {
        out << stateJson.dump(4);
        out.flush();
    }
}

// -------- Отрисовка --------
void Typer::draw()
{
    const int screenRows = term.getRows();
    const int screenCols = term.getCols();

    // 1. Вычисляем позицию курсора (по символам '\n')
    size_t inputLen = m_input.size();
    size_t cursorRow = 0; // номер строки (0-based) в m_input
    size_t cursorCol = 0; // позиция в строке
    for (size_t i = 0; i < inputLen; ++i)
    {
        if (m_input[i] == U'\n')
        {
            ++cursorRow;
            cursorCol = 0;
        }
        else
        {
            ++cursorCol;
        }
    }

    // 2. Определяем, с какой строки начинать отображение (чтобы курсор был внизу)
    size_t startRow = 0;
    if (cursorRow >= static_cast<size_t>(screenRows) - 1)
    {
        startRow = cursorRow - (screenRows - 2);
    }

    // 3. Находим индекс в m_input, соответствующий началу строки startRow
    size_t startIdx = 0;
    if (startRow > 0)
    {
        size_t row = 0;
        for (size_t i = 0; i < inputLen; ++i)
        {
            if (m_input[i] == U'\n')
            {
                ++row;
                if (row == startRow)
                {
                    startIdx = i + 1; // после переноса
                    break;
                }
            }
        }
    }

    // 4. Формируем буфер
    std::string output;
    output.reserve(screenRows * (screenCols + 6) + 32);
    output += "\033[?25l"; // скрыть курсор
    output += "\033[H\033[J"; // курсор в (1,1), очистить экран

    // 5. Отрисовываем символы, начиная с startIdx, но не более screenRows-1 строк
    size_t currentRow = 0;
    size_t currentCol = 0;
    size_t idx = startIdx;
    size_t textLen = m_text.size();
    size_t inputLenLocal = m_input.size();
    size_t maxLen = std::max(textLen, inputLenLocal);

    // Рисуем реальные символы, пока не кончится текст или не заполним экран (кроме последней строки)
    while (currentRow < static_cast<size_t>(screenRows - 1) && idx < maxLen)
    {
        char32_t ch = 0;
        bool hasInput = false;
        bool isCorrect = false;

        if (idx < inputLenLocal)
        {
            hasInput = true;
            ch = m_input[idx];
            if (idx < textLen && ch == m_text[idx])
                isCorrect = true;
        }
        else if (idx < textLen)
        {
            ch = m_text[idx];
        }
        else
        {
            ch = ' '; // запасной вариант
        }

        // Выбор цвета
        if (hasInput && isCorrect)
        {
            output += "\033[32m"; // зелёный
        }
        else if (hasInput && !isCorrect)
        {
            output += "\033[41;37m"; // красный фон, белый текст
        }
        else if (idx < textLen)
        {
            output += "\033[90m"; // серый (подсказка)
        }
        else
        {
            output += "\033[0m";
        }

        // Вывод символа (в UTF-8)
        output += encodeUtf8(std::u32string(1, ch));
        output += "\033[0m";

        // Обновляем позицию на экране
        if (ch == U'\n')
        {
            ++currentRow;
            currentCol = 0;
        }
        else
        {
            ++currentCol;
            if (currentCol >= static_cast<size_t>(screenCols))
            {
                ++currentRow;
                currentCol = 0;
            }
        }

        ++idx;
    }

    // 6. Заполняем пробелами оставшиеся строки (кроме последней)
    while (currentRow < static_cast<size_t>(screenRows - 1))
    {
        output += ' ';
        ++currentCol;
        if (currentCol >= static_cast<size_t>(screenCols))
        {
            ++currentRow;
            currentCol = 0;
        }
    }

    // 7. Устанавливаем курсор на позицию (cursorRow - startRow + 1, cursorCol + 1)
    int targetRow = static_cast<int>(cursorRow - startRow + 1);
    int targetCol = static_cast<int>(cursorCol + 1);
    if (targetRow < 1)
        targetRow = 1;
    if (targetRow > screenRows)
        targetRow = screenRows;
    if (targetCol < 1)
        targetCol = 1;
    if (targetCol > screenCols)
        targetCol = screenCols;

    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%d;%dH", targetRow, targetCol);
    output += buf;

    output += "\033[?25h"; // показать курсор

    write(STDOUT_FILENO, output.data(), output.size());
}

// -------- Вывод статистики --------
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
            size_t totalPresses = (m_charPresses.count(ch) ? m_charPresses.at(ch) : 0);
            double errPercent = (totalPresses > 0) ? (100.0 * errs / totalPresses) : 0.0;
            std::string chStr = encodeUtf8(std::u32string(1, ch));
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

void Typer::run(
    const std::string& filepath,
    const std::string& utf8Content,
    std::string& savedInput,
    bool reset
)
{
    // 1. Декодируем эталонный текст (UTF-8 -> UTF-32)
    m_text = decodeUtf8(utf8Content);

    // 2. Загружаем сохранённый прогресс, если не запрошен сброс
    if (!reset)
        loadState(filepath, m_input);
    else
        m_input.clear();

    // 3. Сбрасываем всю статистику (как в исходном коде)
    m_correctChars = 0;
    m_errors = 0;
    m_started = false;
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
    m_lastKeyTime = std::chrono::steady_clock::now();

    // 4. Если файл пуст – сразу завершаемся и удаляем прогресс
    if (m_text.empty())
    {
        // Удаляем запись о прогрессе для этого файла
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
        stateJson.erase(filepath);
        std::ofstream out(statePath());
        if (out.is_open())
        {
            out << stateJson.dump(4);
            out.flush();
        }
        term.disableRaw();
        term.clearScreen();
        std::cout << "\033[H";
        std::cout << "Файл пуст. Прогресс сброшен.\n\n";
        printStats();
        std::cout.flush();
        savedInput = encodeUtf8(m_input);
        return;
    }

    // 5. Включаем raw-режим и очищаем экран
    term.enableRaw();
    term.clearScreen();

    bool completed = false; // флаг, что достигнут конец текста (по количеству символов)

    while (true)
    {
        draw(); // отрисовка текущего состояния

        char32_t ch = term.getChar(); // читаем один UTF-8 символ

        // Выход по ESC – сохраняем прогресс
        if (ch == 27)
            break;

        // Измеряем время между нажатиями
        auto now = std::chrono::steady_clock::now();
        std::chrono::milliseconds diff(0);
        if (m_totalKeystrokes > 0)
        {
            diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastKeyTime);
            m_totalTimeBetweenKeys += diff;
            m_timeIntervalsCount++;
        }
        m_lastKeyTime = now;
        ++m_totalKeystrokes;

        // Обработка Backspace – всегда разрешена
        if (ch == 127 || ch == '\b')
        {
            ++m_backspaces;
            if (!m_input.empty())
                m_input.pop_back();
            continue;
        }

        // === ОГРАНИЧЕНИЕ: не позволяем вводить больше символов, чем в эталоне ===
        // Если уже достигли или превысили длину эталона – игнорируем нажатие.
        // (Пользователь может удалить лишние символы Backspace'ом.)
        if (m_input.size() >= m_text.size())
            continue;

        // Обычный ввод
        if (!m_started)
        {
            m_startTime = now;
            m_started = true;
        }

        m_input.push_back(ch);

        size_t inputPos = m_input.size() - 1;
        bool isCorrect = (inputPos < m_text.size() && ch == m_text[inputPos]);
        // (inputPos всегда < m_text.size(), так как мы ограничили выше, но на всякий случай)
        if (inputPos >= m_text.size())
            isCorrect = false;

        // Обновляем статистику
        m_charPresses[ch]++;
        if (isCorrect)
        {
            ++m_correctChars;
            m_charCorrect[ch]++;
            ++m_consecutiveCorrect;
            if (m_consecutiveCorrect > m_maxConsecutiveCorrect)
                m_maxConsecutiveCorrect = m_consecutiveCorrect;
            if (diff.count() > 0)
            {
                m_correctTimeSum += diff;
                m_correctIntervals++;
            }
        }
        else
        {
            ++m_errors;
            m_charErrors[ch]++;
            m_consecutiveCorrect = 0;
            if (diff.count() > 0)
            {
                m_errorTimeSum += diff;
                m_errorIntervals++;
            }
        }

        // === ПРОВЕРКА ЗАВЕРШЕНИЯ ===
        // Как только количество введённых символов достигло длины эталона – выходим.
        if (m_input.size() == m_text.size())
        {
            completed = true;
            break;
        }
    }

    // 6. Восстанавливаем терминал
    term.disableRaw();
    term.clearScreen();
    std::cout << "\033[H";

    // 7. Сохраняем или удаляем прогресс
    if (completed)
    {
        // Удаляем запись о прогрессе для этого файла – задание выполнено
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
        stateJson.erase(filepath);
        std::ofstream out(statePath());
        if (out.is_open())
        {
            out << stateJson.dump(4);
            out.flush();
        }
        std::cout << "Поздравляем! Вы ввели все символы текста. Прогресс сброшен.\n";
    }
    else
    {
        // Выход по ESC – сохраняем прогресс
        saveState(filepath, m_input);
        std::cout << "Прогресс сохранён. Введено символов: " << m_input.size() << "\n";
    }

    std::cout << "\n";
    printStats();
    std::cout.flush();

    // Сохраняем введённый текст в выходной параметр (на случай, если вызывающий захочет его использовать)
    savedInput = encodeUtf8(m_input);
}
