//////////////////////////////////////////////////
// Blabber [TalkManager.h]
//     Handles the simultaneous talk sessions
//     going on.
//////////////////////////////////////////////////

#ifndef TALK_MANAGER_H
#define TALK_MANAGER_H

#include <gloox/messagesessionhandler.h>
#include <gloox/message.h>
#include <gloox/mucroomhandler.h>
#include <gloox/pubsubresulthandler.h>

#include <map>
#include "support/LifetimeRegistry.h"
#include <string>

#include "GenericFunctions.h"

#include "ui/TalkView.h"
#include "ui/Xhtml.h"

enum {
	kWindowList = 'winl',
	kAvatarUpdate = 'avup',
	kHighlightRosterItem = 'hiri',
	kRefreshBookmark = 'rebm',
	kRoomInfo = 'rinf'
};

// FIXME could we replace this with MessageSessionHandler?
class TalkManager : public BHandler, public gloox::MessageSessionHandler,
	gloox::MUCRoomHandler, public gloox::MessageHandler, gloox::PubSub::ResultHandler {
public:
	typedef  std::map<gloox::MessageSession*, TalkView *>        TalkMap;
	typedef  std::map<gloox::MUCRoom*, TalkView *>        GroupMap;

public:
	static TalkManager  *Instance();
      	                ~TalkManager();

	void				CreateTalkSession(const gloox::JID* user,
							gloox::MessageSession* session = nullptr, bool sound_on_new = false);
	void				JoinRoom(const std::string& group_room, const std::string& group_username);
	void				LeaveRoom(const gloox::JID&, const std::string reason = "");
	void				SetNickname(const gloox::JID& room, const std::string newNick);
	void				SetStatus(const gloox::JID& room, const gloox::Presence::PresenceType status);

	void*                IsExistingWindowToGroup(std::string group_room);
	void                 UpdateWindowTitles(const gloox::JID& user, BString newTitle);
	void                 RemoveWindow(TalkView* view);
	bool OwnsView(const TalkView* view, uint64 token) const;

	void                 Reset();

	void				GetAvatar(const gloox::JID& jid, const std::string& hash);

	// MUC handlers from gloox
	// FIXME probably each TalkView could register for these directly?
	void				handleMUCParticipantPresence(gloox::MUCRoom *room,
							const gloox::MUCRoomParticipant participant,
							const gloox::Presence &presence);
	void				handleMUCMessage(gloox::MUCRoom *room,
							const gloox::Message &msg, bool priv);
	bool				handleMUCRoomCreation(gloox::MUCRoom *room);
	void				handleMUCSubject(gloox::MUCRoom *room,
							const std::string &nick,
							const std::string &subject);
	void				handleMUCInviteDecline(gloox::MUCRoom *room,
							const gloox::JID &invitee,
							const std::string &reason);
	void				handleMUCError(gloox::MUCRoom *room,
							gloox::StanzaError error);
	void				handleMUCInfo(gloox::MUCRoom *room, int features,
							const std::string &name,
							const gloox::DataForm *infoForm);
	void				handleMUCItems(gloox::MUCRoom *room,
							const gloox::Disco::ItemList &items);

	// MessageSessionHandler hooks
	void handleMessageSession(gloox::MessageSession* session) final;

	// gloox::MessageHandler hooks
	void handleMessage(const gloox::Message& msg, gloox::MessageSession* session) final;
	void DeliverMessage(const gloox::Message& msg, gloox::MessageSession* session,
		bool outgoing = false);

	// gloox::PubSub::ResultHandler hooks
	virtual void handleItem(const gloox::JID&, const std::string&, const gloox::Tag*);
	virtual void handleItems(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::ItemList&, const gloox::Error*);
	virtual void handleItemPublication(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::ItemList&, const gloox::Error*);
	virtual void handleItemDeletion(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::ItemList&, const gloox::Error*);
	virtual void handleSubscriptionResult(const std::string&, const gloox::JID&, const std::string&, const std::string&, const gloox::JID&, gloox::PubSub::SubscriptionType, const gloox::Error*);
	virtual void handleUnsubscriptionResult(const std::string&, const gloox::JID&, const gloox::Error*);
	virtual void handleSubscriptionOptions(const std::string&, const gloox::JID&, const gloox::JID&, const std::string&, const gloox::DataForm*, const std::string&, const gloox::Error*);
	virtual void handleSubscriptionOptionsResult(const std::string&, const gloox::JID&, const gloox::JID&, const std::string&, const std::string&, const gloox::Error*);
	virtual void handleSubscribers(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::SubscriptionList&, const gloox::Error*);
	virtual void handleSubscribersResult(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::SubscriberList*, const gloox::Error*);
	virtual void handleAffiliates(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::AffiliateList*, const gloox::Error*);
	virtual void handleAffiliatesResult(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::AffiliateList*, const gloox::Error*);
	virtual void handleNodeConfig(const std::string&, const gloox::JID&, const std::string&, const gloox::DataForm*, const gloox::Error*);
	virtual void handleNodeConfigResult(const std::string&, const gloox::JID&, const std::string&, const gloox::Error*);
	virtual void handleNodeCreation(const std::string&, const gloox::JID&, const std::string&, const gloox::Error*);
	virtual void handleNodeDeletion(const std::string&, const gloox::JID&, const std::string&, const gloox::Error*);
	virtual void handleNodePurge(const std::string&, const gloox::JID&, const std::string&, const gloox::Error*);
	virtual void handleSubscriptions(const std::string&, const gloox::JID&, const gloox::PubSub::SubscriptionMap&, const gloox::Error*);
	virtual void handleAffiliations(const std::string&, const gloox::JID&, const gloox::PubSub::AffiliationMap&, const gloox::Error*);
	virtual void handleDefaultNodeConfig(const std::string&, const gloox::JID&, const gloox::DataForm*, const gloox::Error*);

protected:
						TalkManager();

	void				MessageReceived(BMessage*) override;
private:
	void RememberView(const TalkView* view);
    void PostToView(TalkView* view, const BMessage& notice);
	uint64 ViewToken(const TalkView* view) const;
	void				_MentionNotification(const std::string& roomName, const gloox::Message& msg);

private:
	static TalkManager *_instance;

	BDirectory			fAvatarCache;

	// FIXME remove, use gloox::SessionHandler so that all messages are directly routed to the right place
	LifetimeRegistry fViewRegistry;
	TalkMap				fTalkMap;
	GroupMap			fGroupMap;

	XhtmlProcessor		fXhtmlProcessor;
};

#endif
