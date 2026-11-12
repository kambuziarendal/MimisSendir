#include "ChangeNameWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <StringView.h>

#include "../jabber/JabberSpeak.h"
#include "../jabber/Messages.h"
#include "../jabber/TalkManager.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ChangeNameWindow"


ChangeNameWindow::ChangeNameWindow(const gloox::JID& changingUser, BString oldName)
	: BWindow(BRect(0, 0, 100, 100), B_TRANSLATE("Change buddy name"), B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
	fTargetUser(changingUser)
{
	BGroupView* full_view = new BGroupView("main-full", B_VERTICAL);
	full_view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLayout(new BGroupLayout(B_HORIZONTAL));
	AddChild(full_view);

	BStringView *query = new BStringView(NULL, B_TRANSLATE(
		"Specify the new \"Nickname\" you'd like to use:"));

	fNickNameText = new BTextControl(NULL, NULL, "", NULL);

	BButton *cancelButton = new BButton("cancel", B_TRANSLATE("Cancel"), new BMessage(JAB_CANCEL));
	cancelButton->SetTarget(this);

	BButton *okButton = new BButton("ok", B_TRANSLATE("Change nickname"), new BMessage(JAB_OK));
	okButton->MakeDefault(true);
	okButton->SetTarget(this);

	BLayoutBuilder::Group<>(full_view)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(query)
		.Add(fNickNameText)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(cancelButton)
			.Add(okButton)
		.End()
	.End();

	fNickNameText->SetText(oldName);
	fNickNameText->MakeFocus(true);

	CenterOnScreen();
}


ChangeNameWindow::~ChangeNameWindow() {
}

void ChangeNameWindow::MessageReceived(BMessage *message) {
	switch (message->what) {
		case JAB_OK: {
			JabberSpeak::Instance()->SetFriendlyName(fTargetUser, fNickNameText->Text());
			PostMessage(B_QUIT_REQUESTED);
			break;
		}

		case JAB_CANCEL: {
			PostMessage(B_QUIT_REQUESTED);
			break;
		}
		default:
			BWindow::MessageReceived(message);
	}
}
