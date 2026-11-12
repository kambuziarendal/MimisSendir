// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Upload {
struct ImageDimensions { uint32_t width; uint32_t height; };
inline constexpr size_t kPreviewHeaderLimit = 256 * 1024;
inline constexpr uint64_t kPreviewPixelLimit = 16 * 1024 * 1024;

// Inspect at most a 256 KiB prefix before invoking a native decoder. Only PNG
// and JPEG have reviewed dimension checks. Other formats remain downloadable.
// This is resource preflight, not proof that a file is valid or safe to execute.
std::optional<ImageDimensions> InspectPreviewImage(const unsigned char* data, size_t size);
}
