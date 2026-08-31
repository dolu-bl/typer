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

void Terminal::hideCursor()
{
    write(STDOUT_FILENO, "\033[?25l", 6);
}

void Terminal::showCursor()
{
    write(STDOUT_FILENO, "\033[?25h", 6);
}

char32_t Terminal::getChar()
{
    // Читаем первый байт
    unsigned char first;
    if (read(STDIN_FILENO, &first, 1) != 1)
        return 0;

    // Определяем длину UTF-8 последовательности
    size_t len;
    if ((first & 0x80) == 0) // 0xxxxxxx
        len = 1;
    else if ((first & 0xE0) == 0xC0) // 110xxxxx
        len = 2;
    else if ((first & 0xF0) == 0xE0) // 1110xxxx
        len = 3;
    else if ((first & 0xF8) == 0xF0) // 11110xxx
        len = 4;
    else
        return 0; // невалидный

    unsigned char buf[4] = { first, 0, 0, 0 };
    for (size_t i = 1; i < len; ++i)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            return 0;
        // Проверяем, что это continuation byte (10xxxxxx)
        if ((buf[i] & 0xC0) != 0x80)
            return 0;
    }

    // Декодируем в char32_t
    char32_t cp = 0;
    if (len == 1)
        cp = first;
    else if (len == 2)
        cp = ((first & 0x1F) << 6) | (buf[1] & 0x3F);
    else if (len == 3)
        cp = ((first & 0x0F) << 12) | ((buf[1] & 0x3F) << 6) | (buf[2] & 0x3F);
    else if (len == 4)
        cp = ((first & 0x07) << 18) | ((buf[1] & 0x3F) << 12) | ((buf[2] & 0x3F) << 6) | (buf[3] & 0x3F);

    return cp;
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
