// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>

#include "common/transition.hpp"
#include "utils/array.hpp"

namespace common
{
    constexpr std::pair<std::string_view, TransitionType> strTransitionTypePairs[] =
    {
        { "linear", TransitionType::Linear },
        { "cubic",  TransitionType::Cubic },
        { "sine",   TransitionType::Sine },
    };


    std::map<std::string, TransitionType> getStringTransitionTypeMap()
    {
        return utils::constStringViewPairArrayToStringMap(strTransitionTypePairs);
    }


    LinearTransition::LinearTransition(double _x1, double _y1, double _x2, double _y2) :
        x1(_x1), y1(_y1), x2(_x2), y2(_y2)
    {
        if (x1 != x2)
        {
            gradient = (y2 - y1) / (x2 - x1);
            yOffset = y1 - gradient * x1;
        }
    }

    double LinearTransition::calcY(double x)
    {
        if (x1 == x2)
        {
            return y1;
        }

        return gradient * x + yOffset;
    }


    CubicTransition::CubicTransition(double _x1, double _y1, double _x2, double _y2) :
        x1(_x1), y1(_y1), x2(_x2), y2(_y2)
    {
        double yDiff = y2 - y1;
        double xDiff = x2 - x1;
        double xDiffPow2 = xDiff * xDiff;
        double xDiffPow3 = xDiff * xDiff * xDiff;

        if (x1 != x2)
        {
            factor2 =  3 * yDiff / xDiffPow2;
            factor3 = -2 * yDiff / xDiffPow3;
        }
    }

    double CubicTransition::calcY(double x)
    {
        if (x1 == x2)
        {
            return y1;
        }

        double x_x1Diff = x - x1;
        double x_x1DiffPow2 = x_x1Diff * x_x1Diff;
        double x_x1DiffPow3 = x_x1Diff * x_x1Diff * x_x1Diff;

        return factor3 * x_x1DiffPow3 + factor2 * x_x1DiffPow2 + y1;
    }


    SineTransition::SineTransition(double _x1, double _y1, double _x2, double _y2) :
        x1(_x1), y1(_y1), x2(_x2), y2(_y2)
    {
        xScale = std::numbers::pi / (x2 - x1);
        yScale = (y1 - y2) / 2.0;
        yOffset = (y1 + y2) / 2.0;
    }

    double SineTransition::calcY(double x)
    {
        if (x1 == x2)
        {
            return y1;
        }

        return std::cos((x - x1) * xScale) * yScale + yOffset;
    }


    Transition* newTransition(TransitionType type, double x1, double y1, double x2, double y2)
    {
        switch (type)
        {
            case TransitionType::Linear:
                return new LinearTransition(x1, y1, x2, y2);
            case TransitionType::Cubic:
                return new CubicTransition(x1, y1, x2, y2);
            case TransitionType::Sine:
                return new SineTransition(x1, y1, x2, y2);
            default:
                return nullptr;
        }
    }
}
