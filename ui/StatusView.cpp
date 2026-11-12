//////////////////////////////////////////////////
// Interface [StatusView.cpp]
//////////////////////////////////////////////////

#ifndef STATUS_VIEW_H
	#include "StatusView.h"
#endif

#include <ScrollView.h>
#include <ControlLook.h>

#include "ui/HVIFUtil.h"

StatusView::StatusView(const char *name, BBitmap* icon)
	: BStringView(name, "")
	, fIcon(icon)
{
	SetFont(be_plain_font);

	float viewSize = be_control_look->GetScrollBarWidth();
	float fontSize = 9.0f;

	// calculate a font Size that fits into the alocated space
	while (fontSize < 48) {
		SetFontSize(fontSize + 1);
		font_height fh;
		float height;
		GetFontHeight(&fh);
		height = fh.ascent + fh.descent + 1.0f;
		if (height < viewSize) {
			fontSize = fontSize +1;
			continue;
		}
		if (height > viewSize) {
			SetFontSize(fontSize);
			break;
		}
		if (height == viewSize)
			break;
	}

	SetTruncation(B_TRUNCATE_END);
}

StatusView::~StatusView() {
}

void
StatusView::Draw(BRect updateRect)
{
	if (!fIcon) {
		BStringView::Draw(updateRect);
		return;
	}

	SetDrawingMode(B_OP_ALPHA);
	DrawBitmap(fIcon);
	
	if (!Text())
		return;

	if (LowUIColor() == B_NO_COLOR)
		SetLowColor(ViewColor());

	font_height fontHeight;
	GetFontHeight(&fontHeight);

	BRect bounds = Bounds();

	float y = bounds.top + bounds.bottom - ceilf(fontHeight.descent);
	float leftMargin = be_control_look->GetScrollBarWidth() + 3;
	BString truncated = Text();
	if (StringWidth(truncated) > bounds.Width() - leftMargin)
		TruncateString(&truncated, B_TRUNCATE_END, bounds.Width() - leftMargin);
	DrawString(truncated, BPoint(leftMargin, y));
}

void
StatusView::MouseDown(BPoint point)
{
	Invoke();
	BStringView::MouseDown(point);
}
