//////////////////////////////////////////////////
// Blabber [RotateChatFilter.h]
//////////////////////////////////////////////////

#ifndef ROTATE_CHAT_FILTER_H
#define ROTATE_CHAT_FILTER_H

#include <MessageFilter.h>

#include "jabber/TalkManager.h"

#include "ui/TalkView.h"


class RotateChatFilter : public BMessageFilter {
public:
	                      RotateChatFilter();

	virtual filter_result Filter(BMessage *message, BHandler **target);
};

#endif

