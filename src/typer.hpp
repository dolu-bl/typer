#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "terminal.hpp"

class Typer final
{
public:
    void run(
        const std::string& filepath,
        const std::string& utf8Content,
        size_t& offset, // позиция в кодовых точках
        bool reset
    );

private:
    // Декодирует UTF-8 строку в u32string и заполняет байтовые смещения
    static void decodeUtf8(
        const std::string& utf8,
        std::u32string& outText,
        std::vector<size_t>& outByteOffsets
    );

    void draw(size_t pos, bool error);
    void loadState(const std::string& filepath, size_t& offset);
    void saveState(const std::string& filepath, size_t offset) const;
    void printStats() const;

    Terminal term;

    // Текст в разных представлениях
    std::u32string m_text; // для логики (кодовые точки)
    std::string m_utf8; // для вывода в терминал
    std::vector<size_t> m_byteOffsets; // смещение начала каждого символа в байтах; длина = m_text.size()+1

    size_t m_totalChars = 0;
    size_t m_correctChars = 0;
    size_t m_errors = 0;
    std::chrono::steady_clock::time_point m_startTime;
    bool m_started = false;
};
