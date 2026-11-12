/*
 * ChangeTopicWindow.h
 * Copyright (C) 2026 pulkomandy <pulkomandy@kitt>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once

#include <gloox/mucroom.h>

#include <TextView.h>
#include <Window.h>

class BorderedTextView: public BTextView {
public:
	BorderedTextView(const char* name);

	void Draw(BRect updateRect) override;
};

class ChangeTopicWindow : public BWindow {
public:
							ChangeTopicWindow(gloox::MUCRoom& target_room, BString oldSubject);
							~ChangeTopicWindow();

	void					MessageReceived(BMessage *msg) override;

private:
	gloox::MUCRoom&			fTargetRoom;
	BTextView				*fSubjectText;
};
