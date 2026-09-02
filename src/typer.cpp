#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unistd.h>

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

    // WPM: количество слов (1 слово = 5 символов) в минуту
    const double wpm = (minutes > 0) ? (m_correctChars / 5.0 / minutes) : 0.0;

    std::cout
        << "\n========= СТАТИСТИКА =========\n"
        << "  Время работы:     " << elapsed << " сек.\n"
        << "  Правильных симв.: " << m_correctChars << "\n"
        << "  Ошибок:           " << m_errors << "\n"
        << "  Точность:         " << std::fixed << std::setprecision(1) << accuracy << "%\n"
        << "  Скорость (WPM):   " << std::fixed << std::setprecision(1) << wpm << "\n"
        << "===============================\n";
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

        if (ch == 127 || ch == '\b')
        {
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
        if (!m_started)
        {
            m_startTime = std::chrono::steady_clock::now();
            m_started = true;
        }

        ++m_totalChars;
        if (pos < m_text.size() && ch == m_text[pos])
        {
            ++m_correctChars;
            error = false;
            ++pos;
        }
        else
        {
            ++m_errors;
            error = true;
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
