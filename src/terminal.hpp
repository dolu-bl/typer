#pragma once

#include <termios.h>

class Terminal final
{
public:
    Terminal();
    ~Terminal();

    void enableRaw();
    void disableRaw();
    void clearScreen();
    void moveCursor(int row, int col);
    void setColor(int fg, int bg);
    void resetColor();

    // Читает один UTF-8 символ и возвращает его кодовую точку
    char32_t getChar();

    int getRows() const;
    int getCols() const;

    void hideCursor();
    void showCursor();

private:
    void updateSize();

private:
    termios m_origTerminal;
    int m_rows;
    int m_cols;
};
