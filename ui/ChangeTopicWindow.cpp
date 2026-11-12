/*
 * ChangeTopicWindow.cpp
 * Copyright (C) 2026 PulkoMandy <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include "ChangeTopicWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <ControlLook.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <StringView.h>

#include "../jabber/JabberSpeak.h"
#include "../jabber/Messages.h"
#include "../jabber/TalkManager.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ChangeTopicWindow"


BorderedTextView::BorderedTextView(const char* name)
	: BTextView(name)
{
}


void
BorderedTextView::Draw(BRect updateRect)
{
	BTextView::Draw(updateRect);

	BRect rect = Bounds();

	rgb_color base = ViewColor();
	uint32 flags = 0;

	bool active = IsFocus() && Window()->IsActive();
	if (active)
		flags |= BControlLook::B_FOCUSED;

	be_control_look->DrawTextControlBorder(this, rect, updateRect, base, flags);

	//SetHighColor(make_color(255, 128, 0));
	//StrokeRect(rect);
}


ChangeTopicWindow::ChangeTopicWindow(gloox::MUCRoom& target_room, BString oldSubject)
	: BWindow(BRect(0, 0, 100, 100), B_TRANSLATE("Change room subject"), B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fTargetRoom(target_room)
{
	BGroupView* full_view = new BGroupView("main-full", B_VERTICAL);
	full_view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLayout(new BGroupLayout(B_HORIZONTAL));
	AddChild(full_view);

	BStringView *query = new BStringView(NULL, B_TRANSLATE(
		"Specify the subject for this room:"));

	fSubjectText = new BorderedTextView("topic editor");
	fSubjectText->SetWordWrap(false);
	fSubjectText->SetText(oldSubject);

	BButton *cancelButton = new BButton("cancel", B_TRANSLATE("Cancel"), new BMessage(JAB_CANCEL));
	cancelButton->SetTarget(this);

	BButton *okButton = new BButton("ok", B_TRANSLATE("Change subject"), new BMessage(JAB_OK));
	//okButton->MakeDefault(true);
	okButton->SetTarget(this);
	okButton->ResizeToPreferred();

	BLayoutBuilder::Group<>(full_view)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(query)
		.Add(fSubjectText)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(cancelButton)
			.Add(okButton)
		.End()
	.End();

	fSubjectText->MakeFocus(true);
	fSubjectText->SetInsets(3, 3, 3, 3);

	CenterOnScreen();
}


ChangeTopicWindow::~ChangeTopicWindow() {
}

void ChangeTopicWindow::MessageReceived(BMessage *message) {
	switch (message->what) {
		case JAB_OK: {
			fTargetRoom.setSubject(fSubjectText->Text());
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
