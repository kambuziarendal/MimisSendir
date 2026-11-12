// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "ImageSafety.h"
#include <algorithm>
#include <cstring>

namespace Upload {
namespace {
uint32_t Big32(const unsigned char* p)
{
	return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
uint32_t Big16(const unsigned char* p) { return (uint32_t(p[0]) << 8) | p[1]; }
std::optional<ImageDimensions> Bounded(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0 || width > 16384 || height > 16384
		|| uint64_t(width) * height > kPreviewPixelLimit) return {};
	return ImageDimensions{width, height};
}
}

std::optional<ImageDimensions> InspectPreviewImage(const unsigned char* data, size_t size)
{
	if (!data) return {};
	size = std::min(size, kPreviewHeaderLimit);
	static constexpr unsigned char png[] = {137, 80, 78, 71, 13, 10, 26, 10};
	if (size >= 33 && std::memcmp(data, png, sizeof(png)) == 0) {
		if (Big32(data + 8) != 13 || std::memcmp(data + 12, "IHDR", 4) != 0) return {};
		return Bounded(Big32(data + 16), Big32(data + 20));
	}
	if (size < 4 || data[0] != 0xff || data[1] != 0xd8) return {};
	size_t offset = 2;
	while (offset + 1 < size) {
		if (data[offset++] != 0xff) return {};
		while (offset < size && data[offset] == 0xff) ++offset;
		if (offset >= size) return {};
		const unsigned char marker = data[offset++];
		if (marker == 0xda || marker == 0xd9 || marker == 0x00) return {}; // scan/end before frame
		if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7)) continue;
		if (offset + 2 > size) return {};
		const uint32_t length = Big16(data + offset);
		if (length < 2 || length > size - offset) return {};
		// Baseline, extended sequential, or progressive Huffman JPEG only.
		if (marker == 0xc0 || marker == 0xc1 || marker == 0xc2) {
			if (length < 8 || data[offset + 2] != 8) return {};
			const unsigned char components = data[offset + 7];
			if (components == 0 || components > 4 || length != 8U + 3U * components) return {};
			return Bounded(Big16(data + offset + 5), Big16(data + offset + 3));
		}
		// Other frame types may use uncommon dimensions/decoding semantics.
		if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc)
			return {};
		offset += length;
	}
	return {};
}
}
