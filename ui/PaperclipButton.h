// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Button.h>

// Native focus, keyboard and control chrome; resolution-independent paperclip.
class PaperclipButton final : public BButton {
public:
	explicit PaperclipButton(BMessage* message)
		: BButton("Attach file", "", message) {
		SetExplicitMinSize(BSize(34, 34));
		SetExplicitMaxSize(BSize(34, 34));
	}
	void Draw(BRect update) override {
		BButton::Draw(update);
		PushState();
		SetHighColor(tint_color(ui_color(B_CONTROL_TEXT_COLOR), IsEnabled() ? B_NO_TINT : B_DISABLED_LABEL_TINT));
		SetPenSize(1.6f);
		BPoint o = BPoint(Bounds().Width() / 2, Bounds().Height() / 2) - BPoint(7, 11);
		StrokeLine(o + BPoint(0, 6), o + BPoint(0, 17));
		StrokeArc(BRect(o.x, o.y + 12, o.x + 14, o.y + 24), 180, 180);
		StrokeLine(o + BPoint(14, 18), o + BPoint(14, 4));
		StrokeArc(BRect(o.x + 4, o.y - 1, o.x + 14, o.y + 9), 0, 180);
		StrokeLine(o + BPoint(4, 4), o + BPoint(4, 17));
		StrokeArc(BRect(o.x + 4, o.y + 14, o.x + 10, o.y + 20), 180, 180);
		StrokeLine(o + BPoint(10, 17), o + BPoint(10, 6));
		PopState();
	}
};
