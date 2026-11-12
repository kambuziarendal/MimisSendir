#include "AddBuddyWindow.h"

#include "support/AppLocation.h"

#include "ui/ModalAlertFactory.h"

#include "jabber/Agent.h"
#include "jabber/AgentList.h"
#include "jabber/BlabberSettings.h"
#include "jabber/GenericFunctions.h"
#include "jabber/JRoster.h"
#include "jabber/Messages.h"

#include <Button.h>
#include <Application.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <StringView.h>

#include <cstdio>
#include <string.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AddBuddyWindow"


const int kXMPPSelected = 0;
const int kIRCSelected = 1;

AddBuddyWindow *AddBuddyWindow::fInstance = NULL;

AddBuddyWindow *AddBuddyWindow::Instance(BRect frame) {
	if (fInstance == NULL)
		fInstance = new AddBuddyWindow(frame);

	return fInstance;
}


AddBuddyWindow::~AddBuddyWindow() {
	fInstance = NULL;
}


static const char* sXMPPHelpText = B_TRANSLATE_MARK(
	"Please enter a JID of the form username@server (e.g., beoslover@jabber.org).");
static const char* sIRCHelpText  = B_TRANSLATE_MARK(
	"Please enter the user's IRC nickname and server (e.g., haikulover%irc.oftc.net).");


AddBuddyWindow::AddBuddyWindow(BRect frame)
	: BWindow(BRect(), B_TRANSLATE("Add a buddy"), B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_ZOOMABLE)
{
	fNickName = new BTextControl("nickName", B_TRANSLATE("Nickname:"), "", NULL);

	fServiceSelection = new BOptionPopUp("Service Selection", B_TRANSLATE("Online service:"),
		new BMessage(kOptionChanged));
	fServiceSelection->AddOption(B_TRANSLATE("XMPP"), kXMPPSelected);
	fServiceSelection->AddOption(B_TRANSLATE("IRC"), kIRCSelected);

	fHandle = new BTextControl("userID", B_TRANSLATE("XMPP ID:"), "", NULL);

	fHelpText = new BStringView("helpText", sXMPPHelpText);

	BButton *kCancelButton = new BButton(B_TRANSLATE("Cancel"), new BMessage(JAB_CANCEL));
	kCancelButton->SetTarget(this);

	BButton *kOkButton = new BButton(B_TRANSLATE("Add buddy"), new BMessage(JAB_OK));
	kOkButton->MakeDefault(true);
	kOkButton->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.AddGrid()
			.SetInsets(B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS, 0)
			.Add(fNickName->CreateLabelLayoutItem(), 0, 0)
			.Add(fNickName->CreateTextViewLayoutItem(), 1, 0)
			.Add(fServiceSelection, 0, 1, 2, 1)
			.Add(fHandle->CreateLabelLayoutItem(), 0, 2)
			.Add(fHandle->CreateTextViewLayoutItem(), 1, 2)
			.Add(fHelpText, 1, 3)
		.End()
		.AddStrut(B_USE_BIG_SPACING)
		.AddGroup(B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_INSETS, 0, B_USE_WINDOW_INSETS, B_USE_WINDOW_INSETS)
			.AddGlue()
			.Add(kCancelButton)
			.Add(kOkButton)
			.AddGlue()
		.End()
	.End();

	CenterIn(frame);
	fNickName->MakeFocus(true);
}


void AddBuddyWindow::MessageReceived(BMessage *msg) {
	switch (msg->what) {
		case JAB_OK: {
			AddNewUser();
			break;
		}

		case JAB_CANCEL: {
			PostMessage(B_QUIT_REQUESTED);
			break;
		}

		case kOptionChanged:
		{
			switch (fServiceSelection->SelectedOption()) {
				case kXMPPSelected: {
					fHelpText->SetText(sXMPPHelpText);
					fHandle->SetLabel(B_TRANSLATE("XMPP ID:"));
					break;
				}
				case kIRCSelected: {
					fHelpText->SetText(sIRCHelpText);
					fHandle->SetLabel(B_TRANSLATE("IRC handle:"));
					break;
				}
			}
			break;
		}
		default:
			BWindow::MessageReceived(msg);
			break;
	}
}


bool AddBuddyWindow::QuitRequested() {
	fInstance = NULL;
	return true;
}


void AddBuddyWindow::AddNewUser() {
	char buffer[4096];

	if (fNickName->Text()[0] == 0) {
		fNickName->MarkAsInvalid(true);
		fNickName->MakeFocus(true);

		return;
	} else {
		fNickName->MarkAsInvalid(false);
	}

	if (fHandle->Text()[0] == 0) {
		fHandle->MarkAsInvalid(true);
		fHandle->MakeFocus(true);

		return;
	}

	std::string username = GenericFunctions::CrushOutWhitespace(fHandle->Text());

	// make a user to validate against
	std::string validate = UserID::WhyNotValidJabberHandle(username);

	if (fServiceSelection->SelectedOption() == kXMPPSelected && validate.size()) {
		sprintf(buffer, B_TRANSLATE(
			"%s is not a valid XMPP ID for the following reason:\n\n%s\n\n"
			"Please correct it."), fHandle->Text(), validate.c_str());
		ModalAlertFactory::Alert(buffer, B_TRANSLATE("OK"));
		fHandle->MakeFocus(true);
		fHandle->MarkAsInvalid(true);

		return;
	}

	// make sure it's not a duplicate of one already existing (unless itself)
	JRoster::Instance()->Lock();
	if (JRoster::Instance()->FindUser(JRoster::COMPLETE_HANDLE, fHandle->Text())) {
		sprintf(buffer, B_TRANSLATE(
			"%s already exists in your buddy list.\n\n"
			"Please choose another so you won't get confused."), fHandle->Text());
		ModalAlertFactory::Alert(buffer, B_TRANSLATE("OK"));
		fHandle->MakeFocus(true);
		fHandle->MarkAsInvalid(true);

		JRoster::Instance()->Unlock();

		return;
	}
	JRoster::Instance()->Unlock();

	gloox::JID jid(username);

	// add this user to the roster
	JRoster::Instance()->Lock();
	JRoster::Instance()->AddNewUser(jid, fNickName->Text());
	JRoster::Instance()->Unlock();

	// alert all RosterViews
	JRoster::Instance()->RefreshRoster();

	// close window explicitly
	PostMessage(B_QUIT_REQUESTED);
}
