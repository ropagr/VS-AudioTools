// SPDX-License-Identifier: MIT

#include <algorithm>
#include <climits>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "VapourSynth4.h"
#include "VSHelper4.h"

#include "mix.hpp"
#include "common/offset.hpp"
#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "common/transition.hpp"
#include "utils/debug.hpp"
#include "utils/sample.hpp"
#include "utils/vector.hpp"
#include "vsmap/vsmap.hpp"
#include "vsmap/vsmap_common.hpp"
#include "vsutils/audio.hpp"
#include "vsutils/bitshift.hpp"

constexpr const char* FuncName = "Mix";

constexpr int64_t DefaultAudio2StartSample = 0;
constexpr double DefaultAudio1Gain = 1;
constexpr double DefaultAudio2Gain = 1;
constexpr bool DefaultRelativeGain = false;
constexpr int64_t DefaultFadeInSamples = 0;
constexpr int64_t DefaultFadeOutSamples = 0;
constexpr common::TransitionType DefaultFadeType = common::TransitionType::Cubic;
constexpr bool DefaultExtendStart = false;
constexpr bool DefaultExtendEnd = false;
constexpr common::OverflowMode DefaultOverflowMode = common::OverflowMode::Error;
constexpr common::OverflowLog DefaultOverflowLog = common::OverflowLog::Once;


Mix::Mix(VSNode* _audio1, const VSAudioInfo* _audio1Info, double _audio1Gain,
         VSNode* _audio2, const VSAudioInfo* _audio2Info, double _audio2Gain,
         int64_t audio2OffsetSamples, bool _relativeGain,
         int64_t fadeinSamples, int64_t fadeoutSamples, common::TransitionType fadeType,
         bool extendAudio1Start, bool extendAudio1End, std::vector<int> _editChannels,
         common::OverflowMode _overflowMode, common::OverflowLog _overflowLog) :
    audio1(_audio1), audio1Info(*_audio1Info), audio1Gain(_audio1Gain),
    audio2(_audio2), audio2Info(*_audio2Info), audio2Gain(_audio2Gain),
    relativeGain(_relativeGain), editChannels(_editChannels.begin(), _editChannels.end()),
    overflowMode(_overflowMode), overflowLog(_overflowLog)
{
    fadeinAudio2 = true;
    fadeoutAudio2 = true;

    if (audio2OffsetSamples < 0)
    {
        assert(-audio2OffsetSamples <= audio2Info.numSamples);

        if (extendAudio1Start)
        {
            outPosAudio1Start = -audio2OffsetSamples;
            outPosAudio1TrimStart = -audio2OffsetSamples;

            outPosAudio2Start = 0;
            outPosAudio2TrimStart = 0;
            fadeinAudio2 = false;
        }
        else
        {
            outPosAudio1Start = 0;
            outPosAudio1TrimStart = 0;

            outPosAudio2Start = audio2OffsetSamples;
            outPosAudio2TrimStart = 0;
        }
    }
    else
    {
        assert(audio2OffsetSamples <= audio1Info.numSamples);

        outPosAudio1Start = 0;
        outPosAudio1TrimStart = 0;

        outPosAudio2Start = audio2OffsetSamples;
        outPosAudio2TrimStart = audio2OffsetSamples;
    }

    outPosAudio1End = outPosAudio1Start + audio1Info.numSamples;
    outPosAudio2End = outPosAudio2Start + audio2Info.numSamples;

    outPosAudio1TrimEnd = outPosAudio1End;

    if (outPosAudio1End < outPosAudio2End)
    {
        if (extendAudio1End)
        {
            outPosAudio2TrimEnd = outPosAudio2End;
            fadeoutAudio2 = false;
        }
        else
        {
            // trim audio2
            outPosAudio2TrimEnd = outPosAudio1TrimEnd;
        }
    }
    else
    {
        outPosAudio2TrimEnd = outPosAudio2End;
    }

    int64_t outLen = std::max(outPosAudio1TrimEnd, outPosAudio2TrimEnd) - std::min(outPosAudio1TrimStart, outPosAudio2TrimStart);

    audio1FrameSampleOffsets = common::getFrameSampleOffsets(outPosAudio1Start);
    audio2FrameSampleOffsets = common::getFrameSampleOffsets(outPosAudio2Start);

    // overlapping range of audio1 and audio2
    int64_t outPosMixStart = std::max(outPosAudio1TrimStart, outPosAudio2TrimStart);
    int64_t outPosMixEnd = std::min(outPosAudio1TrimEnd, outPosAudio2TrimEnd);

    int64_t mixLen = outPosMixEnd - outPosMixStart;

    fadeinSamples = std::min(fadeinSamples, mixLen);
    fadeoutSamples = std::min(fadeoutSamples, mixLen);

    outPosFadeinStart = outPosMixStart;
    outPosFadeinEnd = outPosMixStart + fadeinSamples;

    outPosFadeoutStart = outPosMixEnd - fadeoutSamples;
    outPosFadeoutEnd = outPosMixEnd;

    // create destination audio information
    outInfo = audio1Info;
    outInfo.numSamples = outLen;
    outInfo.numFrames = vsutils::samplesToFrames(outInfo.numSamples);

    outSampleType = common::getSampleTypeFromAudioFormat(outInfo.format).value();

    if (relativeGain)
    {
        // scale audio1Gain and audio2Gain so they add up to 1
        double totalGain = audio1Gain + audio2Gain;
        if (totalGain == 0)
        {
            audio1Scale = 0;
            audio2Scale = 0;
        }
        else
        {
            audio1Scale = audio1Gain / totalGain;
            audio2Scale = audio2Gain / totalGain;
        }
    }
    else
    {
        audio1Scale = audio1Gain;
        audio2Scale = audio2Gain;
    }

    if (0 < fadeinSamples)
    {
        fadeinTrans = common::newTransition(fadeType, 0, 0, static_cast<double>(fadeinSamples - 1), 1);
    }

    if (0 < fadeoutSamples)
    {
        fadeoutTrans = common::newTransition(fadeType, 0, 1, static_cast<double>(fadeoutSamples - 1), 0);
    }
}

VSNode* Mix::getAudio1()
{
    return audio1;
}


VSNode* Mix::getAudio2()
{
    return audio2;
}


const VSAudioInfo& Mix::getOutInfo()
{
    return outInfo;
}


void Mix::printDebugInfo(VSCore* core, const VSAPI* vsapi)
{
    std::string msg = std::format("{}: audio1.length: {}", FuncName, audio1Info.numSamples);
    vsapi->logMessage(VSMessageType::mtInformation, msg.c_str(), core);

    msg = std::format("{}: audio2.length: {}", FuncName, audio2Info.numSamples);
    vsapi->logMessage(VSMessageType::mtInformation, msg.c_str(), core);

    msg = std::format("{}: outPosAudio1Start: {}", FuncName, outPosAudio1Start);
    vsapi->logMessage(VSMessageType::mtInformation, msg.c_str(), core);

    msg = std::format("{}: outPosAudio2Start: {}", FuncName, outPosAudio2Start);
    vsapi->logMessage(VSMessageType::mtInformation, msg.c_str(), core);

    msg = std::format("{}: outPosAudio1TrimStart: {}", FuncName, outPosAudio1TrimStart);
    vsapi->logMessage(VSMessageType::mtInformation, msg.c_str(), core);

    msg = std::format("{}: outPosAudio1TrimEnd: {}", FuncName, outPosAudio1TrimEnd);
    vsapi->logMessage(VSMessageType::mtInformation, msg.c_str(), core);

    msg = std::format("{}: outPosAudio2TrimStart: {}", FuncName, outPosAudio2TrimStart);
    vsapi->logMessage(VSMessageType::mtInformation, msg.c_str(), core);

    msg = std::format("{}: outPosAudio2TrimEnd: {}", FuncName, outPosAudio2TrimEnd);
    vsapi->logMessage(VSMessageType::mtInformation, msg.c_str(), core);
}


bool Mix::isEditChannel(int ch)
{
    return editChannels.contains(ch);
}


common::OffsetFramePos Mix::outFrameToAudio1Frames(int outFrmNum)
{
    return common::baseFrameToOffsetFramesTrim(outFrmNum, outPosAudio1Start, audio1Info.numSamples, outPosAudio1TrimStart, outPosAudio1TrimEnd, outInfo.numSamples);
}


common::OffsetFramePos Mix::outFrameToAudio2Frames(int outFrmNum)
{
    return common::baseFrameToOffsetFramesTrim(outFrmNum, outPosAudio2Start, audio2Info.numSamples, outPosAudio2TrimStart, outPosAudio2TrimEnd, outInfo.numSamples);
}


void Mix::resetOverflowStats()
{
    overflowStats = { .count = 0, .peak = 0.0 };
}


void Mix::logOverflowStats(VSCore* core, const VSAPI* vsapi)
{
    if (0 < overflowStats.count)
    {
        overflowStats.logVS(FuncName, overflowMode, isFloatSampleType(outSampleType), core, vsapi);
    }
}


void Mix::free(const VSAPI* vsapi)
{
    delete fadeinTrans;
    delete fadeoutTrans;

    vsapi->freeNode(audio1);
    vsapi->freeNode(audio2);
}


template <typename sample_t, size_t IntSampleBits>
bool Mix::writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen,
                            const VSFrame* a1FrmL, const VSFrame* a1FrmR,
                            const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                            const common::OverflowContext& ofCtx)
{
    bool audio2Enabled = isEditChannel(ch);

    sample_t* outFrmPtr = reinterpret_cast<sample_t*>(ofCtx.vsapi->getWritePtr(outFrm, ch));

    const sample_t* a1FrmLPtr = reinterpret_cast<const sample_t*>(a1FrmL ? ofCtx.vsapi->getReadPtr(a1FrmL, ch) : nullptr);
    const sample_t* a1FrmRPtr = reinterpret_cast<const sample_t*>(a1FrmR ? ofCtx.vsapi->getReadPtr(a1FrmR, ch) : nullptr);
    const sample_t* a2FrmLPtr = reinterpret_cast<const sample_t*>(a2FrmL ? ofCtx.vsapi->getReadPtr(a2FrmL, ch) : nullptr);
    const sample_t* a2FrmRPtr = reinterpret_cast<const sample_t*>(a2FrmR ? ofCtx.vsapi->getReadPtr(a2FrmR, ch) : nullptr);

    constexpr vsutils::BitShift bitShift = vsutils::getSampleBitShift<sample_t, IntSampleBits>();

    for (int s = 0; s < outFrmLen; ++s)
    {
        int64_t outPos = outPosFrmStart + s;

        if (outPosAudio1TrimStart <= outPos && outPos < outPosAudio1TrimEnd)
        {
            // audio1
            sample_t audio1Sample = common::getOffsetSample(s, audio1FrameSampleOffsets, a1FrmLPtr, a1FrmRPtr);
            if constexpr (bitShift.required)
            {
                audio1Sample >>= bitShift.count;
            }

            if (audio2Enabled && outPosAudio2TrimStart <= outPos && outPos < outPosAudio2TrimEnd)
            {
                // mix audio1 and audio2
                sample_t audio2Sample = common::getOffsetSample(s, audio2FrameSampleOffsets, a2FrmLPtr, a2FrmRPtr);
                if constexpr (bitShift.required)
                {
                    audio2Sample >>= bitShift.count;
                }

                int64_t fadeinPos = outPos - outPosFadeinStart;
                int64_t fadeoutPos = outPos - outPosFadeoutStart;
                double audio1FadeinScale = 1;
                double audio2FadeinScale = 1;
                double audio1FadeoutScale = 1;
                double audio2FadeoutScale = 1;

                if (outPosFadeinStart <= outPos && outPos < outPosFadeinEnd && fadeinTrans)
                {
                    if (fadeinAudio2)
                    {
                        audio2FadeinScale = fadeinTrans->calcY(static_cast<double>(fadeinPos));
                    }
                    else
                    {
                        audio1FadeinScale = fadeinTrans->calcY(static_cast<double>(fadeinPos));
                    }
                }

                if (outPosFadeoutStart <= outPos && outPos < outPosFadeoutEnd && fadeoutTrans)
                {
                    if (fadeoutAudio2)
                    {
                        audio2FadeoutScale = fadeoutTrans->calcY(static_cast<double>(fadeoutPos));
                    }
                    else
                    {
                        audio1FadeoutScale = fadeoutTrans->calcY(static_cast<double>(fadeoutPos));
                    }
                }

                // mix audio1Sample and audio2Sample
                double mixedSample = audio1Scale * audio1FadeinScale * audio1FadeoutScale * utils::convSampleToDouble<sample_t, IntSampleBits>(audio1Sample) +
                                     audio2Scale * audio2FadeinScale * audio2FadeoutScale * utils::convSampleToDouble<sample_t, IntSampleBits>(audio2Sample);

                if (!common::safeWriteSample<sample_t, IntSampleBits>(mixedSample, outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
                {
                    // overflow and error
                    return false;
                }
            }
            else
            {
                // only audio1
                double scaledSample = audio1Scale * utils::convSampleToDouble<sample_t, IntSampleBits>(audio1Sample);

                if (!common::safeWriteSample<sample_t, IntSampleBits>(scaledSample, outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
                {
                    // overflow and error
                    return false;
                }
            }
        }
        else
        {
            if (audio2Enabled)
            {
                if (outPosAudio2TrimStart <= outPos && outPos < outPosAudio2TrimEnd)
                {
                    // only audio2
                    sample_t audio2Sample = common::getOffsetSample(s, audio2FrameSampleOffsets, a2FrmLPtr, a2FrmRPtr);
                    if constexpr (bitShift.required)
                    {
                        audio2Sample >>= bitShift.count;
                    }

                    double scaledSample = audio2Scale * utils::convSampleToDouble<sample_t, IntSampleBits>(audio2Sample);

                    if (!common::safeWriteSample<sample_t, IntSampleBits>(scaledSample, outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
                    {
                        // overflow and error
                        return false;
                    }
                }
                else
                {
                    // no audio, not supposed to happen
                    assertm(false, "sample is neither in the left nor in the right frame");
                }
            }
            else
            {
                // fill with zeros
                if (!common::safeWriteSample<sample_t, IntSampleBits>(0, outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
                {
                    // overflow and error
                    return false;
                }
            }
        }
    }

    return true;
}


template <typename sample_t, size_t IntSampleBits>
bool Mix::writeFrameImpl(VSFrame* outFrm, int outFrmNum,
                     const VSFrame* a1FrmL, const VSFrame* a1FrmR,
                     const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                     const common::OverflowContext& ofCtx)
{
    int64_t outPosFrmStart = vsutils::frameToFirstSample(outFrmNum);
    int outFrmLen = ofCtx.vsapi->getFrameLength(outFrm);

    for (int ch = 0; ch < audio1Info.format.numChannels; ++ch)
    {
        if (!writeFrameChannel<sample_t, IntSampleBits>(ch, outFrm, outPosFrmStart, outFrmLen, a1FrmL, a1FrmR, a2FrmL, a2FrmR, ofCtx))
        {
            return false;
        }
    }
    return true;
}


bool Mix::writeFrame(VSFrame* outFrm, int outFrmNum,
                     const VSFrame* a1FrmL, const VSFrame* a1FrmR,
                     const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                     VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    common::OverflowContext ofCtx =
        { .mode = overflowMode, .log = overflowLog, .funcName = FuncName,
          .frameCtx = frameCtx, .core = core, .vsapi = vsapi };

    switch (outSampleType)
    {
        case common::SampleType::Int8:
            return writeFrameImpl<int8_t, 8>(outFrm, outFrmNum, a1FrmL, a1FrmR, a2FrmL, a2FrmR, ofCtx);
        case common::SampleType::Int16:
            return writeFrameImpl<int16_t, 16>(outFrm, outFrmNum, a1FrmL, a1FrmR, a2FrmL, a2FrmR, ofCtx);
        case common::SampleType::Int24:
            return writeFrameImpl<int32_t, 24>(outFrm, outFrmNum, a1FrmL, a1FrmR, a2FrmL, a2FrmR, ofCtx);
        case common::SampleType::Int32:
            return writeFrameImpl<int32_t, 32>(outFrm, outFrmNum, a1FrmL, a1FrmR, a2FrmL, a2FrmR, ofCtx);
        case common::SampleType::Float32:
            return writeFrameImpl<float, 0>(outFrm, outFrmNum, a1FrmL, a1FrmR, a2FrmL, a2FrmR, ofCtx);
        case common::SampleType::Float64:
            return writeFrameImpl<double, 0>(outFrm, outFrmNum, a1FrmL, a1FrmR, a2FrmL, a2FrmR, ofCtx);
        default:
            return false;
    }
}


static void VS_CC mixFree(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
    Mix* data = static_cast<Mix*>(instanceData);
    // skip logging since the total number of overflows is unreliable
    // data->logNumOverflows(core, vsapi);
    data->free(vsapi);
    delete data;
}


static const VSFrame* VS_CC mixGetFrame(int outFrmNum, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    Mix* data = static_cast<Mix*>(instanceData);

    common::OffsetFramePos a1FrmNums = data->outFrameToAudio1Frames(outFrmNum);
    common::OffsetFramePos a2FrmNums = data->outFrameToAudio2Frames(outFrmNum);

    if (activationReason == VSActivationReason::arInitial)
    {
        if (0 <= a1FrmNums.left)
        {
            vsapi->requestFrameFilter(a1FrmNums.left, data->getAudio1(), frameCtx);
        }

        if (0 <= a1FrmNums.right)
        {
            vsapi->requestFrameFilter(a1FrmNums.right, data->getAudio1(), frameCtx);
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

        const VSFrame* a1FrmL = nullptr;
        const VSFrame* a1FrmR = nullptr;
        const VSFrame* a2FrmL = nullptr;
        const VSFrame* a2FrmR = nullptr;
        const VSFrame* propFrm = nullptr;

        if (0 <= a1FrmNums.left)
        {
            a1FrmL = vsapi->getFrameFilter(a1FrmNums.left, data->getAudio1(), frameCtx);
            if (!propFrm)
            {
                propFrm = a1FrmL;
            }
        }

        if (0 <= a1FrmNums.right)
        {
            a1FrmR = vsapi->getFrameFilter(a1FrmNums.right, data->getAudio1(), frameCtx);
            if (!propFrm)
            {
                propFrm = a1FrmR;
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

        bool success = data->writeFrame(outFrm, outFrmNum, a1FrmL, a1FrmR, a2FrmL, a2FrmR, frameCtx, core, vsapi);

        if (a1FrmL)
        {
            vsapi->freeFrame(a1FrmL);
        }

        if (a1FrmR)
        {
            vsapi->freeFrame(a1FrmR);
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


static void VS_CC mixCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
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
        vsapi->freeNode(audio1);
        return;
    }

    const VSAudioInfo* audio2Info = vsapi->getAudioInfo(audio2);

    if (!vsh::isSameAudioInfo(audio1Info, audio2Info))
    {
        std::string errMsg = std::format("{}: clips have different audio format", FuncName);
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

    // clip2_offset_samples:int:opt
    // clip2_offset_seconds:float:opt
    int64_t audio2OffsetSamples = vsmap::getOptSamples("clip2_offset_samples", "clip2_offset_seconds", in, out, vsapi, DefaultAudio2StartSample, audio1Info->sampleRate);

    // check audio2 start sample position
    if ((0 < audio2OffsetSamples && audio1Info->numSamples < audio2OffsetSamples) ||
        (audio2OffsetSamples < 0 && audio2Info->numSamples < -audio2OffsetSamples))
    {
        std::string errMsg = std::format("{}: invalid clip2 start: clip2 does not overlap with clip1", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    // clip1_gain:float:opt
    double audio1Gain = vsmap::getOptDouble("clip1_gain", in, vsapi, DefaultAudio1Gain);
    if (audio1Gain < 0)
    {
        std::string errMsg = std::format("{}: negative clip1_gain", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    // clip2_gain:float:opt
    double audio2Gain = vsmap::getOptDouble("clip2_gain", in, vsapi, DefaultAudio2Gain);
    if (audio2Gain < 0)
    {
        std::string errMsg = std::format("{}: negative clip2_gain", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    // relative_gain:int:opt
    bool relativeGain = vsmap::getOptBool("relative_gain", in, vsapi, DefaultRelativeGain);

    // fadein_samples:int:opt
    // fadein_seconds:float:opt
    // fadein_samples has a higher priority than fadein_seconds
    int64_t fadeinSamples = vsmap::getOptSamples("fadein_samples", "fadein_seconds", in, out, vsapi, DefaultFadeInSamples, audio1Info->sampleRate);
    if (fadeinSamples < 0)
    {
        std::string errMsg = std::format("{}: negative fadein length", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    // fadeout_samples:int:opt
    // fadeout_seconds:float:opt
    // fadeout_samples has a higher priority than fadeout_seconds
    int64_t fadeoutSamples = vsmap::getOptSamples("fadeout_samples", "fadeout_seconds", in, out, vsapi, DefaultFadeOutSamples, audio1Info->sampleRate);
    if (fadeoutSamples < 0)
    {
        std::string errMsg = std::format("{}: negative fadeout length", FuncName);
        vsapi->mapSetError(out, errMsg.c_str());
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    // fade_type:data:opt
    std::optional<common::TransitionType> optFadeType = vsmap::getOptTransitionTypeFromString("fade_type", FuncName, in, out, vsapi, DefaultFadeType);
    if (!optFadeType.has_value())
    {
        vsapi->freeNode(audio1);
        vsapi->freeNode(audio2);
        return;
    }

    // extend_start:int:opt
    // extend or trim
    bool extendStart = vsmap::getOptBool("extend_start", in, vsapi, DefaultExtendStart);

    // extend_end:int:opt
    // extend or trim
    bool extendEnd = vsmap::getOptBool("extend_end", in, vsapi, DefaultExtendEnd);

    // channels:int[]:opt
    std::vector<int> defaultChannels;
    std::optional<std::vector<int>> optChannels = vsmap::getOptChannels("channels", FuncName, in, out, vsapi, defaultChannels, audio1Info->format.numChannels);
    if (!optChannels.has_value())
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

    Mix* data = new Mix(audio1, audio1Info, audio1Gain, audio2, audio2Info, audio2Gain, audio2OffsetSamples, relativeGain,
                        fadeinSamples, fadeoutSamples, optFadeType.value(), extendStart, extendEnd, optChannels.value(),
                        optOverflowMode.value(), optOverflowLog.value());

    //data->printDebugInfo(core, vsapi);

    VSFilterDependency deps[] = {{ audio1, VSRequestPattern::rpGeneral }, { audio2, VSRequestPattern::rpGeneral }};

    // fmParallelRequests: strict sequential frame requests for overflow logging
    vsapi->createAudioFilter(out, FuncName, &data->getOutInfo(), mixGetFrame, mixFree, VSFilterMode::fmParallelRequests, deps, 2, data, core);
}


void mixInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi)
{
    vspapi->registerFunction(FuncName,
                             "clip1:anode;"
                             "clip2:anode;"
                             "clip2_offset_samples:int:opt;"
                             "clip2_offset_seconds:float:opt;"
                             "clip1_gain:float:opt;"
                             "clip2_gain:float:opt;"
                             "relative_gain:int:opt;"
                             "fadein_samples:int:opt;"
                             "fadein_seconds:float:opt;"
                             "fadeout_samples:int:opt;"
                             "fadeout_seconds:float:opt;"
                             "fade_type:data:opt;"
                             "extend_start:int:opt;"
                             "extend_end:int:opt;"
                             "channels:int[]:opt;"
                             "overflow:data:opt;"
                             "overflow_log:data:opt;",
                             "return:anode;",
                             mixCreate, nullptr, plugin);
}
