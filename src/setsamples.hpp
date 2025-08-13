// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <vector>

#include "VapourSynth4.h"

#include "common/overflow.hpp"
#include "common/sampletype.hpp"

class SetSamples
{
public:
    SetSamples(VSNode* audio, const VSAudioInfo* audioInfo, double sample, int64_t outPosStart, int64_t outPosEnd,
               std::vector<int> channels, common::OverflowMode overflowMode, common::OverflowLog overflowLog);

    VSNode* getAudio();

    const VSAudioInfo& getOutInfo();

    void resetOverflowStats();

    void logOverflowStats(VSCore* core, const VSAPI* vsapi);

    void free(const VSAPI* vsapi);

    bool writeFrame(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi);

private:
    VSNode* audio;
    const VSAudioInfo audioInfo;

    common::SampleType outSampleType;

    double sample;

    // inclusive
    int64_t outPosStart;
    // exclusive
    int64_t outPosEnd;

    std::vector<int> editChannels;
    std::vector<int> copyChannels;

    common::OverflowMode overflowMode;
    common::OverflowLog overflowLog;

    common::OverflowStats overflowStats = { .count = 0, .peak = 0.0 };

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen, const VSFrame* inFrm,
                           const common::OverflowContext& ofCtx);

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameImpl(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, const common::OverflowContext& ofCtx);
};


void setsamplesInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi);
