/*
 * Copyright 2001-2022, Distributed under terms of the MIT license.
 *
 * Authors:
 *	John Blanco
 *	Adrien Destugues <pulkomandy@pulkomandy.tk>
 *	Pascal Abresch
 *	Humdinger <humdingerb@gmail.com>
 */

#include "CustomStatusWindow.h"

#include <cstdio>

#include <Application.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <StringView.h>

#include "support/AppLocation.h"

#include "ui/PictureView.h"
#include "ui/MainWindow.h"

#include "BlabberSettings.h"
#include "JabberSpeak.h"
#include "Messages.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "CustomStatusWindow"


CustomStatusWindow *CustomStatusWindow::_instance = NULL;

CustomStatusWindow *CustomStatusWindow::Instance(BRect frame) {
	if (_instance == NULL)
		_instance = new CustomStatusWindow(frame);

	return _instance;
}


CustomStatusWindow::CustomStatusWindow(BRect frame)
	: BWindow(BRect(), B_TRANSLATE("Create a custom status"),
		B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS |
		B_NOT_ZOOMABLE |
		B_NOT_RESIZABLE)
	{

	_chat = new BRadioButton("status", B_TRANSLATE("Chat"), NULL);
	_away = new BRadioButton("status", B_TRANSLATE("Away"), NULL);
	_xa = new BRadioButton("status", B_TRANSLATE("Extended away"), NULL);
	_dnd = new BRadioButton("status", B_TRANSLATE("Do not disturb"), NULL);

	BStringView *query = new BStringView(NULL, B_TRANSLATE(
		"Please choose a status category on the left\n"
		"and enter a detailed status text below:"));

	// handle
	_handle = new BTextControl(NULL, NULL, "", NULL);
	_handle->SetDivider(0);

	std::optional<std::string> status = BlabberSettings::Instance()->Data("last-custom-more-exact-status");
	if (status) {
		_handle->SetText(status->c_str());
	} else {
		_handle->SetText(B_TRANSLATE("I'm at my computer."));
	}

	BButton *cancel = new BButton("cancel", B_TRANSLATE("Cancel"), new BMessage(JAB_CANCEL));
	cancel->SetTarget(this);

	BButton *ok = new BButton("ok", B_TRANSLATE("OK"), new BMessage(JAB_OK));

	ok->MakeDefault(true);
	ok->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGroup(B_HORIZONTAL)
			.AddGroup(B_VERTICAL, 0)
				.Add(_chat)
				.Add(_away)
				.Add(_xa)
				.Add(_dnd)
			.End()
			.AddStrut(B_USE_BIG_SPACING)
			.AddGroup(B_VERTICAL)
				.Add(query)
				.Add(_handle)
			.End()
		.End()
		.AddStrut(B_USE_BIG_SPACING)
		.AddGroup(B_HORIZONTAL, B_USE_ITEM_SPACING)
			.AddGlue()
			.Add(cancel)
			.Add(ok)
			.AddGlue()
		.End();

	// set defaults
	std::string exact_status;

	auto s = BlabberSettings::Instance()->Data("last-custom-exact-status");
	if (s) {
		// get last status
		exact_status = *s;

		// map to radio buttons
		if (exact_status == "away") {
			_away->SetValue(1);
		} else if (exact_status == "xa") {
			_xa->SetValue(1);
		} else if (exact_status == "dnd") {
			_dnd->SetValue(1);
		} else {
			_chat->SetValue(1);
		}
	} else {
		_chat->SetValue(1);
	}

	CenterIn(frame);
	_handle->MakeFocus(true);
}

CustomStatusWindow::~CustomStatusWindow() {
	_instance = NULL;
}

void CustomStatusWindow::MessageReceived(BMessage *msg) {
	switch (msg->what) {
		//// JAB_OK
		case JAB_OK: {
			if (_chat->Value()) {
				JabberSpeak::Instance()->SendPresence(gloox::Presence::Chat, _handle->Text());
				BlabberSettings::Instance()->SetData("last-custom-exact-status", "chat");
			} else if (_away->Value()) {
				JabberSpeak::Instance()->SendPresence(gloox::Presence::Away, _handle->Text());
				BlabberSettings::Instance()->SetData("last-custom-exact-status", "away");
			} else if (_xa->Value()) {
				JabberSpeak::Instance()->SendPresence(gloox::Presence::XA, _handle->Text());
				BlabberSettings::Instance()->SetData("last-custom-exact-status", "xa");
			} else if (_dnd->Value()) {
				JabberSpeak::Instance()->SendPresence(gloox::Presence::DND, _handle->Text());
				BlabberSettings::Instance()->SetData("last-custom-exact-status", "dnd");
			}

			BlabberSettings::Instance()->SetTag("last-used-custom-status", true);
			BlabberSettings::Instance()->SetData("last-custom-more-exact-status", _handle->Text());
			BlabberSettings::Instance()->WriteToFile();

			// update menu
			BlabberMainWindow::Instance()->SetCustomStatus(_handle->Text());

			PostMessage(B_QUIT_REQUESTED);

			break;
		}

		//// JAB_CANCEL
		case JAB_CANCEL: {
			PostMessage(B_QUIT_REQUESTED);
			break;
		}
	}
}
