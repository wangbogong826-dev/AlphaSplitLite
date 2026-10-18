#include "../src/AlphaSplitCore.h"
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    // Isolate the pixel-conversion cost using the intended 1080p NDI input and
    // 2560x720 side-by-side Fill/Key output. This does NOT benchmark NDI, GDI,
    // monitor output, or the full application.
    constexpr int sourceWidth = 1920;
    constexpr int sourceHeight = 1080;
    constexpr int sourceStride = sourceWidth * 4;
    constexpr int outputWidth = 2560;
    constexpr int outputHeight = 720;
    constexpr int warmupFrames = 10;
    constexpr int measuredFrames = 200;

    std::vector<uint8_t> source(static_cast<size_t>(sourceStride) * sourceHeight);
    std::vector<uint8_t> output(static_cast<size_t>(outputWidth) * outputHeight * 4);
    uint32_t state = 0x13579BDFu;
    for (auto& byte : source) {
        state = state * 1664525u + 1013904223u;
        byte = static_cast<uint8_t>(state >> 24);
    }

    const auto convert = [&]() {
        return AlphaSplitCore::BuildFillKey(
            source.data(), sourceWidth, sourceHeight, sourceStride,
            AlphaSplitCore::PixelFormat::BGRA,
            output.data(), outputWidth, outputHeight);
    };

    for (int i = 0; i < warmupFrames; ++i) {
        if (!convert()) {
            std::cerr << "Warmup conversion failed.\n";
            return 2;
        }
    }

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < measuredFrames; ++i) {
        if (!convert()) {
            std::cerr << "Measured conversion failed.\n";
            return 3;
        }
    }
    const auto finish = std::chrono::steady_clock::now();
    const double elapsedSeconds = std::chrono::duration<double>(finish - start).count();
    const double fps = measuredFrames / elapsedSeconds;
    const double millisecondsPerFrame = elapsedSeconds * 1000.0 / measuredFrames;

    // Read a byte from the output so optimizing compilers cannot discard work.
    const unsigned checksum = static_cast<unsigned>(output[0]) + output[output.size() / 2] + output.back();
    std::cout << "AlphaSplit Lite core benchmark (1920x1080 BGRA -> 2560x720 Fill/Key)\n";
    std::cout << "Warmup frames: " << warmupFrames << "\n";
    std::cout << "Measured frames: " << measuredFrames << "\n";
    std::cout << std::fixed << std::setprecision(2)
              << "Conversion throughput: " << fps << " fps\n"
              << "Average conversion time: " << millisecondsPerFrame << " ms/frame\n";
    std::cout << "25 fps conversion budget: 40.00 ms/frame\n";
    std::cout << "Output checksum: " << checksum << "\n\n";
    std::cout << "IMPORTANT: This measures pixel conversion only. Full-app NDI reception, buffer copies,\n"
                 "GDI drawing, and display output add overhead; verify the full application separately.\n";
    return 0;
}
