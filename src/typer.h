#pragma once

#include <string>

#include "renderer.h"
#include "stats_collector.h"
#include "terminal.h"

class Typer final
{
public:
    void run(
        const std::string& filepath,
        const std::string& utf8Content,
        std::string& savedInput,
        bool reset
    );

private:
    Terminal m_terminal;
    Renderer m_renderer { m_terminal };
    StatsCollector m_stats;
    std::u32string m_text;
    std::u32string m_input;
};
