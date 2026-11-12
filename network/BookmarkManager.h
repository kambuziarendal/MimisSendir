/*
 * BookmarkManager.h
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once


#include <gloox/bookmarkhandler.h>
#include <gloox/bookmarkstorage.h>
#include <gloox/client.h>

#include "jabber/JabberSpeak.h"

#include "bookmark2storage.h"

#include <Handler.h>

#include <memory>


enum {
	kBookmarks = 'bkmk'
};

class BookmarkManager: public BHandler, public gloox::BookmarkHandler
{
	public:
		static BookmarkManager& Instance();

		void Connect(bool usePepNativeBookamrks);
		void Disconnect();
		void SetBookmark(BString jid, BString nick, BString name, bool autojoin);
		const gloox::ConferenceListItem* GetBookmark(BString jid);
		void RememberName(const std::string& jid, const std::string& name);
		void RemoveBookmark(BString jid);

		// gloox::BookmarkHandler
		void handleBookmarks(const gloox::BookmarkList& bList,
			const gloox::ConferenceList& cList) final;

	private:
		std::unique_ptr<gloox::BookmarkStorage> fBookmarks;
		std::unique_ptr<gloox::Bookmark2Storage> fPepBookmarks;

		gloox::BookmarkList fURIBookmarks;
		gloox::ConferenceList fConferences;
};
