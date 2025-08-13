// SPDX-License-Identifier: MIT

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "VapourSynth4.h"

#include "utils/debug.hpp"
#include "vsutils/audio.hpp"

namespace vsutils
{
    int64_t secondsToSamples(double seconds, int sampleRate)
    {
        return static_cast<int64_t>(std::round(seconds * static_cast<double>(sampleRate)));
    }

    double samplesToSeconds(int64_t samples, int sampleRate)
    {
        return static_cast<double>(samples) / static_cast<double>(sampleRate);
    }

    // returns the number of frames required to hold the specfified amount of samples
    int samplesToFrames(int64_t samples)
    {
        assertm(0 <= samples, "negative samples");

        int64_t frames = ((samples - 1) / VS_AUDIO_FRAME_SAMPLES) + 1;

        assertm(frames <= std::numeric_limits<int>::max(), "frames out of int range");

        return static_cast<int>(frames);
    }


    int getFrameSampleCount(int frame, int64_t totalSamples)
    {
        assertm(0 <= frame, "negative frame");

        int totalFrames = samplesToFrames(totalSamples);

        assertm(frame < totalFrames, "frame out of range");

        if (totalFrames <= frame)
        {
            return 0;
        }

        if (frame == totalFrames - 1)
        {
            // last frame
            return static_cast<int>(totalSamples - (frame * VS_AUDIO_FRAME_SAMPLES));
        }

        return VS_AUDIO_FRAME_SAMPLES;
    }


    bool isLastFrame(int frame, int64_t totalSamples)
    {
        assertm(0 <= frame, "negative frame");

        return frame == samplesToFrames(totalSamples) - 1;
    }


    // returns the first sample of a frame (inclusive)
    int64_t frameToFirstSample(int frame)
    {
        assertm(0 <= frame, "negative frame");

        return static_cast<int64_t>(frame) * VS_AUDIO_FRAME_SAMPLES;
    }


    // returns the last sample of a frame (exclusive)
    // or -1 if frame is outside of all samples
    int64_t frameToLastSample(int frame, int64_t totalSamples)
    {
        assertm(0 <= frame, "negative frame");

        int frameSamples = getFrameSampleCount(frame, totalSamples);
        if (frameSamples == 0)
        {
            return -1;
        }

        return frameToFirstSample(frame) + frameSamples;
    }


    int sampleToFrame(int64_t sample)
    {
        assertm(0 <= sample, "negative sample");

        int64_t frame = sample / VS_AUDIO_FRAME_SAMPLES;

        assertm(frame <= std::numeric_limits<int>::max(), "frame out of int range");

        return static_cast<int>(frame);
    }


    void copyFrameChannel(int channel, VSFrame* outFrame, const VSFrame* inFrame, int bytesPerSample, const VSAPI* vsapi)
    {
        int numSamples = vsapi->getFrameLength(outFrame);

        const uint8_t* inFramePtr = vsapi->getReadPtr(inFrame, channel);
        uint8_t* outFramePtr = vsapi->getWritePtr(outFrame, channel);

        std::memcpy(&outFramePtr[0], &inFramePtr[0], numSamples * bytesPerSample);
    }


    std::vector<int> getChannelsFromChannelLayout(uint64_t channelLayout)
    {
        std::vector<int> result;

        std::bitset<64> chBits(channelLayout);

        for (int i = static_cast<int>(VSAudioChannels::acFrontLeft); i <= static_cast<int>(VSAudioChannels::acLowFrequency2) && i < 64; ++i)
        {
            if (chBits[i])
            {
                result.push_back(i);
            }
        }

        return result;
    }


    uint64_t toChannelLayout(const std::vector<int>& channels)
    {
        uint64_t result = 0;
        for (const int& ch : channels)
        {
            if (0 <= ch)
            {
                result |= (1ULL << ch);
            }
        }
        return result;
    }
}
