// SPDX-License-Identifier: MIT

#include <format>
#include <optional>
#include <string>
#include <vector>

#include "VapourSynth4.h"

#include "common/peak.hpp"
#include "common/sampletype.hpp"
#include "vsmap/vsmap.hpp"
#include "vsmap/vsmap_common.hpp"

constexpr const char* FuncName = "FindPeak";

constexpr bool DefaultNormalize = true;


static void VS_CC findpeakCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
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
    std::optional<common::SampleType> optSampleType = common::getSampleTypeFromAudioFormat(audioInfo->format);
    if (!optSampleType.has_value())
    {
        std::string errMsg = std::format("{}: unsupported audio format", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio);
        return;
    }

    // normalize:int:opt
    bool normalize = vsmap::getOptBool("normalize", in, vsapi, DefaultNormalize);

    // channels:int[]:opt
    std::vector<int> defaultChannels;
    std::optional<std::vector<int>> optChannels = vsmap::getOptChannels("channels", FuncName, in, out, vsapi, defaultChannels, audioInfo->format.numChannels);
    if (!optChannels.has_value())
    {
        vsapi->freeNode(audio);
        return;
    }

    // blocking operation
    double peak = common::findPeak(audio, audioInfo, optChannels.value(), normalize, vsapi);
    vsapi->freeNode(audio);

    vsapi->mapSetFloat(out, "return", peak, VSMapAppendMode::maReplace);
}


void findpeakInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi)
{
    vspapi->registerFunction(FuncName,
                             "clip:anode;"
                             "normalize:int:opt;"
                             "channels:int[]:opt;",
                             "return:float;",
                             findpeakCreate, nullptr, plugin);
}
