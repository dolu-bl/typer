#include <algorithm>
#include <cstdio>
#include <unistd.h>

#include "renderer.h"
#include "utf8_utils.h"

Renderer::Renderer(Terminal& term)
    : m_term(term)
{
}

void Renderer::draw(const std::u32string& text, const std::u32string& input)
{
    const int screenRows = m_term.getRows();
    const int screenCols = m_term.getCols();

    size_t inputLen = input.size();
    size_t cursorRow = 0;
    size_t cursorCol = 0;
    for (size_t i = 0; i < inputLen; ++i)
    {
        if (input[i] == U'\n')
        {
            ++cursorRow;
            cursorCol = 0;
        }
        else
        {
            ++cursorCol;
        }
    }

    size_t startRow = 0;
    if (cursorRow >= static_cast<size_t>(screenRows) - 1)
    {
        startRow = cursorRow - (screenRows - 2);
    }

    size_t startIdx = 0;
    if (startRow > 0)
    {
        size_t row = 0;
        for (size_t i = 0; i < inputLen; ++i)
        {
            if (input[i] == U'\n')
            {
                ++row;
                if (row == startRow)
                {
                    startIdx = i + 1;
                    break;
                }
            }
        }
    }

    std::string output;
    output.reserve(screenRows * (screenCols + 6) + 32);
    output += "\033[?25l";
    output += "\033[H\033[J";

    size_t currentRow = 0;
    size_t currentCol = 0;
    size_t idx = startIdx;
    size_t textLen = text.size();
    size_t inputLenLocal = input.size();
    size_t maxLen = std::max(textLen, inputLenLocal);

    while (currentRow < static_cast<size_t>(screenRows - 1) && idx < maxLen)
    {
        char32_t ch = 0;
        bool hasInput = false;
        bool isCorrect = false;

        if (idx < inputLenLocal)
        {
            hasInput = true;
            ch = input[idx];
            if (idx < textLen && ch == text[idx])
                isCorrect = true;
        }
        else if (idx < textLen)
        {
            ch = text[idx];
        }
        else
        {
            ch = ' ';
        }

        if (hasInput && isCorrect)
        {
            output += "\033[32m";
        }
        else if (hasInput && !isCorrect)
        {
            output += "\033[41;37m";
        }
        else if (idx < textLen)
        {
            output += "\033[90m";
        }
        else
        {
            output += "\033[0m";
        }

        output += utf8utils::encodeUtf8(std::u32string(1, ch));
        output += "\033[0m";

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
    output += "\033[?25h";

    (void)write(STDOUT_FILENO, output.data(), output.size());
}
