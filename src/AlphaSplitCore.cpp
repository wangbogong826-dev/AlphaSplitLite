#include "AlphaSplitCore.h"
#include <climits>
#include <cstddef>
#include <cstdint>

namespace AlphaSplitCore {

bool BuildFillKey(const uint8_t* source,
                  int sourceWidth,
                  int sourceHeight,
                  int sourceStrideBytes,
                  PixelFormat format,
                  uint8_t* destination,
                  int outputWidth,
                  int outputHeight) noexcept {
    if (!source || !destination || sourceWidth <= 0 || sourceHeight <= 0 ||
        outputWidth <= 0 || outputHeight <= 0 || (outputWidth % 2) != 0) {
        return false;
    }

    const int64_t minStride = static_cast<int64_t>(sourceWidth) * 4;
    if (minStride > INT_MAX || sourceStrideBytes < minStride) return false;

    // Reject unknown enum values instead of silently interpreting them as RGBX.
    switch (format) {
    case PixelFormat::BGRA:
    case PixelFormat::BGRX:
    case PixelFormat::RGBA:
    case PixelFormat::RGBX:
        break;
    default:
        return false;
    }

    const bool isBgr = format == PixelFormat::BGRA || format == PixelFormat::BGRX;
    const bool hasAlpha = format == PixelFormat::BGRA || format == PixelFormat::RGBA;
    const int halfWidth = outputWidth / 2;

    for (int y = 0; y < outputHeight; ++y) {
        const int sy = static_cast<int>((static_cast<int64_t>(y) * sourceHeight) / outputHeight);
        const uint8_t* sourceRow = source + static_cast<size_t>(sy) * static_cast<size_t>(sourceStrideBytes);
        uint8_t* destinationRow = destination + static_cast<size_t>(y) * static_cast<size_t>(outputWidth) * 4;
        for (int x = 0; x < halfWidth; ++x) {
            const int sx = static_cast<int>((static_cast<int64_t>(x) * sourceWidth) / halfWidth);
            const uint8_t* pixel = sourceRow + static_cast<size_t>(sx) * 4;
            const uint8_t b = isBgr ? pixel[0] : pixel[2];
            const uint8_t g = pixel[1];
            const uint8_t r = isBgr ? pixel[2] : pixel[0];
            const uint8_t a = hasAlpha ? pixel[3] : 255;

            uint8_t* fill = destinationRow + static_cast<size_t>(x) * 4;
            fill[0] = b; fill[1] = g; fill[2] = r; fill[3] = 255;

            uint8_t* key = destinationRow + static_cast<size_t>(x + halfWidth) * 4;
            key[0] = a; key[1] = a; key[2] = a; key[3] = 255;
        }
    }
    return true;
}

} // namespace AlphaSplitCore
