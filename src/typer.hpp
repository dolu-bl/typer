#pragma once

#include <chrono>
#include <string>

#include "terminal.hpp"

class Typer final
{
public:
    void run(
        const std::string& filepath,
        const std::string& content,
        size_t& offset,
        bool reset
    );

private:
    void draw(const std::string& text, size_t pos, bool error);
    void loadState(const std::string& filepath, size_t& offset);
    void saveState(const std::string& filepath, size_t offset) const;
    void printStats() const;

    Terminal term;

    size_t m_totalChars = 0;
    size_t m_correctChars = 0;
    size_t m_errors = 0;
    std::chrono::steady_clock::time_point m_startTime;
    bool m_started = false;
};
