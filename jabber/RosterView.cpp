//////////////////////////////////////////////////
// Blabber [RosterView.cpp]
//////////////////////////////////////////////////

#include "RosterView.h"
#include "ui/ConversationRow.h"
#include <FindDirectory.h>
#include <Path.h>
#include <openssl/sha.h>

#include <cstdio>

#include <Beep.h>
#include <Alert.h>
#include <Catalog.h>
#include <MenuItem.h>
#include <TranslationUtils.h>

#include "BlabberSettings.h"
#include "config/PrivateProfile.h"
#include "ui/BuddyInfoWindow.h"
#include "JabberSpeak.h"
#include "Messages.h"
#include "../ui/ModalAlertFactory.h"
#include "SoundSystem.h"
#include "TalkManager.h"

#include <strings.h>
#include <unistd.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "RosterView"


RosterView::RosterView()
	: EdgeScrollListView() {
	SetExplicitMinSize(BSize(StringWidth("Firstname M. Lastname") + 26, B_SIZE_UNSET));
	SetViewUIColor(B_LIST_BACKGROUND_COLOR);
}
RosterView::~RosterView() {
    Avatars::Unwatch(this);
    while (CountItems()) delete RemoveItem(int32(0));
    delete _popup;
}

int RosterView::ListComparison(const void *a, const void *b) {
	const char *str_a = (*(RosterItem **)a)->Text();
	const char *str_b = (*(RosterItem **)b)->Text();

	return strcasecmp(str_a, str_b);
}

void RosterView::AttachedToWindow() {
	// superclass call
	BOutlineListView::AttachedToWindow();

	SetSelectionMessage(new BMessage(kRosterSelectionChanged));

	// popup menu
	_popup = new BPopUpMenu(NULL, false, false);

		_chat_item         = new BMenuItem(B_TRANSLATE("Chat" B_UTF8_ELLIPSIS), new BMessage(JAB_OPEN_CHAT));
		_message_item      = new BMenuItem(B_TRANSLATE("Send message" B_UTF8_ELLIPSIS), new BMessage(JAB_OPEN_MESSAGE));
		_change_user_item  = new BMenuItem(B_TRANSLATE("Edit buddy"), new BMessage(JAB_OPEN_EDIT_BUDDY_WINDOW));
		_remove_user_item  = new BMenuItem(B_TRANSLATE("Remove buddy"), new BMessage(JAB_REMOVE_BUDDY));
		_user_info_item    = new BMenuItem(B_TRANSLATE("Get buddy info"), new BMessage(JAB_USER_INFO));

		_presence          = new BMenu(B_TRANSLATE("Presence"));

			_subscribe_presence   = new BMenuItem(B_TRANSLATE("Subscribe"), new BMessage(JAB_SUBSCRIBE_PRESENCE));
			_unsubscribe_presence = new BMenuItem(B_TRANSLATE("Unsubscribe"), new BMessage(JAB_UNSUBSCRIBE_PRESENCE));

			_presence->AddItem(_subscribe_presence);
			_presence->AddItem(_unsubscribe_presence);

		_availability          = new BMenu(B_TRANSLATE("Status"));

		_availability->AddItem(new BMenuItem(B_TRANSLATE("Online"), new BMessage(BLAB_ONLINE)));
		_availability->AddItem(new BMenuItem(B_TRANSLATE("I'm available for chat"), new BMessage(BLAB_AVAILABLE_FOR_CHAT)));
		_availability->AddItem(new BMenuItem(B_TRANSLATE("I will be away temporarily"), new BMessage(BLAB_AWAY_TEMPORARILY)));
		_availability->AddItem(new BMenuItem(B_TRANSLATE("Do not disturb me"), new BMessage(BLAB_DO_NOT_DISTURB)));
		_availability->AddItem(new BMenuItem(B_TRANSLATE("I will be away for an extended time period"), new BMessage(BLAB_AWAY_EXTENDED)));

	_popup->AddItem(_chat_item);
	_popup->AddItem(_message_item);
	fFavoriteItem = new BMenuItem(B_TRANSLATE("Add to favorites"), new BMessage('fFav'));
	_popup->AddItem(fFavoriteItem);
    fHideItem=new BMenuItem(B_TRANSLATE("Hide"),new BMessage('cHid'));
    _popup->AddItem(fHideItem);
	_popup->AddSeparatorItem();
	_popup->AddItem(_change_user_item);
	_popup->AddItem(_remove_user_item);
	_popup->AddSeparatorItem();
	_popup->AddItem(_user_info_item);
	_popup->AddSeparatorItem();
	_popup->AddItem(_presence);

	// Redirect popup menu events to the window
	_presence->SetTargetForItems(Window());
	_availability->SetTargetForItems(this);
	_popup->SetTargetForItems(Window());
	fFavoriteItem->SetTarget(this);
    fHideItem->SetTarget(this);

	BookmarkManager::Instance().StartWatching(this, kBookmarks);
	TalkManager::Instance()->StartWatching(this, kAvatarUpdate);
    Avatars::Watch(this);
}

RosterItem *RosterView::CurrentItemSelection() {
	int32 index = CurrentSelection();

	if (index >= 0) {
		return dynamic_cast<RosterItem *>(ItemAt(index));
	} else {
		return NULL;
	}
}

BookmarkItem* RosterView::CurrentBookmarkSelection()
{
	int32 index = CurrentSelection();

	if (index >= 0) {
		return dynamic_cast<BookmarkItem *>(ItemAt(index));
	} else {
		return NULL;
	}
}

void RosterView::MouseDown(BPoint point) {
	// accept first click
	Window()->Activate(true);

	// get mouse info before it's too late!
	uint32 buttons = 0;
	GetMouse(&point, &buttons, true);

	// superclass stuff
	BOutlineListView::MouseDown(point);

	if (buttons & B_SECONDARY_MOUSE_BUTTON) {
		// update menu before presentation
		UpdatePopUpMenu();

		BPoint screen_point(point);
		ConvertToScreen(&screen_point);

		BRect r(screen_point.x - 4, screen_point.y - 20, screen_point.x + 24, screen_point.y + 4);
		_popup->Go(screen_point, true, true, r, false);
		//_popup->Go(screen_point, true, true, false);
	}
}

void RosterView::RemoveSelected() {
	if (CurrentItemSelection()) {
		// numeric and object based selections
		int32       selected = CurrentSelection();
		RosterItem *item     = CurrentItemSelection();

		if (item == NULL) {
			// not a roster item, won't remove
			return;
		}

		// remove item from view
		RemoveItem(CurrentSelection());

		// select next buddy for continuity
		if (ItemAt(selected))
			Select(selected);
		else if (ItemAt(selected - 1))
			Select(selected - 1);
	}
}

void RosterView::MessageReceived(BMessage* message)
{
	switch(message->what) {
        case 'cHid': {
            const std::string jid=message->GetString("jid", "");
            auto candidate=fRecent;
            if(jid.empty() || !candidate.Hide(jid,real_time_clock_usecs()))break;
            if(fRecentPath.empty() || !candidate.Save(fRecentPath)) {
                (new BAlert("hide",B_TRANSLATE("Could not hide the conversation. The list is unchanged."),B_TRANSLATE("OK")))->Go(nullptr);
            } else {fRecent=std::move(candidate);UpdateRoster();}
            break;
        }
        case 'fFav': {
            bool group = false;
            auto jid = SelectedJid(group);
            // Popup actions carry the original target even if live metadata
            // rebuilds the list before the menu action arrives.
            if (message->HasString("jid")) {
                jid = message->GetString("jid", "");
                group = message->GetBool("group", false);
            }
            if (jid.empty() || fAccount.empty()) break;
            const auto* old = fRecent.Find(jid);
            bool favorite = !old || !old->favorite;
            if (message->HasBool("favorite")) favorite = message->GetBool("favorite", false);
            Conversations::Summary item;
            item.jid = jid; item.group = group;
            if (group) {
                auto room = fRooms.find(jid);
                if (room != fRooms.end()) item.name = room->second;
            } else {
                JRoster::Instance()->Lock();
                const auto* contact = JRoster::Instance()->FindUser(gloox::JID(jid));
                if (contact) item.name = contact->FriendlyName();
                JRoster::Instance()->Unlock();
            }
            auto candidate = fRecent;
            if (!candidate.SetFavorite(item, favorite)) {
                (new BAlert("favorites", B_TRANSLATE("The favorites list is full. Remove a favorite first."), B_TRANSLATE("OK")))->Go(nullptr);
            } else if (fRecentPath.empty() || !candidate.Save(fRecentPath)) {
                (new BAlert("favorites", B_TRANSLATE("Could not save favorites. The previous list is unchanged."), B_TRANSLATE("OK")))->Go(nullptr);
            } else {
                fRecent = std::move(candidate);
                UpdateRoster();
            }
            break;
        }
		case kWindowList:
		{
			// Group bookmarks may be under either Signal or generic XMPP headers.
			Invalidate();
			break;
		}
		case B_OBSERVER_NOTICE_CHANGE:
		{
			int32 what = message->FindInt32("be:observe_change_what");
			switch (what) {
				case kBookmarks:
				{
					fBookmarkBatch = true;
                    const auto previousRooms = fRooms;
					if (message->GetBool("snapshot", false)) fRooms.clear();
					int index = 0;
					BString name;
				   	while (message->FindString("jid", index, &name) == B_OK) {
						gloox::JID jid(name.String());
						if (message->FindBool("delete", index)) {
							UnlinkBookmark(jid);
						} else {
							BString bookmarkName;
							message->FindString("name", index, &bookmarkName);
                            if (bookmarkName.IsEmpty()) {
                                const auto known = previousRooms.find(jid.bare());
                                if (known != previousRooms.end()) bookmarkName = known->second.c_str();
                            }
							LinkBookmark(jid, bookmarkName);
						}
						index++;
					}
					fBookmarkBatch = false;
					if (!fRecentPath.empty()) fRecent.Save(fRecentPath);
					UpdateRoster();
					break;
				}
				case Avatars::kChanged: Invalidate();break;
				case kAvatarUpdate: Invalidate();break;
				default:
				{
					message->PrintToStream();
					break;
				}
			}
			break;
		}

		case BLAB_ONLINE:
		{
			BookmarkItem *bookmark = CurrentBookmarkSelection();
			if (bookmark) {
				TalkManager::Instance()->SetStatus(bookmark->GetUserID(),
					gloox::Presence::PresenceType::Available);
			}
			break;
		}
		case BLAB_AVAILABLE_FOR_CHAT:
		{
			BookmarkItem *bookmark = CurrentBookmarkSelection();
			if (bookmark) {
				TalkManager::Instance()->SetStatus(bookmark->GetUserID(),
					gloox::Presence::PresenceType::Chat);
			}
			break;
		}
		case BLAB_DO_NOT_DISTURB:
		{
			BookmarkItem *bookmark = CurrentBookmarkSelection();
			if (bookmark) {
				TalkManager::Instance()->SetStatus(bookmark->GetUserID(),
					gloox::Presence::PresenceType::DND);
			}
			break;
		}
		case BLAB_AWAY_TEMPORARILY:
		{
			BookmarkItem *bookmark = CurrentBookmarkSelection();
			if (bookmark) {
				TalkManager::Instance()->SetStatus(bookmark->GetUserID(),
					gloox::Presence::PresenceType::Away);
			}
			break;
		}
		case BLAB_AWAY_EXTENDED:
		{
			BookmarkItem *bookmark = CurrentBookmarkSelection();
			if (bookmark) {
				TalkManager::Instance()->SetStatus(bookmark->GetUserID(),
					gloox::Presence::PresenceType::XA);
			}
			break;
		}


		default:
		{
			BOutlineListView::MessageReceived(message);
			break;
		}
	}
}


void RosterView::LinkUser(const UserID*) { UpdateRoster(); }
void RosterView::LinkTransport(const UserID*) { UpdateRoster(); }
void RosterView::LinkBookmark(const gloox::JID& jid, BString name) {
    auto title = Conversations::FriendlyName(jid.bare(), name.String(), true);
    // Room discovery can omit a name even when the bookmark already has one.
    // Missing metadata must not erase the last known human-readable label.
    if (title == Conversations::FriendlyName(jid.bare(), "", true)) {
        const auto existing = fRooms.find(jid.bare());
        if (existing != fRooms.end()) title = existing->second;
        else if (const auto* recent = fRecent.Find(jid.bare())) title = recent->name;
    }
    fRooms[jid.bare()] = title;
    fRecent.Rename(jid.bare(), title);
    if (!fBookmarkBatch) {
        if (!fRecentPath.empty()) fRecent.Save(fRecentPath);
        UpdateRoster();
    }
}
void RosterView::UnlinkUser(const gloox::JID&) { UpdateRoster(); }
void RosterView::UnlinkTransport(const gloox::JID&) { UpdateRoster(); }
void RosterView::UnlinkBookmark(const gloox::JID& jid) { fRooms.erase(jid.bare()); if (!fBookmarkBatch) UpdateRoster(); }

int32 RosterView::FindUser(const gloox::JID& compare_user) {
	for (int i=0; i<FullListCountItems(); ++i) {
		// get item
		RosterItem *item = dynamic_cast<RosterItem *>(FullListItemAt(i));

		if (item == NULL || item->StalePointer()) {
			continue;
		}

		// compare against RosterView
		if (item->GetUserID()->JID().bare() == compare_user.bare()) {
			return i;
		}
	}

	// no match
	return -1;
}

int32 RosterView::FindTransport(const gloox::JID& compare_transport) {
	for (int i=0; i<FullListCountItems(); ++i) {
		// get item
		TransportItem *item = dynamic_cast<TransportItem *>(FullListItemAt(i));

		if (item == NULL) {
			continue;
		}

		// compare against RosterView
		if (item->GetUserID()->JID() == compare_transport) {
			return i;
		}
	}

	// no match
	return -1;
}


int32 RosterView::FindBookmark(const gloox::JID& compare_jid)
{
	for (int i=0; i<FullListCountItems(); ++i) {
		// get item
		BookmarkItem *item = dynamic_cast<BookmarkItem *>(FullListItemAt(i));

		if (item == NULL) {
			continue;
		}

		// compare against RosterView
		if (item->GetUserID() == compare_jid) {
			return i;
		}
	}

	// no match
	return -1;
}

void RosterView::UpdatePopUpMenu() {
	char buffer[1024];
    bool group = false;
    const auto jid = SelectedJid(group);
    const auto* summary = fRecent.Find(jid);
    const bool favorite = summary && summary->favorite;
    fFavoriteItem->SetLabel(favorite ? B_TRANSLATE("Remove from favorites") : B_TRANSLATE("Add to favorites"));
    fFavoriteItem->SetEnabled(!jid.empty());
    auto* toggle = new BMessage('fFav');
    toggle->AddString("jid", jid.c_str());
    toggle->AddBool("group", group);
    toggle->AddBool("favorite", !favorite);
    fFavoriteItem->SetMessage(toggle);
    auto* hide=new BMessage('cHid');hide->AddString("jid",jid.c_str());
    fHideItem->SetMessage(hide);fHideItem->SetEnabled(fMode==0 && summary && !summary->hiddenAt);

	RosterItem *item = CurrentItemSelection();
	BookmarkItem *bookmark = CurrentBookmarkSelection();

	_popup->RemoveItem(_presence);
	_popup->RemoveItem(_availability);

	if (item && !item->StalePointer()) {
		const UserID *user = item->GetUserID();

		// if an item is selected
		_chat_item->SetEnabled(true);
		_message_item->SetEnabled(true);

		sprintf(buffer, B_TRANSLATE("Edit %s"), user->FriendlyName().c_str());
		_change_user_item->SetLabel(buffer);
		_change_user_item->SetEnabled(true);

		sprintf(buffer, B_TRANSLATE("Remove %s"), user->FriendlyName().c_str());
		_remove_user_item->SetLabel(buffer);
		_remove_user_item->SetEnabled(true);

		_user_info_item->SetEnabled(true);

		_popup->AddItem(_presence);

		if (user->HaveSubscriptionTo()) {
			_subscribe_presence->SetEnabled(false);
			_unsubscribe_presence->SetEnabled(true);
		} else {
			_subscribe_presence->SetEnabled(true);
			_unsubscribe_presence->SetEnabled(false);
		}
	} else if (bookmark) {
		// It's a bookmark/group chat, only some features are available
		_chat_item->SetEnabled(true);
		_message_item->SetEnabled(false);

		sprintf(buffer, B_TRANSLATE("Group chat settings"));
		_change_user_item->SetLabel(buffer);
		_change_user_item->SetEnabled(true);

		_remove_user_item->SetLabel(B_TRANSLATE("Leave chat"));
		_remove_user_item->SetEnabled(true);

		_user_info_item->SetEnabled(false);

		_popup->AddItem(_availability);
	} else {
		// if not
		_chat_item->SetEnabled(!jid.empty());
		_message_item->SetEnabled(false);

		sprintf(buffer, B_TRANSLATE("Edit buddy"));
		_change_user_item->SetLabel(buffer);
		_change_user_item->SetEnabled(false);

		sprintf(buffer, B_TRANSLATE("Remove buddy"));
		_remove_user_item->SetLabel(buffer);
		_remove_user_item->SetEnabled(false);

		_user_info_item->SetEnabled(false);
	}
}

std::string RosterView::SelectedJid(bool& group) const {
    auto* item = ItemAt(CurrentSelection());
    if (auto* recent = dynamic_cast<ConversationItem*>(item)) { group = recent->summary.group; return recent->summary.jid; }
    if (auto* contact = dynamic_cast<RosterItem*>(item)) { group = false; return contact->GetUserID()->JID().bare(); }
    if (auto* room = dynamic_cast<BookmarkItem*>(item)) { group = true; return room->GetUserID().bare(); }
    return "";
}
void RosterView::SetMode(int mode) { fMode = mode; UpdateRoster(); }
void RosterView::SetAccount(const std::string& account) {
    if (account == fAccount) return;
    fAccount = account; fRecent.Account(account); fRooms.clear();
    BPath path; find_directory(B_USER_SETTINGS_DIRECTORY, &path);
    path.Append("jabber-for-beos");
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(account.data()), account.size(), digest);
    std::string filename = "recents-";
    for (auto c : digest) { char hex[3]; snprintf(hex, sizeof(hex), "%02x", c); filename += hex; }
    path.Append(filename.c_str());
    // Keep both predecessor formats intact for data-preserving rollback.
    fRecentPath = std::string(path.Path()) + "-v3";
    if(access(fRecentPath.c_str(),F_OK)==0)fRecent.Load(fRecentPath);
    else if(access((std::string(path.Path())+"-v2").c_str(),F_OK)==0)fRecent.Load(std::string(path.Path())+"-v2");
    else fRecent.Load(path.Path());
    UpdateRoster();
}
void RosterView::RecordActivity(BMessage* message) {
    Conversations::Summary item;
    item.jid = message->GetString("jid", ""); item.name = message->GetString("name", "");
    item.preview = message->GetString("preview", ""); item.group = message->GetBool("group", false);
    item.activity = message->GetInt64("time", 0); item.lastMessageId = message->GetString("id", "");
    if (fRecent.Record(item, message->GetBool("incoming", false), message->GetBool("visible", false), message->GetBool("archive", false))) {
        if (!fRecentPath.empty() && !fRecent.Save(fRecentPath)) fprintf(stderr, "Could not save conversation summaries\n");
        UpdateRoster();
    }
}
void RosterView::MarkRead(const std::string& jid) {
    const auto* item = fRecent.Find(jid);
    if (!item || item->unread == 0) return;
    fRecent.Read(jid); if (!fRecentPath.empty()) fRecent.Save(fRecentPath); UpdateRoster();
}
void RosterView::UpdateRoster() {
    bool group = false;
    auto selected = SelectedJid(group);
    // Suppress selection messages while rebuilding. Restoring selection must
    // not reopen a chat or steal focus on incoming activity.
    SetSelectionMessage(nullptr);
    while (CountItems()) delete RemoveItem(int32(0));
    JRoster* roster = JRoster::Instance();
    roster->Lock();
    std::vector<const UserID*> contacts;
    for (auto i = roster->BeginIterator(); i != roster->EndIterator(); ++i) {
        if ((*i)->IsUser()) {
            contacts.push_back(*i);
            fRecent.Rename((*i)->JID().bare(), (*i)->FriendlyName());
        }
    }
    auto recent = fRecent.Recent();
    if(fMode==0)recent.erase(std::remove_if(recent.begin(),recent.end(),[](const auto& item){return item.hiddenAt!=0;}),recent.end());
    const auto isFavorite = [this](const std::string& jid) {
        const auto* item = fRecent.Find(jid);
        return item && item->favorite;
    };
    if (std::any_of(recent.begin(), recent.end(), [](const auto& item) { return item.favorite; })) {
        auto* heading = new BStringItem(B_TRANSLATE("Favorites"));
        heading->SetEnabled(false); AddItem(heading);
        for (const auto& item : recent) {
            if (!item.favorite) continue;
            // Preserve contact/group management actions in their respective
            // navigation modes, even after the row moves into Favorites.
            if (fMode == 2 && item.group && fRooms.count(item.jid)) {
                AddItem(new BookmarkItem(gloox::JID(item.jid), item.name.c_str()));
            } else {
                auto contact = std::find_if(contacts.begin(), contacts.end(), [&item](const auto* user) {
                    return user->JID().bare() == item.jid;
                });
                if (fMode == 1 && !item.group && contact != contacts.end()) AddItem(new RosterItem(*contact));
                else AddItem(new ConversationItem(item));
            }
        }
        auto* section = new BStringItem(fMode == 0 ? B_TRANSLATE("Conversations")
            : fMode == 1 ? B_TRANSLATE("Contacts") : B_TRANSLATE("Groups"));
        section->SetEnabled(false); AddItem(section);
    }
    if (fMode == 0) {
        for (const auto& item : recent) if (!item.favorite) AddItem(new ConversationItem(item));
    } else if (fMode == 1) {
        std::sort(contacts.begin(), contacts.end(), [](const auto* a, const auto* b) {
            auto an = Conversations::FriendlyName(a->JID().bare(), a->FriendlyName(), false);
            auto bn = Conversations::FriendlyName(b->JID().bare(), b->FriendlyName(), false);
            return an == bn ? a->JID().bare() < b->JID().bare() : strcasecmp(an.c_str(), bn.c_str()) < 0;
        });
        for (auto* contact : contacts) if (!isFavorite(contact->JID().bare())) AddItem(new RosterItem(contact));
    } else {
        std::vector<std::pair<std::string, std::string>> rooms(fRooms.begin(), fRooms.end());
        std::sort(rooms.begin(), rooms.end(), [](const auto& a, const auto& b) { return a.second == b.second ? a.first < b.first : a.second < b.second; });
        for (const auto& room : rooms) if (!isFavorite(room.first)) AddItem(new BookmarkItem(gloox::JID(room.first), room.second.c_str()));
    }
    roster->Unlock();
    if (CountItems() == 0) {
        Conversations::Summary empty;
        empty.name = fMode == 0 ? "No conversations yet" : fMode == 1 ? "No contacts yet" : "No groups yet";
        empty.preview = fMode == 0 ? "Choose Contacts or Groups to start" : "Waiting for account metadata";
        auto* item = new ConversationItem(empty);
        item->SetEnabled(false);
        AddItem(item);
    }
    for (int32 i = 0; i < CountItems(); ++i) {
        std::string jid;
        if (auto* item = dynamic_cast<ConversationItem*>(ItemAt(i))) jid = item->summary.jid;
        if (auto* item = dynamic_cast<RosterItem*>(ItemAt(i))) jid = item->GetUserID()->JID().bare();
        if (auto* item = dynamic_cast<BookmarkItem*>(ItemAt(i))) jid = item->GetUserID().bare();
        if (jid == selected) Select(i);
    }
    SetSelectionMessage(new BMessage(kRosterSelectionChanged));
    Invalidate();
}

std::vector<Conversations::Summary> RosterView::ForwardTargets() const {
    std::map<std::string,Conversations::Summary> targets;
    for(const auto& item:fRecent.Recent())targets[item.jid]=item;
    for(const auto& [jid,name]:fRooms){auto& item=targets[jid];item.jid=jid;item.name=Conversations::FriendlyName(jid,name,true);item.group=true;}
    auto* roster=JRoster::Instance();roster->Lock();
    for(auto i=roster->BeginIterator();i!=roster->EndIterator();++i) {
        const auto jid=(*i)->JID().bare();if(jid.empty())continue;
        auto& item=targets[jid];item.jid=jid;item.name=Conversations::FriendlyName(jid,(*i)->FriendlyName(),false);
    }
    roster->Unlock();
    std::vector<Conversations::Summary> result;for(const auto& [jid,item]:targets)result.push_back(item);
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.name<b.name;});return result;
}
