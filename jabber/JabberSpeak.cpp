#include "config/PrivateProfile.h"
#include "network/DeliveryReceipt.h"
#include "network/ChatMarker.h"
#include "network/Reactions.h"
#include "network/Reply.h"
#include "network/Correction.h"
#include "support/ReplyDraft.h"
#include "network/references.h"
//////////////////////////////////////////////////
// Blabber [JabberSpeak.cpp]
//////////////////////////////////////////////////

#include <gloox/jid.h>
#include <gloox/capabilities.h>
#include <gloox/tlsdefault.h>
#include <stdexcept>
#include <gloox/carbons.h>
#include <gloox/connectiontcpclient.h>
#include <gloox/connectiontls.h>
#include <gloox/dataformitem.h>
#include <gloox/disco.h>
#include <gloox/dns.h>
#include <gloox/forward.h>
#include <gloox/pubsubevent.h>
#include <gloox/registration.h>
#include <gloox/rostermanager.h>
#include <gloox/xhtmlim.h>

#include "network/confirm.h"
#include "network/references.h"

#include <cstdio>
#include <Roster.h>
#include <unistd.h>
#include <sys/utsname.h>

#include "support/LogHandler.h"
#include "ui/ModalAlertFactory.h"

#include "BlabberApp.h"
#include "AgentList.h"
#include "GenericFunctions.h"
#include "JabberSpeak.h"
#include "ui/AvatarCache.h"
#include "JRoster.h"
#include "MessageRepeater.h"
#include "Messages.h"
#include "TalkManager.h"
#include "UserID.h"

#include <stdlib.h>

#include <AppFileInfo.h>
#include <Catalog.h>
#include <FindDirectory.h>
#include <File.h>
#include <Path.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "JabberSpeak"


JabberSpeak *JabberSpeak::_instance = NULL;

//////////////////////////////////////////////////
// CREATORS
//////////////////////////////////////////////////

// FIXME use the GlooxHandler instead for all XMPP things

JabberSpeak *JabberSpeak::Instance() {
	if (_instance == NULL) {
		_instance = new JabberSpeak();
	}

	return _instance;
}

JabberSpeak::JabberSpeak()
	: fClient(NULL)
	, fVCardManager(NULL)
{
	// grab a handle to the settings now for convenience later
	_blabber_settings = BlabberSettings::Instance();
}

JabberSpeak::~JabberSpeak() {
	_instance = NULL;

	delete fVCardManager;
}

void JabberSpeak::Reset() {
	// MainWindow may ask to reset immediately after Disconnect. Do not destroy
	// its sessions/maps while gloox can still be in a callback.
	if (!fConnectionState.IsIdle()) {
		SendDisconnect();
		return;
	}
	_ResetDisconnected();
}

bool JabberSpeak::HandleConnectionStopped(uint64 generation)
{
	// Only receipt of the published notice releases Drained. An ordinary Reset
	// must not acknowledge it before the worker has enqueued that notice.
	if (generation != ConnectionGeneration() || !fConnectionState.IsDrained()) return false;
	_ResetDisconnected();
	return true;
}

void JabberSpeak::_ResetDisconnected()
{
	BlabberMainWindow* window = BlabberMainWindow::Instance();
	if (window->Lock()) {
		window->CloseTalkViews();
		window->ShowLogin();
		window->Unlock();
	}
	BookmarkManager::Instance().Disconnect();
	TalkManager::Instance()->Reset();
	JRoster::Instance()->Lock();
	JRoster::Instance()->RemoveAllUsers();
	JRoster::Instance()->Unlock();
	JRoster::Instance()->RefreshRoster();
	AgentList::Instance()->RemoveAllAgents();
	fConferenceNode = gloox::JID();
	_iq_map.clear();
	_got_some_agent_info = false;
    // Runtime and UI callbacks have drained. Destroy client-dependent helpers
    // before the client, instead of leaking one full runtime per reconnect.
    {
        std::lock_guard<std::mutex> lock(fUploadLock);
        fUploadManager.reset();
    }
    delete fRegistration; fRegistration = nullptr;
    delete fVCardManager; fVCardManager = nullptr;
    delete fPubSubManager; fPubSubManager = nullptr;
    delete fClient; fClient = nullptr;
    fBridgeReady = false;
	fConnectionState.AcknowledgeCleanup();
}

void JabberSpeak::JabberSpeakReset() {
	_curr_realname           = "";
	_curr_login              = gloox::JID();
	_password                = "";
	_am_logged_in            = false;
	_reconnecting            = false;
	_got_some_agent_info     = false;

	_iq_map.clear();
}

//////////////////////////////////////////////////
// STANDARD METHODS
//////////////////////////////////////////////////

const std::string JabberSpeak::CurrentRealName() const {
	return _curr_realname;
}

//////////////////////////////////////////////////
// OUTGOING COMMUNICATION
//////////////////////////////////////////////////

void JabberSpeak::SendConnect(gloox::JID username, std::string password, std::string realname,
	bool suppress_auto_connect)
{
    // Credentials and auto-login are resolved asynchronously by MainWindow.
    // Never ask the keyring (or wait for its OS dialog) on a caller's UI thread.
    (void)suppress_auto_connect;
    if (!username || password.empty() || username.server().empty()) return;

	if (!_ReserveConnection()) return;
	// save username/password
	_curr_realname = realname;
	_curr_login    = username;
	_password      = password;

	_StartConnectionThread();
}

void JabberSpeak::SendConnect(std::string realname)
{
	if (!_ReserveConnection()) return;
	_curr_realname = realname;
	_curr_login    = gloox::JID();
	_password      = "";

	_StartConnectionThread();
}


bool JabberSpeak::_ReserveConnection()
{
	// Duplicate login messages cannot replace a live runtime. If stopping, the
	// pending stopped notice restores the login screen once cleanup completes.
	if (!fConnectionState.TryStart()) return false;
    fBridgeReady = false;
    fBridgeDomain = _blabber_settings->Data("signal-bridge-jid").value_or(PrivateProfile::kSignalDomain);
    fSignalContactDomain = _blabber_settings->Data("signal-contact-domain").value_or(PrivateProfile::kSignalDomain);
    fSignalGroupDomain = _blabber_settings->Data("signal-group-domain").value_or(PrivateProfile::kSignalDomain);
	++fConnectionGeneration;
	return true;
}


void JabberSpeak::_StartConnectionThread()
{
	_connection_thread_id = spawn_thread(_SpawnConnectionThread,
		"connection_listener", B_LOW_PRIORITY, this);
	if (_connection_thread_id >= 0 && resume_thread(_connection_thread_id) == B_OK) return;
	if (_connection_thread_id >= 0) kill_thread(_connection_thread_id);
	BMessage error;
	error.AddString("hint", "Could not start the connection thread. Try again.");
	SendNotices(kConnectionRefused, &error);
	BMessage stopped(kConnectionStopped);
	stopped.AddUInt64("connection-generation", ConnectionGeneration());
	fConnectionState.MarkDrained();
	BlabberMainWindow::Instance()->PostMessage(&stopped);
}

int32 JabberSpeak::_SpawnConnectionThread(void *obj)
{
	auto* self = static_cast<JabberSpeak*>(obj);
	try {
		self->_ConnectionThread();
	} catch (const std::exception&) {
		BMessage error;
		error.AddString("hint", "Connection stopped unexpectedly. Please reconnect.");
		self->SendNotices(kConnectionRefused, &error);
	}
	{
		std::lock_guard<std::mutex> lock(self->fUploadLock);
		if (self->fUploadManager) self->fUploadManager->Disconnected();
	}
	self->_am_logged_in = false;
	self->_FailQueuedText();
	BMessage stopped(kConnectionStopped);
	stopped.AddUInt64("connection-generation", self->ConnectionGeneration());
	self->fConnectionState.MarkDrained();
	// No self/client access after posting. UI cleanup may then finish app quit.
	BlabberMainWindow::Instance()->PostMessage(&stopped);
	return 0;
}


void JabberSpeak::_ConnectionThread()
{
	if (fConnectionState.StopRequested()) return;
	std::string domain;
	if (_curr_login == "") {
		domain = "anon.jabberfr.org";
		fClient = new gloox::Client("anon.jabberfr.org");
		fClient->setSASLMechanisms(gloox::SaslMechAnonymous);
	} else {
		gloox::JID jid(_curr_login);
		domain = jid.server();
		fClient = new gloox::Client(jid, _password);
	}

	// Each connection owns its manager and client, including across reconnects.
	gloox::Client* connection = fClient;
	auto uploads = std::make_shared<Upload::Manager>(*connection, Upload::Profile::Load());
    auto archives = std::make_shared<Chat::ArchiveManager>(*connection);
	{
		std::lock_guard<std::mutex> lock(fUploadLock);
		fUploadManager = uploads;
        fArchiveManager = archives;
	}

	// Prepare for handling carbons
	fClient->registerStanzaExtension(new Chat::Receipt());
    fClient->registerStanzaExtension(new Chat::Marker());
    fClient->disco()->addFeature("urn:xmpp:chat-markers:0");
    fClient->registerStanzaExtension(new Chat::Reactions());
    fClient->registerStanzaExtension(new Chat::Correction());
    fClient->disco()->addFeature("urn:xmpp:reactions:0");
    fClient->disco()->addFeature("urn:xmpp:message-correct:0");
    fClient->registerStanzaExtension(new Chat::StableId());
	fClient->registerStanzaExtension(new gloox::Forward());
	fClient->registerStanzaExtension(new gloox::Carbons());

	for (auto& ext: fClient->presenceExtensions()) {
		gloox::Capabilities* caps = const_cast<gloox::Capabilities*>(dynamic_cast<const gloox::Capabilities*>(ext));
		if (caps) {
			caps->setNode("https://github.com/kambuziarendal/MimisSendir");
			break;
		}
	}

	// Register for roster events
	fClient->rosterManager()->registerRosterListener(JRoster::Instance());

	// Raw XMPP debug logging would expose upload bearer headers, signed URLs and
	// private conversations. Do not register an XML logger in the private client.

	// Register for connection events
	fClient->registerConnectionListener(this);

	// Register for incoming chat sessions
	fClient->registerMessageSessionHandler(TalkManager::Instance());
    // gloox 1.0.28 has no Error session-handler slot. A message from an
    // existing Chat-only session falls back here, not to a new session.
    // ClientBase returns after session delivery, so this does not duplicate it.
    fClient->registerMessageHandler(TalkManager::Instance());

	// Setup for handling VCard
	fVCardManager = new gloox::VCardManager(fClient);

	// Prepare our answer to version requests for disco
	_ProcessVersionRequest();

	// By default gloox pretends to be a bot, let's say we're a PC instead
	fClient->disco()->setIdentity("client", "pc");

	// Set up for getting avatar notifications
	fClient->disco()->addFeature("urn:xmpp:avatar:metadata+notify");

	// XEP-0372 references (for user mentions)
	fClient->disco()->addFeature("urn:xmpp:reference:0");

	// Set up for getting XHTML-IM
	fClient->disco()->addFeature(gloox::XMLNS_XHTML_IM);

	// XEP-0402 PEP-Native bookmarks
	fClient->disco()->addFeature(XMLNS_PEP_BOOKMARKS "+notify");

	// XEP-0045 MUC
	fClient->disco()->addFeature("http://jabber.org/protocol/muc");

	fPubSubManager = new gloox::PubSub::Manager(fClient);
	fClient->registerStanzaExtension(new gloox::PubSub::Event());
	fClient->registerStanzaExtension(new gloox::XHtmlIM());
	fClient->registerStanzaExtension(new Reference());
	fClient->registerStanzaExtension(new Confirm());

	// An optional explicit endpoint supports private/test networks without SRV.
	// The JID remains the authentication/TLS identity. Never disable validation.
	auto* settings = BlabberSettings::Instance();
	// Explicit compatibility option for servers rejecting old gloox SCRAM.
	// Never select PLAIN on an unencrypted connection.
	if (_curr_login != "" && settings->Data("xmpp-auth-mechanism").value_or("") == "plain-over-tls") {
		fClient->setTls(gloox::TLSRequired);
		fClient->setSASLMechanisms(gloox::SaslMechPlain);
	}
	const auto caFile = settings->Data("xmpp-ca-file").value_or("");
	if (!caFile.empty()) fClient->setCACerts({caFile});
	const auto endpoint = settings->Data("xmpp-connect-host").value_or("");
	if (!endpoint.empty()) {
		int port = 5222;
		try {
			const auto text = settings->Data("xmpp-connect-port").value_or("5222");
			size_t used = 0;
			port = std::stoi(text, &used);
			if (used != text.size() || port < 1 || port > 65535) throw std::invalid_argument("port");
		} catch (const std::exception&) {
			BMessage error;
			error.AddString("hint", "The configured XMPP port must be between 1 and 65535.");
			SendNotices(kConnectionRefused, &error);
			uploads->Disconnected();
    archives->Disconnected();
			return;
		}
		fClient->setServer(endpoint);
		fClient->setPort(port);
		fClient->setTls(gloox::TLSRequired);
		// Verify the account domain rather than an override IP/route hostname.
		// gloox 1.0.28's GnuTLS init ignores its CA argument, so apply explicit
		// trust after initialization. TLS and certificate checks remain required.
		auto* tls = new gloox::TLSDefault(fClient, domain);
		if (!tls->init()) {
			delete tls;
			BMessage error;
			error.AddString("hint", "Could not initialize secure XMPP connection.");
			SendNotices(kConnectionRefused, &error);
			uploads->Disconnected();
    archives->Disconnected();
			return;
		}
		if (!caFile.empty()) tls->setCACerts({caFile});
		fClient->setEncryptionImpl(tls);
	} else {
		// XEP-0368 SRV records for XMPP over TLS
		gloox::DNS::HostMap hostmap = gloox::DNS::resolve("xmpps-client", "tcp", domain, fClient->logInstance());

		// gloox error handling for DNS::resolve is not great: it will reply with the default domain
		// and port if the specific DNS record asked does not exist. This would be likely fine for the
		// default xmpp-client query, but for xmpps-client it leads to trying to send TLS in the
		// unencrypted port.
		const auto& host = hostmap.begin();
		if (host != hostmap.end() && (host->first != domain || host->second != 5222)) {
			// We have a reply for SRV records, use direct-TLS
			printf("Connect to %s on port %d from SRV records\n", host->first.c_str(), host->second);
			fClient->setServer(host->first);
			fClient->setPort(host->second);

			gloox::ConnectionTCPClient* tcp = new gloox::ConnectionTCPClient(fClient->logInstance(), host->first, host->second);
			gloox::ConnectionTLS* connection = new gloox::ConnectionTLS(fClient, tcp, fClient->logInstance());
			fClient->setConnectionImpl(connection);
		} else {
			// Make sure the connection will be encrypted with starttls
			fClient->setTls(gloox::TLSRequired);
		}
	}

	// Pump new attachment requests on the same thread that owns gloox callbacks.
	if (connection->connect(false)) {
		while (true) {
			if (fConnectionState.StopRequested()) {
				connection->disconnect();
				break;
			}
			_SendQueuedText();
			uploads->Tick();
            archives->Tick();
			if (connection->recv(100000) != gloox::ConnNoError) break;
		}
	}
	uploads->Disconnected();
    archives->Disconnected();
}


void JabberSpeak::SendDisconnect() {
	_am_logged_in = false;
	fConnectionState.RequestStop();
}

void JabberSpeak::SendSubscriptionRequest(std::string username) {
	gloox::Subscription subscription(gloox::Subscription::Subscribe,
		gloox::JID(username));
	fClient->send(subscription);
}

void JabberSpeak::SendUnsubscriptionRequest(std::string username) {
	gloox::Subscription subscription(gloox::Subscription::Unsubscribe,
		gloox::JID(username));
	fClient->send(subscription);
}

void JabberSpeak::SetFriendlyName(const gloox::JID& who, BString friendlyName)
{
	gloox::RosterItem* item = fClient->rosterManager()->getRosterItem(who);
	if (item) {
		item->setName(friendlyName.String());
		fClient->rosterManager()->synchronize();
	} else {
		const gloox::ConferenceListItem* bookmark
			= BookmarkManager::Instance().GetBookmark(who.full().c_str());
		if (bookmark) {
			BookmarkManager::Instance().SetBookmark(who.full().c_str(),
				bookmark->nick.c_str(), friendlyName, bookmark->autojoin);
		} else {
			debugger("User not found.");
		}
	}
}

void JabberSpeak::RemoveFromRoster(const UserID *removed_user) {
	fClient->rosterManager()->remove(gloox::JID(removed_user->Handle()));
}


void JabberSpeak::SendPresence(gloox::Presence::PresenceType type, std::string status) {
	gloox::Presence presence(type, gloox::JID(), status);
	fClient->send(presence);
}


void JabberSpeak::SendLastPresence() {
	bool last_used_custom   = BlabberSettings::Instance()->Tag("last-used-custom-status");
	auto s = BlabberSettings::Instance()->Data("last-custom-exact-status");
	gloox::Presence::PresenceType last_status = (gloox::Presence::PresenceType)(s ? atoi(
		s->c_str()) : 0);
	auto last_custom_status = BlabberSettings::Instance()->Data("last-custom-more-exact-status");

	if (last_used_custom && last_custom_status) {
		SendPresence(last_status, *last_custom_status);
		BlabberMainWindow::Instance()->SetCustomStatus(*last_custom_status);
	} else {
		if (last_status == gloox::Presence::Chat) {
			BlabberMainWindow::Instance()->PostMessage(BLAB_AVAILABLE_FOR_CHAT);
		} else if (last_status == gloox::Presence::Away) {
			BlabberMainWindow::Instance()->PostMessage(BLAB_AWAY_TEMPORARILY);
		} else if (last_status == gloox::Presence::XA) {
			BlabberMainWindow::Instance()->PostMessage(BLAB_AWAY_EXTENDED);
		} else if (last_status == gloox::Presence::DND) {
			BlabberMainWindow::Instance()->PostMessage(BLAB_DO_NOT_DISTURB);
		} else {
			BlabberMainWindow::Instance()->PostMessage(BLAB_AVAILABLE_FOR_CHAT);
		}
	}
}

void JabberSpeak::SendGroupUnvitation(std::string _group_room, std::string _group_username) {
	// assemble group ID
	std::string group_presence = _group_room + "/" + _group_username;	

	// Send presence Stanza
	gloox::Presence presence(gloox::Presence::Unavailable, gloox::JID(group_presence));
	fClient->send(presence);

	// Disable autologin in bookmarks and store username
	const gloox::ConferenceListItem* item
		= BookmarkManager::Instance().GetBookmark(_group_room.c_str());
	if (item) {
		BookmarkManager::Instance().SetBookmark(_group_room.c_str(),
			_group_username.c_str(), item->name.c_str(), false);
	}
}

void JabberSpeak::UnregisterWithAgent(std::string agent) {
	// Find registration in roster
	JRoster::Instance()->Lock();

	UserID *user = NULL;
	if (AgentList::Instance()->GetAgentByService(agent)) {
		user = JRoster::Instance()->FindUser(JRoster::TRANSPORT_ID, AgentList::Instance()->GetAgentByService(agent)->JID());
	}

	// call RemoveFromRoster()
	if (user) {
		RemoveFromRoster(user);
	}

	JRoster::Instance()->Unlock();
}


void JabberSpeak::RequestVCard(const gloox::JID& jid)
{
	fVCardManager->fetchVCard(jid, this);
}


void
JabberSpeak::RequestPubSubItem(const gloox::JID& jid, const std::string& node,
	const std::string& subid, gloox::PubSub::ResultHandler* handler)
{
	fPubSubManager->requestItems(jid, node, subid, 1, handler);
}


void JabberSpeak::_ProcessVersionRequest(void) {
	std::string strVersion("Haiku");
	BPath path;
	if (find_directory(B_BEOS_LIB_DIRECTORY, &path) == B_OK) {
		path.Append("libbe.so");

		BAppFileInfo appFileInfo;
		version_info versionInfo;
		BFile file;
		if (file.SetTo(path.Path(), B_READ_ONLY) == B_OK
			&& appFileInfo.SetTo(&file) == B_OK
			&& appFileInfo.GetVersionInfo(&versionInfo, 
				B_APP_VERSION_KIND) == B_OK
			&& versionInfo.short_info[0] != '\0')
				strVersion = versionInfo.short_info;

		if (strVersion == "Walter")
			strVersion = "Haiku";

		BString appVersion;
		appVersion << " " << versionInfo.major << "." << versionInfo.middle;
		if (versionInfo.minor > 0)
			appVersion << "." << versionInfo.minor;

		// Add the version variety
		const char* variety = NULL;
		switch (versionInfo.variety) {
			case B_DEVELOPMENT_VERSION:
				variety = "development";
				break;
			case B_ALPHA_VERSION:
				variety = "alpha";
				break;
			case B_BETA_VERSION:
				variety = "beta";
				break;
			case B_GAMMA_VERSION:
				variety = "gamma";
				break;
			case B_GOLDEN_MASTER_VERSION:
				variety = "gold master";
				break;
		}

		if (variety)
			appVersion << "-" << variety;

		strVersion += appVersion;
	}

	fClient->disco()->setVersion("MímisSendir", APP_VERSION, strVersion);
}

void
JabberSpeak::onConnect()
{
	if (fConnectionState.StopRequested()) return;
	_am_logged_in = true;
	{
		std::lock_guard<std::mutex> lock(fUploadLock);
		if (fUploadManager) fUploadManager->Connected();
	}

	fprintf(stderr, "Logged in!\n");
	MessageRepeater::Instance()->PostMessage(JAB_LOGGED_IN);
    if (!fBridgeDomain.empty()) {
        gloox::Presence probe(gloox::Presence::Probe, gloox::JID(fBridgeDomain));
        fClient->send(probe);
    }
	//SendLastPresence();


	if (_curr_login == "") {
		// Anonymous users choose rooms themselves; never auto-join upstream support.

		GlooxClient()->disco()->getDiscoItems(gloox::JID("anon.jabberfr.org"), "", this,
			kDiscoveryServer);
	} else {
		// Logged in with account: get own discovery items to see if we should be using bookmarks
		// v1 or v2.
		GlooxClient()->disco()->getDiscoInfo(_curr_login, "", this, kDiscoveryOwnJID);
		GlooxClient()->disco()->getDiscoItems(_curr_login.server(), "", this, kDiscoveryServer);
	}

	// Enable message carbons
	gloox::IQ iq(gloox::IQ::Set, gloox::JID());
	iq.addExtension(new gloox::Carbons(gloox::Carbons::Enable));
	fClient->send(iq, this, 1);

	_reconnecting = false;
}


void
JabberSpeak::onDisconnect(gloox::ConnectionError e)
{
	{
		std::lock_guard<std::mutex> lock(fUploadLock);
		if (fUploadManager) fUploadManager->Disconnected();
	}


	BookmarkManager::Instance().Disconnect();
	if (e == gloox::ConnAuthenticationFailed) {
		gloox::AuthenticationError ae = fClient->authError();

		if (ae == gloox::SaslNotAuthorized) {
			// Incorrect login or password, highlight them in main window
			// The code below gets us back to login screen
			SendNotices(kAuthenticationFailed);
		} else {
			fprintf(stderr, "%s(ConnAuthenticationFailed) -> AuthenticationError: %d\n",
				__PRETTY_FUNCTION__, ae);
			BMessage message;
			message.AddString("hint", "server authentication failed");
			SendNotices(kConnectionRefused, &message);
		}
	} else if (e == gloox::ConnDnsError) {
		BMessage message;
		message.AddString("hint", "server name could not be resolved; check DNS and network access");
		SendNotices(kConnectionRefused, &message);
	} else if (e == gloox::ConnConnectionRefused) {
		BMessage message;
		message.AddString("hint", "make sure your internet access is online");
		SendNotices(kConnectionRefused, &message);
	} else if (e == gloox::ConnTlsFailed || e == gloox::ConnTlsNotAvailable) {
		BMessage message;
		message.AddString("hint", "impossible to set up a secure connection with the server");
		SendNotices(kConnectionRefused, &message);
	} else {
		fprintf(stderr, "%s(%d)\n", __PRETTY_FUNCTION__, e);
		if (e != gloox::ConnUserDisconnected && e != gloox::ConnNoError
			&& !fConnectionState.StopRequested()) {
			BMessage message;
			message.AddString("hint", "connection interrupted; check network access and reconnect");
			SendNotices(kConnectionRefused, &message);
		}
	}

	_am_logged_in = false;
	// Manual reconnect only. The wrapper drains this runtime and asks the UI
	// to clean old sessions before another connection can acquire the gate.

}

bool
JabberSpeak::onTLSConnect(const gloox::CertInfo& info)
{
	// Certificate is verified as valid by gloox already
	return info.status == gloox::CertOk;
}

bool
JabberSpeak::handleIq(const gloox::IQ&)
{
	return true;
}


void
JabberSpeak::handleIqID(const gloox::IQ&, int)
{
}


//#pragma mark - VCardHandler


void
JabberSpeak::handleVCard(const gloox::JID& jid, const gloox::VCard* vcard)
{
	if(!vcard)return;
    // Own-vCard replies may omit the from JID or carry our resource.
    const auto avatarJid=jid.bare().empty()||jid.bare()==fClient->jid().bare()?fClient->jid().bare():jid.full();
    Avatars::Receive(fClient->jid().bare(),avatarJid,vcard->photo().binval);
	BMessage message;
	message.AddString("jid", jid.bare().c_str());

	if (!vcard->formattedname().empty())
		message.AddString("Name", vcard->formattedname().c_str());
	if (!vcard->nickname().empty())
		message.AddString("Nickname", vcard->nickname().c_str());
	if (!vcard->url().empty())
		message.AddString("url", vcard->url().c_str());
	if (!vcard->bday().empty())
		message.AddString("Birhtday", vcard->bday().c_str());
	if (!vcard->title().empty())
		message.AddString("Title", vcard->title().c_str());
	if (!vcard->role().empty())
		message.AddString("Role", vcard->role().c_str());
	if (!vcard->note().empty())
		message.AddString("Note", vcard->note().c_str());
	if (!vcard->desc().empty())
		message.AddString("Description", vcard->desc().c_str());
	if (!vcard->mailer().empty())
		message.AddString("Mailer", vcard->mailer().c_str());
	if (!vcard->rev().empty())
		message.AddString("Revision", vcard->rev().c_str());
	if (!vcard->uid().empty())
		message.AddString("User ID", vcard->uid().c_str());
	if (!vcard->tz().empty())
		message.AddString("Timezone", vcard->tz().c_str());
	if (!vcard->sortstring().empty())
		message.AddString("Sort string", vcard->sortstring().c_str());
	if (!vcard->org().name.empty())
		message.AddString("Organization", vcard->org().name.c_str());
	for (std::string unit: vcard->org().units)
		message.AddString("Unit", unit.c_str());

	// Things not meant for direct display as strings
	for (gloox::VCard::Email email: vcard->emailAddresses()) {
		message.AddString("mail:address", email.userid.c_str());
		int32 flags = 0;
		if (email.pref)
			flags |= 1;
		if (email.home)
			flags |= 2;
		if (email.work)
			flags |= 4;
		if (email.internet)
			flags |= 8;
		if (email.x400)
			flags |= 16;
		message.AddInt32("mail:flags", flags);
	}

	for (gloox::VCard::Telephone tel: vcard->telephone()) {
		message.AddString("phone:number", tel.number.c_str());
		int32 flags = 0;
		if (tel.pref)
			flags |= 1;
		if (tel.home)
			flags |= 2;
		if (tel.work)
			flags |= 4;
		if (tel.voice)
			flags |= 8;
		if (tel.fax)
			flags |= 16;
		if (tel.pager)
			flags |= 32;
		if (tel.msg)
			flags |= 64;
		if (tel.cell)
			flags |= 128;
		if (tel.video)
			flags |= 256;
		if (tel.bbs)
			flags |= 512;
		if (tel.modem)
			flags |= 1024;
		if (tel.isdn)
			flags |= 2048;
		if (tel.pcs)
			flags |= 4096;
		message.AddInt32("phone:flags", flags);
	}

	for (gloox::VCard::Label label: vcard->labels()) {
		BString str;
		for (auto line: label.lines)
			str << line.c_str() << "\n";
		message.AddString("label:lines", str);

		int32 flags = 0;
		if (label.pref)
			flags |= 1;
		if (label.home)
			flags |= 2;
		if (label.work)
			flags |= 4;
		if (label.postal)
			flags |= 8;
		if (label.parcel)
			flags |= 16;
		if (label.dom)
			flags |= 32;
		if (label.intl)
			flags |= 64;
		message.AddInt32("label:flags", flags);
	}

	for (gloox::VCard::Address address: vcard->addresses()) {
		// Allow empty strings because we don't want to mix different addresses
		// and all we get to differenciate them is the offset in the BMessage
		message.AddString("address:pobox", address.pobox.c_str());
		message.AddString("address:extadd", address.extadd.c_str());
		message.AddString("address:street", address.street.c_str());
		message.AddString("address:locality", address.locality.c_str());
		message.AddString("address:region", address.region.c_str());
		message.AddString("address:postcode", address.pcode.c_str());
		message.AddString("address:country", address.ctry.c_str());

		int32 flags = 0;
		if (address.pref)
			flags |= 1;
		if (address.home)
			flags |= 2;
		if (address.work)
			flags |= 4;
		if (address.postal)
			flags |= 8;
		if (address.parcel)
			flags |= 16;
		if (address.dom)
			flags |= 32;
		if (address.intl)
			flags |= 64;
		message.AddInt32("address:flags", flags);
	}

	if (!vcard->geo().latitude.empty())
		message.AddString("geo:lat", vcard->geo().latitude.c_str());
	if (!vcard->geo().longitude.empty())
		message.AddString("geo:lon", vcard->geo().longitude.c_str());
	message.AddInt32("classification", vcard->classification());

	if (!vcard->photo().binval.empty()) {
		message.AddData("photo:binval", B_RAW_TYPE,
			vcard->photo().binval.c_str(), vcard->photo().binval.length());
	}
	if (!vcard->photo().extval.empty())
		message.AddString("photo:url", vcard->photo().extval.c_str());
	if (!vcard->photo().type.empty()) {
		message.AddData("photo:type", B_MIME_TYPE,
			vcard->photo().type.c_str(), vcard->photo().type.length());
	}
	if (!vcard->logo().binval.empty()) {
		message.AddData("logo:binval", B_RAW_TYPE,
			vcard->logo().binval.c_str(), vcard->logo().binval.length());
	}
	if (!vcard->logo().extval.empty())
		message.AddString("logo:url", vcard->logo().extval.c_str());
	if (!vcard->logo().type.empty()) {
		message.AddData("logo:type", B_MIME_TYPE,
			vcard->logo().type.c_str(), vcard->logo().type.length());
	}

	SendNotices(kVCardReceived, &message);
}


void JabberSpeak::DiscoverMUCs(gloox::DiscoHandler* handler)
{
	GlooxClient()->disco()->getDiscoItems(fConferenceNode, "", handler, 0);
}


void
JabberSpeak::handleVCardResult(VCardContext,
	const gloox::JID&, gloox::StanzaError)
{
	puts(__func__);
}


void
JabberSpeak::handleDiscoInfo(const gloox::JID &from, const gloox::Disco::Info &info, int context)
{
	if (context == kDiscoveryOwnJID)
		BookmarkManager::Instance().Connect(info.hasFeature("urn:xmpp:bookmarks:1#compat"));
	else if (context == kDiscoveryServer) {
		if (info.hasFeature("http://jabber.org/protocol/muc") && info.hasFeature("http://jabber.org/protocol/disco#items")) {
			// Ignore MUC handlers that don't have disco#items, since the main use we have is listing
			// chatrooms. This allows to ignore for example Biboumi gateways.
			fConferenceNode = from;
		} else {
			// TODO discover HTTP upload server, bytestreams proxy, anything else the server may have to offer
			printf("%s - form %p\n", __PRETTY_FUNCTION__, info.form());
			for (auto i: info.features()) {
				printf("    Feature: %s\n", i.c_str());
			}
			for (auto i: info.identities()) {
				printf("    Identity: category %s, type %s, name %s\n", i->category().c_str(), i->type().c_str(), i->name().c_str());
			}
		}
	} else {
		puts(__PRETTY_FUNCTION__);
	}
}


void
JabberSpeak::handleDiscoItems(const gloox::JID &from, const gloox::Disco::Items &items, int context)
{
	(void)from;
	if (context != kDiscoveryServer) {
		puts(__PRETTY_FUNCTION__);
		return;
	}

	for (auto item: items.items()) {
		GlooxClient()->disco()->getDiscoInfo(item->jid(), "", this, kDiscoveryServer);
	}
}


void
JabberSpeak::handleDiscoError(const gloox::JID &from, const gloox::Error *error, int context)
{
	puts(__func__);
	(void)from;
	(void)error;
	(void)context;
}


std::shared_ptr<Upload::Task> JabberSpeak::QueueUpload(const std::string& path,
	const std::string& recipient, bool group, const BMessenger& target, std::shared_ptr<Giphy::Transfer> mediaLease)
{
	std::lock_guard<std::mutex> lock(fUploadLock);
	if (!fUploadManager) return {};
	return fUploadManager->Queue(path, recipient, group, target, std::move(mediaLease));
}

std::string JabberSpeak::QueueText(const std::string& recipient,
    const std::string& text, bool group, const BMessenger& target, const std::string& mention,
    const std::string& replyId, const std::string& replyTo, size_t fallbackEnd, const std::string& replaceId,
    const std::function<bool(const std::string&)>& prepare)
{
    static std::atomic<uint64_t> sequence{0};
    std::lock_guard<std::mutex> lock(fTextLock);
    if (!CanSendTo(recipient) || text.empty() || text.size() > 65536 || fTextQueue.size() >= 32
        || replaceId.size()>1024 || (!replaceId.empty() && !replyId.empty()) || replyId.size()>1024 || replyTo.size()>3071 || fallbackEnd>Chat::Codepoints(text))
        return {};
    const auto id = "renga-" + std::to_string(system_time()) + "-"
        + std::to_string(++sequence);
    if(prepare && !prepare(id))return {};
    fTextQueue.push_back({id, recipient, text, mention, group, target, "", replyId, replyTo, fallbackEnd, replaceId});
    return id;
}

std::string JabberSpeak::QueueReaction(const std::string& recipient, const std::string& id,
    const std::string& emoji, bool group, const BMessenger& target)
{
    static std::atomic<uint64_t> sequence{0};
    std::lock_guard<std::mutex> lock(fTextLock);
    if(!CanSendTo(recipient) || id.empty() || id.size()>1024 || emoji.size()>64
        || emoji.find_first_of("\r\n\t")!=std::string::npos || fTextQueue.size()>=32)return {};
    auto request="reaction-"+std::to_string(system_time())+"-"+std::to_string(++sequence);
    fTextQueue.push_back({request,recipient,emoji,"",group,target,id,"","",0,""});return request;
}

bool JabberSpeak::QueueDisplayed(const std::string& recipient, const std::string& id, bool group) {
    std::lock_guard<std::mutex> lock(fTextLock);
    if (!CanSendTo(recipient) || fConnectionState.StopRequested() || id.empty() || id.size() > 1024
        || fDisplayedQueue.size() >= 64) return false;
    fDisplayedQueue.push_back({recipient, id, group});
    return true;
}

void JabberSpeak::QueueAvatar(const std::string& account,const std::string& jid) {
    std::lock_guard<std::mutex> lock(fTextLock);
    if (account.empty() || jid.empty() || jid.size() > 2048) return;
    const auto request = std::make_pair(account,jid);
    if(fAvatarQueue.size()<512 && std::find(fAvatarQueue.begin(),fAvatarQueue.end(),request)==fAvatarQueue.end()) fAvatarQueue.push_back(request);
}

void JabberSpeak::_SendQueuedText()
{
    std::deque<TextRequest> queued;
    std::deque<DisplayedRequest> displayed;
    std::deque<std::pair<std::string,std::string>> avatars;
    {
        std::lock_guard<std::mutex> lock(fTextLock);
        queued.swap(fTextQueue);
        displayed.swap(fDisplayedQueue);
        // Do not consume Signal avatar requests before the bridge is ready.
        // Rotate deferred entries so they cannot block ordinary XMPP contacts.
        for (size_t n=fAvatarQueue.size(); IsConnected() && n>0 && avatars.size()<4; --n) {
            auto request=std::move(fAvatarQueue.front());fAvatarQueue.pop_front();
            if(request.first!=fClient->jid().bare())continue;
            if(CanSendTo(request.second))avatars.push_back(std::move(request));
            else fAvatarQueue.push_back(std::move(request));
        }
    }
    for(const auto& request:avatars)if(request.first==fClient->jid().bare())fVCardManager->fetchVCard(gloox::JID(request.second),this);
    for (const auto& request : displayed) {
        if (!CanSendTo(request.recipient) || fConnectionState.StopRequested()) continue;
        gloox::Message message(request.group ? gloox::Message::Groupchat : gloox::Message::Chat,
            gloox::JID(request.recipient));
        message.addExtension(new Chat::Marker(request.id));
        fClient->send(message);
    }
    for (const auto& request : queued) {
        const bool connected = CanSendTo(request.recipient) && !fConnectionState.StopRequested();
        if (connected) {
            gloox::Message message(request.group ? gloox::Message::Groupchat : gloox::Message::Chat,
                gloox::JID(request.recipient), request.reactionId.empty() ? request.text : "");
            message.setID(request.id);
            if (!request.reactionId.empty()) message.addExtension(new Chat::Reactions(request.reactionId,
                request.text.empty() ? std::vector<std::string>{} : std::vector<std::string>{request.text}));
            else if (!request.group) message.addExtension(new Chat::Receipt(true));
            if (request.reactionId.empty()) message.addExtension(Chat::Marker::Request());
            if(!request.replaceId.empty()) message.addExtension(new Chat::Correction(request.replaceId));
            if(!request.replyId.empty()) {
                message.addExtension(new Chat::Reply(request.replyId,request.replyTo));
                message.addExtension(new Chat::ReplyFallback(request.fallbackEnd));
            }
            if (!request.mention.empty()) message.addExtension(new Reference("mention", request.mention.c_str()));
            fClient->send(message);
        }
        BMessage notice(connected ? kMessageSubmitted : kMessageDeliveryError);
        notice.AddString("id", request.id.c_str());
        notice.AddInt32("condition", gloox::StanzaErrorRecipientUnavailable);
        request.target.SendMessage(&notice, static_cast<BHandler*>(nullptr), 0);
    }
}

void JabberSpeak::_FailQueuedText()
{
    std::deque<TextRequest> queued;
    {
        std::lock_guard<std::mutex> lock(fTextLock);
        queued.swap(fTextQueue);
        fDisplayedQueue.clear();
        fAvatarQueue.clear();
    }
    for (const auto& request : queued) {
        BMessage notice(kMessageDeliveryError);
        notice.AddString("id", request.id.c_str());
        notice.AddInt32("condition", gloox::StanzaErrorRecipientUnavailable);
        request.target.SendMessage(&notice, static_cast<BHandler*>(nullptr), 0);
    }
}

bool JabberSpeak::CanSendTo(const std::string& recipient) const
{
    const auto domain = gloox::JID(recipient).server();
    return IsConnected() && ((domain != fSignalContactDomain && domain != fSignalGroupDomain) || fBridgeReady);
}

void JabberSpeak::BridgePresence(const gloox::JID& jid, gloox::Presence::PresenceType type)
{
    if (jid.bare() != fBridgeDomain) return;
    const bool wasReady = fBridgeReady;
    fBridgeReady = type == gloox::Presence::Chat;
    if(fBridgeReady && !wasReady) Avatars::RefreshAccount(fClient->jid().bare());
    BMessage notice(kBridgeStatus);
    notice.AddBool("ready", fBridgeReady);
    notice.AddUInt64("generation", ConnectionGeneration());
    BlabberMainWindow::Instance()->PostMessage(&notice);
}

bool JabberSpeak::QueueArchive(const std::string& peer,bool group,const BMessenger& target,int64 since) {
    std::lock_guard<std::mutex> lock(fUploadLock);
    return fArchiveManager && fArchiveManager->Queue(peer,group,target,since);
}
bool JabberSpeak::HandleArchive(const gloox::Message& message) {
    return fArchiveManager && fArchiveManager->Handle(message);
}
