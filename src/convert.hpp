// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "VapourSynth4.h"

#include "common/overflow.hpp"
#include "common/sampletype.hpp"

class Convert
{
public:
    Convert(VSNode* audio, const VSAudioInfo* audioInfo, common::SampleType outSampleType, common::OverflowMode overflowMode, common::OverflowLog overflowLog);

    VSNode* getAudio();

    const VSAudioInfo& getOutInfo();

    bool isPassthrough();

    void resetOverflowStats();

    void logOverflowStats(VSCore* core, const VSAPI* vsapi);

    void free(const VSAPI* vsapi);

    bool writeFrame(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi);

private:
    VSNode* audio;
    VSAudioInfo outInfo;

    common::SampleType inSampleType;
    common::SampleType outSampleType;

    common::OverflowMode overflowMode;
    common::OverflowLog overflowLog;

    common::OverflowStats overflowStats = { .count = 0, .peak = 0.0 };

    template <typename in_sample_t, size_t InSampleIntBits, typename out_sample_t, size_t OutSampleIntBits>
    bool writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen, const VSFrame* inFrm,
                           const common::OverflowContext& ofCtx);

    template <typename in_sample_t, size_t InSampleIntBits, typename out_sample_t, size_t OutSampleIntBits>
    bool writeFrameImpl(VSFrame* outFrm, int outFrmNum, const VSFrame* inFrm, const common::OverflowContext& ofCtx);
};


void convertInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi);
