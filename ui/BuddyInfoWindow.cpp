//////////////////////////////////////////////////
// Blabber [BuddyInfoWindow.cpp]
//////////////////////////////////////////////////

#include "BuddyInfoWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <GroupView.h>
#include <private/shared/IconView.h>
#include <LayoutBuilder.h>
#include <Resources.h>

#include "support/AppLocation.h"

#include "ui/BarberPole.h"
#include "ui/PictureView.h"

#include "../jabber/GenericFunctions.h"
#include "../jabber/JabberSpeak.h"
#include "../jabber/Messages.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "BuddyInfoWindow"


BuddyInfoWindow::BuddyInfoWindow(UserID *querying_user)
	: BWindow(BRect(0, 0, 0, 0), B_TRANSLATE("Buddy information"), B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
	fJID(querying_user->Handle().c_str())
{
	SetLayout(new BGroupLayout(B_VERTICAL));
	BGroupView* full_view = new BGroupView(B_VERTICAL);
	full_view->GroupLayout()->SetInsets(B_USE_WINDOW_SPACING);

	fAvatar = new PictureView("unknown");
	BGridView* surrounding = new BGridView();

	// USER INFORMATION
	BStringView *friendly_label, *friendly_name;
	BStringView *jabberid_label, *jabberid_name;
	BStringView *status_label, *status_name;
	BStringView *realstatus_label = NULL, *realstatus_name = NULL;

	friendly_label = new BStringView(NULL, B_TRANSLATE("Nickname:"));
	if (querying_user->FriendlyName().empty()) {
		friendly_name  = new BStringView(NULL, B_TRANSLATE("<unknown>"));
	} else {
		friendly_name  = new BStringView(NULL, querying_user->FriendlyName().c_str());
	}

	if (!querying_user->MoreExactOnlineStatus().empty()) {
		status_label = new BStringView(NULL, B_TRANSLATE("Personalized status:"));
		status_name  = new BStringView(NULL, querying_user->MoreExactOnlineStatus().c_str());

		surrounding->GridLayout()->AddView(status_label, 0, 1);
		surrounding->GridLayout()->AddView(status_name, 1, 1);
	}

	if (!querying_user->ExactOnlineStatus().empty()) {
		realstatus_label = new BStringView(NULL, B_TRANSLATE("Official status:"));

		if (querying_user->ExactOnlineStatus() == "xa") {
			realstatus_name  = new BStringView(NULL, B_TRANSLATE("Extended away"));
		} else if (querying_user->ExactOnlineStatus() == "away") {
			realstatus_name  = new BStringView(NULL, B_TRANSLATE("Away"));
		} else if (querying_user->ExactOnlineStatus() == "chat") {
			realstatus_name  = new BStringView(NULL, B_TRANSLATE("Available for chat"));
		} else if (querying_user->ExactOnlineStatus() == "dnd") {
			realstatus_name  = new BStringView(NULL, B_TRANSLATE("Do not disturb"));
		}

		surrounding->GridLayout()->AddView(realstatus_label, 0, 2);
		surrounding->GridLayout()->AddView(realstatus_name, 1, 2);
	}

	if (querying_user->UserType() == UserID::JABBER) {
		jabberid_label = new BStringView(NULL, B_TRANSLATE("Jabber ID:"));
		jabberid_name  = new BStringView(NULL, querying_user->Handle().c_str());
	} else {
		jabberid_label = new BStringView(NULL, B_TRANSLATE("Jabber ID:"));
		jabberid_name  = new BStringView(NULL, querying_user->JabberUsername().c_str());
	}

	// OK button
	BButton *ok = new BButton("ok", B_TRANSLATE("OK"), new BMessage(B_QUIT_REQUESTED));

	ok->MakeDefault(true);
	ok->SetTarget(this);

	fSpinner = new BarberPole("wait...");

	BLayoutBuilder::Group<>(full_view)
		.AddGroup(B_HORIZONTAL)
			.AddGroup(B_VERTICAL)
				.Add(fAvatar)
				.AddGlue()
			.End()
			.AddGroup(B_VERTICAL)
				.Add(surrounding)
				.AddGlue()
			.End()
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fSpinner)
			.AddGlue(0)
			.Add(ok)
		.End()
	.End();

	fDetails = surrounding->GridLayout();
	fDetails->SetVerticalSpacing(B_USE_SMALL_SPACING);
	fDetails->AddView(friendly_label, 0, 0);
	fDetails->AddView(friendly_name, 1, 0);
	fDetails->AddView(jabberid_label, 0, 3);
	fDetails->AddView(jabberid_name, 1, 3);

	AddChild(full_view);

	CenterOnScreen();

	// Watch for the result
	JabberSpeak::Instance()->StartWatching(this, kVCardReceived);
	// Ask the client to fetch the vcard
	JabberSpeak::Instance()->RequestVCard(querying_user->JID());
}


BuddyInfoWindow::~BuddyInfoWindow()
{
}


void
BuddyInfoWindow::Show()
{
	BWindow::Show();
	fSpinner->Start();
}


void
BuddyInfoWindow::MessageReceived(BMessage* message)
{
	if (message->what == B_OBSERVER_NOTICE_CHANGE) {
		int32 what = message->FindInt32("be:observe_change_what");

		switch(what) {
			case kVCardReceived:
			{
				fSpinner->Hide();
				int line = 4;

				// Check if the JID matches, first!
				BString jid = message->FindString("jid");
				if (jid != fJID)
					return;

				// Handle non-string fields
				int32 classification = message->FindInt32("classification");
				switch (classification)
				{
					case gloox::VCard::ClassPublic:
						fDetails->AddView(new BStringView(NULL, B_TRANSLATE("Classification:")), 0, line);
						fDetails->AddView(new BStringView(NULL, B_TRANSLATE("Public")), 1, line);
						line++;
						break;
					case gloox::VCard::ClassPrivate:
						fDetails->AddView(new BStringView(NULL, B_TRANSLATE("Classification:")), 0, line);
						fDetails->AddView(new BStringView(NULL, B_TRANSLATE("Private")), 1, line);
						line++;
						break;
					case gloox::VCard::ClassConfidential:
						fDetails->AddView(new BStringView(NULL, B_TRANSLATE("Classification:")), 0, line);
						fDetails->AddView(new BStringView(NULL, B_TRANSLATE("Confidential")), 1, line);
						line++;
						break;
					default:
						break;
				}

				const void* avatar;
				ssize_t avatarSize;
				if (message->FindData("photo:binval", B_RAW_TYPE, &avatar,
					&avatarSize) == B_OK) {
					BMemoryIO io(avatar, avatarSize);
					fAvatar->SetBitmap(&io);
				} else if (message->FindData("logo:binval", B_RAW_TYPE, &avatar,
					&avatarSize) == B_OK) {
					BMemoryIO io(avatar, avatarSize);
					fAvatar->SetBitmap(&io);
				}

				// Now iterate over all string keys
				char* name;
				type_code type;
				int32 count;

				BResources resources;
				extern int main();
				resources.SetToImage((const void*)main);
				size_t size = 0;

				int32 stringCount = message->CountNames(B_STRING_TYPE);
				for (int i = 0; i < stringCount; i++) {
					message->GetInfo(B_STRING_TYPE, i, &name, &type, &count);
					if (strcmp(name, "jid") == 0) {
						// We already display it
					} else {
						for (int j = 0; j < count; j++) {
							BString value = message->FindString(name, j);
							if (value == "")
								continue;

							if (strcmp(name, "mail:address") == 0) {
								int32 flags = message->FindInt32("mail:flags", j);

								BGroupView* title = new BGroupView(B_HORIZONTAL);
								BLayoutBuilder::Group<> builder(title);

								const char* values[] = {
									"preferred",
									"home",
									"work"
								};

								for (unsigned int k = 0; k < B_COUNT_OF(values); k++) {
									if ((values[k] != NULL) && (flags & (1 << k))) {
										const uint8_t* data = (const uint8_t*)resources.LoadResource('VICN',
											values[k], &size);
										IconView* icon = new IconView();
										icon->SetIcon(data, size);
										icon->SetExplicitSize(BSize(20, 20));
										builder.Add(icon);
									}
								}

								builder.Add(new BStringView(NULL, "E-Mail"));
								builder.AddGlue();
								fDetails->AddView(title, 0, line);
							} else if (strncmp(name, "address:", strlen("address:")) == 0) {
								int32 flags = message->FindInt32("address:flags", j);

								BGroupView* title = new BGroupView(B_HORIZONTAL);
								BLayoutBuilder::Group<> builder(title);

								const char* values[] = {
									"preferred",
									"home",
									"work"
								};

								for (unsigned int k = 0; k < B_COUNT_OF(values); k++) {
									if ((values[k] != NULL) && (flags & (1 << k))) {
										const uint8_t* data = (const uint8_t*)resources.LoadResource('VICN',
											values[k], &size);
										IconView* icon = new IconView();
										icon->SetIcon(data, size);
										icon->SetExplicitSize(BSize(20, 20));
										builder.Add(icon);
									}
								}

								builder.Add(new BStringView(NULL, B_TRANSLATE("Address")));
								builder.AddGlue();
								fDetails->AddView(title, 0, line);
							} else {
								// TODO manage flags for phone, label
								fDetails->AddView(new BStringView(NULL, name), 0, line);
							}
							fDetails->AddView(new BStringView(NULL, value), 1, line);
							line++;
						}
					}
				}

				return;
			}
		}
	}

	BWindow::MessageReceived(message);
}
