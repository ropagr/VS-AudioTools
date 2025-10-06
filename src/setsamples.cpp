// SPDX-License-Identifier: MIT

#include <climits>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "VapourSynth4.h"

#include "setsamples.hpp"
#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "utils/sample.hpp"
#include "utils/vector.hpp"
#include "vsmap/vsmap.hpp"
#include "vsmap/vsmap_common.hpp"
#include "vsutils/audio.hpp"
#include "vsutils/bitshift.hpp"

constexpr const char* FuncName = "SetSamples";

constexpr int64_t DefaultStartSample = 0;
constexpr common::OverflowMode DefaultOverflowMode = common::OverflowMode::Error;
constexpr common::OverflowLog DefaultOverflowLog = common::OverflowLog::Once;


SetSamples::SetSamples(VSNode* _audio, const VSAudioInfo* _audioInfo, double _sample, int64_t _outPosStart, int64_t _outPosEnd, std::vector<int> _channels,
                       common::OverflowMode _overflowMode, common::OverflowLog _overflowLog) :
    audio(_audio), audioInfo(*_audioInfo), sample(_sample), outPosStart(_outPosStart), outPosEnd(_outPosEnd),
    editChannels(_channels), overflowMode(_overflowMode), overflowLog(_overflowLog)
{
    outSampleType = common::getSampleTypeFromAudioFormat(audioInfo.format).value();

    copyChannels = utils::vectorInvert(editChannels, 0, audioInfo.format.numChannels);
}


VSNode* SetSamples::getAudio()
{
    return audio;
}


const VSAudioInfo& SetSamples::getOutInfo()
{
    return audioInfo;
}


void SetSamples::resetOverflowStats()
{
    overflowStats = { .count = 0, .peak = 0.0 };
}


void SetSamples::logOverflowStats(VSCore* core, const VSAPI* vsapi)
{
    if (0 < overflowStats.count)
    {
        overflowStats.logVS(FuncName, overflowMode, isFloatSampleType(outSampleType), core, vsapi);
    }
}


void SetSamples::free(const VSAPI* vsapi)
{
    vsapi->freeNode(audio);
}


template <typename sample_t, size_t IntSampleBits>
bool SetSamples::writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen, const VSFrame* inFrm,
                                   const common::OverflowContext& ofCtx)
{
    sample_t* outFrmPtr = reinterpret_cast<sample_t*>(ofCtx.vsapi->getWritePtr(outFrm, ch));

    const sample_t* inFrmPtr = reinterpret_cast<const sample_t*>(ofCtx.vsapi->getReadPtr(inFrm, ch));

    constexpr vsutils::BitShift bitShift = vsutils::getSampleBitShift<sample_t, IntSampleBits>();

    double dSample = utils::convSampleToDouble<sample_t, IntSampleBits>(utils::castSample<sample_t, IntSampleBits>(sample));

    for (int s = 0; s < outFrmLen; ++s)
    {
        int64_t outPos = outPosFrmStart + s;

        if (outPosStart <= outPos && outPos < outPosEnd)
        {
            if (!common::safeWriteSample<sample_t, IntSampleBits>(dSample, outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
            {
                // overflow and error
                return false;
            }
        }
        else
        {
            // copy sample from inFrm
            sample_t inSample = inFrmPtr[s];

            if constexpr (bitShift.required)
            {
                inSample >>= bitShift.count;
            }

            if (!common::safeWriteSample<sample_t, IntSampleBits>(utils::convSampleToDouble<sample_t, IntSampleBits>(inSample), outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
            {
                // overflow and error
                return false;
            }
        }
    }
    return true;
}


template <typename sample_t, size_t IntSampleBits>
bool SetSamples::writeFrameImpl(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, const common::OverflowContext& ofCtx)
{
    int bytesPerSample = audioInfo.format.bytesPerSample;

    // copy channels
    for (const int& ch : copyChannels)
    {
        vsutils::copyFrameChannel(outFrm, ch, inFrm, ch, bytesPerSample, ofCtx.vsapi);
    }

    // edit channels
    int64_t outPosFrmStart = vsutils::frameToFirstSample(outFrmNum);
    int outFrmLen = ofCtx.vsapi->getFrameLength(inFrm);

    for (const int& ch : editChannels)
    {
        if (!writeFrameChannel<sample_t, IntSampleBits>(ch, outFrm, outPosFrmStart, outFrmLen, inFrm, ofCtx))
        {
            return false;
        }
    }
    return true;
}


bool SetSamples::writeFrame(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    common::OverflowContext ofCtx =
        { .mode = overflowMode, .log = overflowLog, .funcName = FuncName,
          .frameCtx = frameCtx, .core = core, .vsapi = vsapi };

    switch (outSampleType)
    {
        case common::SampleType::Int8:
            return writeFrameImpl<int8_t, 8>(outFrm, outFrmNum, inFrm, ofCtx);
        case common::SampleType::Int16:
            return writeFrameImpl<int16_t, 16>(outFrm, outFrmNum, inFrm, ofCtx);
        case common::SampleType::Int24:
            return writeFrameImpl<int32_t, 24>(outFrm, outFrmNum, inFrm, ofCtx);
        case common::SampleType::Int32:
            return writeFrameImpl<int32_t, 32>(outFrm, outFrmNum, inFrm, ofCtx);
        case common::SampleType::Float32:
            return writeFrameImpl<float, 0>(outFrm, outFrmNum, inFrm, ofCtx);
        case common::SampleType::Float64:
            return writeFrameImpl<double, 0>(outFrm, outFrmNum, inFrm, ofCtx);
        default:
            return false;
    }
}


void VS_CC setsamplesFree(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
    SetSamples* data = static_cast<SetSamples*>(instanceData);
    // skip logging since the total number of overflows is unreliable
    // data->logNumOverflows(core, vsapi);
    data->free(vsapi);
    delete data;
}


const VSFrame* VS_CC setsamplesGetFrame(int outFrmNum, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    SetSamples* data = reinterpret_cast<SetSamples*>(instanceData);

    if (activationReason == VSActivationReason::arInitial)
    {
        vsapi->requestFrameFilter(outFrmNum, data->getAudio(), frameCtx);
        return nullptr;
    }

    if (activationReason == VSActivationReason::arAllFramesReady)
    {
        if (outFrmNum == 0)
        {
            data->resetOverflowStats();
        }

        const VSFrame* inFrm = vsapi->getFrameFilter(outFrmNum, data->getAudio(), frameCtx);

        int inFrmLen = vsapi->getFrameLength(inFrm);

        VSFrame *outFrm = vsapi->newAudioFrame(&data->getOutInfo().format, inFrmLen, inFrm, core);

        bool success = data->writeFrame(outFrm, outFrmNum, inFrm, frameCtx, core, vsapi);

        vsapi->freeFrame(inFrm);

        if (outFrmNum == data->getOutInfo().numFrames - 1)
        {
            // last frame
            data->logOverflowStats(core, vsapi);
        }

        if (success)
        {
            return outFrm;
        }

        vsapi->freeFrame(outFrm);
    }
    return nullptr;
}


static void VS_CC setsamplesCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
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

    // sample:float
    err = 0;
    double sample = vsapi->mapGetFloat(in, "sample", 0, &err);
    if (err)
    {
        vsapi->freeNode(audio);
        return;
    }

    // start_sample:int:opt
    int64_t startSample = vsmap::getOptInt64("start_sample", in, vsapi, DefaultStartSample);

    // end_sample:int:opt
    int64_t endSample = vsmap::getOptInt64("end_sample", in, vsapi, audioInfo->numSamples);

    // channels:int[]:opt
    std::vector<int> defaultChannels;
    std::optional<std::vector<int>> optChannels = vsmap::getOptChannels("channels", FuncName, in, out, vsapi, defaultChannels, audioInfo->format.numChannels);

    if (!optChannels.has_value())
    {
        vsapi->freeNode(audio);
        return;
    }

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

    SetSamples* data = new SetSamples(audio, audioInfo, sample, startSample, endSample, optChannels.value(), optOverflowMode.value(), optOverflowLog.value());

    VSFilterDependency deps[] = {{ audio, VSRequestPattern::rpStrictSpatial }};

    // fmParallelRequests: strict sequential frame requests for overflow logging
    vsapi->createAudioFilter(out, FuncName, &data->getOutInfo(), setsamplesGetFrame, setsamplesFree, VSFilterMode::fmParallelRequests, deps, 1, data, core);
}


void setsamplesInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi)
{
    vspapi->registerFunction(FuncName,
                             "clip:anode;"
                             "sample:float;"
                             "start_sample:int:opt;"
                             "end_sample:int:opt;"
                             "channels:int[]:opt;"
                             "overflow:data:opt;"
                             "overflow_log:data:opt;",
                             "return:anode;",
                             setsamplesCreate, nullptr, plugin);
}
