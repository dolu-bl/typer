#include <cstdio>
#include <cstring>
#include <sys/ioctl.h>
#include <unistd.h>

#include "terminal.hpp"

Terminal::Terminal()
{
    updateSize();
}

Terminal::~Terminal()
{
    disableRaw();
}

void Terminal::enableRaw()
{
    tcgetattr(STDIN_FILENO, &m_origTerminal);
    termios raw = m_origTerminal;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void Terminal::disableRaw()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_origTerminal);
}

void Terminal::clearScreen()
{
    write(STDOUT_FILENO, "\033[2J\033[H", 6);
}

void Terminal::moveCursor(int row, int col)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%d;%dH", row, col);
    write(STDOUT_FILENO, buf, strlen(buf));
}

void Terminal::setColor(int fg, int bg)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%d;%dm", 30 + fg, 40 + bg);
    write(STDOUT_FILENO, buf, strlen(buf));
}

void Terminal::resetColor()
{
    write(STDOUT_FILENO, "\033[0m", 4);
}

char Terminal::getChar()
{
    char ch;
    read(STDIN_FILENO, &ch, 1);
    return ch;
}

void Terminal::updateSize()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    m_rows = w.ws_row;
    m_cols = w.ws_col;
}

int Terminal::getRows() const
{
    return m_rows;
}

int Terminal::getCols() const
{
    return m_cols;
}
