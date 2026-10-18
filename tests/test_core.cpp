#include "../src/AlphaSplitCore.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <climits>
#include <iostream>
#include <random>
#include <vector>

static void testBgraAlphaToFillKey() {
    // Two BGRA pixels, output canvas has two Fill pixels then two Key pixels.
    const std::array<uint8_t, 8> src = {10, 20, 30, 0, 40, 50, 60, 128};
    std::array<uint8_t, 16> dst{};
    assert(AlphaSplitCore::BuildFillKey(src.data(), 2, 1, 8,
        AlphaSplitCore::PixelFormat::BGRA, dst.data(), 4, 1));
    const std::array<uint8_t, 16> expected = {
        10,20,30,255, 40,50,60,255,
        0,0,0,255, 128,128,128,255
    };
    assert(dst == expected);
}

static void testRgbaChannelOrder() {
    const std::array<uint8_t, 4> src = {200, 100, 50, 77}; // RGBA
    std::array<uint8_t, 8> dst{};
    assert(AlphaSplitCore::BuildFillKey(src.data(), 1, 1, 4,
        AlphaSplitCore::PixelFormat::RGBA, dst.data(), 2, 1));
    const std::array<uint8_t, 8> expected = {50,100,200,255, 77,77,77,255};
    assert(dst == expected);
}

static void testNoAlphaProducesWhiteKey() {
    const std::array<uint8_t, 4> src = {1, 2, 3, 0}; // BGRX: last byte ignored
    std::array<uint8_t, 8> dst{};
    assert(AlphaSplitCore::BuildFillKey(src.data(), 1, 1, 4,
        AlphaSplitCore::PixelFormat::BGRX, dst.data(), 2, 1));
    const std::array<uint8_t, 8> expected = {1,2,3,255, 255,255,255,255};
    assert(dst == expected);
}

static void testRejectInvalidStrideAndOddOutputWidth() {
    const std::array<uint8_t, 4> src = {0,0,0,0};
    std::array<uint8_t, 8> dst{};
    assert(!AlphaSplitCore::BuildFillKey(src.data(), 1, 1, 3,
        AlphaSplitCore::PixelFormat::BGRA, dst.data(), 2, 1));
    assert(!AlphaSplitCore::BuildFillKey(src.data(), 1, 1, 4,
        AlphaSplitCore::PixelFormat::BGRA, dst.data(), 3, 1));
}

static void testPaddedStrideAndVerticalScaling() {
    // Two 1-pixel rows padded to 8 bytes; output is 2x4 so each row doubles.
    const std::array<uint8_t, 16> src = {
        1,2,3,10, 99,99,99,99,
        4,5,6,20, 88,88,88,88
    };
    std::array<uint8_t, 32> dst{};
    assert(AlphaSplitCore::BuildFillKey(src.data(), 1, 2, 8,
        AlphaSplitCore::PixelFormat::BGRA, dst.data(), 2, 4));
    const std::array<uint8_t, 32> expected = {
        1,2,3,255, 10,10,10,255,
        1,2,3,255, 10,10,10,255,
        4,5,6,255, 20,20,20,255,
        4,5,6,255, 20,20,20,255
    };
    assert(dst == expected);
}

static void testRgbxChannelOrderAndIgnoredX() {
    const std::array<uint8_t, 4> src = {9, 8, 7, 6}; // RGBX
    std::array<uint8_t, 8> dst{};
    assert(AlphaSplitCore::BuildFillKey(src.data(), 1, 1, 4,
        AlphaSplitCore::PixelFormat::RGBX, dst.data(), 2, 1));
    const std::array<uint8_t, 8> expected = {7,8,9,255, 255,255,255,255};
    assert(dst == expected);
}

static void testRejectUnknownPixelFormat() {
    const std::array<uint8_t, 4> src = {1, 2, 3, 4};
    std::array<uint8_t, 8> dst{};
    const auto unknown = static_cast<AlphaSplitCore::PixelFormat>(999);
    assert(!AlphaSplitCore::BuildFillKey(src.data(), 1, 1, 4,
        unknown, dst.data(), 2, 1));
}

static void testRejectZeroDimensions() {
    const std::array<uint8_t, 4> src = {0,0,0,0};
    std::array<uint8_t, 8> dst{};
    assert(!AlphaSplitCore::BuildFillKey(src.data(), 0, 1, 4,
        AlphaSplitCore::PixelFormat::BGRA, dst.data(), 2, 1));
    assert(!AlphaSplitCore::BuildFillKey(src.data(), 1, 1, 4,
        AlphaSplitCore::PixelFormat::BGRA, dst.data(), 2, 0));
}



static void testAlphaBoundaryValuesArePreserved() {
    // Alpha values must be copied exactly into all three Key color channels.
    const std::array<uint8_t, 24> src = {
        1,2,3,0,  4,5,6,1,  7,8,9,127,
        10,11,12,128,  13,14,15,254,  16,17,18,255
    };
    std::array<uint8_t, 48> dst{};
    assert(AlphaSplitCore::BuildFillKey(src.data(), 6, 1, 24,
        AlphaSplitCore::PixelFormat::BGRA, dst.data(), 12, 1));
    const std::array<uint8_t, 6> alphas = {0, 1, 127, 128, 254, 255};
    for (size_t i = 0; i < alphas.size(); ++i) {
        const size_t key = (i + 6) * 4;
        assert(dst[key] == alphas[i]);
        assert(dst[key + 1] == alphas[i]);
        assert(dst[key + 2] == alphas[i]);
        assert(dst[key + 3] == 255);
    }
}

static void testRejectNullPointersAndOverflowingSourceStride() {
    const std::array<uint8_t, 4> src = {1,2,3,4};
    std::array<uint8_t, 8> dst{};
    assert(!AlphaSplitCore::BuildFillKey(nullptr, 1, 1, 4,
        AlphaSplitCore::PixelFormat::BGRA, dst.data(), 2, 1));
    assert(!AlphaSplitCore::BuildFillKey(src.data(), 1, 1, 4,
        AlphaSplitCore::PixelFormat::BGRA, nullptr, 2, 1));
    // Must reject before any pixel access when width*4 cannot fit in int.
    assert(!AlphaSplitCore::BuildFillKey(src.data(), INT_MAX / 4 + 1, 1, INT_MAX,
        AlphaSplitCore::PixelFormat::BGRA, dst.data(), 2, 1));
}


static void testRealistic1080pToTargetCanvas() {
    // Exercise the exact production output size with a typical 1080p NDI frame.
    constexpr int sw = 1920, sh = 1080, ow = 2560, oh = 720;
    constexpr int stride = sw * 4;
    std::vector<uint8_t> src(static_cast<size_t>(stride) * sh);
    std::vector<uint8_t> dst(static_cast<size_t>(ow) * oh * 4, 0xCD);
    for (int y = 0; y < sh; ++y) {
        for (int x = 0; x < sw; ++x) {
            uint8_t* p = src.data() + static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4;
            p[0] = static_cast<uint8_t>(x & 0xff);
            p[1] = static_cast<uint8_t>(y & 0xff);
            p[2] = static_cast<uint8_t>((x + y) & 0xff);
            p[3] = static_cast<uint8_t>((x * 3 + y * 5) & 0xff);
        }
    }
    assert(AlphaSplitCore::BuildFillKey(src.data(), sw, sh, stride,
        AlphaSplitCore::PixelFormat::BGRA, dst.data(), ow, oh));

    const std::array<int, 4> xs = {0, 1, 639, 1279};
    const std::array<int, 4> ys = {0, 1, 359, 719};
    for (int y : ys) {
        const int sy = static_cast<int>((static_cast<int64_t>(y) * sh) / oh);
        for (int x : xs) {
            const int sx = static_cast<int>((static_cast<int64_t>(x) * sw) / (ow / 2));
            const uint8_t* p = src.data() + static_cast<size_t>(sy) * stride + static_cast<size_t>(sx) * 4;
            const uint8_t* fill = dst.data() + (static_cast<size_t>(y) * ow + x) * 4;
            const uint8_t* key = dst.data() + (static_cast<size_t>(y) * ow + x + ow / 2) * 4;
            assert(fill[0] == p[0] && fill[1] == p[1] && fill[2] == p[2] && fill[3] == 255);
            assert(key[0] == p[3] && key[1] == p[3] && key[2] == p[3] && key[3] == 255);
        }
    }
}

static void testDeterministicRandomizedFrames() {
    std::mt19937 rng(0xA17A5u);
    const std::array<AlphaSplitCore::PixelFormat, 4> formats = {
        AlphaSplitCore::PixelFormat::BGRA,
        AlphaSplitCore::PixelFormat::BGRX,
        AlphaSplitCore::PixelFormat::RGBA,
        AlphaSplitCore::PixelFormat::RGBX
    };

    for (int trial = 0; trial < 300; ++trial) {
        const int sw = 1 + static_cast<int>(rng() % 9);
        const int sh = 1 + static_cast<int>(rng() % 8);
        const int ow = 2 * (1 + static_cast<int>(rng() % 10));
        const int oh = 1 + static_cast<int>(rng() % 12);
        const int stride = sw * 4 + static_cast<int>(rng() % 9);
        const auto format = formats[rng() % formats.size()];
        const bool bgr = format == AlphaSplitCore::PixelFormat::BGRA ||
                         format == AlphaSplitCore::PixelFormat::BGRX;
        const bool alpha = format == AlphaSplitCore::PixelFormat::BGRA ||
                           format == AlphaSplitCore::PixelFormat::RGBA;

        std::vector<uint8_t> src(static_cast<size_t>(stride) * sh);
        for (auto& value : src) value = static_cast<uint8_t>(rng() & 0xff);
        std::vector<uint8_t> dst(static_cast<size_t>(ow) * oh * 4, 0xCD);
        assert(AlphaSplitCore::BuildFillKey(src.data(), sw, sh, stride, format,
                                            dst.data(), ow, oh));

        const int half = ow / 2;
        for (int y = 0; y < oh; ++y) {
            const int sy = static_cast<int>((static_cast<int64_t>(y) * sh) / oh);
            for (int x = 0; x < half; ++x) {
                const int sx = static_cast<int>((static_cast<int64_t>(x) * sw) / half);
                const uint8_t* pixel = src.data() + static_cast<size_t>(sy) * stride + sx * 4;
                const uint8_t b = bgr ? pixel[0] : pixel[2];
                const uint8_t g = pixel[1];
                const uint8_t r = bgr ? pixel[2] : pixel[0];
                const uint8_t a = alpha ? pixel[3] : 255;
                const size_t row = static_cast<size_t>(y) * ow * 4;
                const size_t fill = row + static_cast<size_t>(x) * 4;
                const size_t key = row + static_cast<size_t>(x + half) * 4;
                assert(dst[fill] == b && dst[fill + 1] == g && dst[fill + 2] == r && dst[fill + 3] == 255);
                assert(dst[key] == a && dst[key + 1] == a && dst[key + 2] == a && dst[key + 3] == 255);
            }
        }
    }
}

int main() {
    testBgraAlphaToFillKey();
    testRgbaChannelOrder();
    testNoAlphaProducesWhiteKey();
    testRejectInvalidStrideAndOddOutputWidth();
    testPaddedStrideAndVerticalScaling();
    testRgbxChannelOrderAndIgnoredX();
    testRejectZeroDimensions();
    testRejectUnknownPixelFormat();
    testAlphaBoundaryValuesArePreserved();
    testRejectNullPointersAndOverflowingSourceStride();
    testRealistic1080pToTargetCanvas();
    testDeterministicRandomizedFrames();
    std::cout << "AlphaSplitCore tests passed: 12/12 (including 300 randomized frames and 1080p-to-target test)\n";
    return 0;
}
