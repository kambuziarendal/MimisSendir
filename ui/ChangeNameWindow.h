/**
 * @file ChangeNameWindow.h
 * @brief Change a contact's friendly name
 */

#pragma once

#include <gloox/jid.h>

#include <TextControl.h>
#include <Window.h>

class ChangeNameWindow : public BWindow {
public:
							ChangeNameWindow(const gloox::JID& changing_user, BString oldName);
							~ChangeNameWindow();

	void					MessageReceived(BMessage *msg);

private:
	const gloox::JID&		fTargetUser;
	BTextControl			*fNickNameText;
};
