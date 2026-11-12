//////////////////////////////////////////////////
// Blabber [TransportPreferencesView.h]
//     Presents user preferences for
//     editing Transport accounts.
//////////////////////////////////////////////////

#ifndef TRANSPORT_PREFERENCES_VIEW_H
#define TRANSPORT_PREFERENCES_VIEW_H

#ifndef __STRING__
	#include <string>
#endif

#ifndef _BOX_H
	#include <interface/Box.h>
#endif

#ifndef _BUTTON_H
	#include <interface/Button.h>
#endif

#ifndef _MENU_FIELD_H
	#include <interface/MenuField.h>
#endif

#ifndef _MENU_ITEM_H
	#include <interface/MenuItem.h>
#endif

#ifndef _POP_UP_MENU_H
	#include <interface/PopUpMenu.h>
#endif

#ifndef _TEXT_CONTROL_H
	#include <interface/TextControl.h>
#endif

#ifndef _TEXT_VIEW_H
	#include <interface/TextView.h>
#endif

#ifndef _VIEW_H
	#include <interface/View.h>
#endif

class TransportPreferencesView : public BView {
public:
	               TransportPreferencesView();
	virtual       ~TransportPreferencesView();

	void           AttachedToWindow();
	void           MessageReceived(BMessage *msg);
	
	void           UpdateFile();

private:
	BMenuField   *_agent_list;
	BPopUpMenu   *_agent_entries;
	
	BTextView    *_transport_id_info;
	BTextControl *_username;
	BTextControl *_password;
	
	BButton      *_register;
	BButton      *_unregister;
	
	bool          _current_transport_registered;
	std::string   _curr_transport;
};

#endif
