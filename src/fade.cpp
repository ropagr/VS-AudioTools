// SPDX-License-Identifier: MIT

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include "fade.hpp"
#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "common/transition.hpp"
#include "utils/sample.hpp"
#include "utils/vector.hpp"
#include "vsutils/audio.hpp"
#include "vsutils/bitshift.hpp"

Fade::Fade(VSNode* _audio, const VSAudioInfo* _audioInfo, int64_t _outPosFadeStart, int64_t _fadeSamples, std::vector<int> channels,
           common::Transition* _fadeTrans, common::OverflowMode _overflowMode, common::OverflowLog _overflowLog, const char* _funcName) :
    audio(_audio), audioInfo(*_audioInfo), outPosFadeStart(_outPosFadeStart), fadeSamples(_fadeSamples), editChannels(channels),
    fadeTrans(_fadeTrans), overflowMode(_overflowMode), overflowLog(_overflowLog), funcName(_funcName)
{
    outPosFadeEnd = outPosFadeStart + fadeSamples;

    outSampleType = common::getSampleTypeFromAudioFormat(audioInfo.format).value();

    outFrameFadeStart = vsutils::sampleToFrame(outPosFadeStart);

    outFrameFadeEnd = vsutils::sampleToFrame(outPosFadeEnd - 1) + 1;

    copyChannels = utils::vectorInvert(editChannels, 0, audioInfo.format.numChannels);
}


VSNode* Fade::getAudio()
{
    return audio;
}


const VSAudioInfo& Fade::getOutInfo()
{
    return audioInfo;
}


int Fade::getFadeStartFrame()
{
    return outFrameFadeStart;
}


int Fade::getFadeEndFrame()
{
    return outFrameFadeEnd;
}


void Fade::resetOverflowStats()
{
    overflowStats = { .count = 0, .peak = 0.0 };
}


void Fade::logOverflowStats(VSCore* core, const VSAPI* vsapi)
{
    if (0 < overflowStats.count)
    {
        overflowStats.logVS(funcName, overflowMode, isFloatSampleType(outSampleType), core, vsapi);
    }
}


void Fade::free(const VSAPI* vsapi)
{
    delete fadeTrans;
    vsapi->freeNode(audio);
}


template <typename sample_t, size_t IntSampleBits>
bool Fade::writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen, const VSFrame* inFrm,
                             const common::OverflowContext& ofCtx)
{
    sample_t* outFrmPtr = reinterpret_cast<sample_t*>(ofCtx.vsapi->getWritePtr(outFrm, ch));
    const sample_t* inFrmPtr = reinterpret_cast<const sample_t*>(ofCtx.vsapi->getReadPtr(inFrm, ch));

    constexpr vsutils::BitShift bitShift = vsutils::getSampleBitShift<sample_t, IntSampleBits>();

    for (int s = 0; s < outFrmLen; ++s)
    {
        int64_t outPos = outPosFrmStart + s;
        if (outPosFadeStart <= outPos && outPos < outPosFadeEnd && fadeTrans)
        {
            // sample inside fade transition
            int64_t fadePos = outPos - outPosFadeStart;

            double fadeScale = fadeTrans->calcY(static_cast<double>(fadePos));

            sample_t inSample = inFrmPtr[s];

            if constexpr (bitShift.required)
            {
                inSample >>= bitShift.count;
            }

            double scaledSample = fadeScale * utils::convSampleToDouble<sample_t, IntSampleBits>(inSample);

            if (!common::safeWriteSample<sample_t, IntSampleBits>(scaledSample, outFrmPtr, s, outPos, ch, ofCtx, overflowStats))
            {
                // overflow and error
                return false;
            }
        }
        else
        {
            // sample outside transition -> copy sample
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
bool Fade::writeFrameImpl(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, const common::OverflowContext& ofCtx)
{
    int bytesPerSample = audioInfo.format.bytesPerSample;

    // copy channels
    for (const int& ch : copyChannels)
    {
        vsutils::copyFrameChannel(outFrm, ch, inFrm, ch, bytesPerSample, ofCtx.vsapi);
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


bool Fade::writeFrame(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    common::OverflowContext ofCtx =
        { .mode = overflowMode, .log = overflowLog, .funcName = funcName,
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


void VS_CC fadeFree(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
    Fade* data = static_cast<Fade*>(instanceData);
    // skip logging since the total number of overflows is unreliable
    // data->logNumOverflows(core, vsapi);
    data->free(vsapi);
    delete data;
}


const VSFrame* VS_CC fadeGetFrame(int outFrmNum, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    Fade* data = static_cast<Fade*>(instanceData);

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

        if (outFrmNum < data->getFadeStartFrame() || data->getFadeEndFrame() <= outFrmNum)
        {
            return inFrm;
        }

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
