// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "VapourSynth4.h"

namespace vsmap
{
    VSNode* getOptClip(const char* varName, const VSMap* in, const VSAPI* vsapi, VSNode* defaultValue);

    bool getOptBool(const char* varName, const VSMap* in, const VSAPI* vsapi, bool defaultValue);

    double getOptDouble(const char* varName, const VSMap* in, const VSAPI* vsapi, double defaultValue);

    float getOptFloat(const char* varName, const VSMap* in, const VSAPI* vsapi, float defaultValue);

    int getOptInt(const char* varName, const VSMap* in, const VSAPI* vsapi, int defaultValue);

    int64_t getOptInt64(const char* varName, const VSMap* in, const VSAPI* vsapi, int64_t defaultValue);

    std::optional<std::vector<int>> getIntArray(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi);

    std::vector<int> getOptIntArray(const char* varName, const VSMap* in, const VSAPI* vsapi, const std::vector<int>& defaultValue);

    std::optional<std::vector<int64_t>> getInt64Array(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi);

    std::vector<int64_t> getOptInt64Array(const char* varName, const VSMap* in, const VSAPI* vsapi, const std::vector<int64_t>& defaultValue);

    std::optional<std::vector<double>> getDoubleArray(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi);

    std::vector<double> getOptDoubleArray(const char* varName, const VSMap* in, const VSAPI* vsapi, const std::vector<double>& defaultValue);

    std::optional<std::vector<float>> getFloatArray(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi);

    std::vector<float> getOptFloatArray(const char* varName, const VSMap* in, const VSAPI* vsapi, const std::vector<float>& defaultValue);
}
