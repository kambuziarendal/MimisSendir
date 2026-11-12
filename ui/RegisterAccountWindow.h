/*
 * RegisterAccountWindow.h
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once

#include "../network/GlooxHandler.h"

#include <CardLayout.h>
#include <GridView.h>
#include <GroupView.h>
#include <ListView.h>
#include <StringView.h>
#include <TextControl.h>
#include <Url.h>
#include <Window.h>



class RegisterAccountWindow: public BWindow
{
	public:
		RegisterAccountWindow(BHandler* target);

		void MessageReceived(BMessage* message) override;
		void Show() override;

	private:
		void onDisconnect(gloox::ConnectionError error, gloox::StreamError streamError);
		void handleDataForm(const gloox::JID& from, BView* form);
		void handleMedia(BString type, BUrl uri);

		void handleRegistrationResult(const gloox::JID&, gloox::RegistrationResult r);

		BView* getRegistrationView();

	private:
		BCardLayout* fLayout;

		// Server selection items
		BTextView* fWelcome;
		BListView* fServerList;
		BTextControl* fServerBox;

		// Connection in progress
		BStringView* fWaitingMessage;

		// Registration form items
		BGridView* fRegistrationForm;

		BString fUsername;
		BString fPassword;
#if 0
		BTextView* fInstructions;
		BTextControl* fUsername;
		BTextControl* fNickname;
		BTextControl* fPassword;
		BTextControl* fFirstName;
		BTextControl* fLastName;
		BTextControl* fEmail;
		BTextControl* fPostalAddress;
		BTextControl* fCity;
		BTextControl* fState;
		BTextControl* fZip;
		BTextControl* fPhoneNumber;
		BTextControl* fUrl;
		BTextControl* fDate;
		BTextControl* fMisc;
		BTextControl* fExtra;
#endif

		GlooxHandler* fConnection;
};


