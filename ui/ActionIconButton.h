// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once

#include "ActionIcon.h"
#include <Button.h>
#include <cmath>

// Keep BButton's messages, enabled state, labels and native keyboard handling.
// Only its drawing changes. Decode once, never while repainting the control.
class ActionIconButton : public BButton {
public:
    ActionIconButton(const char* name, const char* label, BMessage* action,
        const char* icon, int iconSize = 24, float buttonSize = 36, bool bezel = false)
        : BButton(name, label, action), fIconSize(iconSize), fBezel(bezel) {
        SetToolTip(label);
        SetExplicitMinSize(BSize(buttonSize, buttonSize));
        SetExplicitMaxSize(BSize(buttonSize, buttonSize));
        SetActionIcon(icon);
    }

    void SetSelected(bool selected) { fSelected = selected; Invalidate(); }
    void SetActionIcon(const char* name) {
        fIcon = LoadActionIcon(name, fIconSize);
        fDisabledIcon = DisabledActionIcon(fIcon.get());
        Invalidate();
    }

    void Draw(BRect) override {
        PushState();
        const auto panel = ui_color(B_PANEL_BACKGROUND_COLOR);
        SetHighColor(panel); FillRect(Bounds());
        BRect shape = Bounds(); shape.InsetBy(2, 2);
        const bool pressed = Value() == B_CONTROL_ON;
        if (fBezel || pressed || fSelected) {
            auto fill = tint_color(panel, B_LIGHTEN_1_TINT);
            if (fSelected) {
                const auto accent = ui_color(B_KEYBOARD_NAVIGATION_COLOR);
                fill = {uint8((panel.red * 3 + accent.red) / 4),
                    uint8((panel.green * 3 + accent.green) / 4),
                    uint8((panel.blue * 3 + accent.blue) / 4), 255};
            }
            if (pressed) fill = tint_color(fill, B_DARKEN_1_TINT);
            SetHighColor(fill); FillRoundRect(shape, 3, 3);
            SetHighUIColor(fSelected ? B_KEYBOARD_NAVIGATION_COLOR : B_CONTROL_BORDER_COLOR);
            StrokeRoundRect(shape, 3, 3);
            SetHighColor(tint_color(fill, pressed || fSelected ? B_DARKEN_1_TINT : B_LIGHTEN_2_TINT));
            StrokeLine(BPoint(shape.left + 2, shape.bottom - 2), BPoint(shape.left + 2, shape.top + 2));
            StrokeLine(BPoint(shape.left + 2, shape.top + 2), BPoint(shape.right - 2, shape.top + 2));
        }
        const float x = std::floor((Bounds().Width() + 1 - fIconSize) / 2);
        const float y = std::floor((Bounds().Height() + 1 - fIconSize) / 2);
        const auto* bitmap = IsEnabled() ? fIcon.get() : fDisabledIcon.get();
        if (bitmap) DrawActionIcon(this, bitmap, BRect(x, y, x + fIconSize - 1, y + fIconSize - 1));
        else {
            // A damaged/missing resource must not silently remove the action.
            SetHighUIColor(B_PANEL_TEXT_COLOR);
            DrawString(Label(), BPoint(3, Bounds().Height() / 2));
        }
        if (IsFocus()) {
            SetHighUIColor(B_KEYBOARD_NAVIGATION_COLOR); SetPenSize(2);
            StrokeLine(BPoint(7, Bounds().bottom - 2), BPoint(Bounds().right - 7, Bounds().bottom - 2));
        }
        PopState();
    }

private:
    int fIconSize;
    bool fBezel;
    bool fSelected = false;
    std::shared_ptr<BBitmap> fIcon;
    std::shared_ptr<BBitmap> fDisabledIcon;
};
