// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <set>
#include <vector>

#include "VapourSynth4.h"

#include "common/offset.hpp"
#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "common/transition.hpp"

class Mix
{
public:
    Mix(VSNode* audio1, const VSAudioInfo* audio1Info, double audio1Gain,
        VSNode* audio2, const VSAudioInfo* audio2Info, double audio2Gain,
        int64_t audio2OffsetSamples, bool relativeGain,
        int64_t fadeinSamples, int64_t fadeoutSamples, common::TransitionType fadeType,
        bool extendAudio1Start, bool extendAudio1End, std::vector<int> editChannels,
        common::OverflowMode overflowMode, common::OverflowLog overflowLog);

    VSNode* getAudio1();
    VSNode* getAudio2();

    const VSAudioInfo& getOutInfo();

    common::OffsetFramePos outFrameToAudio1Frames(int outFrmNum);
    common::OffsetFramePos outFrameToAudio2Frames(int outFrmNum);

    void resetOverflowStats();

    void logOverflowStats(VSCore* core, const VSAPI* vsapi);

    void free(const VSAPI* vsapi);

    void printDebugInfo(VSCore* core, const VSAPI* vsapi);

    bool writeFrame(VSFrame* outFrm, int outFrmNum,
                    const VSFrame* a1FrmL, const VSFrame* a1FrmR,
                    const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                    VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi);

private:
    VSNode* audio1;
    const VSAudioInfo audio1Info;
    double audio1Gain;
    double audio1Scale;

    VSNode* audio2;
    const VSAudioInfo audio2Info;
    double audio2Gain;
    double audio2Scale;

    VSAudioInfo outInfo;
    common::SampleType outSampleType;

    // relative or absolute gain
    bool relativeGain;

    //std::vector<int> editChannels;
    std::set<int> editChannels;

    common::OverflowMode overflowMode;
    common::OverflowLog overflowLog;

    common::OverflowStats overflowStats = { .count = 0, .peak = 0.0 };

    // fade in/out audio2 or audio1, depending on which clip starts later or ends first
    // which depends on extendAudio1Start and extendAudio1End
    bool fadeinAudio2;
    bool fadeoutAudio2;

    int64_t outPosAudio1Start;
    int64_t outPosAudio1End;
    // audio2 start/end can be outside of destination output
    int64_t outPosAudio2Start;
    int64_t outPosAudio2End;

    int64_t outPosAudio1TrimStart;
    int64_t outPosAudio1TrimEnd;
    int64_t outPosAudio2TrimStart;
    int64_t outPosAudio2TrimEnd;

    common::FrameSampleOffsets audio1FrameSampleOffsets;
    common::FrameSampleOffsets audio2FrameSampleOffsets;

    int64_t outPosFadeinStart;
    int64_t outPosFadeinEnd;

    int64_t outPosFadeoutStart;
    int64_t outPosFadeoutEnd;

    // fade in transition is going from (0, 0) to (fadeinSamples - 1, 1)
    common::Transition* fadeinTrans = nullptr;

    // fade out transition is going from (0, 1) to (fadeoutSamples - 1, 0)
    common::Transition* fadeoutTrans = nullptr;

    bool isEditChannel(int ch);

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen,
                           const VSFrame* a1FrmL, const VSFrame* a1FrmR,
                           const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                           const common::OverflowContext& ofCtx);

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameImpl(VSFrame* outFrm, int outFrmNum,
                        const VSFrame* a1FrmL, const VSFrame* a1FrmR,
                        const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                        const common::OverflowContext& ofCtx);
};

void mixInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi);
