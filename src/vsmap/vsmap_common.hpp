// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "VapourSynth4.h"

#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "common/transition.hpp"
#include "utils/map.hpp"
#include "utils/string.hpp"

namespace vsmap
{
    int64_t getOptSamples(const char* varNameSamples, const char* varNameSeconds, const VSMap* in, VSMap* out, const VSAPI* vsapi, int64_t defaultValue, int sampleRate);

    uint64_t getOptChannelLayout(const char* varName, const VSMap* in, const VSAPI* vsapi, uint64_t defaultValue);

    std::optional<std::vector<int>> getOptChannels(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, const std::vector<int>& defaultValue, int numChannels);


    template<typename T>
    static std::optional<T> getValueFromStringImpl(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, const std::map<std::string, T>& strValueMap, std::optional<T> defaultValue)
    {
        int err = 0;
        const char* strVarChars = vsapi->mapGetData(in, varName, 0, &err);
        if (err)
        {
            // string variable not defined
            if (defaultValue.has_value())
            {
                // return provided default value
                return defaultValue;
            }

            // no default value provided -> error
            std::string allowedValues = utils::stringJoin(utils::mapGetKeys(strValueMap), ", ");

            std::string errMsg = std::format("{}: {} not specified, must be one of: {}", logFuncName, varName, allowedValues);
            vsapi->mapSetError(out, errMsg.c_str());
            return std::nullopt;
        }

        std::string strVar(strVarChars);
        if (auto optSampleType = utils::mapGet(strValueMap, strVar))
        {
            return optSampleType;
        }

        std::string allowedValues = utils::stringJoin(utils::mapGetKeys(strValueMap), ", ");

        std::string errMsg = std::format("{}: invalid {} value: {}, must be one of: {}", logFuncName, varName, strVar, allowedValues);
        vsapi->mapSetError(out, errMsg.c_str());
        return std::nullopt;
    }


    template<typename T>
    std::optional<T> getOptValueFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, std::map<std::string, T> strValueMap, T defaultValue)
    {
        return getValueFromStringImpl<T>(varName, logFuncName, in, out, vsapi, strValueMap, defaultValue);
    }


    template<typename T>
    std::optional<T> getValueFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, std::map<std::string, T> strValueMap)
    {
        return getValueFromStringImpl<T>(varName, logFuncName, in, out, vsapi, strValueMap, std::nullopt);
    }

    /** no error handling needed **/
    std::optional<common::OverflowMode> getOverflowModeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi);

    /** no error handling needed **/
    std::optional<common::OverflowMode> getOptOverflowModeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, common::OverflowMode defaultValue);

    /** no error handling needed **/
    std::optional<common::OverflowLog> getOverflowLogFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi);

    /** no error handling needed **/
    std::optional<common::OverflowLog> getOptOverflowLogFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, common::OverflowLog defaultValue);

    /** no error handling needed **/
    std::optional<common::SampleType> getSampleTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi);

    /** no error handling needed **/
    std::optional<common::SampleType> getOptSampleTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, common::SampleType defaultValue);

    /** no error handling needed **/
    std::optional<common::SampleType> getVapourSynthSampleTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi);

    /** no error handling needed **/
    std::optional<common::SampleType> getOptVapourSynthSampleTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, common::SampleType defaultValue);

    /** no error handling needed **/
    std::optional<common::TransitionType> getTransitionTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi);

    /** no error handling needed **/
    std::optional<common::TransitionType> getOptTransitionTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, common::TransitionType defaultValue);
}
