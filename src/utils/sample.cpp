// SPDX-License-Identifier: MIT

#include <algorithm>

#include "utils/sample.hpp"

namespace utils
{
    double clampDoubleSample(double sample)
    {
        return std::clamp<double>(sample, -1, 1);
    }
}
