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
        std::string& savedInput, // вход/выход: введённый текст (UTF-8), может быть пустым
        bool reset
    );

private:
    // Декодирует UTF-8 строку в u32string
    static std::u32string decodeUtf8(const std::string& utf8);
    // Кодирует u32string в UTF-8
    static std::string encodeUtf8(const std::u32string& u32str);

    void draw(); // больше не нужны pos и error, состояние хранится в m_input
    void loadState(const std::string& filepath, std::u32string& input);
    void saveState(const std::string& filepath, const std::u32string& input) const;
    void printStats() const;

    Terminal term;

    // Эталонный текст (декодированный)
    std::u32string m_text;
    // Введённый пользователем текст (декодированный)
    std::u32string m_input;

    // Базовая статистика
    size_t m_correctChars = 0;
    size_t m_errors = 0;
    std::chrono::steady_clock::time_point m_startTime;
    bool m_started = false;

    // Расширенная статистика
    std::unordered_map<char32_t, size_t> m_charPresses;
    std::unordered_map<char32_t, size_t> m_charErrors;
    std::unordered_map<char32_t, size_t> m_charCorrect;
    size_t m_backspaces = 0;
    size_t m_totalKeystrokes = 0;
    size_t m_consecutiveCorrect = 0;
    size_t m_maxConsecutiveCorrect = 0;
    std::chrono::steady_clock::time_point m_lastKeyTime;
    std::chrono::milliseconds m_totalTimeBetweenKeys { 0 };
    size_t m_timeIntervalsCount = 0;
    std::chrono::milliseconds m_correctTimeSum { 0 };
    size_t m_correctIntervals = 0;
    std::chrono::milliseconds m_errorTimeSum { 0 };
    size_t m_errorIntervals = 0;
};
