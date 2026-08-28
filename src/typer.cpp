#include <cstdlib>
#include <fstream>
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
    
    // Находим начало текущей строки
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

    term.enableRaw();
    term.clearScreen();

    size_t pos = offset;
    bool error = false;

    while (pos < content.size())
    {
        draw(content, pos, error);
        char ch = term.getChar();

        if (ch == 27)
            break; // ESC

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

        if (pos < content.size() && ch == content[pos])
        {
            error = false;
            ++pos;
        }
        else
        {
            error = true;
        }
    }

    saveState(filepath, pos);
    offset = pos;
    term.disableRaw();
    term.clearScreen();
}
