#include "network/ArchiveManager.h"
//////////////////////////////////////////////////
// Blabber [JabberSpeak.h]
//     This monolith talks the jabber language!
//////////////////////////////////////////////////

#ifndef JABBER_SPEAK_H
#define JABBER_SPEAK_H

#include <gloox/bookmarkstorage.h>
#include <gloox/client.h>
#include <gloox/connectionlistener.h>
#include <gloox/presence.h>
#include <gloox/pubsubmanager.h>
#include <gloox/registration.h>
#include <gloox/softwareversion.h>
#include <gloox/vcardhandler.h>
#include <gloox/vcardmanager.h>

#include "../network/BookmarkManager.h"
#include "../network/UploadManager.h"
#include "support/ConnectionLifecycle.h"

#include <map>
#include <deque>
#include <string>
#include <Looper.h>
#include "Agent.h"
#include "AgentList.h"
#include "BlabberSettings.h"
#include "../ui/TalkView.h"
#include "UserID.h"

enum {
	kAuthenticationFailed = 'Aerr',
	kConnectionRefused = 'Cerr',
	kVCardReceived = 'vcrd',
	kConnectionStopped = 'cStp',
    kBridgeStatus = 'bRdy'
};

enum  {
	kDiscoveryOwnJID = 1,
	kDiscoveryServer = 2,
};

class JabberSpeak : public BHandler,
	public gloox::ConnectionListener, public gloox::IqHandler,
	public gloox::VCardHandler, public gloox::DiscoHandler
{
public:
	enum iq_intent {LOGIN, ROSTER, AGENTS, REGISTER, SEND_REGISTER, UNREGISTER, SEND_UNREGISTER, NEW_USER, MESSAGE, CHAT};

	typedef std::map<std::string, iq_intent>                  IQMap;
	typedef std::map<std::string, iq_intent>::iterator        IQIter;
	typedef std::map<std::string, iq_intent>::const_iterator  ConstIQIter;

public:
	// CREATORS
	static JabberSpeak      *Instance();
	                        ~JabberSpeak();

	void                     Reset();
	bool HandleConnectionStopped(uint64 generation);
	void                     JabberSpeakReset();
 
	// OUTGOING COMMUNICATION
	void                     SendConnect(gloox::JID username, std::string password,
								std::string realname = "", bool suppress_auto_connect = false);
	void                     SendConnect(std::string realname);
	void                     SendDisconnect();
	bool IsReadyForConnection() const { return fConnectionState.IsIdle(); }
	bool IsConnected() const { return _am_logged_in; }
    void BridgePresence(const gloox::JID& jid, gloox::Presence::PresenceType type);
    bool CanSendTo(const std::string& recipient) const;
    bool BridgeReady() const { return fBridgeReady; }
	uint64 ConnectionGeneration() const { return fConnectionGeneration.load(); }
	void                     SendSubscriptionRequest(std::string username); 
	void                     SendUnsubscriptionRequest(std::string username);
	void                     SetFriendlyName(const gloox::JID& who, BString name);
	void                     RemoveFromRoster(const UserID *removed_user);
	void                     SendPresence(gloox::Presence::PresenceType = gloox::Presence::Probe,
								std::string status = "");
	void                     SendLastPresence();
	void	                 SendGroupUnvitation(std::string _group_room,
								std::string _group_username);

	void                     _SendUserRegistration(std::string username, std::string password,
								std::string resource);
	void	                 UnregisterWithAgent(std::string agent);

	void					RequestVCard(const gloox::JID& jid);
	void					RequestPubSubItem(const gloox::JID& jid,
								const std::string& node, const std::string& subid,
								gloox::PubSub::ResultHandler* handler);

	void					DiscoverMUCs(gloox::DiscoHandler* handler);

	// SELECTORS
	const std::string       CurrentRealName() const;

	// gloox::ConnectionListener
	void					onConnect() final;
	void					onDisconnect(gloox::ConnectionError e) final;
	bool					onTLSConnect(const gloox::CertInfo& info) final;

	// gloox::IqHandler
	bool					handleIq(const gloox::IQ&) final;
	void					handleIqID(const gloox::IQ&, int) final;

	// gloox::VCardHandler
	void					handleVCard(const gloox::JID& jid, const gloox::VCard* vcard) final;
	void					handleVCardResult(VCardContext context,
								const gloox::JID& jid, gloox::StanzaError se) final;

	// gloox::DiscoHandler
	void					handleDiscoInfo(const gloox::JID &from, const gloox::Disco::Info &info, int context) override;
	void					handleDiscoItems(const gloox::JID &from, const gloox::Disco::Items &items, int context) override;
	void					handleDiscoError(const gloox::JID &from, const gloox::Error *error, int context) override;

	gloox::Client* 			GlooxClient() { return fClient; }

	std::string QueueText(const std::string& recipient, const std::string& text,
        bool group, const BMessenger& target, const std::string& mention = "",
        const std::string& replyId = "", const std::string& replyTo = "", size_t fallbackEnd = 0);
	std::shared_ptr<Upload::Task> QueueUpload(const std::string& path,
		const std::string& recipient, bool group, const BMessenger& target, std::shared_ptr<Giphy::Transfer> mediaLease = {});

protected:
	// CREATORS
	                         JabberSpeak();
	                    
private:
	static int32            _SpawnConnectionThread(void *obj);
	void                    _ConnectionThread();
	bool                    _ReserveConnection();
	void                    _StartConnectionThread();
	void                    _ResetDisconnected();

	// OUTGOING COMMUNICATION
	void					_ProcessVersionRequest(void);

	// pointer to the singleton
	BlabberSettings        *_blabber_settings;
	
	// some information about the current login status
	std::string             _curr_realname;
	gloox::JID              _curr_login;
	std::string             _password;
	
	std::atomic_bool        _am_logged_in{false};
    std::atomic_bool fBridgeReady{false};
    std::string fBridgeDomain;
    std::string fSignalContactDomain, fSignalGroupDomain;
	bool                    _reconnecting = false;

	bool                    _got_some_agent_info = false;
	
	// communication state information
	IQMap                   _iq_map;

	// CREATORS
	static JabberSpeak      *_instance;

	// threading
	thread_id                _connection_thread_id = -1;

public:
    void QueueAvatar(const std::string& account,const std::string& jid);
    bool QueueArchive(const std::string& peer,bool group,const BMessenger& target,int64 since=0);
    bool HandleArchive(const gloox::Message& message);
    std::string QueueReaction(const std::string& recipient, const std::string& id,
        const std::string& emoji, bool group, const BMessenger& target);
private:
    struct TextRequest {
        std::string id, recipient, text, mention;
        bool group;
        BMessenger target;
        std::string reactionId;
        std::string replyId, replyTo;
        size_t fallbackEnd=0;
    };
    std::mutex fTextLock;
    std::deque<TextRequest> fTextQueue;
    std::deque<std::pair<std::string,std::string>> fAvatarQueue;
    void _SendQueuedText();
    void _FailQueuedText();
	std::mutex fUploadLock;
	std::shared_ptr<Upload::Manager> fUploadManager;
    std::shared_ptr<Chat::ArchiveManager> fArchiveManager;
	ConnectionLifecycle fConnectionState;
	std::atomic<uint64> fConnectionGeneration{0};
	gloox::Client*			fClient;
	gloox::Registration*	fRegistration = nullptr;
	gloox::VCardManager*	fVCardManager;
	gloox::PubSub::Manager*	fPubSubManager = nullptr;
	gloox::JID				fConferenceNode;
};

#endif
