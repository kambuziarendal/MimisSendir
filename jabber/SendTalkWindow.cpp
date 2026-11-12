/*
 * Copyright 2001-2022, Distributed under terms of the MIT license.
 *
 * Authors:
 *	John Blanco
 *	Adrien Destugues <pulkomandy@pulkomandy.tk>
 *	Humdinger <humdingerb@gmail.com>
 */

#include "SendTalkWindow.h"

#include <cstdio>

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>

#include "support/AppLocation.h"

#include "ui/ModalAlertFactory.h"

#include "Agent.h"
#include "AgentList.h"
#include "BlabberSettings.h"
#include "JabberSpeak.h"
#include "Messages.h"
#include "TalkManager.h"
#include "UserID.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SendTalkWindow"

SendTalkWindow::SendTalkWindow(BRect frame)
	: BWindow(BRect(), NULL, B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
{
	_handle = new BTextControl(B_TRANSLATE("Jabber ID:"), NULL, NULL);

	_handle->TextView()->SetExplicitMinSize(BSize(be_plain_font->StringWidth(
		"VeryLongNamedSomebody@jabberadoodle.org"), B_SIZE_UNSET));

	auto s = BlabberSettings::Instance()->Data("last-talk-sent-to");
	if (s)
		_handle->SetText(s->c_str());
	else
		_handle->SetText("somebody@jabber.org");

	BButton *cancel = new BButton("cancel", B_TRANSLATE("Cancel"), new BMessage(JAB_CANCEL));
	cancel->SetTarget(this);

	BButton *ok = new BButton("ok", B_TRANSLATE("Start chat"), new BMessage(JAB_OK));

	SetTitle(B_TRANSLATE("Start new chat"));

	ok->MakeDefault(true);
	ok->SetTarget(this);

	BGridLayout* layout;
	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.AddGrid()
			.SetInsets(B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS, 0)
			.GetLayout(&layout)
			.Add(_handle->CreateLabelLayoutItem(), 0, 1)
			.Add(_handle->CreateTextViewLayoutItem(), 1, 1)
		.End()
		.AddStrut(B_USE_BIG_SPACING)
		.AddGroup(B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_INSETS, 0, B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS)
			.AddGlue()
			.Add(cancel)
			.Add(ok)
			.AddGlue()
		.End()
	.End();

	_chat_services_selection = new BPopUpMenu("Jabber");
	_chat_services = new BMenuField("chat_services", B_TRANSLATE("Online service: "),
		_chat_services_selection);

	BMenuItem *default_item = new BMenuItem("Jabber", new BMessage(AGENT_MENU_CHANGED_TO_JABBER));
	_chat_services_selection->AddItem(default_item);
	default_item->SetMarked(true);

	BLayoutBuilder::Grid<>(layout)
		.Add(_chat_services->CreateLabelLayoutItem(), 0, 0)
		.Add(_chat_services->CreateMenuBarLayoutItem(), 1, 0);

	_handle->MakeFocus(true);
	CenterIn(frame);
}

SendTalkWindow::~SendTalkWindow() {
}

void SendTalkWindow::MessageReceived(BMessage *msg) {
	switch (msg->what) {
		case AGENT_MENU_CHANGED_TO_JABBER: {
//			_enter_note->SetText("Please enter a Jabber ID of the form username@server (e.g., beoslover@jabber.org).");
			_handle->SetLabel("Jabber ID:");
			break;
		}

		//// JAB_OK
		case JAB_OK: {
			std::string user(ValidateUser());

			if (!user.empty()) {
				BlabberSettings::Instance()->SetData("last-talk-sent-to", user.c_str());

				// create session
				gloox::JID jid(user);
				TalkManager::Instance()->CreateTalkSession(&jid);
			}

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

std::string
SendTalkWindow::ValidateUser()
{
	char buffer[4096];

	if (!strcmp(_handle->Text(), "")) {
		sprintf(buffer, "Please specify a user's handle.");
		ModalAlertFactory::Alert(buffer, "OK");
		_handle->MakeFocus(true);

		return "";
	}

	// internally replace the username with a proper one if necessary (transports)
	std::string username = _handle->Text();

	// if not Jabber
	if (strcasecmp(_handle->Label(), "Jabber ID:")) {
		username = GenericFunctions::CrushOutWhitespace(username);
	}


	// make a user to validate against
	std::string validate = UserID::WhyNotValidJabberHandle(username);

	if (!strcasecmp(_handle->Label(), "Jabber ID:") && validate.size()) {
		sprintf(buffer, "%s is not a valid Jabber ID for the following reason:\n\n%s\n\nPlease correct it.", _handle->Text(), validate.c_str());
		ModalAlertFactory::Alert(buffer, "Hmm, better check that...");
		_handle->MakeFocus(true);

		return "";
	}

	return username;
}
