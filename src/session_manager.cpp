#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "session_manager.h"
#include "utf8_utils.h"

using json = nlohmann::json;

std::string SessionManager::statePath()
{
    const char* home = getenv("HOME");
    return std::string(home ? home : ".") + "/.typer_sessions.json";
}

bool SessionManager::loadPosition(const std::string& filepath, size_t& position)
{
    std::ifstream file(statePath());
    if (!file.is_open())
        return false;

    try
    {
        json root;
        file >> root;
        if (root.is_object() && root.contains(filepath))
        {
            const auto& entry = root[filepath];
            if (entry.is_number())
            {
                position = entry.get<size_t>();
                return true;
            }

            if (entry.is_string())
            {
                std::string utf8Input = entry.get<std::string>();
                std::u32string u32 = utf8utils::decodeUtf8(utf8Input);
                position = u32.size();
                return true;
            }

            if (entry.is_object()
                && entry.contains("position")
                && entry["position"].is_number())
            {
                position = entry["position"].get<size_t>();
                return true;
            }
        }
    }
    catch (...)
    {
    }
    return false;
}

void SessionManager::savePosition(const std::string& filepath, size_t position)
{
    json root;
    std::ifstream in(statePath());
    if (in.is_open())
    {
        try
        {
            in >> root;
        }
        catch (...)
        {
        }
    }
    if (!root.is_object())
        root = json::object();
    if (!root.contains(filepath) || !root[filepath].is_object())
        root[filepath] = json::object();
    root[filepath]["position"] = position;
    std::ofstream out(statePath());
    if (out.is_open())
    {
        out << root.dump(4);
        out.flush();
    }
}

void SessionManager::erasePosition(const std::string& filepath)
{
    json root;
    std::ifstream in(statePath());
    if (in.is_open())
    {
        try
        {
            in >> root;
        }
        catch (...)
        {
        }
    }
    if (root.is_object())
    {
        root.erase(filepath);
        std::ofstream out(statePath());
        if (out.is_open())
        {
            out << root.dump(4);
            out.flush();
        }
    }
}

void SessionManager::loadStatsSummary(const std::string& filepath, bool showSummary)
{
    if (!showSummary)
        return;

    std::ifstream in(statePath());
    if (!in.is_open())
        return;

    try
    {
        json root;
        in >> root;
        if (!root.is_object() || !root.contains(filepath) || !root[filepath].is_object())
            return;

        auto& entry = root[filepath];
        if (!entry.contains("stats") || !entry["stats"].is_object())
            return;

        auto& stats = entry["stats"];
        if (stats.contains("aggregated") && stats["aggregated"].is_object())
        {
            auto& agg = stats["aggregated"];
            std::cout << "\n--- Прошлые сессии для этого файла ---\n";
            if (agg.contains("total_sessions"))
                std::cout << "Всего сессий: " << agg["total_sessions"].get<size_t>() << "\n";
            if (agg.contains("total_time_sec"))
                std::cout << "Общее время: " << agg["total_time_sec"].get<size_t>() << " сек.\n";
            if (agg.contains("total_chars"))
                std::cout << "Всего символов: " << agg["total_chars"].get<size_t>() << "\n";
            if (agg.contains("total_errors"))
                std::cout << "Всего ошибок: " << agg["total_errors"].get<size_t>() << "\n";
            if (agg.contains("avg_accuracy"))
                std::cout << "Средняя точность: " << std::fixed << std::setprecision(1) << agg["avg_accuracy"].get<double>() << "%\n";
            if (agg.contains("avg_wpm"))
                std::cout << "Средняя скорость: " << std::fixed << std::setprecision(1) << agg["avg_wpm"].get<double>() << " WPM\n";
            std::cout << "----------------------------------------\n\n";
        }
    }
    catch (...)
    {
    }
}

void SessionManager::saveStats(
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
)
{
    json root;
    std::ifstream in(statePath());
    if (in.is_open())
    {
        try
        {
            in >> root;
        }
        catch (...)
        {
        }
    }
    if (!root.is_object())
        root = json::object();
    if (!root.contains(filepath) || !root[filepath].is_object())
        root[filepath] = json::object();
    auto& entry = root[filepath];
    if (!entry.contains("stats") || !entry["stats"].is_object())
        entry["stats"] = json::object();
    auto& stats = entry["stats"];

    if (!stats.contains("sessions") || !stats["sessions"].is_array())
        stats["sessions"] = json::array();

    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    std::string startStr = std::ctime(&now_t);
    startStr.pop_back();

    json session;
    session["start"] = startStr;
    session["end"] = startStr;
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    session["duration_sec"] = duration;
    session["chars_typed"] = charsTyped;
    session["errors"] = errors;
    session["accuracy"] = accuracy;
    session["wpm"] = wpm;
    session["backspaces"] = backspaces;
    session["max_consecutive_correct"] = maxConsecutive;
    session["avg_key_interval_ms"] = avgKeyIntervalMs;

    stats["sessions"].push_back(session);

    if (!stats.contains("aggregated") || !stats["aggregated"].is_object())
        stats["aggregated"] = json::object();
    auto& agg = stats["aggregated"];
    size_t totalSessions = agg.value("total_sessions", 0) + 1;
    size_t totalTime = agg.value("total_time_sec", 0) + duration;
    size_t totalChars = agg.value("total_chars", 0) + charsTyped;
    size_t totalErrors = agg.value("total_errors", 0) + errors;

    agg["total_sessions"] = totalSessions;
    agg["total_time_sec"] = totalTime;
    agg["total_chars"] = totalChars;
    agg["total_errors"] = totalErrors;

    if (totalSessions > 0)
    {
        agg["avg_accuracy"] = (totalChars > 0)
            ? (100.0 * (totalChars - totalErrors) / totalChars)
            : 0.0;
        agg["avg_wpm"] = (totalTime > 0)
            ? (static_cast<double>(totalChars) / 5.0 / (totalTime / 60.0))
            : 0.0;
    }
    else
    {
        agg["avg_accuracy"] = 0.0;
        agg["avg_wpm"] = 0.0;
    }

    std::ofstream out(statePath());
    if (out.is_open())
    {
        out << root.dump(4);
        out.flush();
    }
}
