// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "ActionIcon.h"

#include <DataIO.h>
#include <Resources.h>
#include <TranslationUtils.h>
#include <cstdio>

std::shared_ptr<BBitmap> LoadActionIcon(const char* name, int size)
{
    static const int resourceAnchor = 0;
    BResources resources;
    if (resources.SetToImage(&resourceAnchor) != B_OK) return {};
    char resourceName[96];
    std::snprintf(resourceName, sizeof(resourceName), "%s-%d", name, size);
    size_t length = 0;
    const void* data = resources.LoadResource('RICO', resourceName, &length);
    if (!data || !length) return {};
    BMemoryIO stream(data, length);
    auto bitmap = std::shared_ptr<BBitmap>(BTranslationUtils::GetBitmap(&stream));
    if (!bitmap || bitmap->InitCheck() != B_OK) return {};
    return bitmap;
}

std::shared_ptr<BBitmap> DisabledActionIcon(const BBitmap* source)
{
    if (!source) return {};
    auto bitmap = std::make_shared<BBitmap>(source->Bounds(), B_RGBA32);
    if (bitmap->InitCheck() != B_OK
        || bitmap->ImportBits(source) != B_OK) return {};
    for (int y = 0; y <= bitmap->Bounds().IntegerHeight(); ++y) {
        auto* row = static_cast<uint8*>(bitmap->Bits()) + y * bitmap->BytesPerRow();
        for (int x = 0; x <= bitmap->Bounds().IntegerWidth(); ++x) {
            auto* pixel = row + 4 * x;
            const uint8 gray = (pixel[0] * 11 + pixel[1] * 59 + pixel[2] * 30) / 100;
            pixel[0] = pixel[1] = pixel[2] = gray;
            pixel[3] = pixel[3] * 2 / 5;
        }
    }
    return bitmap;
}

void DrawActionIcon(BView* view, const BBitmap* icon, BRect frame)
{
    if (!icon) return;
    view->PushState();
    view->SetDrawingMode(B_OP_ALPHA);
    view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
    view->DrawBitmap(icon, icon->Bounds(), frame, B_FILTER_BITMAP_BILINEAR);
    view->PopState();
}
