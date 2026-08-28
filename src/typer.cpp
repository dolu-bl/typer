#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "typer.hpp"

using json = nlohmann::json;

static std::string statePath()
{
    const char* home = getenv("HOME");
    return std::string(home ? home : ".") + "/.typer_sessions.json";
}

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
        out << stateJson.dump(4);
}

void Typer::draw(const std::string& text, size_t pos, bool error)
{
    term.clearScreen();
    int rows = term.getRows(), cols = term.getCols();

    size_t start = pos;
    while (start > 0 && text[start - 1] != '\n')
        --start;

    size_t curRow = 0;
    size_t curCol = 0;
    size_t cursorRow = 0;
    size_t cursorCol = 0;

    for (size_t i = start; i < text.size() && curRow < rows; ++i)
    {
        char ch = text[i];
        if (ch == '\n' || curCol >= cols - 1)
        {
            curRow++;
            curCol = 0;
            if (ch == '\n')
                continue;
        }

        term.moveCursor(curRow + 1, curCol + 1);
        if (i < pos)
        {
            term.setColor(2, 0); // зелёный текст на чёрном фоне
        }
        else if (i == pos && error)
        {
            term.setColor(7, 1); // белый текст на красном фоне
        }
        else
        {
            term.resetColor();
        }
        write(STDOUT_FILENO, &ch, 1);

        if (i == pos)
        {
            cursorRow = curRow;
            cursorCol = curCol;
        }
        curCol++;
    }

    term.resetColor();
    if (pos >= text.size())
    {
        cursorRow = rows - 1;
        cursorCol = cols - 1;
    }
    term.moveCursor(cursorRow + 1, cursorCol + 1);
}

// -------- Вывод статистики ------------------------------------------------
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

// -------- Основной цикл ------------------------------------------------
void Typer::run(
    const std::string& filepath,
    const std::string& content,
    size_t& offset,
    bool reset
)
{
    if (!reset)
        loadState(filepath, offset);
    else
        offset = 0;

    // Сброс статистики при новом запуске
    m_totalChars = 0;
    m_correctChars = 0;
    m_errors = 0;
    m_started = false;

    term.enableRaw();
    term.clearScreen();

    size_t pos = offset;
    bool error = false;

    while (pos < content.size())
    {
        draw(content, pos, error);
        char ch = term.getChar();

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
        if (pos < content.size() && ch == content[pos])
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

    // Восстанавливаем терминал и очищаем экран
    term.disableRaw();
    term.clearScreen();
    std::cout << "\033[H"; // курсор в левый верхний угол

    // Сообщение о сохранении (теперь оно первым)
    std::cout << "Прогресс сохранён. Остановились на позиции " << pos << "\n\n";

    // Статистика
    printStats();

    // Принудительный сброс буфера
    std::cout.flush();
}
