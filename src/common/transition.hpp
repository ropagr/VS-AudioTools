// SPDX-License-Identifier: MIT

#pragma once

#include <map>
#include <string>

namespace common
{
    enum class TransitionType
    {
        Linear,
        Cubic,
        Sine,
    };

    std::map<std::string, TransitionType> getStringTransitionTypeMap();


    class Transition
    {
    public:
        virtual ~Transition() = default;

        virtual double calcY(double x) = 0;
    };


    class LinearTransition : public Transition
    {
    public:
        LinearTransition(double x1, double y1, double x2, double y2);

        double calcY(double x) override;

    private:
        double x1;
        double y1;
        double x2;
        double y2;

        double gradient;
        double yOffset;
    };


    class CubicTransition : public Transition
    {
    public:
        CubicTransition(double x1, double y1, double x2, double y2);

        double calcY(double x) override;

    private:
        double x1;
        double y1;
        double x2;
        double y2;
        double factor3 = 0;
        double factor2 = 0;
    };


    class SineTransition : public Transition
    {
    public:
        SineTransition(double x1, double y1, double x2, double y2);

        double calcY(double x) override;

    private:
        double x1;
        double y1;
        double x2;
        double y2;

        double xScale;
        double yScale;
        double yOffset;
    };


    Transition* newTransition(TransitionType type, double x1, double y1, double x2, double y2);
}
