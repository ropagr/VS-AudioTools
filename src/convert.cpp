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

#include "convert.hpp"
#include "common/offset.hpp"
#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "utils/sample.hpp"
#include "vsmap/vsmap.hpp"
#include "vsmap/vsmap_common.hpp"
#include "vsutils/audio.hpp"
#include "vsutils/bitshift.hpp"

constexpr const char* FuncName = "Convert";

constexpr common::OverflowMode DefaultOverflowMode = common::OverflowMode::Error;
constexpr common::OverflowLog DefaultOverflowLog = common::OverflowLog::Once;


Convert::Convert(VSNode* _audio, const VSAudioInfo* inInfo, common::SampleType _outSampleType,
                 common::OverflowMode _overflowMode, common::OverflowLog _overflowLog) :
    audio(_audio), outSampleType(_outSampleType), overflowMode(_overflowMode), overflowLog(_overflowLog)
{
    inSampleType = common::getSampleTypeFromAudioFormat(inInfo->format).value();

    outInfo = *inInfo;
    common::applySampleTypeToAudioFormat(outSampleType, outInfo.format);
}


VSNode* Convert::getAudio()
{
    return audio;
}

const VSAudioInfo& Convert::getOutInfo()
{
    return outInfo;
}


bool Convert::isPassthrough()
{
    return inSampleType == outSampleType;
}


void Convert::resetOverflowStats()
{
    overflowStats = { .count = 0, .peak = 0.0 };
}


void Convert::logOverflowStats(VSCore* core, const VSAPI* vsapi)
{
    if (0 < overflowStats.count)
    {
        overflowStats.logVS(FuncName, overflowMode, isFloatSampleType(outSampleType), core, vsapi);
    }
}


void Convert::free(const VSAPI* vsapi)
{
    vsapi->freeNode(audio);
}


template <typename in_sample_t, size_t InSampleIntBits, typename out_sample_t, size_t OutSampleIntBits>
bool Convert::writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen, const VSFrame* inFrm,
                                const common::OverflowContext& ofCtx)
{
    out_sample_t* outFrmPtr = reinterpret_cast<out_sample_t*>(ofCtx.vsapi->getWritePtr(outFrm, ch));

    const in_sample_t* inFrmPtr = reinterpret_cast<const in_sample_t*>(ofCtx.vsapi->getReadPtr(inFrm, ch));

    constexpr vsutils::BitShift inBitShift = vsutils::getSampleBitShift<in_sample_t, InSampleIntBits>();

    for (int s = 0; s < outFrmLen; ++s)
    {
        int64_t outPos = outPosFrmStart + s;

        in_sample_t sample = inFrmPtr[s];

        if constexpr (inBitShift.required)
        {
            sample >>= inBitShift.count;
        }

        if (!common::safeWriteSample<out_sample_t, OutSampleIntBits>(
                utils::convSampleToDouble<in_sample_t, InSampleIntBits>(sample), outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
        {
            // overflow and error
            return false;
        }
    }

    return true;
}


template <typename in_sample_t, size_t InSampleIntBits, typename out_sample_t, size_t OutSampleIntBits>
bool Convert::writeFrameImpl(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, const common::OverflowContext& ofCtx)
{
    int64_t outPosFrmStart = vsutils::frameToFirstSample(outFrmNum);
    int outFrmLen = ofCtx.vsapi->getFrameLength(outFrm);

    for (int ch = 0; ch < outInfo.format.numChannels; ++ch)
    {
        if (!writeFrameChannel<in_sample_t, InSampleIntBits, out_sample_t, OutSampleIntBits>(ch, outFrm, outPosFrmStart, outFrmLen, inFrm, ofCtx))
        {
            return false;
        }
    }
    return true;
}


bool Convert::writeFrame(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    common::OverflowContext ofCtx =
        { .mode = overflowMode, .log = overflowLog, .funcName = FuncName,
          .frameCtx = frameCtx, .core = core, .vsapi = vsapi };

    switch (inSampleType)
    {
        case common::SampleType::Int8:
            switch (outSampleType)
            {
                case common::SampleType::Int8:
                    return writeFrameImpl<int8_t, 8, int8_t, 8>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int16:
                    return writeFrameImpl<int8_t, 8, int16_t, 16>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int24:
                    return writeFrameImpl<int8_t, 8, int32_t, 24>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int32:
                    return writeFrameImpl<int8_t, 8, int32_t, 32>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float32:
                    return writeFrameImpl<int8_t, 8, float, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float64:
                    return writeFrameImpl<int8_t, 8, double, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                default:
                    return false;
            }

        case common::SampleType::Int16:
            switch (outSampleType)
            {
                case common::SampleType::Int8:
                    return writeFrameImpl<int16_t, 16, int8_t, 8>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int16:
                    return writeFrameImpl<int16_t, 16, int16_t, 16>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int24:
                    return writeFrameImpl<int16_t, 16, int32_t, 24>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int32:
                    return writeFrameImpl<int16_t, 16, int32_t, 32>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float32:
                    return writeFrameImpl<int16_t, 16, float, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float64:
                    return writeFrameImpl<int16_t, 16, double, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                default:
                    return false;
            }

        case common::SampleType::Int24:
            switch (outSampleType)
            {
                case common::SampleType::Int8:
                    return writeFrameImpl<int32_t, 24, int8_t, 8>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int16:
                    return writeFrameImpl<int32_t, 24, int16_t, 16>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int24:
                    return writeFrameImpl<int32_t, 24, int32_t, 24>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int32:
                    return writeFrameImpl<int32_t, 24, int32_t, 32>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float32:
                    return writeFrameImpl<int32_t, 24, float, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float64:
                    return writeFrameImpl<int32_t, 24, double, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                default:
                    return false;
            }

        case common::SampleType::Int32:
            switch (outSampleType)
            {
                case common::SampleType::Int8:
                    return writeFrameImpl<int32_t, 32, int8_t, 8>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int16:
                    return writeFrameImpl<int32_t, 32, int16_t, 16>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int24:
                    return writeFrameImpl<int32_t, 32, int32_t, 24>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int32:
                    return writeFrameImpl<int32_t, 32, int32_t, 32>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float32:
                    return writeFrameImpl<int32_t, 32, float, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float64:
                    return writeFrameImpl<int32_t, 32, double, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                default:
                    return false;
            }

        case common::SampleType::Float32:
            switch (outSampleType)
            {
                case common::SampleType::Int8:
                    return writeFrameImpl<float, 0, int8_t, 8>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int16:
                    return writeFrameImpl<float, 0, int16_t, 16>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int24:
                    return writeFrameImpl<float, 0, int32_t, 24>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int32:
                    return writeFrameImpl<float, 0, int32_t, 32>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float32:
                    return writeFrameImpl<float, 0, float, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float64:
                    return writeFrameImpl<float, 0, double, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                default:
                    return false;
            }

        case common::SampleType::Float64:
            switch (outSampleType)
            {
                case common::SampleType::Int8:
                    return writeFrameImpl<double, 0, int8_t, 8>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int16:
                    return writeFrameImpl<double, 0, int16_t, 16>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int24:
                    return writeFrameImpl<double, 0, int32_t, 24>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Int32:
                    return writeFrameImpl<double, 0, int32_t, 32>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float32:
                    return writeFrameImpl<double, 0, float, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                case common::SampleType::Float64:
                    return writeFrameImpl<double, 0, double, 0>(outFrm, outFrmNum, inFrm, ofCtx);
                default:
                    return false;
            }

        default:
            return false;
    }
}


static void VS_CC convertFree(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
    Convert* data = static_cast<Convert*>(instanceData);
    // skip logging since the total number of overflows is unreliable
    // data->logNumOverflows(core, vsapi);
    data->free(vsapi);
    delete data;
}


static const VSFrame* VS_CC convertGetFrame(int outFrmNum, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    Convert* data = static_cast<Convert*>(instanceData);

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

        if (data->isPassthrough())
        {
            // pass through if input sample type equals output sample type
            return inFrm;
        }

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


static void VS_CC convertCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
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
    auto optInSampleType = common::getSampleTypeFromAudioFormat(audioInfo->format);
    if (!optInSampleType.has_value())
    {
        std::string errMsg = std::format("{}: unsupported audio format", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio);
        return;
    }

    // sample_type:data
    std::optional<common::SampleType> optOutSampleType = vsmap::getVapourSynthSampleTypeFromString("sample_type", FuncName, in, out, vsapi);
    if (!optOutSampleType.has_value())
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

    if (optOverflowMode.value() == common::OverflowMode::KeepFloat && !common::isFloatSampleType(optOutSampleType.value()))
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

    Convert* data = new Convert(audio, audioInfo, optOutSampleType.value(), optOverflowMode.value(), optOverflowLog.value());

    VSFilterDependency deps[] = {{ audio, rpStrictSpatial }};

    // fmParallelRequests: strict sequential frame requests for overflow logging
    vsapi->createAudioFilter(out, FuncName, &data->getOutInfo(), convertGetFrame, convertFree, VSFilterMode::fmParallelRequests, deps, 1, data, core);
}


void convertInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi)
{
    vspapi->registerFunction(FuncName,
                             "clip:anode;"
                             "sample_type:data;"
                             "overflow:data:opt;"
                             "overflow_log:data:opt;",
                             "return:anode;",
                             convertCreate, nullptr, plugin);
}
