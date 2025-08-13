// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "VapourSynth4.h"

#include "sinetone.hpp"
#include "common/overflow.hpp"
#include "common/peak.hpp"
#include "common/sampletype.hpp"
#include "vsmap/vsmap.hpp"
#include "vsmap/vsmap_common.hpp"
#include "vsutils/audio.hpp"
#include "vsutils/bitshift.hpp"

constexpr const char* FuncName = "SineTone";

constexpr int DefaultSampleRate = 44100;
constexpr int DefaultSeconds = 10;
constexpr common::SampleType DefaultSampleType = common::SampleType::Int16;
constexpr double DefaultFrequency = 500;
constexpr double DefaultAmplitude = 1;
constexpr common::OverflowMode DefaultOverflowMode = common::OverflowMode::Error;
constexpr common::OverflowLog DefaultOverflowLog = common::OverflowLog::Once;


SineTone::SineTone(int64_t numSamples, uint64_t channelLayout, int sampleRate, common::SampleType _sampleType, double _freq, double _amplitude,
                   common::OverflowMode _overflowMode, common::OverflowLog _overflowLog) :
    outSampleType(_sampleType), freq(_freq), overflowMode(_overflowMode), overflowLog(_overflowLog)
{
    outInfo = VSAudioInfo();
    outInfo.numSamples = numSamples;
    outInfo.numFrames = vsutils::samplesToFrames(numSamples);
    outInfo.sampleRate = sampleRate;

    outInfo.format.numChannels = static_cast<int>(vsutils::getChannelsFromChannelLayout(channelLayout).size());
    outInfo.format.channelLayout = channelLayout;

    common::applySampleTypeToAudioFormat(outSampleType, outInfo.format);

    amplitude = common::adjustNormPeak(_amplitude, outSampleType);
    absAmplitude = std::abs(amplitude);
}


const VSAudioInfo& SineTone::getOutInfo()
{
    return outInfo;
}


void SineTone::resetOverflowStats()
{
    overflowStats = { .count = 0, .peak = 0.0 };
}


void SineTone::logOverflowStats(VSCore* core, const VSAPI* vsapi)
{
    if (0 < overflowStats.count)
    {
        overflowStats.logVS(FuncName, overflowMode, isFloatSampleType(outSampleType), core, vsapi);
    }
}


void SineTone::free(const VSAPI* vsapi)
{
}


template <typename sample_t, size_t IntSampleBits>
bool SineTone::writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen,
                                 const common::OverflowContext& ofCtx)
{
    sample_t* outFrmPtr = reinterpret_cast<sample_t*>(ofCtx.vsapi->getWritePtr(outFrm, ch));

    for (int s = 0; s < outFrmLen; ++s)
    {
        int64_t outPos = outPosFrmStart + s;

        double seconds = vsutils::samplesToSeconds(outPos, outInfo.sampleRate);

        // clamp the result to the amplitude in case of precision inaccuracies
        double sample = std::clamp(amplitude * std::sin(2 * std::numbers::pi * seconds * freq), -absAmplitude, absAmplitude);

        if (!common::safeWriteSample<sample_t, IntSampleBits>(sample, outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
        {
            // overflow and error
            return false;
        }
    }

    return true;
}


template <typename sample_t, size_t IntSampleBits>
bool SineTone::writeFrameImpl(VSFrame* outFrm, int outFrmNum, const common::OverflowContext& ofCtx)
{
    int64_t outPosFrmStart = vsutils::frameToFirstSample(outFrmNum);
    int outFrmLen = ofCtx.vsapi->getFrameLength(outFrm);

    for (int ch = 0; ch < outInfo.format.numChannels; ++ch)
    {
        if (!writeFrameChannel<sample_t, IntSampleBits>(ch, outFrm, outPosFrmStart, outFrmLen, ofCtx))
        {
            return false;
        }
    }
    return true;
}



bool SineTone::writeFrame(VSFrame* outFrm, int outFrmNum, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    common::OverflowContext ofCtx =
        { .mode = overflowMode, .log = overflowLog, .funcName = FuncName,
          .frameCtx = frameCtx, .core = core, .vsapi = vsapi };

    switch (outSampleType)
    {
        case common::SampleType::Int8:
            return writeFrameImpl<int8_t, 8>(outFrm, outFrmNum, ofCtx);
        case common::SampleType::Int16:
            return writeFrameImpl<int16_t, 16>(outFrm, outFrmNum, ofCtx);
        case common::SampleType::Int24:
            return writeFrameImpl<int32_t, 24>(outFrm, outFrmNum, ofCtx);
        case common::SampleType::Int32:
            return writeFrameImpl<int32_t, 32>(outFrm, outFrmNum, ofCtx);
        case common::SampleType::Float32:
            return writeFrameImpl<float, 0>(outFrm, outFrmNum, ofCtx);
        case common::SampleType::Float64:
            return writeFrameImpl<double, 0>(outFrm, outFrmNum, ofCtx);
        default:
            return false;
    }
}


void VS_CC sinetoneFree(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
    SineTone* data = static_cast<SineTone*>(instanceData);
    // skip logging since the total number of overflows is unreliable
    // data->logNumOverflows(core, vsapi);
    data->free(vsapi);
    delete data;
}


const VSFrame* VS_CC sinetoneGetFrame(int outFrmNum, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    SineTone* data = static_cast<SineTone*>(instanceData);

    if (activationReason == VSActivationReason::arInitial)
    {
        if (outFrmNum == 0)
        {
            data->resetOverflowStats();
        }

        int outFrmLen = vsutils::getFrameSampleCount(outFrmNum, data->getOutInfo().numSamples);

        VSFrame* outFrm = vsapi->newAudioFrame(&data->getOutInfo().format, outFrmLen, nullptr, core);

        bool success = data->writeFrame(outFrm, outFrmNum, frameCtx, core, vsapi);

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


static void VS_CC sinetoneCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
    int tmpSampleRate = DefaultSampleRate;
    int64_t tmpSamples = DefaultSeconds * tmpSampleRate;
    common::SampleType tmpSampleType = DefaultSampleType;

    std::vector<int> channels = { static_cast<int>(VSAudioChannels::acFrontLeft), static_cast<int>(VSAudioChannels::acFrontRight) };
    uint64_t tmpChannelLayout = vsutils::toChannelLayout(channels);

    // clip:anode:opt
    int clipErr = 0;
    VSNode* audio = vsapi->mapGetNode(in, "clip", 0, &clipErr);
    if (!clipErr)
    {
        const VSAudioInfo* audioInfo = vsapi->getAudioInfo(audio);

        tmpSampleRate = audioInfo->sampleRate;
        tmpSamples = audioInfo->numSamples;
        tmpChannelLayout = audioInfo->format.channelLayout;

        std::optional<common::SampleType> optSampleType = common::getSampleTypeFromAudioFormat(audioInfo->format);
        if (!optSampleType.has_value())
        {
            std::string errMsg = std::format("{}: unsupported sample type of audio clip", FuncName);
            vsapi->mapSetError(out, errMsg.c_str());
            return;
        }
        tmpSampleType = optSampleType.value();
    }

    // sample_rate:int:opt
    int sampleRate = vsmap::getOptInt("sample_rate", in, vsapi, tmpSampleRate);
    if (sampleRate < 0)
    {
        std::string errMsg = std::format("{}: negative sample_rate", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio);
        return;
    }

    // samples:int:opt
    // samples has a higher priority than seconds
    int64_t samples = vsmap::getOptSamples("samples", "seconds", in, out, vsapi, tmpSamples, sampleRate);
    if (samples <= 0)
    {
        std::string errMsg = std::format("{}: negative or zero length", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio);
        return;
    }

    // sample_type:data:opt
    std::optional<common::SampleType> optSampleType = vsmap::getOptVapourSynthSampleTypeFromString("sample_type", FuncName, in, out, vsapi, tmpSampleType);
    if (!optSampleType.has_value())
    {
        vsapi->freeNode(audio);
        return;
    }

    // freq:float:opt
    double freq = vsmap::getOptDouble("freq", in, vsapi, DefaultFrequency);

    if (freq <= 0)
    {
        std::string errMsg = std::format("{}: negative or zero freq", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio);
        return;
    }

    // amp:float:opt
    double amp = vsmap::getOptDouble("amp", in, vsapi, DefaultAmplitude);

    if (1 < std::abs(amp))
    {
        std::string warnMsg = std::format("{}: amp is greater than 1 -> possible sample overflow", FuncName);
        vsapi->logMessage(VSMessageType::mtWarning, warnMsg.c_str(), core);
    }

    // channels:int[]:opt
    uint64_t channelLayout = vsmap::getOptChannelLayout("channels", in, vsapi, tmpChannelLayout);

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

    // free template clip, might be null
    vsapi->freeNode(audio);

    SineTone* data = new SineTone(samples, channelLayout, sampleRate, optSampleType.value(), freq, amp, optOverflowMode.value(), optOverflowLog.value());

    // fmParallelRequests: strict sequential frame requests for overflow logging
    vsapi->createAudioFilter(out, FuncName, &data->getOutInfo(), sinetoneGetFrame, sinetoneFree, VSFilterMode::fmParallelRequests, nullptr, 0, data, core);
}


void sinetoneInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi)
{
    vspapi->registerFunction(FuncName,
                             "clip:anode:opt;"
                             "samples:int:opt;"
                             "seconds:float:opt;"
                             "sample_rate:int:opt;"
                             "sample_type:data:opt;"
                             "freq:float:opt;"
                             "amp:float:opt;"
                             "channels:int[]:opt;"
                             "overflow:data:opt;"
                             "overflow_log:data:opt;",
                             "return:anode;",
                             sinetoneCreate, nullptr, plugin);
}
