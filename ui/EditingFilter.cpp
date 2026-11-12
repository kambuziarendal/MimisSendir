/* Copyright 2001-2023, Jabber4BeOS, Jabber4Haiku and Renga developers.
 * Distributed under the terms of the MIT license.
 */

#include <interface/InterfaceDefs.h>
#include <app/Message.h>

#include "jabber/Messages.h"

#include "ui/EditingFilter.h"
#include "ui/TalkView.h"

EditingFilter::EditingFilter(BTextView *view, TalkView *window)
	: BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, B_KEY_DOWN, NULL) {
	_view   = view;
	_window = window;
}

filter_result EditingFilter::Filter(BMessage *message, __attribute__((unused)) BHandler **target) {
	int32 modifiers;
	int8 byte;

	message->FindInt8("byte", &byte);
	message->FindInt32("modifiers", &modifiers);

	modifiers &= ~(B_NUM_LOCK | B_SCROLL_LOCK | B_CAPS_LOCK);

	// Command up/down allows to refill the input with previously sent messages
	// TODO: if view is empty, also allow this without modifiers (convenient to edit the previous message)
	if (((modifiers & B_COMMAND_KEY) != 0) && (byte == B_UP_ARROW)) {
		_window->RevealPreviousHistory();
		return B_DISPATCH_MESSAGE;
	}

	if (((modifiers & B_COMMAND_KEY) != 0) && (byte == B_DOWN_ARROW)) {
		_window->RevealNextHistory();
		return B_DISPATCH_MESSAGE;
	}


	// Command + B;I;U: bold, italics, underline
	if (((modifiers & B_COMMAND_KEY) != 0) && (byte == 'b' || byte == 'i' || byte == 'u')) {
		_window->TextStyling(byte);
		return B_DISPATCH_MESSAGE;
	}

	if (byte == B_ENTER) {
		// ENTER key management:
		// - With any modifier: insert a newline
		// - Without modifiers: send the message
		// The message is "skipped" from the point of view of the BTextView so it doesn't add an
		// extra newline
		if (modifiers & (B_SHIFT_KEY | B_COMMAND_KEY | B_CONTROL_KEY)) {
			_view->Insert("\n");
			return B_SKIP_MESSAGE;
		} else {
			BMessenger(_window).SendMessage(JAB_CHAT_SENT);
			return B_SKIP_MESSAGE;
		}
	}

	if (_window->IsGroupChat() && (byte == B_TAB)
		&& ((modifiers == B_SHIFT_KEY) || (modifiers == 0))) {
		// TODO scan the current text in _view to find the prefix before the cursor position
		_window->TabComplete(modifiers & (B_SHIFT_KEY));
		// Don't insert the TAB character in the view
		return B_SKIP_MESSAGE;
	}

	// All other keys keep their normal handling
	return B_DISPATCH_MESSAGE;
}
