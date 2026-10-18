#pragma once
#include <cstdint>

namespace AlphaSplitCore {

enum class PixelFormat {
    BGRA,
    BGRX,
    RGBA,
    RGBX
};

// Converts a packed 32-bit source frame into a side-by-side BGRA canvas:
// left half = opaque Fill; right half = opaque grayscale Key derived from Alpha.
// Returns false when dimensions, pointers, or stride are invalid.
bool BuildFillKey(const uint8_t* source,
                  int sourceWidth,
                  int sourceHeight,
                  int sourceStrideBytes,
                  PixelFormat format,
                  uint8_t* destination,
                  int outputWidth,
                  int outputHeight) noexcept;

} // namespace AlphaSplitCore
