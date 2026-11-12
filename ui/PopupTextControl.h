/** Text control with a pop-up arrow on the right and a popup menu allowing to select memorized
 *  values for filling up the text box.
 *
 *  Typical use is for a login box with memorized accounts.
 *
 *  Copyright 20??-2025, Adrien Destugues <pulkomandy@pulkomandy.tk>.
 *  Distributed under the terms of the MIT License.
 */
#pragma once

#include <ControlLook.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <TextControl.h>


class PopupTextControl: public BTextControl
{
	public:
	PopupTextControl(const char* name, const char* label, const char* value)
		: BTextControl(name, label, value, nullptr,
			B_WILL_DRAW | B_NAVIGABLE | B_DRAW_ON_CHILDREN)
		, fMenu(new BPopUpMenu("history", false))
	{
	}

	void AddEntry(const char* str)
	{
		fMenu->AddItem(new BMenuItem(str, NULL));
	}

	void RemoveEntry(const char* str)
	{
		BMenuItem* item = fMenu->FindItem(str);
		if (item)
			fMenu->RemoveItem(item);

		if (strcmp(Text(), str) == 0) {
			SetText("");
		}
	}

	private:

	void FrameMoved(BPoint) override
	{
		TextView()->ResizeBy(-BControlLook::ComposeSpacing(B_USE_WINDOW_SPACING), 0);
	}

	void DrawAfterChildren (BRect update) override
	{
		BRect frame = Bounds();
		frame.left = frame.right - BControlLook::ComposeSpacing(B_USE_WINDOW_SPACING);
		frame.top++;
		frame.bottom--;
		be_control_look->DrawMenuFieldBackground(this, frame, update,
			ui_color(B_PANEL_BACKGROUND_COLOR), true);
	}

	void MouseDown(BPoint where) override;

	private:

	BPopUpMenu* fMenu;
};
