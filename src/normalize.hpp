// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <vector>

#include "VapourSynth4.h"

#include "common/overflow.hpp"
#include "common/sampletype.hpp"

class Normalize
{
public:
    Normalize(VSNode* audio, const VSAudioInfo* audioInfo, double outNormPeak, bool lowerOnly, std::vector<int> editChannels,
              common::OverflowMode overflowMode, common::OverflowLog overflowLog, const VSAPI* vsapi);

    VSNode* getAudio();

    const VSAudioInfo& getOutInfo();

    void resetOverflowStats();

    void logOverflowStats(VSCore* core, const VSAPI* vsapi);

    void free(const VSAPI* vsapi);

    bool writeFrame(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm,
                    VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi);

private:
    VSNode* audio;
    const VSAudioInfo audioInfo;

    common::SampleType outSampleType;

    double outNormPeak;

    double gain;

    std::vector<int> editChannels;
    std::vector<int> copyChannels;

    common::OverflowMode overflowMode;
    common::OverflowLog overflowLog;

    common::OverflowStats overflowStats = { .count = 0, .peak = 0.0 };

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen, const VSFrame* inFrm, const common::OverflowContext& ofCtx);

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameImpl(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, const common::OverflowContext& ofCtx);
};


void normalizeInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi);
