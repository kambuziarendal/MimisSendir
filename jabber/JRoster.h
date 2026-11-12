//////////////////////////////////////////////////
// Blabber [JRoster.h]
//     The official JRoster repository
//////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>

#include "UserID.h"

#include <gloox/rosterlistener.h>

class JRoster: public gloox::RosterListener {
public:
	// search method
	enum search_method         {HANDLE, COMPLETE_HANDLE, TRANSPORT_ID};

	// shortened
	typedef std::vector<UserID *>      RosterList;
	typedef RosterList::iterator       RosterIter;
	typedef RosterList::const_iterator ConstRosterIter;

public:
	static JRoster              *Instance();
	                            ~JRoster();

	void                         AddNewUser(const gloox::JID& new_user, std::string friendlyName);
	void                         RemoveUser(const gloox::JID& removed_user);
	void                         RemoveAllUsers();

	UserID                      *FindUser(search_method search_type, std::string key);
	UserID                      *FindUser(const gloox::JID& comparing_user);

	UserID::online_status        UserStatus(std::string username);

	ConstRosterIter              BeginIterator();
	ConstRosterIter              EndIterator();

	void                         RefreshRoster();

	void                         Lock();
	void                         Unlock();

	void 					handleItemAdded(const gloox::JID&) final;
	void 					handleItemSubscribed(const gloox::JID&) final;
	void 					handleItemRemoved(const gloox::JID&) final;
	void 					handleItemUpdated(const gloox::JID&) final;
	void 					handleItemUnsubscribed(const gloox::JID&) final;
	void 					handleRoster(const gloox::Roster&) final;
	void 					handleRosterPresence(const gloox::RosterItem&, const std::string&, gloox::Presence::PresenceType, const std::string&) final;
	void 					handleSelfPresence(const gloox::RosterItem&, const std::string&, gloox::Presence::PresenceType, const std::string&) final;
	bool 					handleSubscriptionRequest(const gloox::JID&, const std::string&) final;
	bool 					handleUnsubscriptionRequest(const gloox::JID&, const std::string&) final;
	void 					handleNonrosterPresence(const gloox::Presence&) final;
	void 					handleRosterError(const gloox::IQ&) final;

protected:
	                    	JRoster();

private:
	void                    _ProcessUserPresence(UserID *user, const gloox::Presence::PresenceType, const std::string&);

private:
	static JRoster          *_instance;
	RosterList              *_roster;

	sem_id                  _roster_lock;
};

