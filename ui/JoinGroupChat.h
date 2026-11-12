/*
 * JoinGroupChat.h
 * Copyright (C) 2025 pulkomandy <pulkomandy@kitt>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once

#include <Window.h>

#include <gloox/discohandler.h>
#include <gloox/mucroomhandler.h>

#include <set>
#include <unordered_map>

class BColumnListView;
class BMessage;
class BRow;
class BStringView;
class BTextControl;


class JoinGroupChat: public BWindow, public gloox::DiscoHandler {
	public:
						JoinGroupChat(BRect parentFrame);
						~JoinGroupChat();
		void			MessageReceived(BMessage* msg) override;
		void			Show() override;

		// gloox::DiscoHandler (for getting MUC reserved nickname)
		void			handleDiscoInfo(const gloox::JID &from, const gloox::Disco::Info &info,
			int context) override;
		void			handleDiscoItems(const gloox::JID &from, const gloox::Disco::Items &items,
			int context) override;
		void			handleDiscoError(const gloox::JID &from, const gloox::Error *error,
			int context) override;

	private:
		bool			ValidateGroupRoom();
	private:
		BTextControl*	_handle;
		BTextControl*	_name;

		BColumnListView* fRoomList;
		struct RoomInfo {
			BRow* row;
			std::set<BString> features;
		};
		std::unordered_map<std::string, RoomInfo> fRows;

		BStringView*	fPasswordView;
		BStringView*	fHiddenView;
		BStringView*	fMembersView;
		BStringView*	fModeratedView;
		BStringView*	fTemporaryView;
		BStringView*	fNonAnonView;
};
