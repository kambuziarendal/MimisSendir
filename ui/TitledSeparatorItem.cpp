/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2000, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/


#include "TitledSeparatorItem.h"


static const float kMinSeparatorStubX = 10;
static const float kStubToStringSlotX = 5;


TitledSeparatorItem::TitledSeparatorItem(const char* label)
	:
	BMenuItem(label, 0)
{
	_inherited::SetEnabled(false);
}


TitledSeparatorItem::~TitledSeparatorItem()
{
}


void
TitledSeparatorItem::SetEnabled(bool)
{
	// leave disabled
}


void
TitledSeparatorItem::GetContentSize(float* width, float* height)
{
	_inherited::GetContentSize(width, height);

	if (*width)
		*width += 30;
}


inline rgb_color
ShiftMenuBackgroundColor(float by)
{
	return tint_color(ui_color(B_MENU_BACKGROUND_COLOR), by);
}


void
TitledSeparatorItem::Draw()
{
	BRect frame(Frame());

	BMenu* parent = Menu();

	menu_info minfo;
	get_menu_info(&minfo);

	if (minfo.separator > 0) {
		frame.left += 10;
		frame.right -= 10;
	} else {
		frame.left += 1;
		frame.right -= 1;
	}

	float startX = frame.left;
	float endX = frame.right;

	float maxStringWidth = endX - startX - (2 * kMinSeparatorStubX
		+ 2 * kStubToStringSlotX);

	// ToDo:
	// handle case where maxStringWidth turns out negative here

	BString truncatedLabel(Label());
	parent->TruncateString(&truncatedLabel, B_TRUNCATE_END, maxStringWidth);

	maxStringWidth = parent->StringWidth(truncatedLabel.String());

	// first calculate the length of the stub part of the
	// divider line, so we can use it for secondStartX
	float firstEndX = ((endX - startX) - maxStringWidth) / 2
		- kStubToStringSlotX;
	if (firstEndX < 0)
		firstEndX = 0;

	float secondStartX = endX - firstEndX;

	// now finish calculating firstEndX
	firstEndX += startX;

	parent->PushState();

	int32 y = (int32) (frame.top + (frame.bottom - frame.top) / 2);

	parent->BeginLineArray(minfo.separator == 2 ? 6 : 4);
	parent->AddLine(BPoint(frame.left, y), BPoint(firstEndX, y),
		ShiftMenuBackgroundColor(B_DARKEN_1_TINT));
	parent->AddLine(BPoint(secondStartX, y), BPoint(frame.right, y),
		ShiftMenuBackgroundColor(B_DARKEN_1_TINT));

	if (minfo.separator == 2) {
		y++;
		frame.left++;
		frame.right--;
		parent->AddLine(BPoint(frame.left,y), BPoint(firstEndX, y),
			ShiftMenuBackgroundColor(B_DARKEN_1_TINT));
		parent->AddLine(BPoint(secondStartX,y), BPoint(frame.right, y),
			ShiftMenuBackgroundColor(B_DARKEN_1_TINT));
	}
	y++;
	if (minfo.separator == 2) {
		frame.left++;
		frame.right--;
	}
	parent->AddLine(BPoint(frame.left, y), BPoint(firstEndX, y),
		ShiftMenuBackgroundColor(B_DARKEN_1_TINT));
	parent->AddLine(BPoint(secondStartX, y), BPoint(frame.right, y),
		ShiftMenuBackgroundColor(B_DARKEN_1_TINT));

	parent->EndLineArray();

	font_height finfo;
	parent->GetFontHeight(&finfo);

	parent->SetLowColor(parent->ViewColor());
	BPoint loc(firstEndX + kStubToStringSlotX,
		ContentLocation().y + finfo.ascent);

	parent->MovePenTo(loc + BPoint(1, 1));
	parent->SetHighColor(ShiftMenuBackgroundColor(B_DARKEN_1_TINT));
	parent->DrawString(truncatedLabel.String());

	parent->MovePenTo(loc);
	parent->SetHighColor(ShiftMenuBackgroundColor(B_DISABLED_LABEL_TINT));
	parent->DrawString(truncatedLabel.String());

	parent->PopState();
}

