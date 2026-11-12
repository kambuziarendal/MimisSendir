#include "network/BookmarkManager.h"
#include "network/DeliveryReceipt.h"
#include "network/ChatMarker.h"
#include "network/MessagePresentation.h"
#include "network/AttachmentMetadata.h"
#include "network/UploadManager.h"
//////////////////////////////////////////////////
// Blabber [TalkManager.cpp]
//////////////////////////////////////////////////

#include <gloox/base64.h>
#include <gloox/carbons.h>
#include <gloox/instantmucroom.h>
#include <gloox/pubsubevent.h>
#include <gloox/pubsubitem.h>
#include <gloox/rostermanager.h>
#include <gloox/xhtmlim.h>

#include "network/Extensions.h"
#include "network/confirm.h"
#include "network/references.h"

#include <cstdio>
#include <set>
#include <sstream>
#include <stdexcept>

#include <Application.h>
#include <Beep.h>
#include <Catalog.h>
#include <File.h>
#include <FindDirectory.h>
#include <Notification.h>
#include <Path.h>
#include <Url.h>
#include <interface/Window.h>

#include "ui/ModalAlertFactory.h"
#include "ui/Xhtml.h"

#include "BlabberSettings.h"
#include "JabberSpeak.h"
#include "MessageRepeater.h"
#include "Messages.h"
#include "SoundSystem.h"
#include "TalkManager.h"
#include "ui/AvatarCache.h"
#include <gloox/delayeddelivery.h>
#include "support/ConversationStore.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "TalkManager"


TalkManager *TalkManager::_instance = NULL;

TalkManager *TalkManager::Instance() {
	if (_instance == NULL) {
		_instance = new TalkManager();
		be_app->AddHandler(_instance);
	}

	return _instance;
}

TalkManager::TalkManager()
{
	// Create/open the avatars cache directory
	BPath path;
	find_directory(B_USER_CACHE_DIRECTORY, &path);
	BDirectory cacheRoot(path.Path());
	if (!cacheRoot.Contains("MimisSendir")) {
		BDirectory temp;
		cacheRoot.CreateDirectory("MimisSendir", &temp);
		if (!temp.Contains("avatars")) {
			temp.CreateDirectory("avatars", &fAvatarCache);
		}
	}

	fAvatarCache.SetTo(&cacheRoot, "MimisSendir/avatars");
}

TalkManager::~TalkManager() {
	_instance = NULL;
}

void
TalkManager::CreateTalkSession(const gloox::JID* user, gloox::MessageSession* session, bool sound_on_new)
{
	if (!JabberSpeak::Instance()->IsConnected()) return;
	TalkView *window = NULL;

	// This code for when we get called from clicking a user in the roster.
	// There is possibly already a session but the roster doesn't know about it
	// so we have to search it (in a quite inefficient way).
	// FIXME remove all this junk and have each TalkWindow be a SessionHandler
	// for its own session instead of centralizing everything here.
	const bool activate = session == nullptr;
	gloox::JID fullJID = *user;
	if (session == NULL) {
		gloox::RosterManager* rm
			= JabberSpeak::Instance()->GlooxClient()->rosterManager();
		gloox::RosterItem* ri = rm->getRosterItem(fullJID);
		if (ri && !ri->resources().empty()) {
			std::string res = ri->resources().begin()->first;
			fullJID.setResource(res);
		}

		for (TalkMap::iterator i = fTalkMap.begin();
				i != fTalkMap.end(); ++i) {
			if ((*i).first->target().bare() == user->bare()) {
				session = (*i).first;
				break;
			}
		}
	}

	if (session && fTalkMap.find(session) != fTalkMap.end()) {
		window = fTalkMap.at(session);
	} else {
		// Actually create the session, there isn't one matching.
		if (session == NULL) {
			session = new gloox::MessageSession(
					JabberSpeak::Instance()->GlooxClient(), fullJID);
		} else {
			// It's a freshly created session for an incoming message,
			// we just need to create the matching window
		}

		// create a new window
		window = new TalkView(user, "", "", session);
		RememberView(window);

		if (sound_on_new) {
			system_beep(kSoundNewMessage);
		}
	
		// add it to the known list
		fTalkMap[session] = window;
		BlabberMainWindow::Instance()->PostMessage(kWindowList);
		session->registerMessageHandler(this);
	}

	BMessage message(kAddTalkView);
	message.AddBool("activate", activate);
	message.AddPointer("view", window);
	message.AddUInt64("view-token", ViewToken(window));
	message.AddUInt64("connection-generation", JabberSpeak::Instance()->ConnectionGeneration());
	BlabberMainWindow::Instance()->PostMessage(&message);
}


void
TalkManager::JoinRoom(const std::string& group_room, const std::string& group_username)
{
	if (!JabberSpeak::Instance()->IsConnected()) return;
	TalkView *window = NULL;

	// is there a window already?
	if (IsExistingWindowToGroup(group_room))
		window = fGroupMap.at((gloox::MUCRoom*)IsExistingWindowToGroup(group_room));
	else {
		// create a new window
		window = new TalkView(nullptr, group_room, group_username, NULL);
		RememberView(window);

		gloox::JID jid(group_room);
		jid.setResource(group_username);
		gloox::MUCRoom* room = new gloox::InstantMUCRoom(
				JabberSpeak::Instance()->GlooxClient(), jid, this);

		room->setRequestHistory(25, gloox::MUCRoom::HistoryMaxStanzas);
		// add it to the known list
		fGroupMap[room] = window;

		// AttachedToWindow starts the join after the view has a valid looper
		// and its presence observers are registered. Fast replies must not be lost.
		BlabberMainWindow::Instance()->PostMessage(kWindowList);
	}

	BMessage message(kAddTalkView);
	message.AddPointer("view", window);
	message.AddUInt64("view-token", ViewToken(window));
	message.AddUInt64("connection-generation", JabberSpeak::Instance()->ConnectionGeneration());
	BlabberMainWindow::Instance()->PostMessage(&message);
}


void TalkManager::LeaveRoom(const gloox::JID& jid, std::string leaveMessage)
{
	std::string group_room = jid.bare();
	gloox::MUCRoom* room = (gloox::MUCRoom*)IsExistingWindowToGroup(group_room);
	if (room) {
		BlabberMainWindow::Instance()->PostMessage(JAB_CLOSE_CHAT, fGroupMap[room]);
		room->leave(leaveMessage);
		fGroupMap.erase(room);
		//delete room;
	}
}


void TalkManager::SetNickname(const gloox::JID& jid, std::string newNick)
{
	std::string group_room = jid.bare();
	gloox::MUCRoom* room = (gloox::MUCRoom*)IsExistingWindowToGroup(group_room);
	if (room) {
		room->setNick(newNick);
	}
}


void TalkManager::SetStatus(const gloox::JID& jid, gloox::Presence::PresenceType presence)
{
	std::string group_room = jid.bare();
	gloox::MUCRoom* room = (gloox::MUCRoom*)IsExistingWindowToGroup(group_room);
	if (room) {
		room->setPresence(presence);
	}
}


void TalkManager::handleMessage(const gloox::Message& msg, gloox::MessageSession* session)
{
    if(JabberSpeak::Instance()->HandleArchive(msg))return;
	if (Chat::Classify(msg) == Chat::MessageKind::DeliveryError) {
		DeliverMessage(msg, session);
		return;
	}
	const Confirm* confirm = msg.findExtension<const Confirm>(ExtConfirm);
	if (confirm != nullptr) {
		BString alertMessage;
		alertMessage.SetToFormat("The service '%s' wants to confirm your identity to access '%s' "
			"using the method '%s'.\nThe verification code for this request is: '%s'.\n"
			"Check the verification code and that you are the one initiating this request.",
			msg.from().full().c_str(), confirm->url().c_str(), confirm->method().c_str(),
			confirm->id().c_str());
		BAlert* alert = new BAlert("Authentification request", alertMessage, "Yes", "No");
		BMessage* message = new BMessage('HTAC');
		message->AddString("id", confirm->id().c_str());
		message->AddString("method", confirm->method().c_str());
		message->AddString("url", confirm->url().c_str());
		message->AddString("from", msg.from().full().c_str());
		BInvoker* invoker = new BInvoker(message, this, Looper());
		alert->Go(invoker);

		// Do not add the message to a talk window in this case
		return;
	}

	// First check if it's a carbon
	if (msg.hasEmbeddedStanza()) {
		// get the possible carbon extension
		const gloox::Carbons *carbon = msg.findExtension<const gloox::Carbons>(
			gloox::ExtCarbons);

		// if the extension exists and contains a message, use it as the real message
        if (carbon && carbon->embeddedStanza()
            && msg.from().bare() == JabberSpeak::Instance()->GlooxClient()->jid().bare()) {
			const gloox::Message* message = static_cast<gloox::Message*>(
				carbon->embeddedStanza());

            const bool sent = carbon->type() == gloox::Carbons::Sent;
            const auto peer = sent ? message->to() : message->from();
            gloox::MessageSession* peerSession = nullptr;
            for (const auto& [candidate, view] : fTalkMap) {
                (void)view;
                if (candidate->target().bare() == peer.bare()) { peerSession = candidate; break; }
            }
            if (!peerSession && Chat::Classify(*message) == Chat::MessageKind::Content) {
                peerSession = new gloox::MessageSession(JabberSpeak::Instance()->GlooxClient(), peer);
                peerSession->registerMessageHandler(this);
            }
            if (peerSession) DeliverMessage(*message, peerSession, sent);

			return;
		}
	}

	const gloox::PubSub::Event* event
		= msg.findExtension<gloox::PubSub::Event>(gloox::ExtPubSubEvent);
	if (event) {
		if (event->node() == "urn:xmpp:avatar:metadata") {
			for (auto item: event->items()) {
				GetAvatar(msg.from(), item->item);
			}
			return;
		} else if (event->node() == "urn:xmpp:bookmarks:1") {
			for (auto item: event->items()) {
				BMessage message(kRefreshBookmark);
				message.AddString("jid", item->item.c_str());
				if (item->retract) {
					message.AddBool("delete", true);
				} else {
					const auto* payload = item->payload;
					const auto* conference = payload && payload->name() == "conference"
						? payload : payload ? payload->findChild("conference") : nullptr;
					if (!conference) continue;
					BookmarkManager::Instance().RememberName(item->item, conference->findAttribute("name"));
					message.AddString("newName", conference->findAttribute("name").c_str());
				}
				BlabberMainWindow::Instance()->PostMessage(&message);
			}
			return;
		} else {
			printf("Got unexpected pubsub event node %s\n", event->node().c_str());
		}
	}

	DeliverMessage(msg, session);
}

void TalkManager::DeliverMessage(const gloox::Message& msg,
	gloox::MessageSession* session, bool outgoing)
{
	const auto kind = Chat::Classify(msg);
    if(kind==Chat::MessageKind::DeliveryError && !session) {
        for(const auto& [room,view]:fGroupMap) {
            if(view && msg.from().bare()==room->name()+"@"+room->service()) {
                BMessage failure(kMessageDeliveryError);failure.AddString("id",msg.id().c_str());
                failure.AddInt32("condition",msg.error()?msg.error()->error():gloox::StanzaErrorUndefined);
                PostToView(view,failure);return;
            }
        }
    }
	// gloox may create a separate session for an error subtype or resource.
	// Route controls to the existing peer view, without opening a new chat.
	if (!outgoing && kind != Chat::MessageKind::Content && !fTalkMap.count(session)) {
		for (const auto& entry : fTalkMap) {
			if (entry.first->target().bare() == msg.from().bare()) {
				session = entry.first;
				break;
			}
		}
	}
    if (kind != Chat::MessageKind::DeliveryError) {
        if (const auto* receipt = msg.findExtension<Chat::Receipt>(Chat::Receipt::kExtension);
            receipt && !receipt->id().empty() && !outgoing && !msg.when()) {
            auto found = fTalkMap.find(session);
            if (found != fTalkMap.end() && msg.from().bare() == session->target().bare()) {
                BMessage notice(kMessageReceipt);
                notice.AddString("id", receipt->id().c_str());
                PostToView(found->second, notice);
            }
        }
    }
    if (!outgoing && kind != Chat::MessageKind::DeliveryError && session
        && msg.from().bare() == session->target().bare()) {
        const auto* marker = msg.findExtension<Chat::Marker>(Chat::Marker::kExtension);
        auto found = fTalkMap.find(session);
        if (marker && !marker->id().empty() && found != fTalkMap.end()) {
            BMessage read('mRed');
            read.AddString("id", marker->id().c_str());
            read.AddString("actor", msg.from().bare().c_str());
            PostToView(found->second, read);
        }
    }
    if (kind == Chat::MessageKind::Correction) {
        const auto* correction=msg.findExtension<Chat::Correction>(Chat::Correction::kExtension);
        auto found=fTalkMap.find(session);
        if(found!=fTalkMap.end() && correction) {
            BMessage edit('mCor');edit.AddString("replace",correction->id().c_str());edit.AddString("id",msg.id().c_str());
            edit.AddString("body",msg.body().c_str());edit.AddString("sender",msg.from().bare().c_str());edit.AddBool("outgoing",outgoing);
            edit.AddInt64("time",msg.when()?Conversations::ArchiveTime(msg.when()->stamp()):real_time_clock_usecs());PostToView(found->second,edit);
        }
        return;
    }
    if (kind == Chat::MessageKind::Reaction) {
        const auto* r=msg.findExtension<Chat::Reactions>(Chat::Reactions::kExtension);
        auto found=fTalkMap.find(session);
        if(r && !r->id().empty() && found!=fTalkMap.end()) {
            BMessage n('rInc');n.AddString("id",r->id().c_str());
            n.AddString("actor",outgoing ? "self" : msg.from().bare().c_str());
            n.AddString("name",outgoing ? "You" : "Contact");
            for(const auto& e:r->values())n.AddString("emoji",e.c_str());
            PostToView(found->second,n);
        }
        return;
    }
	if (kind == Chat::MessageKind::Ignore) return;
	if (kind == Chat::MessageKind::Content && session && !fTalkMap.count(session)) {
		CreateTalkSession(&session->target(), session, false);
	}
	try {
		TalkView* window = fTalkMap.at(session);
		if (kind == Chat::MessageKind::DeliveryError) {
			BMessage failure(kMessageDeliveryError);
			failure.AddInt32("condition", msg.error()
				? msg.error()->error() : gloox::StanzaErrorUndefined);
			failure.AddString("id", msg.id().c_str());
			PostToView(window, failure);
			return;
		}
		// submit the chat
		BMessage notification(kIncomingMessage);
		notification.AddBool("outgoing", outgoing);
		notification.AddString("content", Upload::AttachmentBody(msg).c_str());
		notification.AddString("id", msg.id().c_str());
        const auto* marker = msg.findExtension<Chat::Marker>(Chat::Marker::kExtension);
        notification.AddBool("markable", marker && marker->markable());
		if (const auto* delay = msg.when()) {
			notification.AddBool("archive", true);
			notification.AddInt64("time", Conversations::ArchiveTime(delay->stamp()));
		}
		notification.AddString("attachment", Upload::AttachmentUrl(msg, Upload::Profile::Load().authority).c_str());
		PostToView(window, notification);
	} catch(const std::out_of_range&) {
		printf("%s: no window found for session %p with %s, ignoring message\n",
			__func__, session, session ? session->target().full().c_str(): "NULL");
	}
}


void
TalkManager::handleMessageSession(gloox::MessageSession* session)
{
	// don't create a window for the carbons session, but still register to
	// get the messages.
	if (session->threadID().empty() && session->types() == gloox::Message::Headline) {
		session->registerMessageHandler(this);
		return;
	}

    // Wait for actual content. A new session may contain only an error,
    // typing state or receipt; none may open a chat or play a reply sound.
    session->registerMessageHandler(this);
}


//#pragma mark - PubSub ResultHandler
void TalkManager::handleItem(const gloox::JID&, const std::string&, const gloox::Tag*)
{
	puts(__func__);
}


void TalkManager::handleItems(const std::string&, const gloox::JID& jid,
	const std::string& node, const gloox::PubSub::ItemList& itemList,
	const gloox::Error*)
{
	if (node == "urn:xmpp:avatar:data") {
		for (auto item: itemList) {
			// item->id() has the checksum
            if(!item||!item->tag()||!item->tag()->findChild("data"))continue;
			std::string base64 = item->tag()->findChild("data")->cdata();

			// need to remove all newlines from the base64...
			size_t pos;
			for (;;) {
				pos = base64.find('\n');
				if (pos == std::string::npos)
					break;
				base64.erase(pos, 1);
			}

			if(base64.size()>1400000)continue;
			const std::string decoded = gloox::Base64::decode64(base64);
            Avatars::Receive(JabberSpeak::Instance()->GlooxClient()->jid().bare(),jid.bare(),decoded);
			BMessage message(kAvatarUpdate);
			message.AddString("jid", jid.full().c_str());
			message.AddData("avatar", B_RAW_TYPE, decoded.data(), decoded.length());
			SendNotices(kAvatarUpdate, &message);


		}
	} else {
		printf("unknown pubsub item in %s(%s, %s)\n", __func__,
			jid.full().c_str(), node.c_str());
	}
}


void TalkManager::handleItemPublication(const std::string&, const gloox::JID&,
	const std::string&, const gloox::PubSub::ItemList&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleItemDeletion(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::ItemList&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleSubscriptionResult(const std::string&, const gloox::JID&, const std::string&, const std::string&, const gloox::JID&, gloox::PubSub::SubscriptionType, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleUnsubscriptionResult(const std::string&, const gloox::JID&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleSubscriptionOptions(const std::string&, const gloox::JID&, const gloox::JID&, const std::string&, const gloox::DataForm*, const std::string&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleSubscriptionOptionsResult(const std::string&, const gloox::JID&, const gloox::JID&, const std::string&, const std::string&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleSubscribers(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::SubscriptionList&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleSubscribersResult(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::SubscriberList*, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleAffiliates(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::AffiliateList*, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleAffiliatesResult(const std::string&, const gloox::JID&, const std::string&, const gloox::PubSub::AffiliateList*, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleNodeConfig(const std::string&, const gloox::JID&, const std::string&, const gloox::DataForm*, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleNodeConfigResult(const std::string&, const gloox::JID&, const std::string&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleNodeCreation(const std::string&, const gloox::JID&, const std::string&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleNodeDeletion(const std::string&, const gloox::JID&, const std::string&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleNodePurge(const std::string&, const gloox::JID&, const std::string&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleSubscriptions(const std::string&, const gloox::JID&, const gloox::PubSub::SubscriptionMap&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleAffiliations(const std::string&, const gloox::JID&, const gloox::PubSub::AffiliationMap&, const gloox::Error*)
{
	puts(__func__);
}


void TalkManager::handleDefaultNodeConfig(const std::string&, const gloox::JID&, const gloox::DataForm*, const gloox::Error*)
{
	puts(__func__);
}


void* TalkManager::IsExistingWindowToGroup(std::string group_room) {
	// check names
	for (auto i = fGroupMap.begin(); i != fGroupMap.end(); ++i) {
		if ((*i).second->GetGroupRoom() == group_room) {
			return (*i).first;
		}
	}

	// no matches
	return nullptr;
}


void
TalkManager::RemoveWindow(TalkView* window)
{
	fViewRegistry.Forget(window);

	for (GroupMap::iterator i = fGroupMap.begin(); i != fGroupMap.end(); ++i) {
		if ((*i).second == window) {
			delete (*i).first;
			fGroupMap.erase(i);
			if (BlabberMainWindow::Instance() != nullptr)
				BlabberMainWindow::Instance()->PostMessage(kWindowList);
			return;
		}
	}

	for (TalkMap::iterator i = fTalkMap.begin(); i != fTalkMap.end(); ++i) {
		if ((*i).second == window) {
			fTalkMap.erase(i);
			if (BlabberMainWindow::Instance() != nullptr)
				BlabberMainWindow::Instance()->PostMessage(kWindowList);
			return;
		}
	}
}


void TalkManager::RememberView(const TalkView* view)
{
	fViewRegistry.Remember(view);
}

uint64 TalkManager::ViewToken(const TalkView* view) const
{
	return fViewRegistry.Token(view);
}

bool TalkManager::OwnsView(const TalkView* view, uint64 token) const
{
	return fViewRegistry.Matches(view, token);
}

void
TalkManager::Reset()
{
	// MainWindow closed attached views after the runtime drained. A UI event
	// can have queued an unattached view behind the stopped message; reclaim
	// those too before allowing the next account/connection to start.
	std::set<TalkView*> pending;
	for (const auto& entry : fTalkMap) pending.insert(entry.second);
	for (const auto& entry : fGroupMap) pending.insert(entry.second);
	for (TalkView* view : pending) {
		if (view->Window() == nullptr) delete view;
	}
	fTalkMap.clear();
	fGroupMap.clear();
	fViewRegistry.Clear();
}


void
TalkManager::GetAvatar(const gloox::JID& jid, const std::string& hash)
{
    // Normalized thumbnails are cached by account/peer on the avatar worker.
    // Retain old cache files for rollback, but never decode them on the UI looper.
    if(hash.size()!=40||hash.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)return;
    JabberSpeak::Instance()->RequestPubSubItem(jid,"urn:xmpp:avatar:data",hash,this);
}


void
TalkManager::handleMUCParticipantPresence(gloox::MUCRoom *room,
							const gloox::MUCRoomParticipant participant,
							const gloox::Presence &presence)
{
	BMessage msg;
	BString fullRoom;
	fullRoom.SetToFormat("%s@%s", room->name().c_str(), room->service().c_str());
	msg.AddString("room", fullRoom);
	msg.AddString("occupant-jid", participant.nick->full().c_str());
	if (participant.jid)
		msg.AddString("real-jid", participant.jid->full().c_str());
    if(participant.jid)Avatars::Alias(JabberSpeak::Instance()->GlooxClient()->jid().bare(),participant.nick->full(),participant.jid->bare());
	msg.AddInt32("affiliation", participant.affiliation);
	msg.AddInt32("role", participant.role);
	msg.AddInt32("presence", presence.subtype());

	std::string status = presence.status();

	if (status != "") {
		// TODO there seem to be no status code forwarded by gloox to detect if the presence is a
		// user-generated leave message or not.
		// (I think that would be detected by the presence of a status code in the presence?)
		// For now, ignore some specific server-generated messages found in the wild...
		if ((status != "Kicked: recipient unavailable")
		&& (status != "Kicked: remote server not found")
		&& (status != "Kicked: service unavailable")
		&& (status != "SIGHUP received")
		&& (status != "Disconnected: closed")
		&& (status != "Disconnected: No route to host")
		&& (status != "Disconnected: Received SIGTERM")
		&& (status != "Disconnected: Replaced by new connection")
		&& (status != "Disconnected: Hibernating too long")
		&& (status != "Disconnected: Broken pipe")
		&& (status != "Disconnected: unexpected eof while reading")
		&& (status != "Disconnected: Too many unacked stanzas remaining, session can't be resumed"))
		{
			msg.AddString("status", status.c_str());
		}
	}

	if (participant.flags & gloox::UserNickChanged) {
		msg.AddString("newNick", participant.newNick.c_str());
	}

	// TODO check for UserNickChanged in flags (and other flags in MUCUserFlags as appropriate)
	// TODO also include the reason of the presence if any (for example the kick or leave message)
	if ((participant.role == gloox::MUCRoomRole::RoleNone) || (participant.flags & gloox::UserNickChanged)) {
		msg.what = JAB_GROUP_CHATTER_OFFLINE;
	} else {
		msg.what = JAB_GROUP_CHATTER_ONLINE;
	}
	SendNotices(msg.what, &msg);
}


void
TalkManager::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case 'HTAC':
		{
			// User reply for XEP-0070 HTTP auth confirmation request
			// Send a message back, either as chat (if request is accepted) or error (if rejected)
			int32 which = message->FindInt32("which"); // 0 = grant, 1 = reject
			BString id = message->FindString("id");
			BString method = message->FindString("method");
			BString url = message->FindString("url");
			BString from = message->FindString("from");
			printf("from: %s\n", from.String());

			gloox::Message reply(which == 0 ? gloox::Message::MessageType::Chat
					: gloox::Message::MessageType::Error,
				gloox::JID(from.String()));
			Confirm* confirm = new Confirm(id, method, url);
			reply.addExtension(confirm);
			if (which != 0) {
				gloox::Error* error = new gloox::Error(gloox::StanzaErrorTypeAuth, gloox::StanzaErrorNotAuthorized);
				reply.addExtension(error);
			}
			JabberSpeak::Instance()->GlooxClient()->send(reply);

			break;
		}
		default:
			printf("Unexpected message in %s\n", __PRETTY_FUNCTION__);
			message->PrintToStream();
	}
}


void
TalkManager::_MentionNotification(const std::string& roomName, const gloox::Message& msg)
{
	BNotification notification(B_INFORMATION_NOTIFICATION);
	notification.SetGroup(roomName.c_str());
	notification.SetTitle(Conversations::FriendlyName(msg.from().bare(), msg.from().resource(), false).c_str());
	notification.SetContent(msg.body().c_str());
	notification.Send();

	BMessage message(kHighlightRosterItem);
	message.AddString("room_jid", msg.from().bare().c_str());
	message.AddInt32("flags", BookmarkItem::NICKNAME_HIGHLIGHT);
	BlabberMainWindow::Instance()->PostMessage(&message);
}


struct ReferenceCompare
{
	bool operator()(const Reference& a, const Reference& b) const
	{
		return a.span().first < b.span().first;
	}
};


void
TalkManager::handleMUCMessage(gloox::MUCRoom *room,
	const gloox::Message &msg, bool priv)
{
    if(JabberSpeak::Instance()->HandleArchive(msg))return;
	TalkView *window = NULL;
    auto known = fGroupMap.find(room);
    if (known == fGroupMap.end() || !known->second) return;
    if (msg.subtype() == gloox::Message::Error || msg.error()) {
        BMessage notice(kMessageDeliveryError);
        notice.AddString("id", msg.id().c_str());
        notice.AddInt32("condition", msg.error() ? msg.error()->error() : gloox::StanzaErrorUndefined);
        PostToView(known->second, notice);
        return;
    }
    if (!priv && msg.from().bare() == room->name() + "@" + room->service()
        && !msg.from().resource().empty() && msg.from().resource() != room->nick()) {
        const auto* marker = msg.findExtension<Chat::Marker>(Chat::Marker::kExtension);
        if (marker && !marker->id().empty()) {
            BMessage read('mRed');
            read.AddString("id", marker->id().c_str());
            read.AddString("actor", msg.from().full().c_str());
            PostToView(known->second, read);
        }
    }
    if (const auto* correction=msg.findExtension<Chat::Correction>(Chat::Correction::kExtension)) {
        if(!priv) {
            BMessage edit('mCor');edit.AddString("replace",correction->id().c_str());edit.AddString("id",msg.id().c_str());
            edit.AddString("body",msg.body().c_str());edit.AddString("sender",msg.from().full().c_str());edit.AddBool("outgoing",msg.from().resource()==room->nick());
            edit.AddInt64("time",msg.when()?Conversations::ArchiveTime(msg.when()->stamp()):real_time_clock_usecs());PostToView(known->second,edit);
        }
        return;
    }
    if (const auto* r=msg.findExtension<Chat::Reactions>(Chat::Reactions::kExtension)) {
        if(!r->id().empty() && !priv) {
            BMessage n('rInc');n.AddString("id",r->id().c_str());
            n.AddString("actor",msg.from().resource()==room->nick() ? "self" : msg.from().full().c_str());
            n.AddString("name",msg.from().resource()==room->nick() ? "You" : msg.from().resource().c_str());
            for(const auto& e:r->values())n.AddString("emoji",e.c_str());
            PostToView(known->second,n);
        }
        return;
    }
    if (Upload::AttachmentBody(msg).empty()) return;
	if (!priv && !msg.when() && msg.from().resource() == room->nick()) {
		// Queue after the send/upload notice on the same window looper. A fast
		// echo must not overtake local rendering or create a second attachment.
		BMessage echo(kOwnGroupEcho);
		echo.AddString("id", msg.id().c_str());
        echo.AddString("reaction-id",Chat::ReactionId(msg,true).c_str());
		echo.AddString("content", Upload::AttachmentBody(msg).c_str());
		echo.AddString("attachment", Upload::AttachmentUrl(msg, Upload::Profile::Load().authority).c_str());
		PostToView(known->second, echo);
		return;
	}

	std::list<TextSpan> runs;
	bool highlight = false;
	bool autoHyperlink = false;

	std::string group_username = msg.from().resource();
	const gloox::XHtmlIM* xhtml = msg.findExtension<gloox::XHtmlIM>(gloox::ExtXHtmlIM);
	if (xhtml != nullptr && xhtml->xhtml() != nullptr) {
		// We got (valid) XHTML-IM, parse it and replace the plaintext body
		CharacterStyle baseStyle;
		fXhtmlProcessor.ProcessXhtml(xhtml->xhtml(), baseStyle, runs);
		autoHyperlink = false;
	} else {
		// Extensions are not sorted in any specific order, so, first of all, extract references and
		// sort them by begin position
		std::set<Reference, ReferenceCompare> references;
		autoHyperlink = true;

		for (const auto ext: msg.extensions())
		{
			if (ext->extensionType() != ExtReferences)
				continue;

			const Reference* reference = static_cast<const Reference*>(ext);
			if (reference->type() != "mention") {
				// TODO handle SIMS (references to images to be displayed with the content)
				fprintf(stderr, "Unknown extension type %s: %s\n", reference->type().c_str(),
					reference->uri().c_str());
				continue;
			}

			references.insert(*reference);
		}

		CharacterStyle run;
		int start = 0;
		BString msgBody(Upload::AttachmentBody(msg).c_str());

		for (const auto& reference: references)
		{
			const auto& span = reference.span();
			const std::string& uri = reference.uri();

			std::string mentionnedNick = uri.substr(uri.find('/') + 1);
			BString nickname(BUrl::UrlDecode(mentionnedNick.c_str(), true));
			if (nickname.String() == room->nick()) {
				highlight = true;
			}

			if (span.first >= 0 && span.second > span.first) {
				// Convert span from characters to bytes
				int32 begin;
				int32 end;
				begin = msgBody.CountBytes(0, span.first);
				end = msgBody.CountBytes(0, span.second);

				BString mentionned;

				run.SetForegroundColor(B_DOCUMENT_TEXT_COLOR);
				run.SetBold(false);
				msgBody.CopyInto(mentionned, start, begin - start);
				runs.emplace_back(mentionned, run);

				// Create run for the mentionned name
				run.SetForegroundColor(ConsistentColor(nickname.String()));
				run.SetBold(true);
				msgBody.CopyInto(mentionned, begin, end - begin);
				runs.emplace_back(mentionned, run);

				start = end;
			}
		}

		if (references.empty() && BString(msg.body().c_str()).IFindFirst(room->nick().c_str()) != B_ERROR) {
			// Message did not include any references, check manually if the user nickname is
			// contained in the text, until more clients support references and also for things
			// that really can't (Biboumi)

			highlight = true;
			run.SetForegroundColor(ConsistentColor(room->nick().c_str()));
			run.SetBold(true);
		} else {
			run.SetForegroundColor(B_DOCUMENT_TEXT_COLOR);
			run.SetBold(false);
		}

		// Create run to restore normal text for the remaining part of the message
		BString endText;
		msgBody.CopyInto(endText, start, msgBody.Length() - start);
		runs.emplace_back(endText, run);
	}

	if (priv) {
		// This is a private message. We need to locate or create the corresponding message session
		// Unlike other talk sessions, we can't index by the bare JID here, because that would be
		// the groupchat address. So we have to use the full JID.
		gloox::MessageSession* session = nullptr;
		for (TalkMap::iterator i = fTalkMap.begin();
				i != fTalkMap.end(); ++i) {
			if ((*i).first->target() == msg.from()) {
				session = (*i).first;
				window = fTalkMap.at(session);
				break;
			}
		}

		if (!session) {
			session = new gloox::MessageSession(JabberSpeak::Instance()->GlooxClient(), msg.from());
			window = new TalkView(&msg.from(), "", "", session);
			RememberView(window);

#if 0
			if (sound_on_new) {
				// play a sound
				system_beep(kSoundNewMUCMessage);
			}
#endif

			// add it to the known list
			fTalkMap[session] = window;
			//BlabberMainWindow::Instance()->PostMessage(kWindowList);
			session->registerMessageHandler(this);
			BMessage message(kAddTalkView);
			message.AddPointer("view", window);
			message.AddUInt64("view-token", ViewToken(window));
			message.AddUInt64("connection-generation", JabberSpeak::Instance()->ConnectionGeneration());
			BlabberMainWindow::Instance()->PostMessage(&message);
		}
		// FIXME since we just created the window, we may not be able to lock it immediately.
		// AddToTalk should REALLY be done through a BMessage and not directly.
		while(!window->LockLooper());
        const bool accepted = window->AddToTalk(group_username, runs, TalkView::MAIN_RECIPIENT,
            autoHyperlink, msg.when() ? Conversations::ArchiveTime(msg.when()->stamp()) : 0,
            msg.when() != nullptr, msg.id(), Upload::AttachmentUrl(msg, Upload::Profile::Load().authority));
        if (accepted)
            window->AddAttachment(Upload::AttachmentUrl(msg, Upload::Profile::Load().authority));
        window->UnlockLooper();

        // Backlog and duplicate content must not resurrect a room highlight.
        if (accepted && !msg.when()) {
            if (highlight) {
                const std::string jid = room->name() + "@" + room->service();
                const auto* bookmark = BookmarkManager::Instance().GetBookmark(jid.c_str());
                _MentionNotification(Conversations::FriendlyName(jid,
                    bookmark ? bookmark->name : "", true), msg);
            }
            BMessage message(kHighlightRosterItem);
            message.AddString("room_jid", msg.from().bare().c_str());
            message.AddString("who", msg.from().resource().c_str());
            message.AddInt32("flags", BookmarkItem::NICKNAME_HIGHLIGHT);
            BlabberMainWindow::Instance()->PostMessage(&message, fGroupMap.at(room));
        }
		return;
	}

	window = fGroupMap.at(room);
	// submit the chat
	if (window) {

		if (group_username.empty()) {
			// FIXME send a message to the view instead (locking and unlocking for every message is slow)
			window->LockLooper();
			window->AddToTalk(B_TRANSLATE("System:"), runs, TalkView::OTHER, autoHyperlink, 0, false, "", Upload::AttachmentUrl(msg, Upload::Profile::Load().authority));
			window->AddAttachment(Upload::AttachmentUrl(msg, Upload::Profile::Load().authority));
			window->UnlockLooper();
		} else {

			// FIXME send a message to the view instead (locking and unlocking for every message is slow)
			bool isLocked = window->LockLooper();
			if (!isLocked) return;

			// Do not append another attachment after suppressing our own echo.
            const auto type = group_username == window->GetGroupUsername()
                ? TalkView::LOCAL : TalkView::MAIN_RECIPIENT;
            const bool accepted = window->AddToTalk(group_username, runs, type, autoHyperlink,
                msg.when() ? Conversations::ArchiveTime(msg.when()->stamp()) : 0,
                msg.when() != nullptr, Chat::ReactionId(msg,true), Upload::AttachmentUrl(msg, Upload::Profile::Load().authority), msg.from().full(), msg.id(), msg.findExtension<Chat::Marker>(Chat::Marker::kExtension) && msg.findExtension<Chat::Marker>(Chat::Marker::kExtension)->markable());
            if (accepted)
                window->AddAttachment(Upload::AttachmentUrl(msg, Upload::Profile::Load().authority));

            // Notify only after the UI/history has accepted a genuinely new
            // incoming message. Mention parsing alone cannot distinguish a
            // replay on room join from live content, even with the same ID.
            if (accepted && !msg.when() && type == TalkView::MAIN_RECIPIENT) {
                if (highlight) {
                    const std::string jid = room->name() + "@" + room->service();
                    const auto* bookmark = BookmarkManager::Instance().GetBookmark(jid.c_str());
                    _MentionNotification(Conversations::FriendlyName(jid,
                        bookmark ? bookmark->name : "", true), msg);
                } else {
                    BMessage activity(kHighlightRosterItem);
                    activity.AddString("room_jid", msg.from().bare().c_str());
                    activity.AddInt32("flags", BookmarkItem::ACTIVITY);
                    BlabberMainWindow::Instance()->PostMessage(&activity);
                }
            }

			// May not be locked if the view is not added to a winow yet
			if (isLocked)
				window->UnlockLooper();
		}
	}
}


bool
TalkManager::handleMUCRoomCreation(gloox::MUCRoom *room __attribute__((unused)))
{
	fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
	return false;
}


void
TalkManager::handleMUCSubject(gloox::MUCRoom *room,
							const std::string &nick,
							const std::string &subject)
{
	TalkView* window = fGroupMap.at(room);
	BString topic;
	topic.SetToFormat(B_TRANSLATE("Set topic to %s\n"), subject.c_str());

	// FIXME just send a BMessage to the view and let it handle this
	bool isLocked = window->LockLooper();
	window->SetStatus(subject);
	std::list<TextSpan> runs;
	runs.emplace_back(topic, TalkView::SystemMessagesStyle());
	window->AddToTalk(nick, runs, TalkView::OTHER, true);
	if (isLocked)
		window->UnlockLooper();
}


void
TalkManager::handleMUCInviteDecline(gloox::MUCRoom *room __attribute__((unused)),
							const gloox::JID &invitee __attribute__((unused)),
							const std::string &reason __attribute__((unused)))
{
	fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
}


void
TalkManager::handleMUCError(gloox::MUCRoom *room,
							gloox::StanzaError error)
{
	TalkView* window = fGroupMap.at(room);
	if (window != NULL) {
		// FIXME just send a BMessage to the view and let it handle this
		window->LockLooper();
		window->ShowError(error);
		window->UnlockLooper();
	} else {
		fprintf(stderr, "%s(%s, %d)\n", __PRETTY_FUNCTION__, room->name().c_str(), error);
	}
}


void
TalkManager::handleMUCInfo(gloox::MUCRoom *room, int features, const std::string &name,
							const gloox::DataForm *infoForm __attribute__((unused)))
{
	BString roomjid;
	roomjid.SetToFormat("%s@%s", room->name().c_str(), room->service().c_str());
    // Room information arrives after joining and after configuration changes.
    // Retry the room vCard even if an earlier pre-join request had no photo.
    JabberSpeak::Instance()->QueueAvatar(JabberSpeak::Instance()->GlooxClient()->jid().bare(), roomjid.String());
	BMessage message(kRoomInfo);
	message.AddString("jid", roomjid);
	message.AddInt32("flags", features);
	message.AddString("name", name.c_str());

	// TODO Create BView from DataForm (see GlooxHandler.h for registration example) and archive it
	// into BMessage

	BlabberMainWindow::Instance()->PostMessage(&message);
}


void
TalkManager::handleMUCItems(gloox::MUCRoom *room __attribute__((unused)),
							const gloox::Disco::ItemList &items __attribute__((unused)))
{
	fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
}

void TalkManager::PostToView(TalkView* view, const BMessage& notice)
{
    BMessage envelope(kDeliverTalkNotice);
    envelope.AddPointer("view", view);
    envelope.AddUInt64("view-token", ViewToken(view));
    envelope.AddUInt64("connection-generation", JabberSpeak::Instance()->ConnectionGeneration());
    envelope.AddMessage("payload", &notice);
    BlabberMainWindow::Instance()->PostMessage(&envelope);
}
