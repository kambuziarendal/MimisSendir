//////////////////////////////////////////////////
// Blabber [JRoster.cpp]
//////////////////////////////////////////////////

#include <kernel/OS.h>

#include <gloox/rostermanager.h>
#include <Catalog.h>

#include "JabberSpeak.h"
#include "JRoster.h"
#include "Messages.h"
#include "MessageRepeater.h"
#include "ui/ModalAlertFactory.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "JRoster"


JRoster *JRoster::_instance = NULL;

JRoster *JRoster::Instance() {
	if (_instance == NULL) {
		_instance = new JRoster();
	}
	
	return _instance;
}

JRoster::~JRoster() {
	_instance = NULL;

	// destroy semaphore
	delete_sem(_roster_lock);
}


void JRoster::AddNewUser(const gloox::JID& new_user, std::string friendlyName) {
	_roster->push_back(new UserID(new_user));

	// communicate the new user to the server
	gloox::Client* client = JabberSpeak::Instance()->GlooxClient();
	gloox::StringList groups;
	client->rosterManager()->add(new_user, friendlyName, groups);
	client->rosterManager()->synchronize();

	// refresh all roster views
	RefreshRoster();
}

void JRoster::RemoveUser(const gloox::JID& removed_user) {
	for (RosterIter i = _roster->begin(); i != _roster->end(); ++i) {
		if ((*i)->JID() == removed_user) {

			// for transports
			if ((*i)->UserType() == UserID::TRANSPORT) {
				Agent *agent = AgentList::Instance()->GetAgentByID((*i)->TransportID());

				if (agent) {
					agent->SetRegisteredFlag(false);
				}
			}
			// goodbye memory
			delete (*i);
			_roster->erase(i);
			
			break;	
		}
	}
}

void JRoster::RemoveAllUsers() {
	// BUGBUG need more elegant way of deleting all users (check STL guide)
	while(_roster->begin() != _roster->end()) {
		RosterIter i = _roster->begin();
		UserID *user = *i;

		_roster->erase(i);
		delete user;
	}
}
	
UserID *JRoster::FindUser(search_method search_type, std::string name) {
	if (search_type == JRoster::HANDLE) {
		for (RosterIter i = _roster->begin(); i != _roster->end(); ++i) {
			if (!strcasecmp(name.c_str(), (*i)->JabberHandle().c_str())) {
				return (*i);
			}
		}
	}

	if (search_type == JRoster::COMPLETE_HANDLE) {
		for (RosterIter i = _roster->begin(); i != _roster->end(); ++i) {
			if (!strcasecmp(name.c_str(), (*i)->JabberCompleteHandle().c_str())) {
				return (*i);
			}
		}
	}

	if (search_type == JRoster::TRANSPORT_ID) {
		for (RosterIter i = _roster->begin(); i != _roster->end(); ++i) {
			if (!strcasecmp(name.c_str(), (*i)->TransportID().c_str())) {
				return (*i);
			}
		}
	}

	return NULL;
}

UserID *JRoster::FindUser(const gloox::JID& comparing_user) {
	return FindUser(JRoster::HANDLE, comparing_user.bare());
}

UserID::online_status JRoster::UserStatus(std::string username) {
	UserID *user = FindUser(COMPLETE_HANDLE, username);
	
	if (user != NULL) {
		return user->OnlineStatus();
	}

	return UserID::UNKNOWN;
}

JRoster::ConstRosterIter JRoster::BeginIterator() {
	return _roster->begin();
}

JRoster::ConstRosterIter JRoster::EndIterator() {
	return _roster->end();
}

void JRoster::RefreshRoster() {
	// update everyone to the change
	MessageRepeater::Instance()->PostMessage(BLAB_UPDATE_ROSTER);
	MessageRepeater::Instance()->PostMessage(TRANSPORT_UPDATE);
}

void JRoster::Lock() {
	acquire_sem(_roster_lock);
}

void JRoster::Unlock() {
	release_sem(_roster_lock);
}

void
JRoster::handleItemAdded(const gloox::JID& jid)
{
    handleItemUpdated(jid);
}


void
JRoster::handleItemSubscribed(const gloox::JID&)
{
	printf("%s\n", __PRETTY_FUNCTION__);
#if 0
		user->SetOnlineStatus(UserID::ONLINE);

		if (entity->Child("status")) {
			sprintf(buffer, "[%s]\n\n%s", asker, entity->Child("status")->Data());
		} else {
			sprintf(buffer, B_TRANSLATE("Your subscription request was accepted by %s!"), asker);
		}
		
		ModalAlertFactory::Alert(buffer, "OK");
#endif
}


void
JRoster::handleItemRemoved(const gloox::JID& jid)
{
    Lock();
    RemoveUser(gloox::JID(jid.bare()));
    Unlock();
    RefreshRoster();
}


void
JRoster::handleItemUpdated(const gloox::JID& jid)
{
	gloox::Client* client = JabberSpeak::Instance()->GlooxClient();
	gloox::RosterItem* item = client->rosterManager()->getRosterItem(jid);
	if (item == nullptr) {
		printf("%s(%s) user not found in roster\n", __PRETTY_FUNCTION__, jid.full().c_str());
		return;
	}
    Lock();
    UserID* roster_user = FindUser(jid);
    if (!roster_user) {
        roster_user = new UserID(gloox::JID(jid.bare()));
        _roster->push_back(roster_user);
    }
    roster_user->SetSubscriptionStatus(item->subscription());
    Unlock();
    RefreshRoster();
}


void
JRoster::handleItemUnsubscribed(const gloox::JID&)
{
	printf("%s\n", __PRETTY_FUNCTION__);
#if 0
		user->SetOnlineStatus(UserID::UNKNOWN);
#endif
}


void
JRoster::handleRoster(const gloox::Roster& roster)
{
	Lock();

	for (auto item: roster) {
		UserID user(item.first);
		user.SetSubscriptionStatus(item.second->subscription());

		// obtain a handle to the user (is there a new one?)
		UserID *roster_user;
			
		if (user.IsUser()) {
			roster_user = FindUser(JRoster::HANDLE, user.JabberHandle());
		} else if (user.UserType() == UserID::TRANSPORT) {
			roster_user = FindUser(JRoster::TRANSPORT_ID, user.TransportID());
		} else {
			continue;
		}

		// if we have duplicates, settle disputes
		if (roster_user) {
#if 0
			// process if it's a removal
			if (entity->Child(i)->Attribute("subscription") && !strcasecmp(entity->Child(i)->Attribute("subscription"), "remove")) {
				// remove from the list
				RemoveUser(roster_user);

				continue;
			}
#endif

			// update the new roster item
			*roster_user = user;
		} else {
			// create the user
			roster_user = new UserID(item.first);

			*roster_user = user;

			// add to the list
			_roster->push_back(roster_user);
		}
	}

	Unlock();	

	// update all RosterViews
	RefreshRoster();
}


void
JRoster::handleRosterPresence(const gloox::RosterItem& item,
	const std::string& resource __attribute__((unused)), gloox::Presence::PresenceType presenceType,
	const std::string& message)
{
	int num_matches = 0;
	const gloox::JID& jid = item.jidJID();
    JabberSpeak::Instance()->BridgePresence(jid, presenceType);

	Lock();

	for (JRoster::ConstRosterIter i = BeginIterator();
		i != EndIterator(); ++i)
	{
		UserID *user = NULL;
		if ((*i)->IsUser() && !strcasecmp(UserID(jid.full()).JabberHandle().c_str(),
			(*i)->JabberHandle().c_str()))
		{
			// found another match
			++num_matches;
			user = *i;
			_ProcessUserPresence(user, presenceType, message);
		} else if ((*i)->UserType() == UserID::TRANSPORT
			&& !strcasecmp(UserID(jid.full()).TransportID().c_str(),
				(*i)->TransportID().c_str()))
		{
			// found another match
			++num_matches;
			user = *i;
			_ProcessUserPresence(user, presenceType, message);
		}
	}
	if (num_matches == 0) {
#if 0
		UserID user(jid.full());
		ProcessUserPresence(&user, presence);
#endif
		puts("user not found");
	}

	Unlock();

	// update all RosterViews
	RefreshRoster();
}


void
JRoster::handleSelfPresence(const gloox::RosterItem&, const std::string& resource,
	gloox::Presence::PresenceType, const std::string& msg)
{
	// TODO this gets called for giving us our current presence and message,
	// and also for all other resources, so we can know that the same user
	// is also online elsewhere
	printf("%s(%s, %s)\n", __PRETTY_FUNCTION__, resource.c_str(), msg.c_str());
}


bool
JRoster::handleSubscriptionRequest(const gloox::JID& JID, const std::string& reason)
{
	BString message;
	if (reason.empty())
		message.SetToFormat(B_TRANSLATE("%s would like to subscribe to your presence so they may "
			"know if you're online or not. Would you like to allow it?"), JID.bare().c_str());
	else
		message.SetToFormat(B_TRANSLATE("%s would like to subscribe to your presence so they may "
			"know if you're online or not. Would you like to allow it?\nThey write:\n%s"),
			JID.bare().c_str(), reason.c_str());

	gloox::Client* client = JabberSpeak::Instance()->GlooxClient();
	// query for presence authorization (for users)
	int32 answer = 0;
	// TODO this uses a synchrnous alert so the gloox thread will be blocked (no other message can
	// be sent or received) until the alert is answered by the user. Instead we should use
	// asynchronous mode
	answer = ModalAlertFactory::Alert(message.String(), B_TRANSLATE("No, I prefer privacy"),
		B_TRANSLATE("Yes, grant them my presence"));

	// send back the response
	if (answer == 1) {
		gloox::Subscription subscription(gloox::Subscription::Subscribed, JID);
		client->send(subscription);
		return true;
	}
	gloox::Subscription subscription(gloox::Subscription::Unsubscribed, JID);
	client->send(subscription);
	return false;
}


bool
JRoster::handleUnsubscriptionRequest(const gloox::JID&, const std::string&)
{
	printf("%s\n", __PRETTY_FUNCTION__);
#if 0
		sprintf(buffer, B_TRANSLATE("%s no longer wishes to know your online status."), asker);
		ModalAlertFactory::NonModalAlert(buffer, B_TRANSLATE("I feel so unloved"));
#endif
	return false;
}


void
JRoster::handleNonrosterPresence(const gloox::Presence& presence)
{
    JabberSpeak::Instance()->BridgePresence(presence.from(), presence.subtype());
	printf("%s\n", __PRETTY_FUNCTION__);
}


void
JRoster::handleRosterError(const gloox::IQ&)
{
	printf("%s\n", __PRETTY_FUNCTION__);
}

JRoster::JRoster() {
	_roster = new RosterList;
	
	// create semaphore
	_roster_lock = create_sem(1, "roster sempahore");
}

void JRoster::_ProcessUserPresence(UserID *user,
	gloox::Presence::PresenceType type, const std::string& message) {
#if 0
	// get best asker name
	const char *asker;
					
	if (user && user->FriendlyName().size() > 0) {
		// they have a friendly name
		asker = user->FriendlyName().c_str();
	} else {
		// they have a JID
		asker = entity.from().full();
	}
#endif

	// reflect presence
	if (user && type == gloox::Presence::Unavailable) {
		user->SetOnlineStatus(UserID::OFFLINE);
	} else if (user && type == gloox::Presence::Available) {
		user->SetOnlineStatus(UserID::ONLINE);
	}

	if (user && (type == gloox::Presence::Unavailable
			|| type == gloox::Presence::Available)) {
#if 0
		if (entity->Child("show") && entity->Child("show")->Data()) {
			user->SetExactOnlineStatus(entity->Child("show")->Data());
		}
#endif

		user->SetMoreExactOnlineStatus(message);
	}
}

