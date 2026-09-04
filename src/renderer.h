#pragma once

#include <string>

#include "terminal.h"

class Renderer final
{
public:
    Renderer(Terminal& term);

    void draw(const std::u32string& text, const std::u32string& input);

private:
    Terminal& m_term;
};
