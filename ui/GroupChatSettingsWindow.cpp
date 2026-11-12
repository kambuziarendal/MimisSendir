/*
 * GroupChatSettingsWindow.cpp
 * Copyright (C) 2024 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include "GroupChatSettingsWindow.h"

#include <ui/MainWindow.h>

#include <network/BookmarkManager.h>

#include <jabber/TalkManager.h>

#include <gloox/bookmarkhandler.h>

#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <SeparatorView.h>


#define B_TRANSLATION_CONTEXT "Group chat settings"


constexpr uint32_t kApplySettings = 'aply';


GroupChatSettingsWindow::GroupChatSettingsWindow(const gloox::JID& bookmark)
	: BWindow(BRect(0, 0, 100, 100), "", B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
	, fJID(bookmark.bare().c_str())
{
	BString title;
	title.SetToFormat(B_TRANSLATE("Group chat settings for %s"), bookmark.bare().c_str());
	SetTitle(title);

	BGroupView* full_view = new BGroupView("main-full", B_VERTICAL);
	full_view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLayout(new BGroupLayout(B_VERTICAL));
	AddChild(full_view);

	const gloox::ConferenceListItem* item
		= BookmarkManager::Instance().GetBookmark(bookmark.full().c_str());

	std::string name = item->name;
	if (name.empty())
		name = item->jid;

	fRoomName = new BStringView("room_name", item->name.c_str());
	fRoomName->SetFont(be_bold_font);

	fAutojoin = new BCheckBox(B_TRANSLATE("Autojoin"));
	fAutojoin->SetValue(item->autojoin);
	fBookmarkName = new BTextControl("bookmark name", B_TRANSLATE("Bookmark name"), name.c_str(),
		nullptr);
	fNickname = new BTextControl("nickname", B_TRANSLATE("Nickname"), item->nick.c_str(), nullptr);

	fPasswordConfig = new BCheckBox("password", B_TRANSLATE("Password protected"), nullptr);
	fPasswordConfig->SetEnabled(false);
	fPasswordHelp = new BStringView("", "");

	fLoggingConfig = new BCheckBox("logging", B_TRANSLATE("Public logging"), nullptr);
	fLoggingConfig->SetEnabled(false);
	fLoggingHelp = new BStringView("", "");

	fHiddenConfig = new BCheckBox("hidden", B_TRANSLATE("Hidden"), nullptr);
	fHiddenConfig->SetEnabled(false);
	fHiddenHelp = new BStringView("", "");

	fPersistentConfig = new BCheckBox("persistent", B_TRANSLATE("Persistent"), nullptr);
	fPersistentConfig->SetEnabled(false);
	fPersistentHelp = new BStringView("", "");

	fMembersOnlyConfig = new BCheckBox("members_only", B_TRANSLATE("Members only"), nullptr);
	fMembersOnlyConfig->SetEnabled(false);
	fMembersOnlyHelp = new BStringView("", "");

	fModeratedConfig = new BCheckBox("moderated", B_TRANSLATE("Moderated"), nullptr);
	fModeratedConfig->SetEnabled(false);
	fModeratedHelp = new BStringView("", "");

	fAnonymousConfig = new BCheckBox("anonymous", B_TRANSLATE("Anonymous"), nullptr);
	fAnonymousConfig->SetEnabled(false);
	fAnonymousHelp = new BStringView("", "");

	BLayoutBuilder::Group<>(full_view)
		.SetInsets(0)

		// TODO add a card with a barber pole until the MUC info is loaded
		.AddGrid()
			.SetInsets(B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS, 0)
			.Add(fRoomName, 0, 0, 2, 1)
			.Add(fPasswordConfig, 0, 1)
			.Add(fPasswordHelp, 1, 1)
			.Add(fLoggingConfig, 0, 2)
			.Add(fLoggingHelp, 1, 2)
			.Add(fHiddenConfig, 0, 3)
			.Add(fHiddenHelp, 1, 3)
			.Add(fPersistentConfig, 0, 4)
			.Add(fPersistentHelp, 1, 4)
			.Add(fMembersOnlyConfig, 0, 5)
			.Add(fMembersOnlyHelp, 1, 5)
			.Add(fModeratedConfig, 0, 6)
			.Add(fModeratedHelp, 1, 6)
			.Add(fAnonymousConfig, 0, 7)
			.Add(fAnonymousHelp, 1, 7)
		.End()
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGrid()
			.SetInsets(B_USE_WINDOW_INSETS, 0, B_USE_WINDOW_INSETS, B_USE_SMALL_SPACING)
			.SetSpacing(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
			.SetExplicitMinSize(BSize(fAutojoin->StringWidth("x") * 60, B_SIZE_UNSET))
			.AddTextControl(fBookmarkName, 0, 0)
			.AddTextControl(fNickname, 0, 1)
			.Add(fAutojoin, 0, 2)
			.AddGlue(1, 2)
		.End()
		.Add(new BSeparatorView(B_HORIZONTAL))
		// TODO separate tab for this info maybe?
		.AddGroup(B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_INSETS, B_USE_SMALL_SPACING, B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS)
			.AddGlue()
			.Add(new BButton(B_TRANSLATE("Cancel"), new BMessage(B_QUIT_REQUESTED)))
			.Add(new BButton(B_TRANSLATE("OK"), new BMessage(kApplySettings)))
		.End()
	.End();

	CenterOnScreen();

	BlabberMainWindow::Instance()->StartWatching(this, kRoomInfo);

	gloox::MUCRoom* room = (gloox::MUCRoom*)TalkManager::Instance()->IsExistingWindowToGroup(item->jid);
	room->getRoomInfo();
}


void
GroupChatSettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kApplySettings:
			BookmarkManager::Instance().SetBookmark(fJID, fNickname->Text(), fBookmarkName->Text(),
				fAutojoin->Value());
			TalkManager::Instance()->SetNickname(gloox::JID(fJID.String()), fNickname->Text());

			// TODO also change the nickname live
			BWindow::Close();
			break;
		case B_OBSERVER_NOTICE_CHANGE:
		{
			// TODO check orig_what == kRoomInfo
			uint32 flags = message->FindInt32("flags");
			// Password protection
			if (flags & gloox::FlagPasswordProtected) {
				fPasswordConfig->SetValue(B_CONTROL_ON);
				fPasswordHelp->SetText("Chatters need a password to enter this room.");
			} else if (flags & gloox::FlagUnsecured) {
				fPasswordConfig->SetValue(B_CONTROL_OFF);
				fPasswordHelp->SetText("No password needed to enter this room.");
			} else {
				fPasswordConfig->SetValue(B_CONTROL_PARTIALLY_ON);
				fPasswordHelp->SetText("The server did not specify if a password is needed.");
			}

			// Public logging
			if (flags & gloox::FlagPublicLogging) {
				fLoggingConfig->SetValue(B_CONTROL_ON);
				fLoggingHelp->SetText("Anyone can read the room logs.");
			} else if (flags & gloox::FlagPublicLoggingOff) {
				fLoggingConfig->SetValue(B_CONTROL_OFF);
				fLoggingHelp->SetText("Only users present in the room can read the logs.");
			} else {
				fLoggingConfig->SetValue(B_CONTROL_PARTIALLY_ON);
				fLoggingHelp->SetText("The server did not specify who can access the logs.");
			}

			if (flags & gloox::FlagHidden) {
				fHiddenConfig->SetValue(B_CONTROL_ON);
				fHiddenHelp->SetText("Not listed in server's room list");
			} else if (flags & gloox::FlagPublic) {
				fHiddenConfig->SetValue(B_CONTROL_OFF);
				fHiddenHelp->SetText("Listed in server's room list");
			} else {
				fHiddenConfig->SetValue(B_CONTROL_PARTIALLY_ON);
				fHiddenHelp->SetText("Server does not specify if room is visible by other users");
			}

			if (flags & gloox::FlagPersistent) {
				fPersistentConfig->SetValue(B_CONTROL_ON);
				fPersistentHelp->SetText("Room continues to exist even if everyone leaves");
			} else if (flags & gloox::FlagTemporary) {
				fPersistentConfig->SetValue(B_CONTROL_OFF);
				fPersistentHelp->SetText("Room disappears as soon as everyone leaves");
			} else {
				fPersistentConfig->SetValue(B_CONTROL_PARTIALLY_ON);
				fPersistentHelp->SetText("Room persistence unknown");
			}

			if (flags & gloox::FlagMembersOnly) {
				fMembersOnlyConfig->SetValue(B_CONTROL_ON);
				fMembersOnlyHelp->SetText("People need an invitation to join the room");
			} else if (flags & gloox::FlagOpen) {
				fMembersOnlyConfig->SetValue(B_CONTROL_OFF);
				fMembersOnlyHelp->SetText("Anyone with the address can join the room");
			} else {
				fMembersOnlyConfig->SetValue(B_CONTROL_PARTIALLY_ON);
				fMembersOnlyHelp->SetText("The server does not specify who can join the room");
			}

			if (flags & gloox::FlagModerated) {
				fModeratedConfig->SetValue(B_CONTROL_ON);
				fModeratedHelp->SetText("People must request permission to talk");
			} else if (flags & gloox::FlagUnmoderated) {
				fModeratedConfig->SetValue(B_CONTROL_OFF);
				fModeratedHelp->SetText("Everyone in the room can talk");
			} else {
				fModeratedConfig->SetValue(B_CONTROL_PARTIALLY_ON);
				fModeratedHelp->SetText("Server does not specifies who can talk");
			}

			if (flags & gloox::FlagNonAnonymous) {
				fAnonymousConfig->SetValue(B_CONTROL_OFF);
				fAnonymousHelp->SetText("Everyone can see your address");
			} else if (flags & gloox::FlagSemiAnonymous) {
				fAnonymousConfig->SetValue(B_CONTROL_PARTIALLY_ON);
				fAnonymousHelp->SetText("Only moderators can see your address");
			} else if (flags & gloox::FlagFullyAnonymous) {
				fAnonymousConfig->SetValue(B_CONTROL_PARTIALLY_ON);
				fAnonymousHelp->SetText("Only server administrators can see your address");
			} else {
				fAnonymousConfig->SetValue(B_CONTROL_OFF);
				fAnonymousHelp->SetText("The server does not specify who can see your address");
			}

			BString name = message->FindString("name");
			fRoomName->SetText(name);

			break;
		}
		default:
			BWindow::MessageReceived(message);
			break;
	}
}
