//////////////////////////////////////////////////
// Jabber [PeopleListItem.cpp]
//////////////////////////////////////////////////

#include "PeopleListItem.h"

#include <interface/Font.h>
#include <interface/View.h>

#include "jabber/JabberSpeak.h"

PeopleListItem::PeopleListItem(std::string occupant_jid, gloox::MUCRoomAffiliation affiliation,
	gloox::MUCRoomRole role, gloox::Presence::PresenceType presence, BString realJID, BString status)
	: BListItem()
	, fOccupantJID(occupant_jid)
	, fRealJID(realJID.String())
	, fAffiliation(affiliation)
	, fRole(role)
	, fPresence(presence)
	, fStatus(status)
	, fMarked(false)
{
	// TODO if appropriate, add popup menu allowing to interact with the user: send a whisper,
	// start a private chat, ...
	// Maybe also show the status message there if any
}


PeopleListItem::~PeopleListItem()
{
}


void
PeopleListItem::DrawItem(BView *owner, BRect frame, __attribute__((unused)) bool complete)
{
	if (IsSelected()) {
		owner->SetLowUIColor(B_LIST_SELECTED_BACKGROUND_COLOR);
		owner->SetHighUIColor(B_LIST_SELECTED_ITEM_TEXT_COLOR);
	} else {
		owner->SetLowUIColor(B_LIST_BACKGROUND_COLOR);
		owner->SetHighUIColor(B_LIST_ITEM_TEXT_COLOR);
	}
	
	if (fMarked)
		owner->SetFont(be_bold_font);
	else
		owner->SetFont(be_plain_font);

	// Preserve selection contrast and system dark/light colors. Away state uses
	// italic text rather than hardcoded colors that can disappear on dark themes.
	if (fPresence == gloox::Presence::Away || fPresence == gloox::Presence::XA
		|| fPresence == gloox::Presence::DND) {
		BFont font;
		owner->GetFont(&font);
		font.SetFace(font.Face() | B_ITALIC_FACE);
		owner->SetFont(&font);
	}

	// clear rectangle
	owner->FillRect(frame, B_SOLID_LOW);

	// construct text positioning
	font_height fh;
	owner->GetFontHeight(&fh);

	float height = fh.ascent + fh.descent;
	float vpos = frame.bottom - (frame.Height() - height) / 2 - fh.descent;

	// draw information
	owner->DrawString(Nickname().c_str(), BPoint(frame.left + 5.0, vpos));

	// TODO also handle role in addition to affiliation. Also, maybe use a nicer way to display
	// these with icons instead of legacy IRC style characters
	BPoint affiliationPos(frame.right - owner->StringWidth("@") - 5.0, vpos);
	if (fAffiliation == gloox::AffiliationOwner)
		owner->DrawString("&", affiliationPos);
	else if (fAffiliation == gloox::AffiliationAdmin)
		owner->DrawString("@", affiliationPos);
	else if (fRole == gloox::RoleModerator)
		owner->DrawString("%", affiliationPos);
	else if (fRole == gloox::RoleVisitor)
		owner->DrawString("-", affiliationPos);
}


void
PeopleListItem::Update(BView *owner, const BFont *font)
{
	BListItem::Update(owner, font);

	// set height to accomodate graphics and text
	font_height fh;
	owner->GetFontHeight(&fh);
	float height = fh.ascent + fh.descent;
	SetHeight(height);
}


void
PeopleListItem::Set(gloox::MUCRoomAffiliation affiliation, gloox::MUCRoomRole role,
	gloox::Presence::PresenceType presence, BString status)
{
	fAffiliation = affiliation;
	fRole = role;
	fPresence = presence;
	fStatus = status;
}

void
PeopleListItem::SetMarked(bool marked)
{
	fMarked = marked;
}

const std::string&
PeopleListItem::Nickname() const
{
	return fOccupantJID.resource();
}


const std::string&
PeopleListItem::OccupantJID() const
{
	return fOccupantJID.full();
}
