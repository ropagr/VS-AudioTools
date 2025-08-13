// SPDX-License-Identifier: MIT

#include <bitset>
#include <format>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "VapourSynth4.h"

#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "common/transition.hpp"
#include "utils/string.hpp"
#include "vsmap/vsmap.hpp"
#include "vsmap/vsmap_common.hpp"
#include "vsutils/audio.hpp"

namespace vsmap
{
    int64_t getOptSamples(const char* sampleVarName, const char* secondsVarName, const VSMap* in, VSMap* out, const VSAPI* vsapi, int64_t defaultValue, int sampleRate)
    {
        int err = 0;
        int64_t samples = vsapi->mapGetInt(in, sampleVarName, 0, &err);
        if (err)
        {
            // samples not defined -> try seconds
            err = 0;
            double seconds = vsapi->mapGetFloat(in, secondsVarName, 0, &err);
            if (err)
            {
                // seconds not defined
                return defaultValue;
            }

            return static_cast<int64_t>(sampleRate * seconds);
        }

        return samples;
    }


    uint64_t getOptChannelLayout(const char* varName, const VSMap* in, const VSAPI* vsapi, uint64_t defaultValue)
    {
        std::vector<int> defaultChannels = vsutils::getChannelsFromChannelLayout(defaultValue);
        std::vector<int> channels = getOptIntArray(varName, in, vsapi, defaultChannels);

        std::bitset<64> result;

        for (const int& ch : channels)
        {
            if (static_cast<int>(VSAudioChannels::acFrontLeft) <= ch && ch <= static_cast<int>(acLowFrequency2))
            {
                result.set(ch);
            }
        }
        return result.to_ullong();
    }


    std::optional<std::vector<int>> getOptChannels(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi,
                                                   const std::vector<int>& defaultValue, int numChannels)
    {
        std::vector<int> channels = getOptIntArray(varName, in, vsapi, defaultValue);

        if (channels.size() == 0)
        {
            // add all channels
            for (int ch = 0; ch < numChannels; ++ch)
            {
                channels.push_back(ch);
            }
        }
        else
        {
            // check provided channels
            for (const int& ch : channels)
            {
                if (numChannels <= ch)
                {
                    std::string errMsg = std::format("{}: invalid channel number: {}, number of channels: {}", logFuncName, ch, numChannels);
                    vsapi->mapSetError(out, errMsg.c_str());
                    return std::nullopt;
                }
            }
        }
        return channels;
    }


    std::optional<common::OverflowMode> getOverflowModeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi)
    {
        return getValueFromString(varName, logFuncName, in, out, vsapi, common::getStringOverflowModeMap());
    }


    std::optional<common::OverflowMode> getOptOverflowModeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, common::OverflowMode defaultValue)
    {
        return getOptValueFromString(varName, logFuncName, in, out, vsapi, common::getStringOverflowModeMap(), defaultValue);
    }


    std::optional<common::OverflowLog> getOverflowLogFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi)
    {
        return getValueFromString(varName, logFuncName, in, out, vsapi, common::getStringOverflowLogMap());
    }


    std::optional<common::OverflowLog> getOptOverflowLogFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, common::OverflowLog defaultValue)
    {
        return getOptValueFromString(varName, logFuncName, in, out, vsapi, common::getStringOverflowLogMap(), defaultValue);
    }


    std::optional<common::SampleType> getSampleTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi)
    {
        return getValueFromString(varName, logFuncName, in, out, vsapi, common::getStringSampleTypeMap());
    }


    std::optional<common::SampleType> getOptSampleTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, common::SampleType defaultValue)
    {
        return getOptValueFromString(varName, logFuncName, in, out, vsapi, common::getStringSampleTypeMap(), defaultValue);
    }


    std::optional<common::SampleType> getVapourSynthSampleTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi)
    {
        return getValueFromString(varName, logFuncName, in, out, vsapi, common::getStringVapourSynthSampleTypeMap());
    }


    std::optional<common::SampleType> getOptVapourSynthSampleTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, common::SampleType defaultValue)
    {
        return getOptValueFromString(varName, logFuncName, in, out, vsapi, common::getStringVapourSynthSampleTypeMap(), defaultValue);
    }


    std::optional<common::TransitionType> getTransitionTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi)
    {
        return getValueFromString(varName, logFuncName, in, out, vsapi, common::getStringTransitionTypeMap());
    }


    std::optional<common::TransitionType> getOptTransitionTypeFromString(const char* varName, const char* logFuncName, const VSMap* in, VSMap* out, const VSAPI* vsapi, common::TransitionType defaultValue)
    {
        return getOptValueFromString(varName, logFuncName, in, out, vsapi, common::getStringTransitionTypeMap(), defaultValue);
    }
}
