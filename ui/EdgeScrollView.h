// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once

#include <LayoutUtils.h>
#include <OutlineListView.h>
#include <ScrollBar.h>
#include <ScrollView.h>

// Retain Haiku's scrollbar model for wheel, keyboard, selection and range
// clamping, but give the entire visible width to the list. No scrollbar chrome.
class EdgeScrollView final : public BScrollView {
public:
    explicit EdgeScrollView(BView* target)
        : BScrollView("roster-scroll", target, 0, false, true, B_NO_BORDER) {
        ScrollBar(B_VERTICAL)->Hide();
    }
    BSize MinSize() override {
        return BLayoutUtils::ComposeSize(ExplicitMinSize(), Target()->MinSize());
    }
    BSize PreferredSize() override {
        return BLayoutUtils::ComposeSize(ExplicitPreferredSize(), Target()->PreferredSize());
    }
    void DoLayout() override {
        Target()->MoveTo(B_ORIGIN);
        Target()->ResizeTo(Bounds().Width(), Bounds().Height());
    }
};

class EdgeScrollListView : public BOutlineListView {
public:
    explicit EdgeScrollListView(const char* name = "roster-list")
        : BOutlineListView(name, B_SINGLE_SELECTION_LIST) {}

    bool MoreAbove() const { return Bounds().top > 0.5f; }
    bool MoreBelow() const {
        auto* bar = ScrollBar(B_VERTICAL);
        if (!bar) return false;
        float minimum, maximum;
        bar->GetRange(&minimum, &maximum);
        return Bounds().top < maximum - 0.5f;
    }
    void ScrollTo(BPoint position) override {
        BOutlineListView::ScrollTo(position);
        // Native scrolling copies existing pixels. Repaint to prevent an old
        // edge hint being copied into the middle of the list.
        Invalidate();
    }
    void FrameResized(float width, float height) override {
        BOutlineListView::FrameResized(width, height);
        Invalidate();
    }
    void Draw(BRect update) override {
        BOutlineListView::Draw(update);
        if (MoreAbove()) DrawHint(true);
        if (MoreBelow()) DrawHint(false);
    }

private:
    void DrawHint(bool above) {
        PushState();
        const BRect bounds = Bounds();
        const float x = (bounds.left + bounds.right) / 2;
        const float y = above ? bounds.top + 7 : bounds.bottom - 7;
        SetHighUIColor(B_LIST_BACKGROUND_COLOR);
        FillRoundRect(BRect(x - 15, y - 6, x + 15, y + 6), 5, 5);
        SetHighUIColor(B_LIST_ITEM_TEXT_COLOR);
        SetPenSize(1.6f);
        const float direction = above ? -1 : 1;
        StrokeLine(BPoint(x - 4, y - direction), BPoint(x, y + 2 * direction));
        StrokeLine(BPoint(x, y + 2 * direction), BPoint(x + 4, y - direction));
        PopState();
    }
};
