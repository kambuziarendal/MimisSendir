//////////////////////////////////////////////////
// Blabber [OnlineStatusItem.cpp]
//////////////////////////////////////////////////

#ifndef ONLINE_STATUS_ITEM_H
	#include "OnlineStatusItem.h"
#endif

#ifndef __CSTRING__
	#include <cstring>
#endif

#ifndef _TRANSLATION_UTILS_H
	#include "TranslationUtils.h"
#endif

OnlineStatusItem::OnlineStatusItem(const char *resource, const char *label, BMessage *msg)
	: BMenuItem(label, msg) {
	_label = strdup(label);

	// load graphic
	_resource = BTranslationUtils::GetBitmap(resource);
}

OnlineStatusItem::~OnlineStatusItem() {
	delete _label;
	delete _resource;
}

void OnlineStatusItem::DrawContent() {
	// get frame rectangle
	float width, height;
	GetContentSize(&width, &height);

	// get frame position (top left corner)
	BPoint pos = ContentLocation();
	
	// construct rectangle of area
	BRect frame(pos.x, pos.y, pos.x + width, pos.y + height);

/*	// clear rectangle
	if (IsSelected()) {
		Menu()->SetHighColor(152, 152, 152, 255);
	} else {
		Menu()->SetHighColor(Menu()->ViewColor());
	}

	Menu()->FillRect(frame);*/
	
	// draw a graphic
	Menu()->SetDrawingMode(B_OP_ALPHA);
	Menu()->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	Menu()->DrawBitmapAsync(_resource, BPoint(frame.left + (20 - _resource->Bounds().Width()), frame.top + 1));

	Menu()->SetHighColor(0, 0, 0, 255);

	// construct text positioning
	font_height fh;
	Menu()->GetFontHeight(&fh);
	
	height = fh.ascent + fh.descent;

	// draw name
	Menu()->DrawString(_label, BPoint(frame.left + 24, frame.bottom - ((frame.Height() - height) / 2) - fh.descent));

	// reset
	Menu()->SetDrawingMode(B_OP_COPY);
}

void OnlineStatusItem::GetContentSize(float *width, float *height) {
	// get default values
	BMenuItem::GetContentSize(width, height);
	
	// extend width to accomodate icon
	*width += 22.0;

	// standard height
	*height = 16.0;
}
