//////////////////////////////////////////////////
// Blabber [TalkListItem.h]
//////////////////////////////////////////////////

#ifndef TALK_LIST_ITEM
	#include "TalkListItem.h"
#endif

#ifndef __CSTRING__
	#include <cstring>
#endif

TalkListItem::TalkListItem(const char *user, const char *text, rgb_color name_color)
	: BStringItem(text) {
	_name_color = name_color;
	_user       = strdup(user);
}

TalkListItem::~TalkListItem() {
	delete _user;
}

void TalkListItem::DrawItem(BView *owner, BRect frame, __attribute__((unused)) bool complete) {
	// text characteristics
	owner->SetFont(be_plain_font);
	owner->SetFontSize(10.0);

	// never seems to be selectable (love to add cut + paste later!)
	owner->SetHighColor(owner->ViewColor());
	owner->FillRect(frame);

	float height;

	// construct text positioning
	font_height fh;
	owner->GetFontHeight(&fh);

	height = fh.ascent + fh.descent;

	// font color for user
	owner->SetHighColor(_name_color);

	// draw name
	owner->DrawString("<", BPoint(5.0, frame.bottom - ((frame.Height() - height) / 2) - fh.descent));
	owner->DrawString(_user);
	owner->DrawString("> ");

	// font color for message
	owner->SetHighColor(0, 0, 0, 255);

	// draw message
	owner->DrawString(Text());
}
