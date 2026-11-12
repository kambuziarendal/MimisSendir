//////////////////////////////////////////////////
// Blabber [OnlineStatusItem.h]
//     Represents an online status item for
//     changing your online status.
//////////////////////////////////////////////////

#ifndef ONLINE_STATUS_ITEM_H
#define ONLINE_STATUS_ITEM_H

#ifndef _BITMAP_H
	#include <interface/Bitmap.h>
#endif

#ifndef _MENU_ITEM_H
	#include <interface/MenuItem.h>
#endif

class OnlineStatusItem : public BMenuItem {
public:
	              OnlineStatusItem(const char *resource, const char *label, BMessage *msg);
       	         ~OnlineStatusItem();
	
	virtual void  DrawContent();
	virtual void  GetContentSize(float *width, float *height);
	
private:
	BBitmap     *_resource;
	const char  *_label;
};

#endif
