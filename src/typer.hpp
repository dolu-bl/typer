#pragma once

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

    Terminal term;
};