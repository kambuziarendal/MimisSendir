// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include "support/ConversationStore.h"
#include <Bitmap.h>
#include <ListItem.h>
#include <String.h>
#include <View.h>
#include <ctime>
#include "AvatarCache.h"

inline void DrawConversationRow(BView* owner, BRect frame, bool selected,
    const Conversations::Summary& item, BBitmap* = nullptr)
{
    owner->PushState();
    owner->SetHighUIColor(selected ? B_LIST_SELECTED_BACKGROUND_COLOR : B_LIST_BACKGROUND_COLOR);
    owner->FillRect(frame);
    if (selected) {
        owner->SetHighColor(240, 185, 45);
        owner->FillRect(BRect(frame.left, frame.top + 5, frame.left + 3, frame.bottom - 5));
    }
    const float side = 32;
    BRect face(frame.left + 10, frame.top + 12, frame.left + 10 + side, frame.top + 12 + side);
    owner->SetHighUIColor(B_PANEL_BACKGROUND_COLOR);
    owner->FillEllipse(face);
    owner->SetHighUIColor(selected ? B_LIST_SELECTED_ITEM_TEXT_COLOR : B_LIST_ITEM_TEXT_COLOR);
    Avatars::Draw(owner,face,Avatars::Get(Avatars::Account(),item.jid),item.name);
    owner->SetHighUIColor(selected ? B_LIST_SELECTED_ITEM_TEXT_COLOR : B_LIST_ITEM_TEXT_COLOR);
    BString clock;
    if (item.activity > 0) {
        time_t seconds = item.activity / 1000000, now = time(nullptr);
        struct tm value, today;
        localtime_r(&seconds, &value); localtime_r(&now, &today);
        char text[32];
        strftime(text, sizeof(text), value.tm_year == today.tm_year && value.tm_yday == today.tm_yday ? "%H:%M" : "%d.%m", &value);
        clock = text;
    }
    const float right = frame.right - 10;
    owner->SetFont(be_plain_font);
    owner->DrawString(clock, BPoint(right - owner->StringWidth(clock), frame.top + 22));
    BString name(item.name.c_str());
    owner->SetFont(be_bold_font);
    owner->TruncateString(&name, B_TRUNCATE_END, std::max(10.0f, right - frame.left - 66 - owner->StringWidth(clock)));
    owner->DrawString(name, BPoint(frame.left + 52, frame.top + 22));
    owner->SetFont(be_plain_font);
    BString preview(item.preview.c_str());
    float badge = item.unread ? 35 : 0;
    owner->TruncateString(&preview, B_TRUNCATE_END, std::max(10.0f, right - frame.left - 52 - badge));
    owner->DrawString(preview, BPoint(frame.left + 52, frame.top + 43));
    if (item.unread) {
        BString count; count.SetToFormat("%u", item.unread);
        BRect rect(right - 30, frame.top + 30, right, frame.top + 49);
        owner->SetHighUIColor(B_LIST_SELECTED_BACKGROUND_COLOR);
        owner->FillRoundRect(rect, 8, 8);
        owner->SetHighUIColor(B_LIST_SELECTED_ITEM_TEXT_COLOR);
        owner->DrawString(count, BPoint(rect.left + (30 - owner->StringWidth(count)) / 2, rect.top + 14));
    }
    owner->PopState();
}
class ConversationItem final : public BStringItem {
public:
    explicit ConversationItem(Conversations::Summary item) : BStringItem(item.name.c_str()), summary(std::move(item)) {}
    void Update(BView* owner, const BFont* font) override { BStringItem::Update(owner, font); SetHeight(58); }
    void DrawItem(BView* owner, BRect frame, bool = false) override { DrawConversationRow(owner, frame, IsSelected(), summary); }
    Conversations::Summary summary;
};
