#include <iostream>
#include <unistd.h>

#include "session_manager.h"
#include "typer.h"
#include "utf8_utils.h"

void Typer::run(
    const std::string& filepath,
    const std::string& utf8Content,
    std::string& savedInput,
    bool reset
)
{
    m_text = utf8utils::decodeUtf8(utf8Content);

    // Загружаем позицию
    size_t position = 0;
    if (!reset)
        SessionManager::loadPosition(filepath, position);
    else
        position = 0;

    m_input.clear();
    if (position > 0 && position <= m_text.size())
        m_input = m_text.substr(0, position);
    else if (position > m_text.size())
        m_input = m_text;

    m_stats.reset();

    // Показываем сводку прошлых сессий
    if (!reset)
        SessionManager::loadStatsSummary(filepath, true);

    // Если файл пуст
    if (m_text.empty())
    {
        SessionManager::erasePosition(filepath);
        m_terminal.disableRaw();
        m_terminal.clearScreen();
        std::cout << "\033[H";
        std::cout << "Файл пуст. Прогресс сброшен.\n\n";
        m_stats.printStats();
        std::cout.flush();
        savedInput = utf8utils::encodeUtf8(m_input);
        return;
    }

    m_terminal.enableRaw();
    m_terminal.clearScreen();

    bool completed = false;

    while (true)
    {
        m_renderer.draw(m_text, m_input);

        char32_t ch = m_terminal.getChar();

        if (ch == 27) // ESC
            break;

        auto now = std::chrono::steady_clock::now();
        long long diffMs = -1;
        if (m_stats.isStarted())
        {
            const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_stats.getStartTime()
            );
            diffMs = diff.count();
        }

        if (ch == 127 || ch == '\b')
        {
            m_stats.onBackspace();
            if (!m_input.empty())
                m_input.pop_back();
            continue;
        }

        if (m_input.size() >= m_text.size())
            continue;

        // Измеряем время между нажатиями
        static auto lastKeyTime = std::chrono::steady_clock::now();
        long long intervalMs = -1;
        if (m_stats.isStarted())
        {
            intervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastKeyTime).count();
        }
        lastKeyTime = now;

        m_input.push_back(ch);
        size_t pos = m_input.size() - 1;
        bool isCorrect = (pos < m_text.size() && ch == m_text[pos]);
        if (pos >= m_text.size())
            isCorrect = false;

        m_stats.onKeyPress(ch, isCorrect, intervalMs);

        if (m_input.size() == m_text.size())
        {
            completed = true;
            break;
        }
    }

    m_terminal.disableRaw();
    m_terminal.clearScreen();
    std::cout << "\033[H";

    // Итоговые данные для сохранения
    auto endTime = std::chrono::steady_clock::now();
    auto startTime = m_stats.getStartTime();
    size_t charsTyped = m_stats.getCharsTyped();
    size_t errors = m_stats.getErrors();
    double accuracy = m_stats.getAccuracy();
    double minutes = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count() / 60.0;
    double wpm = m_stats.getWPM(minutes);
    size_t backspaces = m_stats.getBackspaces();
    size_t maxConsec = m_stats.getMaxConsecutiveCorrect();
    long long avgInterval = m_stats.getAvgKeyIntervalMs();

    if (completed)
    {
        SessionManager::erasePosition(filepath);
        std::cout << "Поздравляем! Вы ввели все символы текста. Прогресс сброшен.\n";
    }
    else
    {
        SessionManager::savePosition(filepath, m_input.size());
        std::cout << "Прогресс сохранён. Введено символов: " << m_input.size() << "\n";
    }

    // Сохраняем статистику сессии, если что-то вводили
    if (m_stats.isStarted())
    {
        SessionManager::saveStats(
            filepath,
            startTime,
            endTime,
            charsTyped,
            errors,
            accuracy,
            wpm,
            backspaces,
            maxConsec,
            avgInterval
        );
    }

    std::cout << "\n";
    m_stats.printStats();
    std::cout.flush();

    savedInput = utf8utils::encodeUtf8(m_input);
}
