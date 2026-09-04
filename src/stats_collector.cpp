#include <iomanip>
#include <iostream>

#include "stats_collector.h"

StatsCollector::StatsCollector()
{
    reset();
}

void StatsCollector::reset()
{
    m_started = false;
    m_correctChars = 0;
    m_errors = 0;
    m_charPresses.clear();
    m_charErrors.clear();
    m_charCorrect.clear();
    m_backspaces = 0;
    m_totalKeystrokes = 0;
    m_consecutiveCorrect = 0;
    m_maxConsecutiveCorrect = 0;
    m_totalTimeBetweenKeys = std::chrono::milliseconds { 0 };
    m_timeIntervalsCount = 0;
    m_correctTimeSum = std::chrono::milliseconds { 0 };
    m_correctIntervals = 0;
    m_errorTimeSum = std::chrono::milliseconds { 0 };
    m_errorIntervals = 0;
    m_lastKeyTime = std::chrono::steady_clock::now();
}

void StatsCollector::onKeyPress(char32_t ch, bool isCorrect, long long timeSinceLastMs)
{
    if (!m_started)
    {
        m_startTime = std::chrono::steady_clock::now();
        m_started = true;
    }

    ++m_totalKeystrokes;
    m_charPresses[ch]++;

    if (timeSinceLastMs >= 0)
    {
        m_totalTimeBetweenKeys += std::chrono::milliseconds(timeSinceLastMs);
        m_timeIntervalsCount++;
    }

    if (isCorrect)
    {
        ++m_correctChars;
        m_charCorrect[ch]++;
        ++m_consecutiveCorrect;
        if (m_consecutiveCorrect > m_maxConsecutiveCorrect)
            m_maxConsecutiveCorrect = m_consecutiveCorrect;
        if (timeSinceLastMs >= 0)
        {
            m_correctTimeSum += std::chrono::milliseconds(timeSinceLastMs);
            m_correctIntervals++;
        }
    }
    else
    {
        ++m_errors;
        m_charErrors[ch]++;
        m_consecutiveCorrect = 0;
        if (timeSinceLastMs >= 0)
        {
            m_errorTimeSum += std::chrono::milliseconds(timeSinceLastMs);
            m_errorIntervals++;
        }
    }
}

void StatsCollector::onBackspace()
{
    ++m_backspaces;
}

size_t StatsCollector::getCorrectChars() const
{
    return m_correctChars;
}

size_t StatsCollector::getErrors() const
{
    return m_errors;
}

size_t StatsCollector::getCharsTyped() const
{
    return m_correctChars + m_errors;
}

double StatsCollector::getAccuracy() const
{
    const size_t total = getCharsTyped();
    return (total > 0) ? (100.0 * m_correctChars / total) : 0.0;
}

double StatsCollector::getWPM(double minutes) const
{
    return (minutes > 0) ? (m_correctChars / 5.0 / minutes) : 0.0;
}

size_t StatsCollector::getBackspaces() const
{
    return m_backspaces;
}

size_t StatsCollector::getMaxConsecutiveCorrect() const
{
    return m_maxConsecutiveCorrect;
}

long long StatsCollector::getAvgKeyIntervalMs() const
{
    return (m_timeIntervalsCount > 0) ? (m_totalTimeBetweenKeys.count() / m_timeIntervalsCount) : 0;
}

bool StatsCollector::isStarted() const
{
    return m_started;
}

std::chrono::steady_clock::time_point StatsCollector::getStartTime() const
{
    return m_startTime;
}

void StatsCollector::setStartTime(std::chrono::steady_clock::time_point t)
{
    m_startTime = t;
}

void StatsCollector::printStats() const
{
    if (!m_started)
    {
        std::cout << "Нет данных для статистики (ни одного символа не введено).\n";
        return;
    }

    const auto endTime = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(endTime - m_startTime).count();
    const double minutes = elapsed / 60.0;

    const size_t total = getCharsTyped();
    const double accuracy = getAccuracy();
    const double wpm = getWPM(minutes);

    std::cout
        << "\n========= СТАТИСТИКА ТЕКУЩЕЙ СЕССИИ =========\n"
        << "  Время работы:     " << elapsed << " сек.\n"
        << "  Правильных симв.: " << m_correctChars << "\n"
        << "  Ошибок:           " << m_errors << "\n"
        << "  Точность:         " << std::fixed << std::setprecision(1) << accuracy << "%\n"
        << "  Скорость (WPM):   " << std::fixed << std::setprecision(1) << wpm << "\n"
        << "  Нажатий Backspace: " << m_backspaces << "\n"
        << "  Макс. последовательность без ошибок: " << m_maxConsecutiveCorrect << " симв.\n";

    if (m_timeIntervalsCount > 0)
    {
        double avgMs = static_cast<double>(m_totalTimeBetweenKeys.count()) / m_timeIntervalsCount;
        std::cout << "  Среднее время между нажатиями: " << std::fixed << std::setprecision(1) << avgMs << " мс\n";
    }
    std::cout << "==========================================\n";
}
