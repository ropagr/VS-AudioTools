// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "VapourSynth4.h"

#include "common/overflow.hpp"
#include "common/sampletype.hpp"

class SineTone
{
public:
    SineTone(int64_t numSamples, uint64_t channelLayout, int sampleRate, common::SampleType sampleType,
             double freq, double amplitude, common::OverflowMode overflowMode, common::OverflowLog overflowLog);

    const VSAudioInfo& getOutInfo();

    void resetOverflowStats();

    void logOverflowStats(VSCore* core, const VSAPI* vsapi);

    void free(const VSAPI* vsapi);

    bool writeFrame(VSFrame* outFrm, int outFrmNum, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi);

private:
    VSAudioInfo outInfo;

    common::SampleType outSampleType;

    double freq;
    double amplitude;
    double absAmplitude;

    common::OverflowMode overflowMode;
    common::OverflowLog overflowLog;

    common::OverflowStats overflowStats = { .count = 0, .peak = 0.0 };

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameChannel(int ch, VSFrame* outFrm, int64_t outPosFrmStart, int outFrmLen,
                           const common::OverflowContext& ofCtx);

    template <typename sample_t, size_t IntSampleBits>
    bool writeFrameImpl(VSFrame* outFrm, int outFrmNum, const common::OverflowContext& ofCtx);
};


void sinetoneInit(VSPlugin* plugin, const VSPLUGINAPI* vspapi);
