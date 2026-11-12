//////////////////////////////////////////////////
// Blabber [TalkListItem.h]
//////////////////////////////////////////////////

#ifndef TALK_LIST_ITEM
#define TALK_LIST_ITEM

#ifndef _GRAPHICS_DEFS_H
	#include <interface/GraphicsDefs.h>
#endif

#ifndef _LIST_ITEM_H
	#include <interface/ListItem.h>
#endif

#ifndef _VIEW_H
	#include <interface/View.h>
#endif

class TalkListItem : public BStringItem {
public:
         	   TalkListItem(const char *user, const char *text, rgb_color name_color);
	virtual   ~TalkListItem();

	void       DrawItem(BView *owner, BRect frame, bool complete = false);
	
private:
	rgb_color    _name_color;
	const char  *_user;
};

#endif