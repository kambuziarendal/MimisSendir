/*
 * GroupChatSettingsWindow.h
 * Copyright (C) 2024 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once


#include <gloox/jid.h>

#include <Window.h>


class BCheckBox;
class BStringView;
class BTextControl;


class GroupChatSettingsWindow: public BWindow {
	public:
		GroupChatSettingsWindow(const gloox::JID&);

	private:
		void MessageReceived(BMessage*) override;

	private:
		BString fJID;

		BStringView* fRoomName;

		BTextControl* fBookmarkName;
		BTextControl* fNickname;
		BCheckBox* fAutojoin;

		BCheckBox* fPasswordConfig;
		BCheckBox* fLoggingConfig;
		BCheckBox* fHiddenConfig;
		BCheckBox* fPersistentConfig;
		BCheckBox* fMembersOnlyConfig;
		BCheckBox* fModeratedConfig;
		BCheckBox* fAnonymousConfig;
		BStringView* fPasswordHelp;
		BStringView* fLoggingHelp;
		BStringView* fHiddenHelp;
		BStringView* fPersistentHelp;
		BStringView* fMembersOnlyHelp;
		BStringView* fModeratedHelp;
		BStringView* fAnonymousHelp;
};
