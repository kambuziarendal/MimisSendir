//////////////////////////////////////////////////
// Blabber [RosterItem.cpp]
//////////////////////////////////////////////////

#include "RosterItem.h"
#include "ui/ConversationRow.h"

#include "support/AppLocation.h"

#include <TranslationUtils.h>

#include "ui/HVIFUtil.h"

BBitmap *RosterItem::_kinda_online_icon = NULL;
BBitmap *RosterItem::_offline_icon      = NULL;
BBitmap *RosterItem::_online_icon       = NULL;
BBitmap *RosterItem::_unknown_icon      = NULL;


RosterItem::RosterItem(const UserID *userid)
	: BStringItem(userid->FriendlyName().c_str())
	, fAvatar(NULL)
{
	fSnapshot.reset(new UserID(userid->JID()));
	fSnapshot->SetSubscriptionStatus(userid->SubscriptionStatus());
	fSnapshot->SetOnlineStatus(userid->OnlineStatus());
	fSnapshot->SetExactOnlineStatus(userid->ExactOnlineStatus());
	fSnapshot->SetMoreExactOnlineStatus(userid->MoreExactOnlineStatus());
	_userid           = fSnapshot.get();
	_is_stale_pointer = false;
}


RosterItem::~RosterItem()
{
	delete fAvatar;
}


void RosterItem::SetAvatar(BBitmap* avatar)
{
	delete fAvatar;
	fAvatar = avatar;
}


void RosterItem::DrawItem(BView* owner, BRect frame, bool) {
    if (StalePointer()) return;
    Conversations::Summary item;
    item.jid = _userid->JID().bare();
    item.name = Conversations::FriendlyName(item.jid, Text(), false);
    item.preview = "Contact";
    DrawConversationRow(owner, frame, IsSelected(), item, fAvatar);
}

void RosterItem::Update(BView *owner, const BFont *font) {
	BListItem::Update(owner, font);

	font_height fh;
	owner->GetFontHeight(&fh);


	// set height to accomodate graphics and text
	SetHeight(58);
}

bool RosterItem::StalePointer() const {
	return _is_stale_pointer;
}

const UserID *RosterItem::GetUserID() const {
	if (StalePointer()) {
		return NULL;
	} else {
		return _userid;
	}
}

void RosterItem::SetStalePointer(bool is_stale) {
	_is_stale_pointer = is_stale;
}
