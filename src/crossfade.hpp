// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "VapourSynth4.h"

#include "common/offset.hpp"
#include "common/overflow.hpp"
#include "common/sampletype.hpp"
#include "common/transition.hpp"

class CrossFade
{
public:
    CrossFade(VSNode* audio1, const VSAudioInfo* audio1Info, VSNode* audio2, const VSAudioInfo* audio2Info,
              int64_t fadeSamples, common::TransitionType transType,
              common::OverflowMode overflowMode, common::OverflowLog overflowLog);

    VSNode* getAudio1();

    VSNode* getAudio2();

    const VSAudioInfo& getOutInfo();

    void resetOverflowStats();

    void logOverflowStats(VSCore* core, const VSAPI* vsapi);

    void free(const VSAPI* vsapi);

    int outFrameToAudio1Frame(int outFrmNum);

    common::OffsetFramePos outFrameToAudio2Frames(int outFrmNum);

    bool writeFrame(VSFrame* outFrm, int outFrmNum,
                    const VSFrame* a1Frm, const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                    VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi);

private:
    VSNode* audio1;
    // TODO: store actual value not a pointer
    const VSAudioInfo audio1Info;

    VSNode* audio2;
    // TODO: store actual value not a pointer
    const VSAudioInfo audio2Info;

    VSAudioInfo outInfo;
    common::SampleType outSampleType;

    common::OverflowMode overflowMode;
    common::OverflowLog overflowLog;

    common::OverflowStats overflowStats = { .count = 0, .peak = 0.0 };

    // transition is expected to go from (0, 1) to (samples - 1, 0)
    common::Transition* fadeoutTrans = nullptr;

    // inclusive
    int64_t outPosFadeStart;
    // exclusive
    int64_t outPosFadeEnd;

    common::FrameSampleOffsets audio2FrameSampleOffsets;

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen,
                           const VSFrame* a1Frm, const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                           const common::OverflowContext& ofCtx);

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameImpl(VSFrame* outFrm, int outFrmNum,
                        const VSFrame* a1Frm, const VSFrame* a2FrmL, const VSFrame* a2FrmR,
                        const common::OverflowContext& ofCtx);
};

void crossfadeInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi);
