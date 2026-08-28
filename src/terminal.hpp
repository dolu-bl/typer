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
    char getChar();
    int getRows() const;
    int getCols() const;

private:
    void updateSize();

private:
    termios m_origTerminal;
    int m_rows;
    int m_cols;
};