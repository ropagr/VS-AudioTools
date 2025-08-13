// SPDX-License-Identifier: MIT

#pragma once

#include <vector>

namespace utils
{
    template <typename T>
    bool vectorContains(const std::vector<T>& values, T value)
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }


    template<std::integral T>
    std::vector<T> vectorInvert(const std::vector<T>& numbers, T rangeStart, T rangeEnd)
    {
        std::vector<T> result;
        for (T n = rangeStart; n < rangeEnd; ++n)
        {
            if (!vectorContains(numbers, n))
            {
                result.push_back(n);
            }
        }
        return result;
    }
}
