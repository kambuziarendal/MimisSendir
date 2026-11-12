//////////////////////////////////////////////////
// Blabber [RosterView.h]
//     A graphical view of the buddy list.
//////////////////////////////////////////////////

#ifndef ROSTER_VIEW_H
#define ROSTER_VIEW_H

#include <map>
#include "support/ConversationStore.h"
#include "ui/EdgeScrollView.h"

#include <interface/ListItem.h>
#include <interface/OutlineListView.h>
#include <interface/PopUpMenu.h>

#include "ui/BookmarkItem.h"
#include "ui/RosterSuperitem.h"

#include "JRoster.h"
#include "RosterItem.h"
#include "TransportItem.h"
#include "UserID.h"

class RosterView : public EdgeScrollListView {
public:
	typedef std::map<BListItem *, UserID::online_status>  ItemToStatusMap;
	typedef std::map<UserID::online_status, BListItem *>  StatusToItemMap;

public:
	     	            RosterView();
		               ~RosterView();

	// sort algorithm used before outline list view used
	static int          ListComparison(const void *a, const void *b);

	void                AttachedToWindow();
	RosterItem         *CurrentItemSelection();
	BookmarkItem       *CurrentBookmarkSelection();
	void                MouseDown(BPoint point);
	void                RemoveSelected();
	void                MessageReceived(BMessage*) final;

	void                LinkUser(const UserID *added_user);
	void                LinkTransport(const UserID *added_transport);
	void                LinkBookmark(const gloox::JID& added_bookmark, BString nick);
	void                UnlinkUser(const gloox::JID& removed_user);
	void                UnlinkTransport(const gloox::JID& removed_transport);
	void                UnlinkBookmark(const gloox::JID& removed_bookmark);

	int32               FindUser(const gloox::JID& compare_user);
	int32               FindTransport(const gloox::JID& compare_transport);
	int32               FindBookmark(const gloox::JID& compare_jid);

	void                UpdatePopUpMenu();
	void                UpdateRoster();

    void SetMode(int mode);
    void SetAccount(const std::string& account);
    void RecordActivity(BMessage* message);
    void MarkRead(const std::string& jid);
    std::string SelectedJid(bool& group) const;
    std::vector<Conversations::Summary> ForwardTargets() const;
private:
    Conversations::Store fRecent;
    std::string fRecentPath;
    std::string fAccount;
    std::map<std::string, std::string> fRooms;
    int fMode = 0;
    bool fBookmarkBatch = false;
    BPopUpMenu* _popup = nullptr;
    BMenuItem* fHideItem = nullptr;
    BMenuItem* fFavoriteItem = nullptr;
    BMenuItem *_message_item, *_chat_item, *_change_user_item, *_remove_user_item, *_user_info_item;
    BMenu* _presence;
    BMenuItem *_subscribe_presence, *_unsubscribe_presence;
    BMenu* _availability;
};
#endif
