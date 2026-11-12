// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Bitmap.h>
#include <memory>
#include <cstddef>
namespace Avatars {
// Worker-only decoding, bounded input/dimensions, circular 96 px thumbnail.
std::shared_ptr<BBitmap> DecodeAvatar(const void* bytes,size_t size);
}
