// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <vector>

#include "VapourSynth4.h"

#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "common/transition.hpp"

class Fade
{
public:
    Fade(VSNode* audio, const VSAudioInfo* audioInfo, int64_t outPosStart, int64_t fadeSamples,
         std::vector<int> channels, common::Transition* fadeTrans,
         common::OverflowMode overflowMode, common::OverflowLog overflowLog, const char* funcName);

    VSNode* getAudio();

    const VSAudioInfo& getOutInfo();

    int getFadeStartFrame();

    int getFadeEndFrame();

    void resetOverflowStats();

    void logOverflowStats(VSCore* core, const VSAPI* vsapi);

    void free(const VSAPI* vsapi);

    bool writeFrame(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi);

private:
    VSNode* audio;
    const VSAudioInfo audioInfo;

    common::SampleType outSampleType;

    // inclusive
    int64_t outPosFadeStart;
    // exclusive
    int64_t outPosFadeEnd;

    // fade length
    int64_t fadeSamples;

    std::vector<int> editChannels;
    std::vector<int> copyChannels;

    // transition is expected to go from (0, 0) to (fadeSamples - 1, 1)
    //                           or from (0, 1) to (fadeSamples - 1, 0)
    common::Transition* fadeTrans = nullptr;

    // inclusive
    int outFrameFadeStart;
    // exclusive
    int outFrameFadeEnd;

    common::OverflowMode overflowMode;
    common::OverflowLog overflowLog;

    common::OverflowStats overflowStats = { .count = 0, .peak = 0.0 };

    const char* funcName = nullptr;

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen, const VSFrame* inFrm,
                           const common::OverflowContext& ofCtx);

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameImpl(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, const common::OverflowContext& ofCtx);
};

void VS_CC fadeFree(void* instanceData, VSCore* core, const VSAPI* vsapi);

const VSFrame* VS_CC fadeGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi);
