#pragma once

#include <string>

namespace utf8utils
{

std::u32string decodeUtf8(const std::string& utf8);
std::string encodeUtf8(const std::u32string& u32str);

} // namespace utf8utils
