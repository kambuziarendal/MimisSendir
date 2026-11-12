#include "ui/ActionIconButton.h"
#include "ui/SidebarButton.h"
//////////////////////////////////////////////////
// Blabber [BlabberMainWindow.cpp]
//////////////////////////////////////////////////

#include "MainWindow.h"

#include <gloox/jid.h>

#include "support/AppLocation.h"

#include "ui/AddBuddyWindow.h"
#include "ui/BuddyInfoWindow.h"
#include "ui/ChangeNameWindow.h"
#include "ui/GroupChatSettingsWindow.h"
#include "ui/JoinGroupChat.h"
#include "ui/ModalAlertFactory.h"
#include "ui/PopupTextControl.h"
#include "ui/RegisterAccountWindow.h"
#include "ui/RotateChatFilter.h"
#include "ui/TalkView.h"

#include "jabber/BlabberSettings.h"
#include "jabber/CustomStatusWindow.h"
#include "jabber/GenericFunctions.h"
#include "jabber/JabberSpeak.h"
#include "jabber/Messages.h"
#include "jabber/MessageRepeater.h"
#include "jabber/PreferencesWindow.h"
#include "jabber/RosterItem.h"
#include "jabber/SendTalkWindow.h"
#include "jabber/TalkManager.h"

#include <Application.h>
#include <CardLayout.h>
#include <Catalog.h>
#include <FindDirectory.h>
#include <GroupLayout.h>
#include <GroupView.h>
#include "support/CredentialStore.h"
#include <LayoutBuilder.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <be_apps/NetPositive/NetPositive.h>
#include <Path.h>
#include <RadioButton.h>
#include <Roster.h>
#include <ScrollView.h>
#include <String.h>
#include <private/interface/ToolTip.h>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <stdlib.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "MainWindow"

enum {
	kCreateAccount = 'crea',
	kSelectExistingAccount = 'sexa',

	kRadioCreateAccount = 'rcra',
	kRadioUseAccount = 'ruea',
	kRadioNoAccount = 'rnoa'
};


// Shared native-button behavior and the approved Haiku-inspired icon family.
class SettingsButton final : public ActionIconButton {
public:
    SettingsButton() : ActionIconButton("application-menu", B_TRANSLATE("Settings and menu"),
        new BMessage('aMnu'), "settings", 24, 28) {}
};

// Keep the hidden native menu bar as the owner of commands and shortcuts.
// Each popup is an independent snapshot so asynchronous tracking never borrows
// items that the window may update or destroy during logout/closure.
static void CopyMenu(BMenu* source,BMenu* destination,BHandler* fallback)
{
    for(int32 i=0;i<source->CountItems();++i){
        auto* item=source->ItemAt(i);BMenuItem* copy=nullptr;
        if(item->Submenu()){
            auto* submenu=new BMenu(item->Label());
            CopyMenu(item->Submenu(),submenu,fallback);
            copy=new BMenuItem(submenu);
        }else if(!item->Label()){
            destination->AddSeparatorItem();continue;
        }else{
            copy=new BMenuItem(item->Label(),item->Message()?new BMessage(*item->Message()):nullptr);
            copy->SetTarget(item->Target()?item->Target():fallback);
        }
        copy->SetEnabled(item->IsEnabled());copy->SetMarked(item->IsMarked());
        destination->AddItem(copy);
    }
}

BlabberMainWindow *BlabberMainWindow::_instance = NULL;

static const char* kCredentialLoading =
    "Loading saved password. Check the Haiku keyring prompt if requested.";

BlabberMainWindow *BlabberMainWindow::Instance() {
	static bool done = false;
	if (done)
		return _instance;

	done = true;

	BlabberSettings *settings = BlabberSettings::Instance();

	if (!settings->Data("no-window-on-startup")) {
		float main_window_width, main_window_height;

		// determine what the width and height of the window should be
		auto w = settings->Data("main-window-width");
		auto h = settings->Data("main-window-height");
		if (w && h) {
			main_window_width  = atof(w->c_str());
			main_window_height = atof(h->c_str());
		} else {
			// default
			main_window_width  = 1000;
			main_window_height = 680;
		}

		// create window frame position
		BRect frame(GenericFunctions::CenteredFrame(main_window_width, main_window_height));

		// poition window to last known position
		w = settings->Data("main-window-left");
		h = settings->Data("main-window-top");
		if (w && h) {
			frame.OffsetTo(BPoint(atof(w->c_str()), atof(h->c_str())));
		}

		// create window singleton
		_instance = new BlabberMainWindow(frame);
	}

	return _instance;
}

BlabberMainWindow::~BlabberMainWindow() {
	delete fLogoutTimer;
    delete fRosterTimer;
	// remove self from message family
	MessageRepeater::Instance()->RemoveTarget(this);

	_instance = NULL;
}


void
BlabberMainWindow::LoginSetNoAccount()
{
	while (_login_realname->IsHidden())
		_login_realname->Show();
	while (!_login_username->IsHidden())
		_login_username->Hide();
	while (!_login_password->IsHidden())
		_login_password->Hide();
	while (_login_auto_login->IsHidden())
		_login_auto_login->Show();
	while (!_login_delete->IsHidden())
		_login_delete->Hide();
	while (fAnonymousLimitationsWarning->IsHidden())
		fAnonymousLimitationsWarning->Show();
	_login_login->SetMessage(new BMessage(JAB_LOGIN));
	_login_login->SetLabel(B_TRANSLATE("Connect"));
}


void BlabberMainWindow::MessageReceived(BMessage *msg) {
	switch (msg->what) {

		case kWindowList:
		case 'fFav':
		{
			_roster->MessageReceived(msg);
			break;
		}

		case kCreateAccount:
		{
			(new RegisterAccountWindow(this))->Show();
			break;
		}

        case kLoadCredentials:
            LoadCredentials(true);
            break;
        case Credentials::kResult: {
            const auto operation = std::string(msg->GetString("operation", ""));
            const auto status = msg->GetInt32("status", B_ERROR);
            if (operation == "load") {
                if (msg->GetUInt64("request", 0) != fCredentialRequest
                    || std::string(msg->GetString("account", "")) != _login_username->Text()
                    || fCredentialDraft != _login_password->Text()
                    || !JabberSpeak::Instance()->IsReadyForConnection()) break;
                const bool initial = msg->GetBool("initial", false);
                const char* saved;
                for (int32 index = 0; msg->FindString("saved-account", index, &saved) == B_OK; ++index)
                    _login_username->AddEntry(saved);
                if (status == B_OK) {
                    _login_password->SetText(msg->GetString("password", ""));
                    _login_resource = msg->GetString("resource", "");
                    BlabberSettings::Instance()->RemoveData("import-legacy-credential");
                    BlabberSettings::Instance()->WriteToFile();
                }
                // Connection errors may arrive while the asynchronous keyring load
                // is pending. Only replace the hint owned by this operation.
                if (std::string(fLoginErrorMessage->Text()) == kCredentialLoading)
                    fLoginErrorMessage->SetText(status == B_OK ? "" :
                        "Saved password unavailable. Check the Haiku keyring prompt or enter it manually.");
                if (initial && fAutoLoginPending && status == B_OK
                    && _login_auto_login->Value() && !fExplicitLogout)
                    PostMessage(JAB_LOGIN);
                fAutoLoginPending = false;
            } else if (operation == "remove") {
                if (status == B_OK) {
                    _login_username->RemoveEntry(msg->GetString("account", ""));
                    BlabberSettings::Instance()->RemoveData("import-legacy-credential");
                    BlabberSettings::Instance()->WriteToFile();
                }
                else fLoginErrorMessage->SetText("Account was not removed. Check keyring access.");
            } else if (operation == "save" && status == B_OK) {
                const auto last = BlabberSettings::Instance()->Data("last-login");
                if (last && *last == msg->GetString("account", "")) {
                    BlabberSettings::Instance()->RemoveData("last-password");
                    BlabberSettings::Instance()->WriteToFile();
                }
            } else if (operation == "save" && status != B_OK) {
                ModalAlertFactory::NonModalAlert(
                    "The connection works, but the password was not saved. An existing key was kept unchanged. Check Haiku keyring access before the next login.",
                    B_TRANSLATE("OK"));
            }
            break;
        }
        case kSelectExistingAccount:
            _login_password->SetText("");
            _login_resource.clear();
            LoadCredentials();
            break;

		case kRadioCreateAccount:
		{
			while (!_login_realname->IsHidden())
				_login_realname->Hide();
			while (!_login_username->IsHidden())
				_login_username->Hide();
			while (!_login_password->IsHidden())
				_login_password->Hide();
			while (!_login_auto_login->IsHidden())
				_login_auto_login->Hide();
			while (!_login_delete->IsHidden())
				_login_delete->Hide();
			while (!fAnonymousLimitationsWarning->IsHidden())
				fAnonymousLimitationsWarning->Hide();
			_login_login->SetMessage(new BMessage(kCreateAccount));
			_login_login->SetLabel(B_TRANSLATE("Create a new account"));
			break;
		}

		case kRadioUseAccount:
		{
			while (_login_realname->IsHidden())
				_login_realname->Show();
			while (_login_username->IsHidden())
				_login_username->Show();
			while (_login_password->IsHidden())
				_login_password->Show();
			while (_login_auto_login->IsHidden())
				_login_auto_login->Show();
			while (_login_delete->IsHidden())
				_login_delete->Show();
			while (!fAnonymousLimitationsWarning->IsHidden())
				fAnonymousLimitationsWarning->Hide();
			_login_login->SetMessage(new BMessage(JAB_LOGIN));
			_login_login->SetLabel(B_TRANSLATE("Login"));
			break;
		}

		case kRadioNoAccount:
		{
			LoginSetNoAccount();
			break;
		}

		// transplanted from LoginWindow
		case JAB_LOGIN: {
			if (!JabberSpeak::Instance()->IsReadyForConnection()) break;
			fExplicitLogout = false;
			fLoginErrorMessage->SetText("");
			_status_view->SetState(ConnectionIndicator::Connecting,B_TRANSLATE("Connecting" B_UTF8_ELLIPSIS));
			bool anonymous = _login_password->IsHidden();

			// must pass validation
			if (!anonymous && !ValidateLogin()) {
				break;
			}


			// switch out views
			_login_login->MakeDefault(false);
			BCardLayout* cl = (BCardLayout*)GetLayout();
			cl->SetVisibleItem((int32)0);

			// connect with current username
			if (anonymous) {
				JabberSpeak::Instance()->SendConnect(_login_realname->Text());
			} else {
				gloox::JID myjid(_login_username->Text());
				if (_login_resource.empty()) {
					char buffer[] = "MimisSendir-XXXXXX";
					mktemp(buffer);
					_login_resource = buffer;
				}
				myjid.setResource(_login_resource);
				JabberSpeak::Instance()->SendConnect(myjid, _login_password->Text(), _login_realname->Text());
			}

			break;
		}

        case 'DELA': {
            BMessage request;
            request.AddString("operation", "remove");
            request.AddString("account", _login_username->Text());
            request.AddString("resource", _login_resource.c_str());
            if (!Credentials::Submit(request, BMessenger(this)))
                fLoginErrorMessage->SetText("Keyring is busy. Finish its pending prompt before trying again.");
            break;
        }

		case B_ABOUT_REQUESTED: {
			be_app->PostMessage(B_ABOUT_REQUESTED);
			break;
		}

		case JAB_QUIT: {
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;
		}

		case JAB_PREFERENCES: {
			PreferencesWindow::Instance()->Show();
			PreferencesWindow::Instance()->Activate();
			break;
		}

        case JAB_SETTINGS_SAVED: {
            // Settings can also be opened from the login screen. Keep its draft
            // aligned so the next login does not overwrite the saved preference.
            auto* settings = BlabberSettings::Instance();
            if (msg->GetBool("display-name", false))
                _login_realname->SetText(settings->Data("last-realname").value_or("Me").c_str());
            if (msg->GetBool("auto-login", false))
                _login_auto_login->SetValue(settings->Tag("auto-login") ? B_CONTROL_ON : B_CONTROL_OFF);
            break;
        }

		case JAB_RECONNECTING: {
			// we are connecting
			_status_view->SetState(ConnectionIndicator::Connecting,B_TRANSLATE("Reconnecting" B_UTF8_ELLIPSIS));

			break;
		}

        case kBridgeStatus:
            if (!fExplicitLogout && JabberSpeak::Instance()->IsConnected()
                && msg->GetUInt64("generation", 0) == JabberSpeak::Instance()->ConnectionGeneration())
                _status_view->SetState(msg->GetBool("ready",false)?ConnectionIndicator::Ready:ConnectionIndicator::Limited,
                    msg->GetBool("ready", false)
                    ? "XMPP connected. Signal reports ready."
                    : "XMPP connected. Signal not ready; sending paused.");
            break;
		case JAB_LOGGED_IN: {
			_roster->SetAccount(JabberSpeak::Instance()->GlooxClient()->jid().bare());
			if (fExplicitLogout) {
				JabberSpeak::Instance()->SendDisconnect();
				break;
			}
			// we just logged in
			BString statusText;
			statusText.SetToFormat(B_TRANSLATE("XMPP connected as %s. Signal readiness unconfirmed."), JabberSpeak::Instance()->CurrentRealName().c_str());
            _status_view->SetState(JabberSpeak::Instance()->BridgeReady()?ConnectionIndicator::Ready:ConnectionIndicator::Limited,
                JabberSpeak::Instance()->BridgeReady()
                ? "XMPP connected. Signal reports ready." : statusText.String());

			if (_login_username->TextLength() > 0) {
				// save these settings
				BlabberSettings::Instance()->SetData("last-realname", _login_realname->Text());
				BlabberSettings::Instance()->SetData("last-login", _login_username->Text());

                BMessage save;
                save.AddString("operation", "save");
                save.AddString("account", _login_username->Text());
                save.AddString("password", _login_password->Text());
                save.AddString("resource", _login_resource.c_str());
                if (!Credentials::Submit(save, BMessenger(this)))
                    _status_view->SetDetail("Connected. Keyring is busy; password was not saved.");

				BlabberSettings::Instance()->SetTag("auto-login", _login_auto_login->Value());
				BlabberSettings::Instance()->WriteToFile();
			}

			break;
		}

		case JAB_DISCONNECT: {
			if (fExplicitLogout) break;
			fExplicitLogout = true;
			// Explicit logout changes startup intent, never the stored credential.
			_login_auto_login->SetValue(B_CONTROL_OFF);
			BlabberSettings::Instance()->SetTag("auto-login", false);
			BlabberSettings::Instance()->WriteToFile();
			JabberSpeak::Instance()->SendDisconnect();
			_status_view->SetState(ConnectionIndicator::Connecting,B_TRANSLATE("Disconnecting…"));
			// Expose the login card immediately, but keep login gated on cleanup.
			((BCardLayout*)GetLayout())->SetVisibleItem(1);
			_login_login->SetEnabled(false);
			fLoginErrorMessage->SetText(B_TRANSLATE("Disconnecting…"));
			delete fLogoutTimer;
			BMessage timeout('lTim');
			timeout.AddUInt64("generation", JabberSpeak::Instance()->ConnectionGeneration());
			fLogoutTimer = new BMessageRunner(BMessenger(this), timeout, 10000000, 1);
			JabberSpeak::Instance()->Reset();
			break;
		}

		case 'lTim': {
			if (fExplicitLogout && !JabberSpeak::Instance()->IsReadyForConnection()
				&& msg->GetUInt64("generation", 0) == JabberSpeak::Instance()->ConnectionGeneration())
				fLoginErrorMessage->SetText(B_TRANSLATE("Connection is still closing. You can close the app; login becomes available when cleanup finishes."));
			break;
		}

		case kConnectionStopped: {
			// Reset closes old sessions and restores the login form. Preserve an
			// authentication/TLS error already delivered for this connection.
			BString failure(fExplicitLogout ? "" : fLoginErrorMessage->Text());
			if (!JabberSpeak::Instance()->HandleConnectionStopped(
				msg->GetUInt64("connection-generation", 0))) break;
			delete fLogoutTimer;
			fLogoutTimer = nullptr;
			_login_login->SetEnabled(true);
			if (!failure.IsEmpty()) fLoginErrorMessage->SetText(failure);
			_status_view->SetState(ConnectionIndicator::Offline,B_TRANSLATE("Disconnected"));
			be_app->PostMessage(kConnectionStopped);
			break;
		}

		case JAB_DISCONNECTED: {
            _status_view->SetState(ConnectionIndicator::Offline,B_TRANSLATE("Disconnected"));
			JRoster::Instance()->RefreshRoster();

			break;
		}

		case JAB_RIV:
		{
			std::string jabber_org = "http://github.com/haikuarchives/renga";

			char *argv[] = {const_cast<char *>(jabber_org.c_str()), NULL};
			if (!be_roster->IsRunning("text/html"))
				be_roster->Launch("text/html", 1, argv);
			else {
				BMessenger messenger("text/html");
				BMessage msg(B_NETPOSITIVE_OPEN_URL);
				msg.AddString("be:url", jabber_org.c_str());
				messenger.SendMessage(&msg);
			}

			break;
		}

		case JAB_JABBER_ORG:
		{
			std::string jabber_org = "http://www.jabber.org";

			char *argv[] = {const_cast<char *>(jabber_org.c_str()), NULL};
			if (!be_roster->IsRunning("text/html"))
				be_roster->Launch("text/html", 1, argv);
			else {
				BMessenger messenger("text/html");
				BMessage msg(B_NETPOSITIVE_OPEN_URL);
				msg.AddString("be:url", jabber_org.c_str());
				messenger.SendMessage(&msg);
			}

			break;
		}

		case JAB_FAQ:
		{
			std::string user_guide = "https://wiki.xmpp.org/web/FAQ";

			char *argv[] = {const_cast<char *>(user_guide.c_str()), NULL};
			if (!be_roster->IsRunning("text/html"))
				be_roster->Launch("text/html", 1, argv);
			else {
				BMessenger messenger("text/html");
				BMessage msg(B_NETPOSITIVE_OPEN_URL);
				msg.AddString("be:url", user_guide.c_str());
				messenger.SendMessage(&msg);
			}

			break;
		}

		case JAB_USER_GUIDE:
		{
			BPath path;
			find_directory(B_SYSTEM_DOCUMENTATION_DIRECTORY, &path, false);
			path.Append("packages/mimissendir/Renga/user_guide.html");

			BString resolved("file://");
			resolved += path.Path();
			char *argv[] = {const_cast<char*>(resolved.String()), NULL};
			if (!be_roster->IsRunning("text/html"))
				be_roster->Launch("text/html", 1, argv);
			else {
				BMessenger messenger("text/html");
				BMessage msg(B_NETPOSITIVE_OPEN_URL);
				msg.AddString("be:url", resolved);
				messenger.SendMessage(&msg);
			}

			break;
		}

        case BLAB_UPDATE_ROSTER: {
            // Presence bursts should not rebuild the whole list once per contact.
            if (!fRosterTimer) {
                BMessage refresh('rUpd');
                fRosterTimer = new BMessageRunner(BMessenger(this), &refresh, 75000, 1);
                if (fRosterTimer->InitCheck() != B_OK) {
                    delete fRosterTimer; fRosterTimer = nullptr;
                    _roster->UpdateRoster();
                }
            }
            break;
        }
        case 'rUpd':
            delete fRosterTimer; fRosterTimer = nullptr;
            _roster->UpdateRoster();
            break;

		case kHighlightRosterItem: {
			FlagBookmarkItem(gloox::JID(msg->FindString("room_jid")), msg->FindInt32("flags"));
			break;
		}

		case kRefreshBookmark: {
			if (msg->GetBool("delete", false)) _roster->UnlinkBookmark(gloox::JID(msg->GetString("jid", "")));
			else RefreshBookmark(msg->GetString("jid", ""), msg->GetString("newName", ""));
			break;
		}

		case JAB_SUBSCRIBE_PRESENCE: {
			RosterItem *item = _roster->CurrentItemSelection();

			// if there was a user selected (shouldn't be otherwise)
			if (item) {
				// don't send subscription request if we have it already
				if (!item->GetUserID()->HaveSubscriptionTo()) {
					JabberSpeak::Instance()->SendSubscriptionRequest(item->GetUserID()->JabberHandle());
				}
			}

			break;
		}

		case JAB_UNSUBSCRIBE_PRESENCE: {
			RosterItem *item = _roster->CurrentItemSelection();

			// if there was a user selected (shouldn't be otherwise)
			if (item) {
				// don't send unsubscription request if we don't have it already
				if (item->GetUserID()->HaveSubscriptionTo()) {
					JabberSpeak::Instance()->SendUnsubscriptionRequest(item->GetUserID()->Handle());
				}
			}

			break;
		}

		case 'sbar': {
			const bool collapsed = !fSidebarSplit->IsItemCollapsed(0);
			fSidebarSplit->SetItemCollapsed(0, collapsed);
            static_cast<SidebarButton*>(fSidebarToggle)->SetCollapsed(collapsed);
			break;
		}
        case 'tSta': RefreshConversationStatus();break;
        case 'aMnu': {
            MenusBeginning();
            auto* popup=new BPopUpMenu("MímisSendir menu",false,false);
            CopyMenu(fApplicationMenu,popup,this);
            popup->SetAsyncAutoDestruct(true);
            BPoint point=fSettingsButton->ConvertToScreen(BPoint(0,0));
            popup->Go(point,true,false,true);
            break;
        }
        case 'nRec':
        case 'nCon':
        case 'nGrp': {
            const int mode=msg->what=='nRec'?0:msg->what=='nCon'?1:2;
            _roster->SetMode(mode);
            for(int i=0;i<3;++i)static_cast<ActionIconButton*>(fNavigation[i])->SetSelected(i==mode);
            break;
        }
		case 'cAct': {
			if (msg->GetUInt64("generation", 0) != JabberSpeak::Instance()->ConnectionGeneration()) break;
			_roster->RecordActivity(msg);
			break;
		}
        case 'fSel': {
            if(msg->GetUInt64("generation",0)!=JabberSpeak::Instance()->ConnectionGeneration() || !JabberSpeak::Instance()->IsConnected())break;
            auto* menu=new BPopUpMenu("Forward to conversation");
            for(const auto& item:_roster->ForwardTargets()) {
                auto* action=new BMessage('fDst');action->AddString("body",msg->GetString("body",""));
                action->AddString("jid",item.jid.c_str());action->AddBool("group",item.group);
                action->AddUInt64("generation",JabberSpeak::Instance()->ConnectionGeneration());
                const std::string label=item.name+(item.group?" (group)":"")+" · "+item.jid;
                menu->AddItem(new BMenuItem(label.c_str(),action));
            }
            if(!menu->CountItems()){auto* empty=new BMenuItem("No conversations available",nullptr);empty->SetEnabled(false);menu->AddItem(empty);}
            menu->SetTargetForItems(this);menu->Go(Frame().LeftTop()+BPoint(100,100),true,true,true);break;
        }
        case 'fDst': {
            if(msg->GetUInt64("generation",0)!=JabberSpeak::Instance()->ConnectionGeneration() || !JabberSpeak::Instance()->IsConnected())break;
            const std::string jid=msg->GetString("jid","");const std::string body=msg->GetString("body","");const bool group=msg->GetBool("group",false);
            if(jid.empty()||body.empty()||body.size()>65500)break;
            bool found=false;
            for(int32 i=0;i<fTalkCards->CountItems();++i) {
                auto* view=dynamic_cast<TalkView*>(fTalkCards->ItemAt(i)->View());
                if(view && view->IsGroupChat()==group && (group?view->GetGroupRoom():view->GetUserID().bare())==jid) {
                    AddTalkView(view);view->InsertForwardedDraft(body);found=true;break;
                }
            }
            if(found)break;
            // Keep pending drafts bound to this connection; never send on selection.
            auto& pending=fPendingForwards[jid];if(!pending.empty())pending+="\n\n";pending+=body;
            if(group) {
                const auto* info=BookmarkManager::Instance().GetBookmark(jid.c_str());
                TalkManager::Instance()->JoinRoom(jid,info&&!info->nick.empty()?info->nick:JabberSpeak::Instance()->CurrentRealName());
            } else {gloox::JID target(jid);TalkManager::Instance()->CreateTalkSession(&target);}
            break;
        }
		case JAB_OPEN_CHAT:
		case kRosterSelectionChanged: {
			if (!JabberSpeak::Instance()->IsConnected()) break;
			bool group = false;
			auto jid = _roster->SelectedJid(group);
			if (jid.empty()) break;
			_roster->MarkRead(jid);
			if (group) {
				const auto* info = BookmarkManager::Instance().GetBookmark(jid.c_str());
				TalkManager::Instance()->JoinRoom(jid, info && !info->nick.empty() ? info->nick : JabberSpeak::Instance()->CurrentRealName());
			} else {
				gloox::JID user(jid);
				TalkManager::Instance()->CreateTalkSession(&user);
			}
			break;
		}

		case JAB_SHOW_CHATLOG: {
			BString path;
			// try to get the chatlog path from message
			if(B_OK != msg->FindString("path", &path)) {
				// no path provided with message? try to open history for selected user...
  				RosterItem *item = _roster->CurrentItemSelection();
				if (item != NULL) {
					const UserID *user = item->GetUserID();
					auto chatlog_path = BlabberSettings::Instance()->Data("chatlog-path");
					if(!chatlog_path) {
						BPath path;
						find_directory(B_USER_DIRECTORY, &path);
						chatlog_path = path.Path();
					}
					*chatlog_path += "/" + user->JabberHandle();
					path = chatlog_path->c_str();
				}
			}
			// now attemt to open history file with default application for "text/x-jabber-chatlog" MIME type
			if(path.Length() > 0) {
				BEntry entry(path.String());
				entry_ref ref;
				entry.GetRef(&ref);
				BMessage *msgRefs = new BMessage(B_REFS_RECEIVED);
				msgRefs->AddRef("refs", &ref);
				be_roster->Launch("text/plain", msgRefs);
				//be_roster->Launch(path.String());
			}
			break;
		}

		case JAB_OPEN_NEW_CHAT: {
			(new SendTalkWindow(Frame()))->Show();
			break;
		}

		case JAB_OPEN_NEW_GROUP_CHAT: {
			(new JoinGroupChat(Frame()))->Show();
			break;
		}

		case JAB_OPEN_MESSAGE: {
			// if there's a current selection, begin message with that user
			RosterItem *item = _roster->CurrentItemSelection();

			const UserID *user = item->GetUserID();

			// open message window
			TalkManager::Instance()->CreateTalkSession(&user->JID());

			break;
		}

		case JAB_OPEN_ADD_BUDDY_WINDOW: {
			// open buddy window
			AddBuddyWindow::Instance(Frame())->Show();
			break;
		}

		case JAB_OPEN_EDIT_BUDDY_WINDOW: {
			// pick out user to be edited from RosterView
			RosterItem *item = _roster->CurrentItemSelection();

			if (item != NULL) {
				// open edit buddy window
				(new ChangeNameWindow(item->GetUserID()->JID(), item->GetUserID()->FriendlyName().c_str()))->Show();
			}

			BookmarkItem *bookmark = _roster->CurrentBookmarkSelection();

			if (bookmark != NULL) {
				// open groupchat settings window
				(new GroupChatSettingsWindow(bookmark->GetUserID()))->Show();
			}

			break;
		}

		case JAB_USER_INFO: {
			// pick out user to be analyzed
			RosterItem *item = _roster->CurrentItemSelection();

			if (item != NULL) {
				// pick out user
				UserID *user = const_cast<UserID *>(item->GetUserID());

				// open edit buddy window
				(new BuddyInfoWindow(user))->Show();
			}

			break;
		}

		case JAB_REMOVE_BUDDY: {
			// pick out user to be removed from RosterView
			RosterItem *item = _roster->CurrentItemSelection();

			if (item != NULL) {
				// pick out user
				const UserID *user = item->GetUserID();

				// remove the user
				JabberSpeak::Instance()->RemoveFromRoster(user);
			}

			BookmarkItem *bookmark = _roster->CurrentBookmarkSelection();
			if (bookmark) {
				BookmarkManager::Instance().RemoveBookmark(bookmark->GetUserID().full().c_str());
				// Also leave the room (forward to TalkManager)
				// FIXME this sould be called from the main thread?
				TalkManager::Instance()->LeaveRoom(bookmark->GetUserID());
			}

			break;
		}

		case BLAB_AVAILABLE_FOR_CHAT: {
			JabberSpeak::Instance()->SendPresence(gloox::Presence::Chat);
			_chat_item->SetMarked(true);
			BlabberSettings::Instance()->SetTag("last-used-custom-status", false);
			BlabberSettings::Instance()->SetData("last-custom-exact-status", "chat");
			BlabberSettings::Instance()->WriteToFile();

			break;
		}

		case BLAB_DO_NOT_DISTURB: {
			JabberSpeak::Instance()->SendPresence(gloox::Presence::DND);
			_dnd_item->SetMarked(true);
			BlabberSettings::Instance()->SetTag("last-used-custom-status", false);
			BlabberSettings::Instance()->SetData("last-custom-exact-status", "dnd");
			BlabberSettings::Instance()->WriteToFile();

			break;
		}

		case BLAB_AWAY_TEMPORARILY: {
			JabberSpeak::Instance()->SendPresence(gloox::Presence::Away);
			_away_item->SetMarked(true);
			BlabberSettings::Instance()->SetTag("last-used-custom-status", false);
			BlabberSettings::Instance()->SetData("last-custom-exact-status", "away");
			BlabberSettings::Instance()->WriteToFile();

			break;
		}

		case BLAB_AWAY_EXTENDED: {
			JabberSpeak::Instance()->SendPresence(gloox::Presence::XA);
			_xa_item->SetMarked(true);
			BlabberSettings::Instance()->SetTag("last-used-custom-status", false);
			BlabberSettings::Instance()->SetData("last-custom-exact-status", "xa");
			BlabberSettings::Instance()->WriteToFile();

			break;
		}

		case BLAB_SCHOOL: {
			JabberSpeak::Instance()->SendPresence(gloox::Presence::XA, B_TRANSLATE("Off to School"));
			_school_item->SetMarked(true);
			BlabberSettings::Instance()->SetTag("last-used-custom-status", false);
			BlabberSettings::Instance()->SetData("last-custom-exact-status", "school");
			BlabberSettings::Instance()->WriteToFile();

			break;
		}

		case BLAB_WORK: {
			JabberSpeak::Instance()->SendPresence(gloox::Presence::XA, B_TRANSLATE("Busy at work"));
			_work_item->SetMarked(true);
			BlabberSettings::Instance()->SetTag("last-used-custom-status", false);
			BlabberSettings::Instance()->SetData("last-custom-exact-status", "work");
			BlabberSettings::Instance()->WriteToFile();

			break;
		}

		case BLAB_LUNCH: {
			JabberSpeak::Instance()->SendPresence(gloox::Presence::Away, B_TRANSLATE("Lunch"));
			_lunch_item->SetMarked(true);
			BlabberSettings::Instance()->SetTag("last-used-custom-status", false);
			BlabberSettings::Instance()->SetData("last-custom-exact-status", "lunch");
			BlabberSettings::Instance()->WriteToFile();

			break;
		}

		case BLAB_DINNER: {
			JabberSpeak::Instance()->SendPresence(gloox::Presence::Away, B_TRANSLATE("Dinner"));
			_dinner_item->SetMarked(true);
			BlabberSettings::Instance()->SetTag("last-used-custom-status", false);
			BlabberSettings::Instance()->SetData("last-custom-exact-status", "dinner");
			BlabberSettings::Instance()->WriteToFile();

			break;
		}

		case BLAB_SLEEP: {
			JabberSpeak::Instance()->SendPresence(gloox::Presence::XA, B_TRANSLATE("Sleeping"));
			_sleep_item->SetMarked(true);
			BlabberSettings::Instance()->SetTag("last-used-custom-status", false);
			BlabberSettings::Instance()->SetData("last-custom-exact-status", "sleep");
			BlabberSettings::Instance()->WriteToFile();

			break;
		}

		case BLAB_CUSTOM_STATUS:
		{
			CustomStatusWindow::Instance(Frame())->Show();
			CustomStatusWindow::Instance(Frame())->Activate();
			break;
		}

		case kResetWindow:
		{
			JabberSpeak::Instance()->Reset();
			ShowLogin();

			break;
		}

		case JAB_ROTATE_CHAT_FORWARD:
		{
			int currentIndex = fTalkCards->VisibleIndex();
			int itemCount = fTalkCards->CountItems();

			if (++currentIndex >= itemCount)
				currentIndex = 0;

			if (currentIndex < itemCount) {
				fTalkCards->SetVisibleItem(currentIndex);
                RefreshConversationStatus();
				TalkView* v = (TalkView*)fTalkCards->ItemAt(currentIndex)->View();
				if (v->IsGroupChat()) {
					int item = _roster->FindBookmark(gloox::JID(v->GetGroupRoom()));
					_roster->Select(item);
				} else {
					int item = _roster->FindUser(v->GetUserID());
					_roster->Select(item);
				}
			}
			break;
		}

		case JAB_ROTATE_CHAT_BACKWARD:
		{
			int currentIndex = fTalkCards->VisibleIndex();
			int itemCount = fTalkCards->CountItems();

			if (--currentIndex < 0)
				currentIndex = itemCount - 1;

			if (currentIndex > 0) {
				fTalkCards->SetVisibleItem(currentIndex);
                RefreshConversationStatus();
				TalkView* v = (TalkView*)fTalkCards->ItemAt(currentIndex)->View();
				if (v->IsGroupChat()) {
					int item = _roster->FindBookmark(gloox::JID(v->GetGroupRoom()));
					_roster->Select(item);
				} else {
					int item = _roster->FindUser(v->GetUserID());
					_roster->Select(item);
				}
			}
			break;
		}

        case kDeliverTalkNotice: {
            TalkView* view = nullptr;
            BMessage payload;
            if (msg->FindPointer("view", reinterpret_cast<void**>(&view)) != B_OK
                || msg->FindMessage("payload", &payload) != B_OK) break;
            if (msg->GetUInt64("connection-generation", 0) != JabberSpeak::Instance()->ConnectionGeneration()
                || !TalkManager::Instance()->OwnsView(view, msg->GetUInt64("view-token", 0))) break;
            // kAddTalkView and notices share this queue: the very first message
            // cannot race attachment of its newly created view to the looper.
            if (view->Window() == this) view->MessageReceived(&payload);
            break;
        }
		case kAddTalkView:
		{
			TalkView* view = nullptr;
			if (msg->FindPointer("view", reinterpret_cast<void**>(&view)) != B_OK) break;
			// Check generation and lifetime token before dereferencing a raw view
			// queued before closure. Address reuse in another connection is not enough.
			if (msg->GetUInt64("connection-generation", 0)
				!= JabberSpeak::Instance()->ConnectionGeneration()
				|| !TalkManager::Instance()->OwnsView(view, msg->GetUInt64("view-token", 0))) break;
			AddTalkView(view, msg->GetBool("activate", true));
			break;
		}

		case B_OBSERVER_NOTICE_CHANGE:
		{
			int32 orig_what = msg->FindInt32("be:observe_change_what");
			switch (orig_what) {
				case kAuthenticationFailed:
				{
					ShowLogin();
					fLoginErrorMessage->SetText("Invalid login or password");
					_login_username->MarkAsInvalid(true);
					_login_password->MarkAsInvalid(true);
					break;
				}
				case kConnectionRefused:
				{
					ShowLogin();
					BString hint = msg->FindString("hint");
					BString errorString;
					errorString.SetToFormat("Could not connect with the server (%s)", hint.String());
					fLoginErrorMessage->SetText(errorString);
					break;
				}
			}

			break;
		}
		case kRoomInfo:
			RefreshBookmark(msg->GetString("jid", ""), msg->GetString("name", ""));
			SendNotices(msg->what, msg);
			break;
		default:
			BWindow::MessageReceived(msg);
			break;
	}
}

void BlabberMainWindow::MenusBeginning() {
	char buffer[1024];

	// FILE menu
	if (!_full_view->IsHidden()) {
		_disconnect_item->SetEnabled(true);
	} else {
		_disconnect_item->SetEnabled(false);
	}

	// EDIT menu
	if (RosterItem *item = _roster->CurrentItemSelection()) {
		// if a  item is selected
		sprintf(buffer, B_TRANSLATE("Edit %s"), item->GetUserID()->FriendlyName().c_str());
		_change_buddy_item->SetLabel(buffer);
		_change_buddy_item->SetEnabled(true);

		sprintf(buffer, B_TRANSLATE("Remove %s"), item->GetUserID()->FriendlyName().c_str());
		_remove_buddy_item->SetLabel(buffer);
		_remove_buddy_item->SetEnabled(true);

		_user_info_item->SetEnabled(true);
	} else {
		sprintf(buffer, B_TRANSLATE("Edit contact"));
		_change_buddy_item->SetLabel(buffer);
		_change_buddy_item->SetEnabled(false);

		sprintf(buffer, B_TRANSLATE("Remove contact"));
		_remove_buddy_item->SetLabel(buffer);
		_remove_buddy_item->SetEnabled(false);

		_user_info_item->SetEnabled(false);
	}
}

bool BlabberMainWindow::QuitRequested() {
	// Keep the window alive while callbacks drain; never join its network thread
	// while holding this looper lock.
	if (!JabberSpeak::Instance()->IsReadyForConnection()) {
		be_app->PostMessage(B_QUIT_REQUESTED);
		return false;
	}
	// remember last coordinates
	BlabberSettings::Instance()->SetFloatData("main-window-left", Frame().left);
	BlabberSettings::Instance()->SetFloatData("main-window-top", Frame().top);
	BlabberSettings::Instance()->SetFloatData("main-window-width", Bounds().Width());
	BlabberSettings::Instance()->SetFloatData("main-window-height", Bounds().Height());
	BlabberSettings::Instance()->WriteToFile();

	return BWindow::QuitRequested();
}

BlabberMainWindow::BlabberMainWindow(BRect frame)
	: BWindow(frame, B_TRANSLATE_SYSTEM_NAME("MímisSendir"), B_DOCUMENT_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE)
{

	// editing filter for taksing
	AddCommonFilter(new RotateChatFilter());

	// add self to message family
	MessageRepeater::Instance()->AddTarget(this);

	// encompassing view
	_full_view = new BGroupView("main-full", B_VERTICAL);
	_full_view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// Compact readiness indicator next to the bottom-left menu.
	_status_view = new ConnectionIndicator();

	// menubar
	BMenuBar* menubar = new BMenuBar("menubar");
    fApplicationMenu=menubar;

	// FILE MENU
	BMenu* file_menu = new BMenu(B_TRANSLATE("App"));

		_disconnect_item = new BMenuItem(B_TRANSLATE("Log out"), new BMessage(JAB_DISCONNECT));

		_about_item = new BMenuItem(B_TRANSLATE("About MímisSendir" B_UTF8_ELLIPSIS),
			new BMessage(B_ABOUT_REQUESTED));

		_quit_item = new BMenuItem(B_TRANSLATE("Quit"), new BMessage(JAB_QUIT));
		_quit_item->SetShortcut('Q', 0);
		_preferences_item = new BMenuItem(B_TRANSLATE("Settings" B_UTF8_ELLIPSIS),
			new BMessage(JAB_PREFERENCES));
		_preferences_item->SetShortcut(',', 0);

	file_menu->AddItem(_preferences_item);
	file_menu->AddSeparatorItem();
	file_menu->AddItem(_disconnect_item);
	file_menu->AddSeparatorItem();
	file_menu->AddItem(_quit_item);
	file_menu->SetTargetForItems(MessageRepeater::Instance());

	// EDIT MENU
	BMenu* edit_menu = new BMenu(B_TRANSLATE("Contacts"));

		_add_buddy_item = new BMenuItem(B_TRANSLATE("Add contact"),
			new BMessage(JAB_OPEN_ADD_BUDDY_WINDOW));
		_add_buddy_item->SetShortcut('N', 0);

		_change_buddy_item = new BMenuItem(B_TRANSLATE("Edit contact"),
			new BMessage(JAB_OPEN_EDIT_BUDDY_WINDOW));
		_change_buddy_item->SetShortcut('E', 0);

		_remove_buddy_item = new BMenuItem(B_TRANSLATE("Remove contact"),
			new BMessage(JAB_REMOVE_BUDDY));
		_remove_buddy_item->SetShortcut('T', 0);

		_user_info_item = new BMenuItem(B_TRANSLATE("Contact details"),
			new BMessage(JAB_USER_INFO));
		_user_info_item->SetShortcut('G', 0);

	edit_menu->AddItem(_add_buddy_item);
	edit_menu->AddItem(_change_buddy_item);
	edit_menu->AddItem(_remove_buddy_item);
	edit_menu->AddSeparatorItem();
	edit_menu->AddItem(_user_info_item);
	edit_menu->SetTargetForItems(this);

	// STATUS MENU
	BMenu* status_menu = new BMenu(B_TRANSLATE("Status"));

		_chat_item = new BMenuItem(B_TRANSLATE("I'm available for chat"),
			new BMessage(BLAB_AVAILABLE_FOR_CHAT));
		_away_item = new BMenuItem(B_TRANSLATE("I will be away temporarily"),
			new BMessage(BLAB_AWAY_TEMPORARILY));
		_dnd_item = new BMenuItem(B_TRANSLATE("Do not disturb me"),
			new BMessage(BLAB_DO_NOT_DISTURB));
		_xa_item = new BMenuItem(B_TRANSLATE("I will be away for an extended time period"),
			new BMessage(BLAB_AWAY_EXTENDED));
		_school_item = new BMenuItem(B_TRANSLATE("Off to school"),
			new BMessage(BLAB_SCHOOL));
		_work_item = new BMenuItem(B_TRANSLATE("Busy at work"),
			new BMessage(BLAB_WORK));
		_lunch_item = new BMenuItem(B_TRANSLATE("Lunch"),
			new BMessage(BLAB_LUNCH));
		_dinner_item = new BMenuItem(B_TRANSLATE("Dinner"),
			new BMessage(BLAB_DINNER));
		_sleep_item = new BMenuItem(B_TRANSLATE("Sleeping"),
			new BMessage(BLAB_SLEEP));
		_custom_item = new BMenuItem(B_TRANSLATE("Custom" B_UTF8_ELLIPSIS),
			new BMessage(BLAB_CUSTOM_STATUS));

	status_menu->AddItem(_chat_item);
	status_menu->AddSeparatorItem();
	status_menu->AddItem(_away_item);
	status_menu->AddItem(_dnd_item);
	status_menu->AddItem(_xa_item);
	status_menu->AddSeparatorItem();

	status_menu->AddItem(_school_item);
	status_menu->AddItem(_work_item);
	status_menu->AddItem(_lunch_item);
	status_menu->AddItem(_dinner_item);
	status_menu->AddItem(_sleep_item);

	status_menu->AddSeparatorItem();
	status_menu->AddItem(_custom_item);

	status_menu->SetRadioMode(true);
	_chat_item->SetMarked(true);

	// TALK MENU
	BMenu* talk_menu = new BMenu(B_TRANSLATE("Talk"));
	talk_menu->AddItem(new BMenuItem(B_TRANSLATE("Add/remove favorite"), new BMessage('fFav'), 'F'));

		BMenuItem* rotate_chat_forward_item = new BMenuItem(B_TRANSLATE("Rotate chat forward"),
			new BMessage(JAB_ROTATE_CHAT_FORWARD));
		rotate_chat_forward_item->SetShortcut(B_UP_ARROW, B_SHIFT_KEY);

		BMenuItem* rotate_chat_backward_item = new BMenuItem(B_TRANSLATE("Rotate chat backward"),
			new BMessage(JAB_ROTATE_CHAT_BACKWARD));
		rotate_chat_backward_item->SetShortcut(B_DOWN_ARROW, B_SHIFT_KEY);

		_send_chat_item = new BMenuItem(B_TRANSLATE("Start chat" B_UTF8_ELLIPSIS),
			new BMessage(JAB_OPEN_NEW_CHAT));

		_send_groupchat_item = new BMenuItem(B_TRANSLATE("Start group chat" B_UTF8_ELLIPSIS),
			new BMessage(JAB_OPEN_NEW_GROUP_CHAT));

	talk_menu->AddItem(rotate_chat_forward_item);
	talk_menu->AddItem(rotate_chat_backward_item);
	talk_menu->AddSeparatorItem();
	talk_menu->AddItem(_send_chat_item);
	talk_menu->AddItem(_send_groupchat_item);
	talk_menu->SetTargetForItems(this);

	// HELP MENU
	BMenu* help_menu = new BMenu(B_TRANSLATE("Help"));

	BMenuItem* user_guide_item = new BMenuItem(B_TRANSLATE("Renga manual (upstream)"),
		new BMessage(JAB_USER_GUIDE));
	BMenuItem* faq_item = new BMenuItem(B_TRANSLATE("Renga FAQ (upstream)"), new BMessage(JAB_FAQ));

	help_menu->AddItem(user_guide_item);
	help_menu->AddItem(faq_item);
	help_menu->AddSeparatorItem();
	help_menu->AddItem(_about_item);

	help_menu->SetTargetForItems(this);

	menubar->AddItem(file_menu);
	menubar->AddItem(edit_menu);
	menubar->AddItem(status_menu);
	menubar->AddItem(talk_menu);
	menubar->AddItem(help_menu);


	// tabbed view
	// roster view
	_roster = new RosterView();
	BScrollView* roster_scroller = new EdgeScrollView(_roster);
	_roster->TargetedByScrollView(roster_scroller);

	// Navigation and the selected list share one sidebar instead of leaving
	// an empty navigation column beside the roster.
	BGroupView* navigation = new BGroupView(B_HORIZONTAL, 6);
	BButton* conversations = new ActionIconButton("conversations", B_TRANSLATE("Conversations"), new BMessage('nRec'), "conversations", 48, 64, true);
	BButton* contacts = new ActionIconButton("contacts", B_TRANSLATE("Contacts"), new BMessage('nCon'), "contacts", 48, 64, true);
	BButton* groups = new ActionIconButton("groups", B_TRANSLATE("Groups"), new BMessage('nGrp'), "groups", 48, 64, true);
    fNavigation[0] = conversations; fNavigation[1] = contacts; fNavigation[2] = groups;
    static_cast<ActionIconButton*>(conversations)->SetSelected(true);
	BLayoutBuilder::Group<>(navigation)
		.SetInsets(6)
		.Add(conversations)
		.Add(contacts)
		.Add(groups)
        .AddGlue();
	_roster->SetExplicitMinSize(BSize(220, 120));
    fSettingsButton=new SettingsButton();
    fSidebarToggle = new SidebarButton();
    fSidebarToggle->SetTarget(this);
    fConversationStatus=new StatusView("active-conversation-status");
    fConversationStatus->SetExplicitMinSize(BSize(0,B_SIZE_UNSET));
    fConversationStatus->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,B_SIZE_UNSET));
    fConversationLag=new StatusView("active-conversation-lag");
    fConversationLag->SetExplicitMinSize(BSize(0,B_SIZE_UNSET));
	BLayoutBuilder::Group<>(_full_view, B_VERTICAL, 0)
		.SetInsets(0, 6, 0, 0)
		.Add(menubar, 0)
		.AddSplit(B_HORIZONTAL, 0)
			.GetSplitView(&fSidebarSplit)
			.AddGroup(B_VERTICAL, 0, 0)
				.Add(navigation, 0)
				.Add(roster_scroller)
			.End()
            .AddCards()
                .GetLayout(&fTalkCards)
            .End()
		.End()
        .AddGroup(B_HORIZONTAL,0,0)
            .SetInsets(6,0,18,0)
            .Add(fSidebarToggle,0)
            .Add(fSettingsButton,0)
            .Add(_status_view,0)
            .Add(fConversationStatus)
            .Add(fConversationLag,0)
        .End()
    .End();
    menubar->Hide();

	fSidebarSplit->SetCollapsible(false);
    fSidebarSplit->SetSplitterSize(1);
    // Composer controls must not redistribute the conversation sidebar width.
    fTalkCards->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,B_SIZE_UNLIMITED));
    auto* sidebar=navigation->Parent();
    const float sidebarWidth=std::max(224.0f,sidebar->MinSize().width);
    sidebar->SetExplicitMinSize(BSize(sidebarWidth,B_SIZE_UNSET));
    sidebar->SetExplicitMaxSize(BSize(sidebarWidth,B_SIZE_UNLIMITED));
    sidebar->SetExplicitPreferredSize(BSize(sidebarWidth,B_SIZE_UNSET));
	///// NOW DO LOGIN STUFF
	// encompassing view
	_login_full_view = new BGroupView("login-full", B_VERTICAL);
	_login_full_view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// graphics
	_login_bulb = new PictureView("BEOS:ICON");

	// username/password controls
	_login_realname = new BTextControl(NULL, B_TRANSLATE("Nickname:"), NULL, NULL);

	_login_username = new PopupTextControl(NULL, B_TRANSLATE("XMPP account:"), NULL);
	_login_username->SetMessage(new BMessage(kSelectExistingAccount));

	_login_password = new BTextControl(NULL, B_TRANSLATE("Password:"), NULL, NULL);
	_login_password->TextView()->HideTyping(true);

	_login_auto_login = new BCheckBox(NULL, B_TRANSLATE("Auto-login"), NULL);

	// login button
	_login_login = new BButton("login", B_TRANSLATE("Login"), new BMessage(JAB_LOGIN));
	_login_login->MakeDefault(false);
	_login_login->SetTarget(this);

	_login_delete = new BButton("delete", B_TRANSLATE("Remove account"), new BMessage('DELA'));

	BStringView* appName = new BStringView("app name", B_TRANSLATE_SYSTEM_NAME("MímisSendir"));
	appName->SetAlignment(B_ALIGN_CENTER);
	BFont bigfont(be_plain_font);
	bigfont.SetSize(bigfont.Size() * 3);
	appName->SetFont(&bigfont);

	BStringView* tagline = new BStringView("", B_TRANSLATE("Private conversations through your XMPP bridge"));
	tagline->SetAlignment(B_ALIGN_CENTER);

	BRadioButton* useAccount = new BRadioButton("", B_TRANSLATE("Use existing account"),
		new BMessage(kRadioUseAccount));
	BRadioButton* noAccount = new BRadioButton("", B_TRANSLATE("Start chatting without account"),
		new BMessage(kRadioNoAccount));

	fAnonymousLimitationsWarning = new BTextView("anonymous warning");
	fAnonymousLimitationsWarning->SetText(B_TRANSLATE("To avoid spam and limit abusive behavior, "
		"anonymous users are restricted to a single server and cannot reach all of the XMPP "
		"network."
	));
	fAnonymousLimitationsWarning->MakeEditable(false);
	fAnonymousLimitationsWarning->MakeSelectable(false);
	fAnonymousLimitationsWarning->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	fAnonymousLimitationsWarning->SetExplicitMinSize(BSize(B_SIZE_UNSET, 80));

	fLoginErrorMessage = new BStringView("", "");

	BLayoutBuilder::Group<>(_login_full_view, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.AddGrid(B_USE_SMALL_SPACING)
				.Add(_login_bulb, 0, 0, 1, 1)
				.Add(appName, 1, 0)
				.Add(tagline, 0, 1, 2, 1)
			.End()
			.AddGlue()
		.End()
		.AddStrut(B_USE_WINDOW_SPACING)
		.AddGroup(B_HORIZONTAL)
			.AddGlue(0)
			.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING)
				.Add(new BRadioButton("", B_TRANSLATE("Create an account"),
					new BMessage(kRadioCreateAccount)))
				.Add(useAccount)
				.Add(noAccount)
			.End()
			.AddGlue(0)
		.End()
		.AddStrut(B_USE_WINDOW_SPACING)
		.AddGroup(B_HORIZONTAL)
			.AddGlue(0)
			.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING)
				.Add(fLoginErrorMessage)
				.AddGrid(1, B_USE_SMALL_SPACING)
					.SetExplicitMaxSize(BSize(_login_username->StringWidth("x") * 50, B_SIZE_UNSET))
					.AddTextControl(_login_realname, 0, 0)
					.AddTextControl(_login_username, 0, 1)
					.AddTextControl(_login_password, 0, 2)
				.End()
				.AddGroup(B_HORIZONTAL)
					.Add(_login_auto_login)
					.AddGlue()
					.Add(_login_login)
					.Add(_login_delete)
				.End()
			.End()
			.AddGlue(0)
		.End()
		.Add(fAnonymousLimitationsWarning)
		.AddGlue()
	.End();

	// attach all-encompassing main view to window
	BCardLayout* cl = new BCardLayout();
	SetLayout(cl);
    // Set after ownership attachment: Haiku replaces the layout data proxy there.
    cl->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,B_SIZE_UNLIMITED));
	AddChild(_full_view);
	AddChild(_login_full_view);

	// login always hidden at start
	cl->SetVisibleItem((int32)1);

	// default
	auto s = BlabberSettings::Instance()->Data("last-realname");
	if(s) {
		_login_realname->SetText(s->c_str());
	} else {
		_login_realname->SetText(B_TRANSLATE_COMMENT("Me","Self-referencial 'me'"));
	}

	s = BlabberSettings::Instance()->Data("last-login");
	if (s) {
		_login_username->SetText(s->c_str());
	} else {
		_login_username->SetText(B_TRANSLATE("newuser@jabber.org"));
	}

    // Show the login surface before asking the OS for keyring permission.
    useAccount->SetValue(B_CONTROL_ON);
    fAnonymousLimitationsWarning->Hide();
    _login_password->SetText("");
    fAutoLoginPending = BlabberSettings::Instance()->Tag("auto-login");

	_login_auto_login->SetValue(BlabberSettings::Instance()->Tag("auto-login"));

	// focus to start
	if (BlabberSettings::Instance()->Data("last-login")) {
		_login_password->MakeFocus(true);
	} else {
		_login_username->MakeFocus(true);
	}

	JabberSpeak::Instance()->StartWatchingAll(this);

	// editing filter for taksing
	AddCommonFilter(new RotateChatFilter());
}


bool BlabberMainWindow::ValidateLogin() {
	// existance of username
	if (!strcmp(_login_username->Text(), "")) {
		ModalAlertFactory::Alert(B_TRANSLATE("Wait, you haven't specified your Jabber ID yet.\n"
			"(e.g. haikuFan@jabber.org)"), B_TRANSLATE("OK"),
			NULL, NULL, B_WIDTH_FROM_LABEL, B_STOP_ALERT);
		_login_username->MakeFocus(true);

		return false;
	}

	// validity of username
	std::string validate = UserID::WhyNotValidJabberHandle(_login_username->Text());
	if (validate.size()) {
		BString buffer;
		buffer.SetToFormat(B_TRANSLATE("The Jabber ID you specified must not be yours, because "
			"it's invalid for the following reason:\n\n%s\n\n"
			"If you can't remember it, it's OK to create a new one by checking the "
			"\"Create a new Jabber account!\" box."), validate.c_str());

		ModalAlertFactory::Alert(buffer, B_TRANSLATE("OK"), NULL, NULL,
			B_WIDTH_FROM_LABEL, B_STOP_ALERT);
		_login_username->MakeFocus(true);

		return false;
	}

	gloox::JID username(_login_username->Text());

	// check existance of password
	if (!strcmp(_login_password->Text(), "")) {
		char buffer[1024];
		sprintf(buffer, B_TRANSLATE("You must specify a password so I can make sure it's you, %s."),
			username.username().c_str());

		ModalAlertFactory::Alert(buffer, B_TRANSLATE("OK"), NULL, NULL,
			B_WIDTH_FROM_LABEL, B_STOP_ALERT);
		_login_password->MakeFocus(true);

		return false;
	}

	return true;
}

void BlabberMainWindow::ShowLogin() {
	_login_login->SetEnabled(JabberSpeak::Instance()->IsReadyForConnection());
	// reassign button as default
	_login_login->MakeDefault(true);

	// reset
	_login_realname->SetText("");
	_login_username->SetText("");
	_login_password->SetText("");
	fLoginErrorMessage->SetText("");

	_login_username->MarkAsInvalid(false);
	_login_password->MarkAsInvalid(false);

	// default
	auto s = BlabberSettings::Instance()->Data("last-realname");
	if(s) {
		_login_realname->SetText(s->c_str());
	} else {
		_login_realname->SetText(B_TRANSLATE_COMMENT("Me","Self-referencial 'me'"));
	}

	s = BlabberSettings::Instance()->Data("last-login");
	if (s) {
		_login_username->SetText(s->c_str());
	} else {
		_login_username->SetText(B_TRANSLATE("newuser@jabber.org"));
	}

    _login_resource.clear();
    LoadCredentials();

	// focus to start
	if (BlabberSettings::Instance()->Data("last-login")) {
		_login_password->MakeFocus(true);
	} else {
		_login_username->MakeFocus(true);
	}

	BCardLayout* cl = (BCardLayout*)GetLayout();
	cl->SetVisibleItem(1);
}

void BlabberMainWindow::SetCustomStatus(std::string status)
{
	char buffer[2048];

	// create menued status message
	sprintf(buffer, B_TRANSLATE("[Custom] %s"), status.c_str());

	_custom_item->SetMarked(true);
	_custom_item->SetLabel(buffer);
}


void
BlabberMainWindow::FlagBookmarkItem(const gloox::JID& room, uint32 flags)
{
	int32 pos = _roster->FindBookmark(room);
	if (pos < 0)
		return;

	auto* item = dynamic_cast<BookmarkItem*>(_roster->FullListItemAt(pos));
	item->SetFlag(flags);

	// Need to redo a FindItem because there is no FullListInvalidateItem, so we need to convert the
	// FullList index returned by FindBookmark to a BListView index...
	int32 listIndex = _roster->IndexOf(item);
	// ... and this can fail if the item is in a collapsed part of the list
	if (listIndex < 0)
		return;
	_roster->InvalidateItem(listIndex);
}


void
BlabberMainWindow::RefreshBookmark(const char* const jid, const char* newName)
{
	_roster->LinkBookmark(gloox::JID(jid), newName);
	for (int32 i = 0; i < fTalkCards->CountItems(); ++i) {
		auto* view = dynamic_cast<TalkView*>(fTalkCards->ItemAt(i)->View());
		if (view && view->IsGroupChat() && view->GetGroupRoom() == jid) view->RefreshHeader(newName);
	}
}


void
BlabberMainWindow::AddTalkView(TalkView* view, bool activate)
{
	if (view->Window() != this) {
		fTalkCards->AddView(view);
	}

    const auto key=view->IsGroupChat()?view->GetGroupRoom():view->GetUserID().bare();
    auto pending=fPendingForwards.find(key);
    if(pending!=fPendingForwards.end()){view->InsertForwardedDraft(pending->second);fPendingForwards.erase(pending);activate=true;}
	if (!activate && fTalkCards->CountItems() > 1) return;
	fTalkCards->SetVisibleItem(fTalkCards->IndexOfView(view));
    RefreshConversationStatus();
	_roster->MarkRead(view->IsGroupChat() ? view->GetGroupRoom() : view->GetUserID().bare());
}


void BlabberMainWindow::CloseTalkViews()
{
	// Invoked on/under this window looper after the connection is drained.
    fPendingForwards.clear();
	while (fTalkCards->CountItems() > 0) {
		BView* view = fTalkCards->ItemAt(0)->View();
		if (!view) break;
		view->RemoveSelf();
		delete view;
	}
    RefreshConversationStatus();
}

void BlabberMainWindow::RefreshConversationStatus()
{
    const int32 index=fTalkCards->VisibleIndex();
    auto* view=index>=0 && index<fTalkCards->CountItems()?dynamic_cast<TalkView*>(fTalkCards->ItemAt(index)->View()):nullptr;
    fConversationStatus->SetMessage(nullptr);
    if(!view){
        fConversationStatus->SetText("");fConversationStatus->SetToolTip("");
        fConversationStatus->SetTarget(static_cast<BHandler*>(nullptr));
        fConversationLag->SetText("");return;
    }
    const auto* status=view->ConversationStatus();
    fConversationStatus->SetText(status->Text());
    auto* tip=dynamic_cast<BTextToolTip*>(status->ToolTip());
    fConversationStatus->SetToolTip(tip?tip->Text():status->Text());
    if(status->Message())fConversationStatus->SetMessage(new BMessage(*status->Message()));
    fConversationStatus->SetTarget(view);
    fConversationLag->SetText(view->LagStatus()->Text());
    fConversationLag->SetHighColor(view->LagStatus()->HighColor());
}

void BlabberMainWindow::LoadCredentials(bool initial)
{
    BMessage request;
    request.AddString("operation", "load");
    request.AddString("account", _login_username->Text());
    request.AddBool("initial", initial);
    request.AddBool("import-legacy", BlabberSettings::Instance()->Data("import-legacy-credential")
        .value_or("") == _login_username->Text());
    request.AddUInt64("request", ++fCredentialRequest);
    fCredentialDraft = _login_password->Text();
    fLoginErrorMessage->SetText(kCredentialLoading);
    if (!Credentials::Submit(request, BMessenger(this)))
        fLoginErrorMessage->SetText("Keyring is busy. Finish its pending prompt or enter the password manually.");
}
