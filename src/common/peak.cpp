#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "VapourSynth4.h"

#include "common/peak.hpp"
#include "common/sampletype.hpp"
#include "utils/number.hpp"

namespace common
{
    static std::optional<PeakResult> findFramePeak(const VSFrame* frame, common::SampleType sampleType, const std::vector<int>& channels, bool normalize, const VSAPI* vsapi)
    {
        switch (sampleType)
        {
            case common::SampleType::Int8:
                return findFramePeakImpl<int8_t, 8>(frame, channels, normalize, vsapi);
            case common::SampleType::Int16:
                return findFramePeakImpl<int16_t, 16>(frame, channels, normalize, vsapi);
            case common::SampleType::Int24:
                return findFramePeakImpl<int32_t, 24>(frame, channels, normalize, vsapi);
            case common::SampleType::Int32:
                return findFramePeakImpl<int32_t, 32>(frame, channels, normalize, vsapi);
            case common::SampleType::Float32:
                return findFramePeakImpl<float, 0>(frame, channels, normalize, vsapi);
            case common::SampleType::Float64:
                return findFramePeakImpl<double, 0>(frame, channels, normalize, vsapi);
            default:
                return std::nullopt;
        }
    }


    double findPeak(VSNode* audio, const VSAudioInfo* audioInfo, const std::vector<int>& channels, bool normalize, const VSAPI* vsapi)
    {
        common::SampleType sampleType = common::getSampleTypeFromAudioFormat(audioInfo->format).value();

        double peak = 0;
        // found absolute maximum peak? -> skip remaining frames if true
        bool isMax = false;

        for (int n = 0; n < audioInfo->numFrames && !isMax; ++n)
        {
            const VSFrame* frame = vsapi->getFrame(n, audio, nullptr, 0);
            if (frame)
            {
                if (auto optPeakResult = findFramePeak(frame, sampleType, channels, normalize, vsapi))
                {
                    isMax |= optPeakResult.value().isMax;

                    if (std::abs(peak) < std::abs(optPeakResult.value().value))
                    {
                        peak = optPeakResult.value().value;
                    }
                }

                vsapi->freeFrame(frame);
            }
        }
        return peak;
    }


    double adjustNormPeak(double normPeak, common::SampleType st)
    {
        double absNormPeak = std::abs(normPeak);
        double tmp;
        switch (st)
        {
            case common::SampleType::Int8:
                tmp = std::floor(absNormPeak * static_cast<double>(utils::maxInt<int8_t, 8>)) / static_cast<double>(utils::maxInt<int8_t, 8>);
                break;
            case common::SampleType::Int16:
                tmp = std::floor(absNormPeak * static_cast<double>(utils::maxInt<int16_t, 16>)) / static_cast<double>(utils::maxInt<int16_t, 16>);
                break;
            case common::SampleType::Int24:
                tmp = std::floor(absNormPeak * static_cast<double>(utils::maxInt<int32_t, 24>)) / static_cast<double>(utils::maxInt<int32_t, 24>);
                break;
            case common::SampleType::Int32:
                tmp = std::floor(absNormPeak * static_cast<double>(utils::maxInt<int32_t, 32>)) / static_cast<double>(utils::maxInt<int32_t, 32>);
                break;
            case common::SampleType::Float32:
                // find the next closest float value that is smaller or equal to absNormPeak
                tmp = static_cast<double>(utils::castToFloatTowardsZero<float>(absNormPeak));
                break;
            case common::SampleType::Float64:
            default:
                tmp = absNormPeak;
                break;
        }
        return std::copysign(tmp, normPeak);
    }
}
