#pragma once

#include <chrono>
#include <string>

#include <nlohmann/json.hpp>

class SessionManager
{
public:
    static std::string statePath();

    // Позиция (прогресс)
    static bool loadPosition(const std::string& filepath, size_t& position);
    static void savePosition(const std::string& filepath, size_t position);
    static void erasePosition(const std::string& filepath);

    // Статистика
    static void loadStatsSummary(const std::string& filepath, bool showSummary);
    static void saveStats(
        const std::string& filepath,
        const std::chrono::steady_clock::time_point& start,
        const std::chrono::steady_clock::time_point& end,
        size_t charsTyped,
        size_t errors,
        double accuracy,
        double wpm,
        size_t backspaces,
        size_t maxConsecutive,
        long long avgKeyIntervalMs
    );
};
