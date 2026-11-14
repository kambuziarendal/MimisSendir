// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Button.h>

// Vector artwork follows the native palette and needs no font/emoji coverage.
class JumpToLatestButton final : public BButton {
public:
    JumpToLatestButton() : BButton("jump-to-latest", "Latest messages", new BMessage('mEnd')) {
        SetExplicitMinSize(BSize(36, 36));
        SetExplicitMaxSize(BSize(36, 36));
        SetToolTip("Jump to latest message");
    }
    void Draw(BRect) override {
        PushState();
        SetHighUIColor(B_DOCUMENT_BACKGROUND_COLOR); FillRect(Bounds());
        BRect circle = Bounds(); circle.InsetBy(3, 3);
        SetHighUIColor(Value() == B_CONTROL_ON ? B_CONTROL_HIGHLIGHT_COLOR : B_CONTROL_BACKGROUND_COLOR);
        FillEllipse(circle);
        SetHighUIColor(IsFocus() ? B_KEYBOARD_NAVIGATION_COLOR : B_CONTROL_TEXT_COLOR);
        SetPenSize(2);
        const BPoint center((circle.left + circle.right) / 2, (circle.top + circle.bottom) / 2);
        StrokeLine(center + BPoint(0, -7), center + BPoint(0, 4));
        StrokeLine(center + BPoint(-5, 0), center + BPoint(0, 5));
        StrokeLine(center + BPoint(0, 5), center + BPoint(5, 0));
        StrokeLine(center + BPoint(-6, 8), center + BPoint(6, 8));
        if (IsFocus()) StrokeEllipse(circle);
        PopState();
    }
};
