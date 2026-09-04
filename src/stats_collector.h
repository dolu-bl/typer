#pragma once

#include <chrono>
#include <unordered_map>

class StatsCollector final
{
public:
    StatsCollector();

    void reset();
    void onKeyPress(char32_t ch, bool isCorrect, long long timeSinceLastMs);
    void onBackspace();

    // Итоговые данные
    size_t getCorrectChars() const;
    size_t getErrors() const;
    size_t getCharsTyped() const;
    double getAccuracy() const;
    double getWPM(double minutes) const;
    size_t getBackspaces() const;
    size_t getMaxConsecutiveCorrect() const;
    long long getAvgKeyIntervalMs() const;
    bool isStarted() const;

    // Для вывода статистики
    void printStats() const;

    // Внутренние данные для сохранения (сессия)
    std::chrono::steady_clock::time_point getStartTime() const;
    void setStartTime(std::chrono::steady_clock::time_point t);

private:
    std::chrono::steady_clock::time_point m_startTime;
    bool m_started = false;

    size_t m_correctChars = 0;
    size_t m_errors = 0;
    std::unordered_map<char32_t, size_t> m_charPresses;
    std::unordered_map<char32_t, size_t> m_charErrors;
    std::unordered_map<char32_t, size_t> m_charCorrect;
    size_t m_backspaces = 0;
    size_t m_totalKeystrokes = 0;
    size_t m_consecutiveCorrect = 0;
    size_t m_maxConsecutiveCorrect = 0;
    std::chrono::steady_clock::time_point m_lastKeyTime;
    std::chrono::milliseconds m_totalTimeBetweenKeys;
    size_t m_timeIntervalsCount = 0;
    std::chrono::milliseconds m_correctTimeSum;
    size_t m_correctIntervals = 0;
    std::chrono::milliseconds m_errorTimeSum;
    size_t m_errorIntervals = 0;
};
