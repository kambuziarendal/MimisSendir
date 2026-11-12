// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

namespace Media {
constexpr size_t kFileLimit = 12 * 1024 * 1024;
struct Frame { std::vector<uint8_t> rgba; int delayMs = 100; };
struct Animation {
    uint32_t width = 0, height = 0;
    std::vector<Frame> frames;
};
// Workers only. Decode bounded GIF/WebP into small, composited RGBA frames.
// Invalid/oversized animations return an empty result; original files remain savable.
Animation DecodeAnimation(const uint8_t* data, size_t size, uint32_t maxWidth = 240, uint32_t maxHeight = 180);
Animation DecodeAnimation(FILE* file, uint32_t maxWidth = 240, uint32_t maxHeight = 180);
}
