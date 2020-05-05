#pragma once

#include <string>

namespace http
{
    enum Color
    {
        black =  30,
        red = 31,
        green = 32,
        yellow = 33,
        blue = 34,
        magenta = 35,
        cyan = 36,
        white =  37
    };

    void log(std::string str, Color color);
} // namespace http
