//////////////////////////////////////////////////
// Blabber [BlabberMainWindow.h]
//	 Central information display.
//////////////////////////////////////////////////

#pragma once

#include <Button.h>
#include <CardLayout.h>
#include <CheckBox.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <PopUpMenu.h>
#include <TabView.h>
#include <map>
#include <SplitView.h>
#include <TextControl.h>
#include <Window.h>

#include <gloox/instantmucroom.h>

#include "ui/PictureView.h"
#include "ui/ConnectionIndicator.h"
#include "ui/StatusView.h"

#include "jabber/RosterView.h"


class PopupTextControl;
class TalkView;

enum {
	kResetWindow = 'Wrst',
	kAddTalkView = 'Watv',
    kDeliverTalkNotice = 'Wtno',
    kLoadCredentials = 'kLod'
};

class BlabberMainWindow : public BWindow {
public:
	static BlabberMainWindow	*Instance();
								~BlabberMainWindow();

	virtual void				MessageReceived(BMessage *msg);
	virtual void				MenusBeginning();
	virtual bool				QuitRequested();

	bool						ValidateLogin();
	void						ShowLogin();
	void CloseTalkViews();
    void RefreshConversationStatus();
	void						SetCustomStatus(std::string status);

protected:
								BlabberMainWindow(BRect frame);

private:
	void						FlagBookmarkItem(const gloox::JID& room, uint32 flags);
	void						RefreshBookmark(const char* const jid, const char* const newName);
	void						AddTalkView(TalkView* view, bool activate = true);

	void						LoginSetNoAccount();
    void LoadCredentials(bool initial = false);

private:
	// singleton
	static BlabberMainWindow *_instance;

	// GUI
	BView					*_login_full_view;

	PictureView				*_login_bulb;
	BTextControl			*_login_realname;
	PopupTextControl		*_login_username;
	std::string				_login_resource;
	BTextControl			*_login_password;
	BTextView				*fAnonymousLimitationsWarning;
	BStringView				*fLoginErrorMessage;

	BCheckBox				*_login_auto_login;

	BButton					*_login_login;
	BButton					*_login_delete;

	BMenuItem				*_disconnect_item;
	BMenuItem				*_about_item;
	BMenuItem				*_quit_item;
	BMenuItem				*_add_buddy_item;
	BMenuItem				*_change_buddy_item;
	BMenuItem				*_remove_buddy_item;
	BMenuItem				*_user_info_item;
	BMenuItem				*_chat_item;
	BMenuItem				*_away_item;
	BMenuItem				*_dnd_item;
	BMenuItem				*_xa_item;
	BMenuItem				*_school_item;
	BMenuItem				*_work_item;
	BMenuItem				*_lunch_item;
	BMenuItem				*_dinner_item;
	BMenuItem				*_sleep_item;
	BMenuItem				*_custom_item;

	BMenuItem				*_send_chat_item;
	BMenuItem				*_send_groupchat_item;

	BMenuItem				*_preferences_item;

	BView					*_full_view;

	ConnectionIndicator* _status_view;

	RosterView				*_roster;

	BCardLayout*			fTalkCards;
    std::map<std::string,std::string> fPendingForwards;
	BSplitView* fSidebarSplit = nullptr;
    BButton* fSidebarToggle = nullptr;
    BMenu* fApplicationMenu = nullptr;
    BButton* fSettingsButton = nullptr;
    BButton* fNavigation[3] = {};
    StatusView* fConversationStatus = nullptr;
    StatusView* fConversationLag = nullptr;
	BMessageRunner* fLogoutTimer = nullptr;
    BMessageRunner* fRosterTimer = nullptr;
	bool fExplicitLogout = false;
    uint64 fCredentialRequest = 0;
    std::string fCredentialDraft;
    bool fAutoLoginPending = false;
};
