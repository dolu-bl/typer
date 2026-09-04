#include "utf8_utils.h"

namespace utf8utils
{

std::u32string decodeUtf8(const std::string& utf8)
{
    std::u32string out;
    size_t i = 0;
    while (i < utf8.size())
    {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        size_t len;
        if ((c & 0x80) == 0)
            len = 1;
        else if ((c & 0xE0) == 0xC0)
            len = 2;
        else if ((c & 0xF0) == 0xE0)
            len = 3;
        else if ((c & 0xF8) == 0xF0)
            len = 4;
        else
        {
            ++i;
            continue;
        }

        if (i + len > utf8.size())
            break;

        char32_t cp = 0;
        if (len == 1)
            cp = c;
        else if (len == 2)
            cp = ((c & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
        else if (len == 3)
            cp = ((c & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
        else if (len == 4)
            cp = ((c & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) | ((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);

        out.push_back(cp);
        i += len;
    }
    return out;
}

std::string encodeUtf8(const std::u32string& u32str)
{
    std::string out;
    for (char32_t cp : u32str)
    {
        if (cp <= 0x7F)
        {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp <= 0x7FF)
        {
            out.push_back(0xC0 | ((cp >> 6) & 0x1F));
            out.push_back(0x80 | (cp & 0x3F));
        }
        else if (cp <= 0xFFFF)
        {
            out.push_back(0xE0 | ((cp >> 12) & 0x0F));
            out.push_back(0x80 | ((cp >> 6) & 0x3F));
            out.push_back(0x80 | (cp & 0x3F));
        }
        else if (cp <= 0x10FFFF)
        {
            out.push_back(0xF0 | ((cp >> 18) & 0x07));
            out.push_back(0x80 | ((cp >> 12) & 0x3F));
            out.push_back(0x80 | ((cp >> 6) & 0x3F));
            out.push_back(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

} // namespace utf8utils
