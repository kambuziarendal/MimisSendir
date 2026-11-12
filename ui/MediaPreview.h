// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Bitmap.h>
#include <cstdio>
#include <memory>
#include <vector>
struct MediaPreview {
    std::vector<std::shared_ptr<BBitmap>> frames;
    std::vector<int> delays;
};
// Call on a worker, never on a UI looper.
MediaPreview LoadMediaPreview(FILE* file, bool conversation = false);
