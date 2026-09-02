#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
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

    // Преобразует кодовую точку в UTF-8 строку (для вывода в статистике)
    static std::string utf8FromChar32(char32_t cp);

    void draw(size_t pos, bool error);
    void loadState(const std::string& filepath, size_t& offset);
    void saveState(const std::string& filepath, size_t offset) const;
    void printStats() const;

    Terminal term;

    // Текст в разных представлениях
    std::u32string m_text; // для логики (кодовые точки)
    std::string m_utf8; // для вывода в терминал
    std::vector<size_t> m_byteOffsets; // смещение начала каждого символа в байтах; длина = m_text.size()+1

    // Базовая статистика
    size_t m_totalChars = 0;
    size_t m_correctChars = 0;
    size_t m_errors = 0;
    std::chrono::steady_clock::time_point m_startTime;
    bool m_started = false;

    // Расширенная статистика
    std::unordered_map<char32_t, size_t> m_charPresses; // общее количество нажатий на символ
    std::unordered_map<char32_t, size_t> m_charErrors; // количество ошибок при нажатии этого символа
    std::unordered_map<char32_t, size_t> m_charCorrect; // количество правильных нажатий этого символа
    size_t m_backspaces = 0;
    size_t m_totalKeystrokes = 0; // все нажатия клавиш (включая Backspace)
    size_t m_consecutiveCorrect = 0; // текущая непрерывная последовательность правильных символов
    size_t m_maxConsecutiveCorrect = 0; // максимальная за сессию
    std::chrono::steady_clock::time_point m_lastKeyTime;
    std::chrono::milliseconds m_totalTimeBetweenKeys { 0 }; // сумма всех интервалов между нажатиями
    size_t m_timeIntervalsCount = 0; // количество интервалов (для среднего)
    std::chrono::milliseconds m_correctTimeSum { 0 };
    size_t m_correctIntervals = 0;
    std::chrono::milliseconds m_errorTimeSum { 0 };
    size_t m_errorIntervals = 0;
};
