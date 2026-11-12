/*
 * BookmarkkItem.h
 * Copyright 2019-2021 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once

#include <Bitmap.h>
#include <ListItem.h>
#include <String.h>
#include <View.h>

#include "../jabber/UserID.h"


class BookmarkItem : public BStringItem {
public:
						BookmarkItem(const gloox::JID& userid, BString name);
						~BookmarkItem();

	void				DrawItem(BView *owner, BRect frame, bool complete = false);
	virtual void		Update(BView *owner, const BFont *font);
	
	const gloox::JID&	GetUserID() const;

	enum Flags {
		NICKNAME_HIGHLIGHT = 1,
		ACTIVITY = 2
	};

	void				SetFlag(uint32 flag) { fFlags |= flag; }

private:
	const gloox::JID	_userid;

	int32				fFlags;

	static BBitmap*		_online_icon;
	static BBitmap*		_unknown_icon;
};
