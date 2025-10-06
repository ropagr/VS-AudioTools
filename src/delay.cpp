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

#include "delay.hpp"
#include "common/offset.hpp"
#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "utils/debug.hpp"
#include "utils/sample.hpp"
#include "utils/vector.hpp"
#include "vsmap/vsmap.hpp"
#include "vsmap/vsmap_common.hpp"
#include "vsutils/audio.hpp"
#include "vsutils/bitshift.hpp"

constexpr const char* FuncName = "Delay";

constexpr int64_t DefaultDelaySamples = 0;
constexpr common::OverflowMode DefaultOverflowMode = common::OverflowMode::Error;
constexpr common::OverflowLog DefaultOverflowLog = common::OverflowLog::Once;


Delay::Delay(VSNode* _audio, const VSAudioInfo* _audioInfo, int64_t _offsetSamples, std::vector<int> _editChannels,
             common::OverflowMode _overflowMode, common::OverflowLog _overflowLog) :
    audio(_audio), audioInfo(*_audioInfo), editChannels(_editChannels),
    overflowMode(_overflowMode), overflowLog(_overflowLog)
{
    outSampleType = common::getSampleTypeFromAudioFormat(audioInfo.format).value();

    outPosOffsetStart = _offsetSamples;
    outPosOffsetEnd = outPosOffsetStart + audioInfo.numSamples;

    audioFrameSampleOffsets = common::getFrameSampleOffsets(outPosOffsetStart);

    copyChannels = utils::vectorInvert(editChannels, 0, audioInfo.format.numChannels);
}


VSNode* Delay::getAudio()
{
    return audio;
}


const VSAudioInfo& Delay::getOutInfo()
{
    return audioInfo;
}


common::OffsetFramePos Delay::outFrameToOffsetInFrames(int outFrmNum)
{
    return common::baseFrameToOffsetFrames(outFrmNum, outPosOffsetStart, audioInfo.numSamples, audioInfo.numSamples);
}


size_t Delay::getNumCopyChannels()
{
    return copyChannels.size();
}


size_t Delay::getNumEditChannels()
{
    return editChannels.size();
}


void Delay::resetOverflowStats()
{
    overflowStats = { .count = 0, .peak = 0.0 };
}


void Delay::logOverflowStats(VSCore* core, const VSAPI* vsapi)
{
    if (0 < overflowStats.count)
    {
        overflowStats.logVS(FuncName, overflowMode, isFloatSampleType(outSampleType), core, vsapi);
    }
}


void Delay::free(const VSAPI* vsapi)
{
    vsapi->freeNode(audio);
}


template <typename sample_t, size_t IntSampleBits>
bool Delay::writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen,
                              const VSFrame* offsetInFrmL, const VSFrame* offsetInFrmR,
                              const common::OverflowContext& ofCtx)
{
    sample_t* outFrmPtr = reinterpret_cast<sample_t*>(ofCtx.vsapi->getWritePtr(outFrm, ch));

    const sample_t* offsetInFrmLPtr = reinterpret_cast<const sample_t*>(offsetInFrmL ? ofCtx.vsapi->getReadPtr(offsetInFrmL, ch) : nullptr);
    const sample_t* offsetInFrmRPtr = reinterpret_cast<const sample_t*>(offsetInFrmR ? ofCtx.vsapi->getReadPtr(offsetInFrmR, ch) : nullptr);

    constexpr vsutils::BitShift bitShift = vsutils::getSampleBitShift<sample_t, IntSampleBits>();

    for (int s = 0; s < outFrmLen; ++s)
    {
        int64_t outPos = outPosFrmStart + s;

        if (outPos < outPosOffsetStart || outPosOffsetEnd <= outPos)
        {
            // fill with zeros
            if (!common::safeWriteSample<sample_t, IntSampleBits>(0, outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
            {
                // overflow and error
                return false;
            }
        }
        else
        {
#ifndef NDEBUG
            // only for debug builds
            assertm(offsetInFrmLPtr, "offsetInFrmLPtr null");

            if (audioFrameSampleOffsets.right != 0)
            {
                assertm(offsetInFrmRPtr, "offsetInFrmRPtr null");
            }
#endif

            sample_t inSample = common::getOffsetSample(s, audioFrameSampleOffsets, offsetInFrmLPtr, offsetInFrmRPtr);

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
bool Delay::writeFrameImpl(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm,
                           const VSFrame* offsetInFrmL, const VSFrame* offsetInFrmR,
                           const common::OverflowContext& ofCtx)
{
    // copy channels
    for (const int& ch : copyChannels)
    {
        vsutils::copyFrameChannel(outFrm, ch, inFrm, ch, getOutInfo().format.bytesPerSample, ofCtx.vsapi);
    }

    // edit channels
    int64_t outPosFrmStart = vsutils::frameToFirstSample(outFrmNum);
    int outFrmLen = ofCtx.vsapi->getFrameLength(outFrm);

    for (const int& ch : editChannels)
    {
        if (!writeFrameChannel<sample_t, IntSampleBits>(ch, outFrm, outPosFrmStart, outFrmLen, offsetInFrmL, offsetInFrmR, ofCtx))
        {
            return false;
        }
    }
    return true;
}


bool Delay::writeFrame(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm,
                       const VSFrame* offsetInFrmL, const VSFrame* offsetInFrmR,
                       VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    common::OverflowContext ofCtx =
        { .mode = overflowMode, .log = overflowLog, .funcName = FuncName,
          .frameCtx = frameCtx, .core = core, .vsapi = vsapi };

    switch (outSampleType)
    {
        case common::SampleType::Int8:
            return writeFrameImpl<int8_t, 8>(outFrm, outFrmNum, inFrm, offsetInFrmL, offsetInFrmR, ofCtx);
        case common::SampleType::Int16:
            return writeFrameImpl<int16_t, 16>(outFrm, outFrmNum, inFrm, offsetInFrmL, offsetInFrmR, ofCtx);
        case common::SampleType::Int24:
            return writeFrameImpl<int32_t, 24>(outFrm, outFrmNum, inFrm, offsetInFrmL, offsetInFrmR, ofCtx);
        case common::SampleType::Int32:
            return writeFrameImpl<int32_t, 32>(outFrm, outFrmNum, inFrm, offsetInFrmL, offsetInFrmR, ofCtx);
        case common::SampleType::Float32:
            return writeFrameImpl<float, 0>(outFrm, outFrmNum, inFrm, offsetInFrmL, offsetInFrmR, ofCtx);
        case common::SampleType::Float64:
            return writeFrameImpl<double, 0>(outFrm, outFrmNum, inFrm, offsetInFrmL, offsetInFrmR, ofCtx);
        default:
            return false;
    }
}


static void VS_CC delayFree(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
    Delay* data = static_cast<Delay*>(instanceData);
    // skip logging since the total number of overflows is unreliable
    // data->logNumOverflows(core, vsapi);
    data->free(vsapi);
    delete data;
}


static const VSFrame* VS_CC delayGetFrame(int outFrmNum, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    Delay* data = static_cast<Delay*>(instanceData);

    common::OffsetFramePos offsetInFrmNums = data->outFrameToOffsetInFrames(outFrmNum);

    if (activationReason == VSActivationReason::arInitial)
    {
        bool frmRequested = false;

        if (0 < data->getNumEditChannels())
        {
            if (0 <= offsetInFrmNums.left)
            {
                vsapi->requestFrameFilter(offsetInFrmNums.left, data->getAudio(), frameCtx);
                frmRequested = true;
            }

            if (0 <= offsetInFrmNums.right)
            {
                vsapi->requestFrameFilter(offsetInFrmNums.right, data->getAudio(), frameCtx);
                frmRequested = true;
            }
        }

        if (0 < data->getNumCopyChannels())
        {
            vsapi->requestFrameFilter(outFrmNum, data->getAudio(), frameCtx);
            frmRequested = true;
        }

        if (!frmRequested)
        {
            // request a dummy frame (0) if no frame was requested before
            // apparently you always need to request a frame even if you don't need one
            // otherwise you get a fatal error: No frame returned at the end of processing by Delay
            vsapi->requestFrameFilter(0, data->getAudio(), frameCtx);
        }

        return nullptr;
    }

    if (activationReason == VSActivationReason::arAllFramesReady)
    {
        if (outFrmNum == 0)
        {
            data->resetOverflowStats();
        }

        const VSFrame* inFrm = nullptr;
        const VSFrame* offsetInFrmL = nullptr;
        const VSFrame* offsetInFrmR = nullptr;

        if (0 < data->getNumCopyChannels())
        {
            inFrm = vsapi->getFrameFilter(outFrmNum, data->getAudio(), frameCtx);
        }

        if (0 < data->getNumEditChannels())
        {
            if (0 <= offsetInFrmNums.left)
            {
                offsetInFrmL = vsapi->getFrameFilter(offsetInFrmNums.left, data->getAudio(), frameCtx);
            }

            if (0 <= offsetInFrmNums.right)
            {
                offsetInFrmR = vsapi->getFrameFilter(offsetInFrmNums.right, data->getAudio(), frameCtx);
            }
        }

        int outFrmLen = vsutils::getFrameSampleCount(outFrmNum, data->getOutInfo().numSamples);

        VSFrame* outFrm = vsapi->newAudioFrame(&data->getOutInfo().format, outFrmLen, nullptr, core);

        bool success = data->writeFrame(outFrm, outFrmNum, inFrm, offsetInFrmL, offsetInFrmR, frameCtx, core, vsapi);

        if (inFrm)
        {
            vsapi->freeFrame(inFrm);
        }

        if (offsetInFrmL)
        {
            vsapi->freeFrame(offsetInFrmL);
        }

        if (offsetInFrmR)
        {
            vsapi->freeFrame(offsetInFrmR);
        }

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


static void VS_CC delayCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
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
    int64_t samples = vsmap::getOptSamples("samples", "seconds", in, out, vsapi, DefaultDelaySamples, audioInfo->sampleRate);

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

    Delay* data = new Delay(audio, audioInfo, samples, optChannels.value(), optOverflowMode.value(), optOverflowLog.value());

    VSFilterDependency deps[] = {{ audio, rpGeneral }};

    // fmParallelRequests: strict sequential frame requests for overflow logging
    vsapi->createAudioFilter(out, FuncName, &data->getOutInfo(), delayGetFrame, delayFree, VSFilterMode::fmParallelRequests, deps, 1, data, core);
}


void delayInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi)
{
    vspapi->registerFunction(FuncName,
                             "clip:anode;"
                             "samples:int:opt;"
                             "seconds:float:opt;"
                             "channels:int[]:opt;"
                             "overflow:data:opt;"
                             "overflow_log:data:opt;",
                             "return:anode;",
                             delayCreate, nullptr, plugin);
}
