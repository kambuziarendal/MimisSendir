//////////////////////////////////////////////////
// Jabber [PeopleListItem.h]
//     A listing of an existing user
//////////////////////////////////////////////////

#ifndef PEOPLE_LIST_ITEM_H
#define PEOPLE_LIST_ITEM_H

#include <string>

#include <ListItem.h>
#include <String.h>

#include <gloox/gloox.h>
#include <gloox/jid.h>
#include <gloox/presence.h>


class PeopleListItem : public BListItem
{
public:
											PeopleListItem(std::string user,
												gloox::MUCRoomAffiliation, gloox::MUCRoomRole,
												gloox::Presence::PresenceType, BString realJID,
												BString status);
											~PeopleListItem();

	virtual	void							DrawItem(BView *owner, BRect rect, bool complete);
	virtual	void							Update(BView *owner, const BFont *font);

	void Set(gloox::MUCRoomAffiliation, gloox::MUCRoomRole, gloox::Presence::PresenceType, BString status);
	void SetMarked(bool);

	const	std::string&					Nickname() const;

	/** @return The Occupant JID as defined in XEP-0045 (room@service/nick) */
	const	std::string&					OccupantJID() const;

private:
			gloox::JID						fOccupantJID;
			gloox::JID						fRealJID;
			gloox::MUCRoomAffiliation		fAffiliation;
			gloox::MUCRoomRole				fRole;
			gloox::Presence::PresenceType	fPresence;

			BString							fStatus;
			bool							fMarked;

	friend class PeopleListView;
};

#endif

