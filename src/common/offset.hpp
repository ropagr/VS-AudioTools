// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace common
{
    // left frame sample offset: add offset to a (local) frame sample position to get the (local) sample position of the corresponding offset left frame (positive or zero)
    // right frame sample offset: add offset to a (local) frame sample position to get the (local) sample position of the corresponding offset right frame (negative or zero)
    struct FrameSampleOffsets
    {
        int left;
        int right;
    };

    // returns the left and right frame sample offset
    FrameSampleOffsets getFrameSampleOffsets(int64_t basePosOffsetStart);

    // TODO: rename to OffsetFrameNums
    struct OffsetFramePos
    {
        int left;
        int right;
    };

    OffsetFramePos baseFrameToOffsetFrames(int baseFrame, int64_t basePosOffsetStart, int64_t offsetTotalSamples, int64_t baseTotalSamples);

    OffsetFramePos baseFrameToOffsetFramesTrim(int baseFrame, int64_t basePosOffsetStart, int64_t offsetTotalSamples,
                                               int64_t basePosOffsetTrimStart, int64_t basePosOffsetTrimEnd,
                                               int64_t baseTotalSamples);


    /**
     * returns the corresponding sample of the left or right frame for a given sample position of a base frame
     * No bit shift operation will be applied
     */
    template <typename sample_t>
    sample_t getOffsetSample(int baseFrameSamplePos, const FrameSampleOffsets& offsets,
                             const sample_t* offsetFrameLPtr, const sample_t* offsetFrameRPtr)
    {
        if (offsets.left == 0)
        {
            // assert: offsets.right == 0
            // frames are aligned
            return offsetFrameLPtr[baseFrameSamplePos + offsets.left];
        }

        // assert: offsets.right < 0

        if (-offsets.right <= baseFrameSamplePos)
        {
            // right frame
            return offsetFrameRPtr[baseFrameSamplePos + offsets.right];
        }

        // left frame
        return offsetFrameLPtr[baseFrameSamplePos + offsets.left];
    }
}
