// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <vector>

#include "VapourSynth4.h"

#include "common/offset.hpp"
#include "common/overflow.hpp"
#include "common/sampletype.hpp"

class Delay
{
public:
    // positive samples shift the audio stream to the 'right'
    // negative samples shift the audio stream to the 'left'
    Delay(VSNode* audio, const VSAudioInfo* audioInfo, int64_t samples, std::vector<int> editChannels,
          common::OverflowMode overflowMode, common::OverflowLog overflowLog);

    VSNode* getAudio();

    const VSAudioInfo& getOutInfo();

    common::OffsetFramePos outFrameToOffsetInFrames(int outFrmNum);

    size_t getNumCopyChannels();

    size_t getNumEditChannels();

    void resetOverflowStats();

    void logOverflowStats(VSCore* core, const VSAPI* vsapi);

    void free(const VSAPI* vsapi);

    bool writeFrame(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm,
                    const VSFrame* offsetInFrmL, const VSFrame* offsetInFrmR,
                    VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi);

private:
    VSNode* audio;
    const VSAudioInfo audioInfo;

    common::SampleType outSampleType;

    std::vector<int> editChannels;
    std::vector<int> copyChannels;

    common::OverflowMode overflowMode;
    common::OverflowLog overflowLog;

    common::OverflowStats overflowStats = { .count = 0, .peak = 0.0 };

    // inclusive
    int64_t outPosOffsetStart;
    // exclusive
    int64_t outPosOffsetEnd;

    common::FrameSampleOffsets audioFrameSampleOffsets;

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen,
                           const VSFrame* offsetInFrmL, const VSFrame* offsetInFrmR,
                           const common::OverflowContext& ofCtx);

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameImpl(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm,
                        const VSFrame* offsetInFrmL, const VSFrame* offsetInFrmR,
                        const common::OverflowContext& ofCtx);
};


void delayInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi);
