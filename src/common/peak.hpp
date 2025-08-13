// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <type_traits>
#include <vector>

#include "VapourSynth4.h"

#include "common/sampletype.hpp"
#include "utils/number.hpp"
#include "utils/sample.hpp"
#include "vsutils/bitshift.hpp"

namespace common
{
    struct PeakResult
    {
        double value;
        bool isMax;
    };


    /**
     * returns a pair:
     *     first:  peak value of this frame (actual sample value or normalized sample [0,1])
     *     second: absolute max. peak found? -> skip reading other frames if true
     */
    template <typename sample_t, size_t IntSampleBits>
    requires std::integral<sample_t> || std::floating_point<sample_t>
    PeakResult findFramePeakImpl(const VSFrame* frame, const std::vector<int>& channels, bool normalize, const VSAPI* vsapi)
    {
        if constexpr (std::is_integral_v<sample_t>)
        {
            // Integer
            constexpr vsutils::BitShift bitShift = vsutils::getSampleBitShift<sample_t, IntSampleBits>();

            sample_t posPeak = 0;
            sample_t negPeak = 0;

            bool foundMaxPeak = false;

            int frmLen = vsapi->getFrameLength(frame);

            for (const int& ch : channels)
            {
                const sample_t* frmPtr_sample_t = reinterpret_cast<const sample_t*>(vsapi->getReadPtr(frame, ch));

                for (int s = 0; s < frmLen; ++s)
                {
                    sample_t sample = frmPtr_sample_t[s];

                    if constexpr (bitShift.required)
                    {
                        sample >>= bitShift.count;
                    }

                    if (sample < negPeak)
                    {
                        negPeak = sample;

                        if (negPeak <= -utils::maxInt<sample_t, IntSampleBits>)
                        {
                            if (negPeak == utils::minInt<sample_t, IntSampleBits> || normalize)
                            {
                                // max abs or max normalization value -> break both loops
                                foundMaxPeak = true;
                                break;
                            }
                        }
                    }
                    else if (posPeak < sample)
                    {
                        posPeak = sample;

                        if (posPeak == utils::maxInt<sample_t, IntSampleBits> && normalize)
                        {
                            // max normalization value -> break both loops
                            foundMaxPeak = true;
                            break;
                        }
                    }
                }

                if (foundMaxPeak)
                {
                    break;
                }
            }

            if (normalize)
            {
                // convSymSampleToDouble normalizes posPeak and negPeak to maxInt
                return { .value = std::max(         utils::convSampleToDouble<sample_t, IntSampleBits>(posPeak),
                                           std::abs(utils::convSampleToDouble<sample_t, IntSampleBits>(negPeak))),
                         .isMax = foundMaxPeak };
            }

            double posPeakD = static_cast<double>(posPeak);
            double negPeakD = static_cast<double>(negPeak);

            // compare absolute sample values
            return { .value = posPeakD < std::abs(negPeakD) ? negPeakD : posPeakD,
                     .isMax = foundMaxPeak };
        }

        if constexpr (std::is_floating_point_v<sample_t>)
        {
            // Float
            double peak = 0;

            int frmLen = vsapi->getFrameLength(frame);

            for (const int& ch : channels)
            {
                const sample_t* frmPtr_sample_t = reinterpret_cast<const sample_t*>(vsapi->getReadPtr(frame, ch));

                for (int s = 0; s < frmLen; ++s)
                {
                    sample_t sample = frmPtr_sample_t[s];

                    if (std::abs(peak) < std::abs(sample))
                    {
                        peak = sample;
                    }
                }
            }

            if (normalize)
            {
                return { .value = std::abs(peak),
                         .isMax = false };
            }

            return { .value = peak,
                     .isMax = false };
        }
    }


    /**
     * reads all frames to determine the peak value
     * this is blocking until all frames are read
     * skips the remaining frames if maximum possible peak was found
     */
    double findPeak(VSNode* audio, const VSAudioInfo* audioInfo, const std::vector<int>& channels, bool normalize, const VSAPI* vsapi);


    double adjustNormPeak(double normPeak, common::SampleType st);
}
