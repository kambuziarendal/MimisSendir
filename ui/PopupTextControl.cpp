/*
 * PopupTextControl.cpp
 * Copyright (C) 2023 pulkomandy <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include "PopupTextControl.h"



void PopupTextControl::MouseDown(BPoint where)
{
	BRect frame = Bounds();
	frame.left = frame.right - BControlLook::ComposeSpacing(B_USE_WINDOW_SPACING);
	if (where.x > frame.left) {
		BMenuItem* item = fMenu->Go(TextView()->ConvertToScreen(TextView()->Bounds().LeftBottom()));
		if (item) {
			TextView()->SetText(item->Label());
			Invoke();
		}
	} else {
		BTextControl::MouseDown(where);
	}
}
