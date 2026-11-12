/*
 * PeopleListView.cpp
 * Copyright (C) 2024 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include "PeopleListView.h"

#include "PeopleListItem.h"
#include "TitledSeparatorItem.h"

#include "jabber/Messages.h"

#include <Catalog.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>

#define B_TRANSLATION_CONTEXT "Group chat participants list"

PeopleListView::PeopleListView()
	: BListView()
{
	BMessage* invoke = new BMessage(JAB_OPEN_CHAT);
	invoke->AddString("jid", "");
	SetInvocationMessage(invoke);
}


// TODO if the menu contains nothing else than "start private chat", maybe use a BToolTip instead
// TODO users who have sent private messages should be in bold in the people list?


void
PeopleListView::MouseDown(BPoint where)
{
	PeopleListItem* person = dynamic_cast<PeopleListItem*>(ItemAt(IndexOf(where)));
	if (person == nullptr) {
		BListView::MouseDown(where);
		return;
	}

	BMessage* invoke = InvocationMessage();
	invoke->ReplaceString("jid", person->fOccupantJID.full().c_str());

	BListView::MouseDown(where);

	BMessage* msg = Window()->CurrentMessage();
	uint32 buttons = msg->FindInt32("buttons");

	if (buttons != B_SECONDARY_MOUSE_BUTTON) {
		return;
	}

	BPopUpMenu* menu = new BPopUpMenu("MUC user actions", false, false);
	bool empty = true;

	// TODO maybe something a bit more fancy, we could display the user avatar in this menu as well
	std::string realjid = person->fRealJID.bare();
	if (!realjid.empty()) {
		BMenuItem* item = new TitledSeparatorItem(realjid.c_str());
		menu->AddItem(item);
		empty = false;
	}

	if (!person->fStatus.IsEmpty()) {
		BMenuItem* item = new BMenuItem(person->fStatus, nullptr);
		item->SetEnabled(false);
		menu->AddItem(item);
		empty = false;
	}

	if (!empty) {
		menu->AddSeparatorItem();
	}

	BMessage* message = new BMessage(JAB_OPEN_CHAT);
	message->AddString("jid", person->fOccupantJID.full().c_str());
	BMenuItem* item = new BMenuItem(B_TRANSLATE("Start private chat"), message);
	menu->AddItem(item);
	item->SetTarget(this);

	// TODO add actions: add to contacts (if real JID is known), moderator actions (if available)
	menu->Go(ConvertToScreen(where), true);
	delete menu;
}
