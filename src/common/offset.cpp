// SPDX-License-Identifier: MIT

#include <cstdint>

#include "VapourSynth4.h"

#include "common/offset.hpp"
#include "vsutils/audio.hpp"

namespace common
{
    // add this offset to a frame sample position to get the sample position in the corresponding left offset frame
    // positive or zero
    static int getFrameLSampleOffsetFromFrameRSampleOffset(int frameRSampleOffset)
    {
        // assert: frameRSampleOffset <= 0
        return frameRSampleOffset == 0 ? 0 : frameRSampleOffset + VS_AUDIO_FRAME_SAMPLES;
    }


    // add this offset to a frame sample position to get the sample position in the corresponding right offset frame
    // negative or zero
    static int getFrameRSampleOffset(int64_t basePosOffsetStart)
    {
        return - (static_cast<int>(basePosOffsetStart % VS_AUDIO_FRAME_SAMPLES) + (basePosOffsetStart < 0 ? VS_AUDIO_FRAME_SAMPLES : 0));
    }


    FrameSampleOffsets getFrameSampleOffsets(int64_t basePosOffsetStart)
    {
        int frameRSampleOffset = getFrameRSampleOffset(basePosOffsetStart);

        return { .left = getFrameLSampleOffsetFromFrameRSampleOffset(frameRSampleOffset),
                 .right = frameRSampleOffset };
    }


    OffsetFramePos baseFrameToOffsetFrames(int baseFrame, int64_t basePosOffsetStart, int64_t offsetTotalSamples, int64_t baseTotalSamples)
    {
        int64_t basePosOffsetEnd = basePosOffsetStart + offsetTotalSamples;

        int64_t basePosBaseFrameStart = vsutils::frameToFirstSample(baseFrame);
        int64_t basePosBaseFrameEnd = vsutils::frameToLastSample(baseFrame, baseTotalSamples);

        if (basePosBaseFrameEnd < 0)
        {
            // baseFrame is outside of base clip
            return { .left = -1, .right = -1 };
        }

        if (basePosOffsetEnd <= basePosBaseFrameStart || basePosBaseFrameEnd <= basePosOffsetStart)
        {
            // base frame outside of offset clip
            return { .left = -1, .right = -1 };
        }

        // assert: basePosBaseFrameStart < basePosOffsetEnd && basePosOffsetStart < basePosBaseFrameEnd

        if (basePosBaseFrameStart < basePosOffsetStart)
        {
            // first offset frame (0) is right frame
            return { .left = -1, .right = 0 };
        }

        // assert: basePosOffsetStart <= basePosBaseFrameStart

        int leftOffsetFrame = vsutils::sampleToFrame(basePosBaseFrameStart - basePosOffsetStart);

        int sampleOffsetR = common::getFrameRSampleOffset(basePosOffsetStart);

        if (sampleOffsetR == 0)
        {
            // frames are aligned
            // return only left frame
            return { .left = leftOffsetFrame, .right = -1 };
        }

        if (vsutils::isLastFrame(leftOffsetFrame, offsetTotalSamples))
        {
            // last offset frame is left frame
            return { .left = leftOffsetFrame, .right = -1 };
        }

        return { .left = leftOffsetFrame, .right = leftOffsetFrame + 1 };
    }


    OffsetFramePos baseFrameToOffsetFramesTrim(int baseFrame, int64_t basePosOffsetStart, int64_t offsetTotalSamples,
                                               int64_t basePosOffsetTrimStart, int64_t basePosOffsetTrimEnd,
                                               int64_t baseTotalSamples)
    {
        OffsetFramePos offsetFrame = common::baseFrameToOffsetFrames(baseFrame, basePosOffsetStart, offsetTotalSamples, baseTotalSamples);

        if (0 <= offsetFrame.left)
        {
            int64_t basePosOffsetFrameLStart = basePosOffsetStart + vsutils::frameToFirstSample(offsetFrame.left);
            int64_t basePosOffsetFrameLEnd = basePosOffsetStart + vsutils::frameToLastSample(offsetFrame.left, offsetTotalSamples);

            if (basePosOffsetTrimEnd <= basePosOffsetFrameLStart || basePosOffsetFrameLEnd <= basePosOffsetTrimStart)
            {
                offsetFrame.left = -1;
            }
        }

        if (0 <= offsetFrame.right)
        {
            int64_t basePosOffsetFrameRStart = basePosOffsetStart + vsutils::frameToFirstSample(offsetFrame.right);
            int64_t basePosOffsetFrameREnd = basePosOffsetStart + vsutils::frameToLastSample(offsetFrame.right, offsetTotalSamples);

            if (basePosOffsetTrimEnd <= basePosOffsetFrameRStart || basePosOffsetFrameREnd <= basePosOffsetTrimStart)
            {
                offsetFrame.right = -1;
            }
        }

        return offsetFrame;
    }
}
