// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

#include "VapourSynth4.h"

#include "fade.hpp"
#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "common/transition.hpp"
#include "vsmap/vsmap.hpp"
#include "vsmap/vsmap_common.hpp"

constexpr const char* FuncName = "FadeOut";

constexpr int64_t DefaultFadeSamples = 0;
constexpr common::TransitionType DefaultFadeType = common::TransitionType::Cubic;
constexpr common::OverflowMode DefaultOverflowMode = common::OverflowMode::Error;
constexpr common::OverflowLog DefaultOverflowLog = common::OverflowLog::Once;


static void VS_CC fadeoutCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
    // clip:anode
    int err = 0;
    VSNode* audio = vsapi->mapGetNode(in, "clip", 0, &err);
    if (err)
    {
        return;
    }

    const VSAudioInfo* audioInfo = vsapi->getAudioInfo(audio);

    // check for supported audio format
    auto optSampleType = common::getSampleTypeFromAudioFormat(audioInfo->format);
    if (!optSampleType.has_value())
    {
        std::string errMsg = std::format("{}: unsupported audio format", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio);
        return;
    }

    // samples:int:opt
    // seconds:float:opt
    // samples has a higher priority than seconds
    int64_t fadeSamples = vsmap::getOptSamples("samples", "seconds", in, out, vsapi, DefaultFadeSamples, audioInfo->sampleRate);
    if (fadeSamples < 0)
    {
        std::string errMsg = std::format("{}: negative fade length", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio);
        return;
    }

    // end_sample:int:opt
    // end_second:float:opt
    int64_t endSample = vsmap::getOptSamples("end_sample", "end_second", in, out, vsapi, audioInfo->numSamples, audioInfo->sampleRate);

    // channels:int[]:opt
    std::vector<int> defaultChannels;
    std::optional<std::vector<int>> optChannels = vsmap::getOptChannels("channels", FuncName, in, out, vsapi, defaultChannels, audioInfo->format.numChannels);

    if (!optChannels.has_value())
    {
        vsapi->freeNode(audio);
        return;
    }

    // type:data:opt
    std::optional<common::TransitionType> optFadeType = vsmap::getOptTransitionTypeFromString("type", FuncName, in, out, vsapi, DefaultFadeType);
    if (!optFadeType.has_value())
    {
        vsapi->freeNode(audio);
        return;
    }

    common::Transition* trans = common::newTransition(optFadeType.value(), 0, 1, static_cast<double>(fadeSamples) - 1, 0);

    // overflow:data:opt
    std::optional<common::OverflowMode> optOverflowMode = vsmap::getOptOverflowModeFromString("overflow", FuncName, in, out, vsapi, DefaultOverflowMode);
    if (!optOverflowMode.has_value())
    {
        vsapi->freeNode(audio);
        return;
    }

    if (optOverflowMode.value() == common::OverflowMode::KeepFloat && !common::isFloatSampleType(optSampleType.value()))
    {
        std::string errMsg = std::format("{}: cannot use 'keep_float' overflow mode with an integer sample type", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio);
        return;
    }

    // overflow_log:data:opt
    std::optional<common::OverflowLog> optOverflowLog = vsmap::getOptOverflowLogFromString("overflow_log", FuncName, in, out, vsapi, DefaultOverflowLog);
    if (!optOverflowLog.has_value())
    {
        vsapi->freeNode(audio);
        return;
    }

    Fade* data = new Fade(audio, audioInfo, endSample - fadeSamples, fadeSamples, optChannels.value(), trans, optOverflowMode.value(), optOverflowLog.value(), FuncName);

    VSFilterDependency deps[] = {{ audio, VSRequestPattern::rpStrictSpatial }};

    // fmParallelRequests: strict sequential frame requests for overflow logging
    vsapi->createAudioFilter(out, FuncName, &data->getOutInfo(), fadeGetFrame, fadeFree, VSFilterMode::fmParallelRequests, deps, 1, data, core);
}


void fadeoutInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi)
{
    vspapi->registerFunction(FuncName,
                             "clip:anode;"
                             "samples:int:opt;"
                             "seconds:float:opt;"
                             "end_sample:int:opt;"
                             "end_second:float:opt;"
                             "channels:int[]:opt;"
                             "type:data:opt;"
                             "overflow:data:opt;"
                             "overflow_log:data:opt;",
                             "return:anode;",
                             fadeoutCreate, nullptr, plugin);
}
