// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once

#include <Bitmap.h>
#include <View.h>
#include <memory>

// Original vector masters live in resources/icons/actions. Exact-size PNG
// resources avoid requiring an SVG translator on the user's Haiku system.
std::shared_ptr<BBitmap> LoadActionIcon(const char* name, int size);
std::shared_ptr<BBitmap> DisabledActionIcon(const BBitmap* source);
void DrawActionIcon(BView* view, const BBitmap* icon, BRect frame);
