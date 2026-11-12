//////////////////////////////////////////////////
// Blabber [LoginPreferencesView.h]
//     Allows users to edit login-related user
//     preferences.
//////////////////////////////////////////////////

#ifndef LOGIN_PREFERENCES_VIEW_H
#define LOGIN_PREFERENCES_VIEW_H

#include <interface/Box.h>
#include <interface/CheckBox.h>
#include <interface/TextControl.h>
#include <interface/GroupView.h>


class LoginPreferencesView : public BGroupView {
public:
	               LoginPreferencesView();
	virtual       ~LoginPreferencesView();
  
	void           AttachedToWindow();
	   
	void           UpdateFile();

private:
	BBox         *_surrounding;
	BCheckBox    *_auto_login;
	BCheckBox    *_focus_on_chat;

	BBox         *_surrounding_options;
	BCheckBox    *_show_timestamp;
	BCheckBox    *_show_all_chat;
	BCheckBox    *_double_click;

	BBox         *_surrounding_groupchat;
	BTextControl *_name;
	
	BBox         *_surrounding_chatlog;
	BCheckBox    *_autoopen_chatlog;
	BTextControl *_chatlog_path;
};

#endif

