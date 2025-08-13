// SPDX-License-Identifier: MIT

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "VapourSynth4.h"
#include "VSHelper4.h"

#include "normalize.hpp"
#include "common/overflow.hpp"
#include "common/peak.hpp"
#include "common/sampletype.hpp"
#include "utils/sample.hpp"
#include "utils/vector.hpp"
#include "vsmap/vsmap.hpp"
#include "vsmap/vsmap_common.hpp"
#include "vsutils/audio.hpp"
#include "vsutils/bitshift.hpp"

constexpr const char* FuncName = "Normalize";

constexpr double DefaultNormPeak = 1;
constexpr common::OverflowMode DefaultOverflowMode = common::OverflowMode::Error;
constexpr common::OverflowLog DefaultOverflowLog = common::OverflowLog::Once;


Normalize::Normalize(VSNode* _audio, const VSAudioInfo* _audioInfo, double _outNormPeak,
                     bool lowerOnly, std::vector<int> _editChannels,
                     common::OverflowMode _overflowMode, common::OverflowLog _overflowLog,
                     const VSAPI* vsapi) :
    audio(_audio), audioInfo(*_audioInfo), editChannels(_editChannels), overflowMode(_overflowMode), overflowLog(_overflowLog)
{
    outSampleType = common::getSampleTypeFromAudioFormat(audioInfo.format).value();

    outNormPeak = common::adjustNormPeak(_outNormPeak, outSampleType);

    // blocking operation
    double inNormPeak = common::findPeak(audio, &audioInfo, editChannels, true, vsapi);

    if (lowerOnly && inNormPeak <= outNormPeak)
    {
        gain = 1.0;
    }
    else
    {
        // !lowerOnly || outNormPeak < inNormPeak
        gain = outNormPeak / inNormPeak;
    }

    copyChannels = utils::vectorInvert(editChannels, 0, audioInfo.format.numChannels);
}


VSNode* Normalize::getAudio()
{
    return audio;
}

const VSAudioInfo& Normalize::getOutInfo()
{
    return audioInfo;
}


void Normalize::resetOverflowStats()
{
    overflowStats = { .count = 0, .peak = 0.0 };
}


void Normalize::logOverflowStats(VSCore* core, const VSAPI* vsapi)
{
    if (0 < overflowStats.count)
    {
        overflowStats.logVS(FuncName, overflowMode, isFloatSampleType(outSampleType), core, vsapi);
    }
}


void Normalize::free(const VSAPI* vsapi)
{
    vsapi->freeNode(audio);
}



template <typename sample_t, size_t IntSampleBits>
bool Normalize::writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen, const VSFrame* inFrm, const common::OverflowContext& ofCtx)
{
    sample_t* outFrmPtr = reinterpret_cast<sample_t*>(ofCtx.vsapi->getWritePtr(outFrm, ch));

    const sample_t* inFrmPtr = reinterpret_cast<const sample_t*>(ofCtx.vsapi->getReadPtr(inFrm, ch));

    constexpr vsutils::BitShift bitShift = vsutils::getSampleBitShift<sample_t, IntSampleBits>();

    for (int s = 0; s < outFrmLen; ++s)
    {
        int64_t outPos = outPosFrmStart + s;

        sample_t inSample = inFrmPtr[s];

        if constexpr (bitShift.required)
        {
            inSample >>= bitShift.count;
        }

        double scaledSample = std::clamp(gain * utils::convSampleToDouble<sample_t, IntSampleBits>(inSample), -outNormPeak, outNormPeak);

        if (!common::safeWriteSample<sample_t, IntSampleBits>(scaledSample, outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
        {
            // overflow and error
            return false;
        }
    }

    return true;
}


template <typename sample_t, size_t IntSampleBits>
bool Normalize::writeFrameImpl(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, const common::OverflowContext& ofCtx)
{
    // copy channels
    for (const int& ch : copyChannels)
    {
        vsutils::copyFrameChannel(ch, outFrm, inFrm, getOutInfo().format.bytesPerSample, ofCtx.vsapi);
    }

    // edit channels
    int64_t outPosFrmStart = vsutils::frameToFirstSample(outFrmNum);
    int outFrmLen = ofCtx.vsapi->getFrameLength(outFrm);

    for (const int& ch : editChannels)
    {
        if (!writeFrameChannel<sample_t, IntSampleBits>(ch, outFrm, outPosFrmStart, outFrmLen, inFrm, ofCtx))
        {
            return false;
        }
    }
    return true;
}



bool Normalize::writeFrame(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm,
                           VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
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


static void VS_CC normalizeFree(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
    Normalize* data = static_cast<Normalize*>(instanceData);
    // skip logging since the total number of overflows is unreliable
    // data->logNumOverflows(core, vsapi);
    data->free(vsapi);
    delete data;
}


static const VSFrame* VS_CC normalizeGetFrame(int outFrmNum, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    Normalize* data = static_cast<Normalize*>(instanceData);

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

        VSFrame* outFrm = vsapi->newAudioFrame(&data->getOutInfo().format, inFrmLen, inFrm, core);

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


static void VS_CC normalizeCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
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

    // peak:float:opt
    double outNormPeak = vsmap::getOptDouble("peak", in, vsapi, DefaultNormPeak);
    if (outNormPeak < 0)
    {
        std::string errMsg = std::format("{}: negative peak", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio);
        return;
    }

    if (1 < outNormPeak)
    {
        std::string warnMsg = std::format("{}: peak greater than 1 -> possible clipping", FuncName);
        vsapi->logMessage(VSMessageType::mtWarning, warnMsg.c_str(), core);
    }

    // lower_only:int:opt
    bool lowerOnly = vsmap::getOptBool("lower_only", in, vsapi, false);

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

    Normalize* data = new Normalize(audio, audioInfo, outNormPeak, lowerOnly, optChannels.value(), optOverflowMode.value(), optOverflowLog.value(), vsapi);

    VSFilterDependency deps[] = {{ audio, rpStrictSpatial }};

    // fmParallelRequests: strict sequential frame requests for overflow logging
    vsapi->createAudioFilter(out, FuncName, &data->getOutInfo(), normalizeGetFrame, normalizeFree, VSFilterMode::fmParallelRequests, deps, 1, data, core);
}


void normalizeInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi)
{
    vspapi->registerFunction(FuncName,
                             "clip:anode;"
                             "peak:float:opt;"
                             "lower_only:int:opt;"
                             "channels:int[]:opt;"
                             "overflow:data:opt;"
                             "overflow_log:data:opt;",
                             "return:anode;",
                             normalizeCreate, nullptr, plugin);
}
