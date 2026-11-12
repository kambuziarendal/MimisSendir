/*
 * Copyright 2001-2022, Distributed under terms of the MIT license.
 *
 * Authors:
 *	John Blanco
 *	Adrien Destugues <pulkomandy@pulkomandy.tk>
 *	Humdinger <humdingerb@gmail.com>
 */

#include "JoinGroupChat.h"

#include <Button.h>
#include <Catalog.h>
#include <private/interface/ColumnListView.h>
#include <private/interface/ColumnTypes.h>
#include <LayoutBuilder.h>

#include <gloox/dataformreported.h>

#include "jabber/BlabberSettings.h"
#include "jabber/JabberSpeak.h"
#include "jabber/Messages.h"
#include "jabber/TalkManager.h"

#include "ui/ModalAlertFactory.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "JoinGroupChat"


enum {
	kDiscoInfoNickname = 'disc',
	kDiscoItemsMuc = 'mucd',
	kSelectRoom = 'rsel'
};

enum {
	kColumnJID,
	kColumnName,
	kColumnOccupants,
	kColumnDescription,
	kColumnLanguage,
};


JoinGroupChat::JoinGroupChat(BRect parentFrame)
	: BWindow(BRect(), NULL, B_DOCUMENT_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS)
{
	_handle = new BTextControl(B_TRANSLATE("Room name:"), NULL, NULL);
	_handle->TextView()->SetExplicitMinSize(BSize(be_plain_font->StringWidth(
		"VeryLongNamedSomebody@jabberadoodle.org"), B_SIZE_UNSET));
	auto lastgroup = BlabberSettings::Instance()->Data("last-group-joined");
	if (lastgroup)
		_handle->SetText(lastgroup->c_str());
	else
		_handle->SetText("renga@chat.jabberfr.org");

	BButton *cancel = new BButton("cancel", B_TRANSLATE("Cancel"), new BMessage(JAB_CANCEL));
	cancel->SetTarget(this);

	BButton *ok = new BButton("ok", "", new BMessage(JAB_OK));
	SetTitle(B_TRANSLATE("Start new group chat"));
	ok->SetLabel(B_TRANSLATE("Start group chat"));

	ok->MakeDefault(true);
	ok->SetTarget(this);

	_name = new BTextControl(B_TRANSLATE("Username:"), NULL, NULL);
	auto s = BlabberSettings::Instance()->Data("last-group-username");
	if (s)
		_name->SetText(s->c_str());
	else {
		s = BlabberSettings::Instance()->Data("channel-name");
		_name->SetText(s ? s->c_str() : NULL);
	}

	fRoomList = new BColumnListView("room list", 0);
	fRoomList->AddColumn(new BStringColumn("JID", 300, 70, 300, 0), kColumnJID); // TODO does it need to be visible?
	fRoomList->AddColumn(new BStringColumn("Name", 350, 70, 400, 0), kColumnName);
	fRoomList->AddColumn(new BIntegerColumn("Occupants", 75, 75, 75), kColumnOccupants);
	fRoomList->AddColumn(new BStringColumn("Description", 400, 200, 800, 0), kColumnDescription);
	fRoomList->AddColumn(new BStringColumn("Language", 70, 70, 70, 0), kColumnLanguage);

	fRoomList->SetSelectionMessage(new BMessage(kSelectRoom));

	fPasswordView = new BStringView("password", "This room is password protected.");
	fHiddenView = new BStringView("hidden", "This room is hidden.");
	fMembersView = new BStringView("members-only", "This room is reserved to members.");
	fModeratedView = new BStringView("moderated", "This room is moderated.");
	fTemporaryView = new BStringView("temporary", "This room is temporary.");
	fNonAnonView = new BStringView("nonanon", "This room is not anonymous. Everyone will see your address.");

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.Add(fRoomList, 5)
			.SetExplicitMinSize(BSize(750, 300))
		.AddGroup(B_VERTICAL)
			.SetInsets(B_USE_WINDOW_INSETS, 0, B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS)
			.AddGrid()
				.AddGlue(2, 0)
				.AddTextControl(_name, 0, 0, B_ALIGN_RIGHT)
				.AddTextControl(_handle, 0, 1, B_ALIGN_RIGHT)
			.End()
			.AddGrid()
				.Add(fPasswordView, 0, 0)
				.AddGlue(1, 0)
				.Add(fHiddenView, 0, 1)
				.Add(fMembersView, 0, 2)
				.Add(fModeratedView, 0, 3)
				.Add(fTemporaryView, 0, 4)
				.Add(fNonAnonView, 0, 5)
			.End()
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(cancel)
				.Add(ok)
			.End()
		.End()
	.End();

	fPasswordView->Hide();
	fHiddenView->Hide();
	fMembersView->Hide();
	fModeratedView->Hide();
	fTemporaryView->Hide();
	fNonAnonView->Hide();

	_handle->MakeFocus(true);
	CenterIn(parentFrame);
}


JoinGroupChat::~JoinGroupChat()
{
	// Cancel any pending disco requests to avoid a crash if the window is closed while still
	// gathering rooms information
	JabberSpeak::Instance()->GlooxClient()->disco()->removeDiscoHandler(this);
}


static void
SetVisible(BView* view, bool visible)
{
	if (visible) {
		if (view->IsHidden())
			view->Show();
	} else {
		if (!view->IsHidden())
			view->Hide();
	}
}


void JoinGroupChat::MessageReceived(BMessage* message) {
	switch (message->what) {
		case JAB_OK:
		{
			if (!ValidateGroupRoom()) {
				break;
			}

			BlabberSettings::Instance()->SetData("last-group-joined", _handle->Text());
			BlabberSettings::Instance()->SetData("last-group-username", _name->Text());

			// XEP-0045 Section 7.12: the MUC may force a specific nickname through disco items
			// In that case, the user entered nickname will be ignored.
			JabberSpeak::Instance()->GlooxClient()->disco()->getDiscoInfo(
				gloox::JID(_handle->Text()), "x-roomuser-item", this, kDiscoInfoNickname);

			break;
		}

		case JAB_CANCEL: {
			PostMessage(B_QUIT_REQUESTED);
			break;
		}

		case kDiscoInfoNickname:
		{
			// FIXME get the suggested nickname from the disco reply if any, instead of _name->Text()?
			BString nickname = message->GetString("name", _name->Text());
			// create session
			TalkManager::Instance()->JoinRoom(_handle->Text(), nickname.String());

			// Add to bookmarks - TODO let user decide if they want autojoin?
			BookmarkManager::Instance().SetBookmark(_handle->Text(), nickname, "", true);

			PostMessage(B_QUIT_REQUESTED);
			break;
		}

		case kDiscoItemsMuc:
		{
			const char* jid;
			for (int32 i = 0; (jid = message->FindString("JID", i)); i++) {
				const char* name = message->FindString("name", i);

				// TODO some things may not be MUC rooms.
				// Maybe do not add anything to the list here yet?
				BRow* row = new BRow();
				row->SetField(new BStringField(jid), kColumnJID);
				row->SetField(new BStringField(name), kColumnName);
				fRows[jid].row = row;
				fRoomList->AddRow(row);
			}
			break;
		}

		case gloox::GetRoomInfo:
		{
			const char* jid = message->FindString("JID");

			int32 occupants = 0;
			BString(message->FindString("muc#roominfo_occupants")).ScanWithFormat("%" B_PRId32, &occupants);

			//int32 features = message->FindInt32("features");

			const auto roomRow = fRows.find(jid);
			if (roomRow == fRows.end()) {
				printf("??? received info from unknown room\n");
				return;
			}

			BRow* row = roomRow->second.row;

			if (occupants <= 0) {
				// Remove empty rooms, not very interesting to list them
				fRoomList->RemoveRow(row);
				return;
			}

			row->SetField(new BIntegerField(occupants), kColumnOccupants);

			const char* description = message->FindString("muc#roominfo_description");
			row->SetField(new BStringField(description), kColumnDescription);
			const char* language = message->FindString("muc#roominfo_lang");
			row->SetField(new BStringField(language), kColumnLanguage);

			BString feature;
			for (int i = 0; message->FindString("feature", i, &feature) == B_OK; i++) {
				roomRow->second.features.insert(feature);
			}

			// TODO other fields and features in disco replies that may or may not be useful
			// In particular, the MUC spec says we should warn the user for:
			// - Rooms that have public logs (what you say will be immediately exposed on the web)
			// - Rooms that are not anonymous (your JID will be known to other occupants)
			// It may also be nice to show the avatar in the list of rooms.
			//
			// Field: name muc#roomconfig_changesubject label  value 
			// Field: name muc#roominfo_webchat_url label  value https://chat.jabberfr.org/converse.js/renga@chat.jabberfr.org
			// Field: name muc#roominfo_logs label  value http://chat.jabberfr.org:5280/muc_log/renga/
			// Field: name {http://prosody.im/protocol/muc}roomconfig_allowmemberinvites label Allow members to invite new members value 1
			// Field: name muc#roomconfig_allowinvites label Allow users to invite other users value 1
			// Field: name muc#roominfo_avatarhash label  value e008ed58476ac69fd90244649e03cf65b194aec7

			break;
		}

		case kSelectRoom:
		{
			BRow* row = fRoomList->CurrentSelection();
			if (row) {
				const char* jid = dynamic_cast<BStringField*>(row->GetField(kColumnJID))->String();
				_handle->SetText(jid);

				std::set<BString>& features = fRows[jid].features;

				bool password_set = false;
				bool hidden_set = false;
				bool members_set = false;
				bool moderated_set = false;
				bool persistent_set = false;
				bool anon_set = false;

				for (auto feature: features) {
					if (feature == "muc_unsecured") {
						assert(!password_set);
						password_set = true;
						SetVisible(fPasswordView, false);
					} else if (feature == "muc_public") {
						assert(!hidden_set);
						hidden_set = true;
						SetVisible(fHiddenView, false);
					} else if (feature == "muc_open") {
						assert(!members_set);
						members_set = true;
						SetVisible(fMembersView, false);
					} else if (feature == "muc_membersonly") {
						assert(!members_set);
						members_set = true;
						SetVisible(fMembersView, true);
					} else if (feature == "muc_unmoderated") {
						assert(!moderated_set);
						moderated_set = true;
						SetVisible(fModeratedView, false);
					} else if (feature == "muc_moderated") {
						assert(!moderated_set);
						moderated_set = true;
						SetVisible(fModeratedView, true);
					} else if (feature == "muc_persistent") {
						assert(!persistent_set);
						persistent_set = true;
						SetVisible(fTemporaryView, false);
					} else if (feature == "muc_temporary") {
						assert(!persistent_set);
						persistent_set = true;
						SetVisible(fTemporaryView, true);
					} else if (feature == "muc_semianonymous") {
						assert(!anon_set);
						anon_set = true;
						SetVisible(fNonAnonView, true);
						fNonAnonView->SetText("This room is semi anonymous. Admins will see your address.");
					} else if (feature == "muc_nonanonymous") {
						assert(!anon_set);
						anon_set = true;
						SetVisible(fNonAnonView, true);
						fNonAnonView->SetText("This room is not anonymous. Everyone will see your address.");
					} else
						// Features that are not show to the user
						// TODO don't debug-print the ones that are known and not interesting to show.
						puts(feature.String());
#if 0
http://jabber.org/protocol/muc
http://jabber.org/protocol/muc#request
http://jabber.org/protocol/muc#self-ping-optimization
http://jabber.org/protocol/muc#stable_id
jabber:iq:register
urn:xmpp:hats:0
urn:xmpp:mam:2
urn:xmpp:mam:2#extended
urn:xmpp:message-moderate:0
urn:xmpp:message-moderate:1
urn:xmpp:occupant-id:0
urn:xmpp:sid:0
vcard-temp
#endif

				}

				// Make sure we updated all the fields visibilities
				assert(password_set);
				assert(hidden_set);
				assert(members_set);
				assert(moderated_set);
				assert(persistent_set);
				assert(anon_set);

				// Public logging is not discovered using feature flags, so it will only be known
				// after joining the room.
				// TODO we may guess from the info form that we received but not yet forwarded here.
				// Or we can also use MAM as a good hint.
			}
			break;
		}

		default:
			BWindow::MessageReceived(message);

	}
}


void JoinGroupChat::Show()
{
	JabberSpeak::Instance()->DiscoverMUCs(this);
	BWindow::Show();
}


bool JoinGroupChat::ValidateGroupRoom() {
	char buffer[4096];

	if (!strcmp(_handle->Text(), "")) {
		sprintf(buffer, "Please specify the group's room name.");
		ModalAlertFactory::Alert(buffer, "OK");
		_handle->MakeFocus(true);

		return false;
	}

	if (!strcmp(_name->Text(), "")) {
		sprintf(buffer, "Please specify your handle.");
		ModalAlertFactory::Alert(buffer, "OK");
		_handle->MakeFocus(true);

		return false;
	}

	return true;
}


void
JoinGroupChat::handleDiscoInfo(const gloox::JID &from, const gloox::Disco::Info &info, int context)
{
	BMessage message(context);

	message.AddString("JID", from.full().c_str());

	// TODO can there actually be multiple identities here?
	for (auto identity: info.identities()) {
		message.AddString("category", identity->category().c_str());
		message.AddString("type", identity->type().c_str());
		// Not needed, already found from the disco items
		message.AddString("name", identity->name().c_str());
	}

	for (const auto& i: info.features()) {
		message.AddString("feature", i.c_str());
	}

	if (info.form()) {
		// Unhandled stuff (not needed for MUC?)
		// - Form title
		// - Form type
		// - Instructions
		// - Reported fields
		// - Items
		for (const auto&i : info.form()->fields()) {
			message.AddString(i->name().c_str(), i->value().c_str());
		}
	}

	Looper()->PostMessage(&message, this);
	// May parse form as well here if needed.

	Looper()->PostMessage(&message, this);
}


void
JoinGroupChat::handleDiscoItems(const gloox::JID &from, const gloox::Disco::Items &items, int context)
{
	BMessage message(kDiscoItemsMuc);
	(void)from;
	(void)context;

	for (auto item: items.items()) {
		message.AddString("JID", item->jid().full().c_str());
		message.AddString("name", item->name().c_str());

		JabberSpeak::Instance()->GlooxClient()->disco()->getDiscoInfo(item->jid(), gloox::EmptyString, this, gloox::GetRoomInfo);
	}

	Looper()->PostMessage(&message, this);
}


void
JoinGroupChat::handleDiscoError(const gloox::JID &from, const gloox::Error *error, int context)
{
	printf("%s: %d %d %s\n", __PRETTY_FUNCTION__, error->type(), error->error(), error->text().c_str());
	// Attempting to discover the username failed, likely the group chat doesn't exist.
	// Continue with joining the room (that will create it)

	(void)from;
	(void)error;

	BMessage message(context);
	Looper()->PostMessage(&message, this);
}
