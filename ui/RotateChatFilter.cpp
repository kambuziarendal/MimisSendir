/* Copyright 2001-2023, Jabber4BeOS, Jabber4Haiku and Renga developers.
 * Distributed under the terms of the MIT license.
 */

#include <interface/InterfaceDefs.h>

#include <app/Message.h>

#include "ui/RotateChatFilter.h"

#include "jabber/Messages.h"

RotateChatFilter::RotateChatFilter()
	: BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, B_KEY_DOWN, NULL) {
}

filter_result RotateChatFilter::Filter(BMessage *message, __attribute__((unused)) BHandler **target) {
	int8 byte;
	message->FindInt8("byte", &byte);

	if (byte == B_FUNCTION_KEY) {
		int32 key;

		message->FindInt32("key", &key);
		
		if (key == B_F8_KEY) {
			BlabberMainWindow::Instance()->PostMessage(JAB_ROTATE_CHAT_FORWARD);
			return B_SKIP_MESSAGE;
		}
	}

	return B_DISPATCH_MESSAGE;
}

