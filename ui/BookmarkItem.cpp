/*
 * Copyright 2019-2021 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include "BookmarkItem.h"
#include "ui/ConversationRow.h"

#include "../jabber/TalkManager.h"

#include "../network/BookmarkManager.h"

#include <gloox/bookmarkhandler.h>

#include <String.h>
#include <TranslationUtils.h>

#include "ui/HVIFUtil.h"


BBitmap *BookmarkItem::_online_icon = NULL;
BBitmap *BookmarkItem::_unknown_icon = NULL;


BookmarkItem::BookmarkItem(const gloox::JID& userid, BString name)
	: BStringItem(name)
	, _userid(userid)
	, fFlags(0)
{
}


BookmarkItem::~BookmarkItem() {
}


void BookmarkItem::DrawItem(BView* owner, BRect frame, bool) {
    Conversations::Summary item;
    item.jid = _userid.bare(); item.group = true;
    item.name = Conversations::FriendlyName(item.jid, Text(), true);
    item.preview = "Group";
    DrawConversationRow(owner, frame, IsSelected(), item);
}
void BookmarkItem::Update(BView* owner, const BFont* font) {
    BStringItem::Update(owner, font); SetHeight(58);
}

const gloox::JID& BookmarkItem::GetUserID() const {
	return _userid;
}
