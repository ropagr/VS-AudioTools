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

#include "VapourSynth4.h"
#include "VSHelper4.h"

#include "crossfade.hpp"
#include "common/offset.hpp"
#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "common/transition.hpp"
#include "utils/debug.hpp"
#include "utils/sample.hpp"
#include "vsmap/vsmap.hpp"
#include "vsmap/vsmap_common.hpp"
#include "vsutils/audio.hpp"
#include "vsutils/bitshift.hpp"

constexpr const char* FuncName = "CrossFade";

constexpr int64_t DefaultFadeSamples = 0;
constexpr common::TransitionType DefaultFadeType = common::TransitionType::Cubic;
constexpr common::OverflowMode DefaultOverflowMode = common::OverflowMode::Error;
constexpr common::OverflowLog DefaultOverflowLog = common::OverflowLog::Once;


CrossFade::CrossFade(VSNode* _audio1, const VSAudioInfo* _audio1Info, VSNode* _audio2, const VSAudioInfo* _audio2Info,
                     int64_t fadeSamples, common::TransitionType fadeType,
                     common::OverflowMode _overflowMode, common::OverflowLog _overflowLog) :
    audio1(_audio1), audio1Info(*_audio1Info), audio2(_audio2), audio2Info(*_audio2Info),
    overflowMode(_overflowMode), overflowLog(_overflowLog)
{
    fadeSamples = std::max(fadeSamples, 0LL);

    // create destination audio information
    outInfo = audio1Info;
    outInfo.numSamples = audio1Info.numSamples + audio2Info.numSamples - fadeSamples;
    outInfo.numFrames = vsutils::samplesToFrames(outInfo.numSamples);

    outSampleType = common::getSampleTypeFromAudioFormat(outInfo.format).value();

    outPosFadeStart = audio1Info.numSamples - fadeSamples;
    outPosFadeEnd = audio1Info.numSamples;

    audio2FrameSampleOffsets = common::getFrameSampleOffsets(outPosFadeStart);

    if (0 < fadeSamples)
    {
        fadeoutTrans = common::newTransition(fadeType, 0, 1, static_cast<double>(fadeSamples - 1), 0);
    }
}


VSNode* CrossFade::getAudio1()
{
    return audio1;
}


VSNode* CrossFade::getAudio2()
{
    return audio2;
}


const VSAudioInfo& CrossFade::getOutInfo()
{
    return outInfo;
}


void CrossFade::resetOverflowStats()
{
    overflowStats = { .count = 0, .peak = 0.0 };
}


void CrossFade::logOverflowStats(VSCore* core, const VSAPI* vsapi)
{
    if (0 < overflowStats.count)
    {
        overflowStats.logVS(FuncName, overflowMode, isFloatSampleType(outSampleType), core, vsapi);
    }
}


void CrossFade::free(const VSAPI* vsapi)
{
    delete fadeoutTrans;

    vsapi->freeNode(audio1);
    vsapi->freeNode(audio2);
}


int CrossFade::outFrameToAudio1Frame(int outFrmNum)
{
    return common::baseFrameToOffsetFrames(outFrmNum, 0, audio1Info.numSamples, outInfo.numSamples).left;
}


common::OffsetFramePos CrossFade::outFrameToAudio2Frames(int outFrmNum)
{
    return common::baseFrameToOffsetFrames(outFrmNum, outPosFadeStart, audio2Info.numSamples, outInfo.numSamples);
}


template <typename sample_t, size_t IntSampleBits>
bool CrossFade::writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen,
                                  const VSFrame* a1Frm, const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                                  const common::OverflowContext& ofCtx)
{
    sample_t* outFrmPtr = reinterpret_cast<sample_t*>(ofCtx.vsapi->getWritePtr(outFrm, ch));

    const sample_t* a1FrmPtr = reinterpret_cast<const sample_t*>(a1Frm ? ofCtx.vsapi->getReadPtr(a1Frm, ch) : nullptr);
    const sample_t* a2FrmLPtr = reinterpret_cast<const sample_t*>(a2FrmL ? ofCtx.vsapi->getReadPtr(a2FrmL, ch) : nullptr);
    const sample_t* a2FrmRPtr = reinterpret_cast<const sample_t*>(a2FrmR ? ofCtx.vsapi->getReadPtr(a2FrmR, ch) : nullptr);

    constexpr vsutils::BitShift bitShift = vsutils::getSampleBitShift<sample_t, IntSampleBits>();

    for (int s = 0; s < outFrmLen; ++s)
    {
        int64_t outPos = outPosFrmStart + s;

        if (outPos < outPosFadeStart)
        {
            // only audio1
            assertm(a1FrmPtr, "a1FrmPtr null");

            sample_t a1Sample = a1FrmPtr[s];

            if constexpr (bitShift.required)
            {
                a1Sample >>= bitShift.count;
            }

            if (!common::safeWriteSample<sample_t, IntSampleBits>(utils::convSampleToDouble<sample_t, IntSampleBits>(a1Sample),
                                                                  outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
            {
                // overflow and error
                return false;
            }
        }
        else if (outPosFadeEnd <= outPos)
        {
            // only audio2
#ifndef NDEBUG
            // only for debug builds
            assertm(a2FrmLPtr, "a2FrmLPtr null");

            if (audio2FrameSampleOffsets.right != 0)
            {
                assertm(a2FrmRPtr, "a2FrmRPtr null");
            }
#endif

            sample_t a2Sample = common::getOffsetSample(s, audio2FrameSampleOffsets, a2FrmLPtr, a2FrmRPtr);

            if constexpr (bitShift.required)
            {
                a2Sample >>= bitShift.count;
            }

            if (!common::safeWriteSample<sample_t, IntSampleBits>(utils::convSampleToDouble<sample_t, IntSampleBits>(a2Sample),
                                                                  outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
            {
                // overflow and error
                return false;
            }
        }
        else
        {
            // assert: outPosFadeStart <= outPos && outPos < outPosFadeEnd
            // crossfade sample
#ifndef NDEBUG
            // only for debug builds
            assertm(a1FrmPtr, "a1FrmPtr null");
            assertm(a2FrmLPtr, "a2FrmLPtr null");

            if (audio2FrameSampleOffsets.right != 0)
            {
                assertm(a2FrmRPtr, "a2FrmRPtr null");
            }
#endif

            sample_t a1Sample = a1FrmPtr[s];
            sample_t a2Sample = common::getOffsetSample(s, audio2FrameSampleOffsets, a2FrmLPtr, a2FrmRPtr);

            if constexpr (bitShift.required)
            {
                a1Sample >>= bitShift.count;
                a2Sample >>= bitShift.count;
            }

            int64_t fadePos = outPos - outPosFadeStart;
            double audio1Scale = 1;
            double audio2Scale = 0;

            if (fadeoutTrans)
            {
                audio1Scale = fadeoutTrans->calcY(static_cast<double>(fadePos));
                audio2Scale = 1 - audio1Scale;
            }

            double mixedSample = audio1Scale * utils::convSampleToDouble<sample_t, IntSampleBits>(a1Sample) +
                                 audio2Scale * utils::convSampleToDouble<sample_t, IntSampleBits>(a2Sample);

            if (!common::safeWriteSample<sample_t, IntSampleBits>(mixedSample, outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
            {
                // overflow and error
                return false;
            }
        }
    }
    return true;
}


template <typename sample_t, size_t IntSampleBits>
bool CrossFade::writeFrameImpl(VSFrame* outFrm, int outFrmNum,
                               const VSFrame* a1Frm, const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                               const common::OverflowContext& ofCtx)
{
    int64_t outPosFrmStart = vsutils::frameToFirstSample(outFrmNum);
    int outFrmLen = ofCtx.vsapi->getFrameLength(outFrm);

    for (int ch = 0; ch < outInfo.format.numChannels; ++ch)
    {
        if (!writeFrameChannel<sample_t, IntSampleBits>(ch, outFrm, outPosFrmStart, outFrmLen, a1Frm, a2FrmL, a2FrmR, ofCtx))
        {
            return false;
        }
    }
    return true;
}


bool CrossFade::writeFrame(VSFrame* outFrm, int outFrmNum,
                           const VSFrame* a1Frm, const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                           VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    common::OverflowContext ofCtx =
        { .mode = overflowMode, .log = overflowLog, .funcName = FuncName,
          .frameCtx = frameCtx, .core = core, .vsapi = vsapi };

    switch (outSampleType)
    {
        case common::SampleType::Int8:
            return writeFrameImpl<int8_t, 8>(outFrm, outFrmNum, a1Frm, a2FrmL, a2FrmR, ofCtx);
        case common::SampleType::Int16:
            return writeFrameImpl<int16_t, 16>(outFrm, outFrmNum, a1Frm, a2FrmL, a2FrmR, ofCtx);
        case common::SampleType::Int24:
            return writeFrameImpl<int32_t, 24>(outFrm, outFrmNum, a1Frm, a2FrmL, a2FrmR, ofCtx);
        case common::SampleType::Int32:
            return writeFrameImpl<int32_t, 32>(outFrm, outFrmNum, a1Frm, a2FrmL, a2FrmR, ofCtx);
        case common::SampleType::Float32:
            return writeFrameImpl<float, 0>(outFrm, outFrmNum, a1Frm, a2FrmL, a2FrmR, ofCtx);
        case common::SampleType::Float64:
            return writeFrameImpl<double, 0>(outFrm, outFrmNum, a1Frm, a2FrmL, a2FrmR, ofCtx);
        default:
            return false;
    }
}


static void VS_CC crossfadeFree(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
    CrossFade* data = static_cast<CrossFade*>(instanceData);
    // skip logging since the total number of overflows is unreliable
    // data->logNumOverflows(core, vsapi);
    data->free(vsapi);
    delete data;
}


static const VSFrame* VS_CC crossfadeGetFrame(int outFrmNum, int activationReason, void* instanceData, void** frameData,
                                              VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    CrossFade* data = static_cast<CrossFade*>(instanceData);

    int a1FrmNum = data->outFrameToAudio1Frame(outFrmNum);

    common::OffsetFramePos a2FrmNums = data->outFrameToAudio2Frames(outFrmNum);

    if (activationReason == VSActivationReason::arInitial)
    {
        if (0 <= a1FrmNum)
        {
            vsapi->requestFrameFilter(a1FrmNum, data->getAudio1(), frameCtx);
        }

        if (0 <= a2FrmNums.left)
        {
            vsapi->requestFrameFilter(a2FrmNums.left, data->getAudio2(), frameCtx);
        }

        if (0 <= a2FrmNums.right)
        {
            vsapi->requestFrameFilter(a2FrmNums.right, data->getAudio2(), frameCtx);
        }

        return nullptr;
    }

    if (activationReason == VSActivationReason::arAllFramesReady)
    {
        if (outFrmNum == 0)
        {
            data->resetOverflowStats();
        }

        const VSFrame* a1Frm = nullptr;
        const VSFrame* a2FrmL = nullptr;
        const VSFrame* a2FrmR = nullptr;
        const VSFrame* propFrm = nullptr;

        if (0 <= a1FrmNum)
        {
            a1Frm = vsapi->getFrameFilter(a1FrmNum, data->getAudio1(), frameCtx);
            if (!propFrm)
            {
                propFrm = a1Frm;
            }
        }

        if (0 <= a2FrmNums.left)
        {
            a2FrmL = vsapi->getFrameFilter(a2FrmNums.left, data->getAudio2(), frameCtx);
            if (!propFrm)
            {
                propFrm = a2FrmL;
            }
        }

        if (0 <= a2FrmNums.right)
        {
            a2FrmR = vsapi->getFrameFilter(a2FrmNums.right, data->getAudio2(), frameCtx);
            if (!propFrm)
            {
                propFrm = a2FrmR;
            }
        }

        int outFrmLen = vsutils::getFrameSampleCount(outFrmNum, data->getOutInfo().numSamples);

        VSFrame* outFrm = vsapi->newAudioFrame(&data->getOutInfo().format, outFrmLen, propFrm, core);

        bool success = data->writeFrame(outFrm, outFrmNum, a1Frm, a2FrmL, a2FrmR, frameCtx, core, vsapi);

        if (a1Frm)
        {
            vsapi->freeFrame(a1Frm);
        }

        if (a2FrmL)
        {
            vsapi->freeFrame(a2FrmL);
        }

        if (a2FrmR)
        {
            vsapi->freeFrame(a2FrmR);
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


static void VS_CC crossfadeCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
    // clip1:anode
    int err = 0;
    VSNode* audio1 = vsapi->mapGetNode(in, "clip1", 0, &err);
    if (err)
    {
        return;
    }

    const VSAudioInfo* audio1Info = vsapi->getAudioInfo(audio1);

    // clip2:anode
    err = 0;
    VSNode* audio2 = vsapi->mapGetNode(in, "clip2", 0, &err);
    if (err)
    {
        return;
    }

    const VSAudioInfo* audio2Info = vsapi->getAudioInfo(audio2);

    if (!vsh::isSameAudioInfo(audio1Info, audio2Info))
    {
        std::string errMsg = std::format("{}: clips have a different audio format", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    // check for supported audio format
    auto optSampleType = common::getSampleTypeFromAudioFormat(audio1Info->format);
    if (!optSampleType.has_value())
    {
        std::string errMsg = std::format("{}: unsupported audio format", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    // samples:int:opt
    // seconds:float:opt
    int64_t samples = vsmap::getOptSamples("samples", "seconds", in, out, vsapi, DefaultFadeSamples, audio1Info->sampleRate);
    if (samples < 0)
    {
        std::string errMsg = std::format("{}: negative crossfade length", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    if (audio1Info->numSamples < samples)
    {
        std::string errMsg = std::format("{}: clip1 is shorter than the crossfade length", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    if (audio2Info->numSamples < samples)
    {
        std::string errMsg = std::format("{}: clip2 is shorter than the crossfade length", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    // type:data:opt
    std::optional<common::TransitionType> optFadeType = vsmap::getOptTransitionTypeFromString("type", FuncName, in, out, vsapi, DefaultFadeType);
    if (!optFadeType.has_value())
    {
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    // overflow:data:opt
    std::optional<common::OverflowMode> optOverflowMode = vsmap::getOptOverflowModeFromString("overflow", FuncName, in, out, vsapi, DefaultOverflowMode);
    if (!optOverflowMode.has_value())
    {
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    if (optOverflowMode.value() == common::OverflowMode::KeepFloat && !common::isFloatSampleType(optSampleType.value()))
    {
        std::string errMsg = std::format("{}: cannot use 'keep_float' overflow mode with an integer sample type", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    // overflow_log:data:opt
    std::optional<common::OverflowLog> optOverflowLog = vsmap::getOptOverflowLogFromString("overflow_log", FuncName, in, out, vsapi, DefaultOverflowLog);
    if (!optOverflowLog.has_value())
    {
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    CrossFade* data = new CrossFade(audio1, audio1Info, audio2, audio2Info, samples, optFadeType.value(), optOverflowMode.value(), optOverflowLog.value());

    VSFilterDependency deps[] = {{ audio1, VSRequestPattern::rpStrictSpatial }, { audio2, VSRequestPattern::rpGeneral }};

    // fmParallelRequests: strict sequential frame requests for overflow logging
    vsapi->createAudioFilter(out, FuncName, &data->getOutInfo(), crossfadeGetFrame, crossfadeFree, VSFilterMode::fmParallelRequests, deps, 2, data, core);
}


void crossfadeInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi)
{
    vspapi->registerFunction(FuncName,
                             "clip1:anode;"
                             "clip2:anode;"
                             "samples:int:opt;"
                             "seconds:float:opt;"
                             "type:data:opt;"
                             "overflow:data:opt;"
                             "overflow_log:data:opt;",
                             "return:anode;",
                             crossfadeCreate, nullptr, plugin);
}
