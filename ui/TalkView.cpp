#include <FindDirectory.h>
#include <Path.h>
//////////////////////////////////////////////////
// Blabber [TalkView.cpp]
//////////////////////////////////////////////////

#include <cstdio>
#include <ctime>
#include <sys/time.h>
#include "support/ConversationStore.h"
#include <malloc.h>
#include <sys/stat.h>
#include "ui/ComposerButton.h"
#include "ui/JumpToLatestButton.h"
#include "ui/ConversationAvatar.h"
#include "ui/AvatarCache.h"
#include "ui/ModalAlertFactory.h"
#include <stdlib.h>

#include <Beep.h>
#include <Box.h>
#include <be_apps/NetPositive/NetPositive.h>
#include <Catalog.h>
#include <Clipboard.h>
#include <PopUpMenu.h>
#include "support/ReplyDraft.h"
#include <ControlLook.h>
#include <FindDirectory.h>
#include <GridView.h>
#include <GroupLayout.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <Roster.h>
#include <storage/Path.h>
#include <SplitView.h>
#include <private/interface/ToolTip.h>

#include "support/AppLocation.h"

#include "jabber/BlabberSettings.h"
#include "jabber/GenericFunctions.h"
#include "jabber/JabberSpeak.h"
#include "jabber/Messages.h"
#include "jabber/PreferencesWindow.h"
#include "jabber/SoundSystem.h"
#include "jabber/TalkListItem.h"
#include "jabber/TalkManager.h"

#include "network/references.h"
#include "network/HttpUpload.h"

#include "ui/ChangeTopicWindow.h"
#include "ui/HVIFUtil.h"
#include "ui/PeopleListItem.h"
#include "ui/PeopleListView.h"
#include "ui/RotateChatFilter.h"

#include "TalkView.h"
#include "support/ChatDate.h"
#include "AttachmentController.h"
#include "ReactionController.h"
#include "MediaPicker.h"
#include "support/MessageFormatting.h"
#include "network/AttachmentMetadata.h"

#include "gloox/mucroom.h"
#include "gloox/rostermanager.h"
#include "gloox/stanzaextension.h"

#include "openssl/evp.h"

#define NOTIFICATION_CHAR "√"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "TalkView"


// Send a MUC self-ping every 5 minutes for MUC.
// This is shorter than what the spec recommends (15 minutes), but that recommendation is not
// normative, and it's nice to have the lag-meter update a bit more often than that. Vision uses
// only a few seconds, that's reasonable on IRC, but on XMPP the ping has to go through multiple
// servers and is a lot more complicated, so let's be a bit slower.
static const int kSelfPingDelay = 5 * 60000000;


TalkView::TalkView(const gloox::JID *user, std::string group_room,
		std::string group_username, gloox::MessageSession* session)
	: BGroupView("<talk window>", B_VERTICAL)
	, _session(session)
	, _idle_timer(nullptr)
	, fPingSendTime(0)
	, fIsPinging(false)
{
	_chat_index = -1;

	UserID* uid = NULL;
	if (user) {
		uid = JRoster::Instance()->FindUser(*user);
	}
	_group_room     = group_room;
	_group_username = group_username;

	BBitmap* editIcon = NULL;

	if (!IsGroupChat()) {
		if (uid)
			_current_status = uid->OnlineStatus();
	} else {
		editIcon = LoadIconFromResource("edit", be_control_look->GetScrollBarWidth());
	}

	// status bar
	_status_view = new StatusView("conversation-status", editIcon);
	_status_view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// Force a small minimal size on the status view. It defaults to begin wide enough to show the
	// entire topic string, which is not what we want (it's OK if it gets truncated)
	_status_view->SetExplicitMinSize(BSize(_status_view->StringWidth("M") * 15, B_SIZE_UNSET));
	_status_view->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	if (IsGroupChat()) {
		_status_view->SetMessage(new BMessage(kEditTopic));
	}

	fTimeline         = new TextDocumentView("Timeline");
	fTimelineDocument.SetTo(new TextDocument(), true);
	fTimeline->SetTextDocument(fTimelineDocument);
	fTimelineScroller = new BScrollView("Timeline Scroller", fTimeline, B_WILL_DRAW, false, true);
	fTimeline->TargetedByScrollView(fTimelineScroller);
	fTimeline->SetInsets(12);
	fTimeline->SetEditingEnabled(false);

	sSystemMessagesStyle.SetFont(be_plain_font);
	sSystemMessagesStyle.SetBold(false);
	sSystemMessagesStyle.SetItalic(true);
	sSystemMessagesStyle.SetForegroundColor(B_CONTROL_TEXT_COLOR);

	// message control
	rgb_color text_color = ui_color(B_DOCUMENT_TEXT_COLOR);
	BFont text_font(be_plain_font);

	fMessageInput         = new BTextView("Message Input", &text_font, &text_color, B_WILL_DRAW);
	fMessageInputScroller = new BScrollView("Message Input Scroller", fMessageInput, B_WILL_DRAW, false, false);

	fMessageInput->TargetedByScrollView(fMessageInputScroller);
	fMessageInput->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	fMessageInput->SetWordWrap(true);
	fMessageInput->SetStylable(true);
    font_height draftMetrics;text_font.GetHeight(&draftMetrics);
    const float draftHeight=4*std::ceil(draftMetrics.ascent+draftMetrics.descent+draftMetrics.leading)+8;
    fMessageInputScroller->SetExplicitMinSize(BSize(0,draftHeight));
    fMessageInputScroller->SetExplicitPreferredSize(BSize(B_SIZE_UNSET,draftHeight));

	// editing filter for messaging
	fMessageInput->AddFilter(new EditingFilter(fMessageInput, this));

	fRaiseHand = new BButton("raise hand", "", new BMessage('rqvc'));
	fRaiseHand->SetIcon(LoadIconFromResource("raise-hand", 32));
	fRaiseHand->SetToolTip(B_TRANSLATE("Request voice"));

	BGroupView* gv = new BGroupView(B_HORIZONTAL);
	gv->AddChild(fMessageInputScroller);
    auto* media = new ComposerButton("emoji-media", "Emoji", new BMessage('mPkr'),ComposerButton::Emoji);
    media->SetToolTip("Emoji, GIFs and stickers");gv->AddChild(media);
	fAttach = new ComposerButton("Attach file", "Attach file", new BMessage('uSel'),ComposerButton::Attachment);
	fAttach->SetToolTip(B_TRANSLATE("Attach file…"));
	gv->AddChild(fAttach);
    // Reserve a small slot so showing the jump control never moves the draft,
    // send button or timeline. It is present only while older content is shown.
    auto* jumpSlot = new BGroupView("latest-message-slot", B_VERTICAL, 0);
    jumpSlot->SetExplicitMinSize(BSize(36, 36));
    jumpSlot->SetExplicitMaxSize(BSize(36, 36));
    fJumpToLatest = new JumpToLatestButton();
    jumpSlot->AddChild(fJumpToLatest);
    fJumpToLatest->Hide();
    gv->AddChild(jumpSlot);
    gv->AddChild(new ComposerButton("send-message", B_TRANSLATE("Send"), new BMessage(JAB_CHAT_SENT),ComposerButton::Send));
	gv->AddChild(fRaiseHand);
	fRaiseHand->Hide();

    fEmojiStrip=new BGroupView("inline-emoji",B_HORIZONTAL,2);
    for(const char* emoji:{"😀","😂","❤️","👍","🎉","🙏","🔥","😊"}) {
        auto* insert=new BMessage('eIns');insert->AddString("emoji",emoji);
        auto* choice=new ComposerChoice(emoji,insert,true);choice->SetToolTip(emoji);fEmojiStrip->AddChild(choice);
    }
    for(const auto& option:std::vector<std::pair<const char*,int>>{{"More…",0},{"GIFs",1},{"Stickers",2}}) {
        auto* open=new BMessage('mPkr');open->AddInt32("mode",option.second);
        if (option.second == 0) fEmojiStrip->AddChild(new ComposerChoice(option.first, open));
        else fEmojiStrip->AddChild(new ActionIconButton(option.first, option.first, open,
            option.second == 1 ? "gif" : "sticker", 24, 28));
    }
    BLayoutBuilder::Group<>(fEmojiStrip).AddGlue();
    fEmojiStrip->Hide();
    auto* composer=new BGroupView(B_VERTICAL,2);
    BLayoutBuilder::Group<>(composer).SetInsets(0,2,10,2);
    fReplyStrip=new BGroupView("reply-draft",B_HORIZONTAL,6);
    fReplyLabel=new BStringView("reply-preview","");fReplyLabel->SetTruncation(B_TRUNCATE_END);
    fReplyLabel->SetExplicitMinSize(BSize(100,B_SIZE_UNSET));
    fReplyLabel->SetExplicitPreferredSize(BSize(200,B_SIZE_UNSET));
    BLayoutBuilder::Group<>(fReplyStrip).Add(fReplyLabel,1).Add(new BButton("cancel-reply","Cancel reply",new BMessage('mRcl')));
    fReplyStrip->Hide();composer->AddChild(fReplyStrip);composer->AddChild(fEmojiStrip);composer->AddChild(gv);

	// handle splits
	BSplitView* _split_talk = new BSplitView(B_VERTICAL);
	_split_talk->AddChild(fTimelineScroller);
	_split_talk->AddChild(composer);
	_split_talk->SetItemWeight(0, 12, false);
	_split_talk->SetItemWeight(1, 1, false);
	_split_talk->SetSpacing(0);
	_split_talk->SetCollapsible(false);

	_people = new PeopleListView();
	_people->SetExplicitMinSize(BSize(StringWidth("Firstname M. Lastname"), B_SIZE_UNSET));
	_scrolled_people_pane = new BScrollView(NULL, _people, 0, false, true, B_PLAIN_BORDER);

	BSplitView* _split_group_people = new BSplitView(B_HORIZONTAL);
	fPeopleSplit = _split_group_people;
	_split_group_people->AddChild(_split_talk);
	_split_group_people->AddChild(_scrolled_people_pane);
	_split_group_people->SetItemWeight(0, 10, false);
	_split_group_people->SetItemWeight(1, 1, false);
	_split_group_people->SetSpacing(0);

	{
		_split_group_people->SetItemCollapsed(1, true);
		_split_group_people->SetSplitterSize(0);
	}

	// add GUI components to BView
	BGroupLayout* layout = new BGroupLayout(B_VERTICAL);
	SetLayout(layout);
	layout->SetSpacing(0);

	fLagMeter = new StatusView("lagmeter");
	fUploadProgress = new BStatusBar("upload-progress");
	fUploadProgress->SetMaxValue(100);
	fUploadProgress->Reset("");
	fCancelUpload = new BButton("cancel-upload", B_TRANSLATE("Cancel"), new BMessage('uCan'));
	fCancelUpload->SetEnabled(false);

	fUploadStrip = new BGroupView(B_HORIZONTAL);
	BLayoutBuilder::Group<>(fUploadStrip)
		.Add(fUploadProgress).Add(fCancelUpload).SetInsets(B_USE_SMALL_INSETS);
	fUploadStrip->Hide();
	fHeaderName = new BStringView("conversation-name", "");
	fHeaderName->SetFont(be_bold_font);
	fHeaderName->SetExplicitMinSize(BSize(100, B_SIZE_UNSET));
	fHeaderName->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	fHeaderName->SetTruncation(B_TRUNCATE_END);
	fHeaderDetail = new BStringView("conversation-detail", "");
	fHeaderDetail->SetExplicitMinSize(BSize(100, B_SIZE_UNSET));
	fHeaderDetail->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	fHeaderDetail->SetTruncation(B_TRUNCATE_END);
	fParticipants = new ActionIconButton("participants", B_TRANSLATE("Participants"), new BMessage('pVis'), "participants", 48, 52);
	fParticipants->SetToolTip(B_TRANSLATE("Show or hide participants"));
	RefreshHeader();
    // Retain each conversation's state/ownership, but display only the active
    // conversation in the main window footer. No duplicate status row here.
    auto* retainedStatus=new BGroupView("retained-conversation-status",B_HORIZONTAL);
    BLayoutBuilder::Group<>(retainedStatus).Add(_status_view).Add(fLagMeter);
    retainedStatus->Hide();
	BLayoutBuilder::Group<>(layout)
		.AddGroup(B_HORIZONTAL)
			.SetInsets(6, 6, 6, 6)
			.Add(new ConversationAvatar(fFriendlyName.c_str()))
			.AddGroup(B_VERTICAL, 2)
				.Add(fHeaderName).Add(fHeaderDetail)
			.End()
			.Add(fParticipants, 0)
		.End()
		.Add(_split_group_people)
		.Add(fUploadStrip)
        .Add(retainedStatus,0)
	.End();

	fMessageInput->MakeFocus(true);

	// generate window title
	char buffer[1024];
	std::string user_representation;

	if (!IsGroupChat()) {
		// identify the user
		sprintf(buffer, B_TRANSLATE("Your identity is %s"), JabberSpeak::Instance()->CurrentRealName().c_str());
		SetStatus(buffer);
	} else if (!uid || uid->UserType() == UserID::JABBER) {
		// identify the user
		sprintf(buffer, B_TRANSLATE("Your identity is %s"), JabberSpeak::Instance()->CurrentRealName().c_str());
		SetStatus(buffer);
	} else {
		user_representation = uid->FriendlyName();

		if (user_representation.empty())
			user_representation = uid->JabberUsername();

	}

	if (!IsGroupChat() && user_representation.empty()) {
		if (uid)
			user_representation = uid->FriendlyName();
		else
			user_representation = _session->target().bare();
	}

	// Session diagnostics belong in the replaceable footer, not the conversation.
	// construct timestamp
	std::string message;
	message.resize(128);
	time_t now = time(NULL);
	struct tm *time_struct = localtime(&now);
	strftime(&message[0], message.size()-1, B_TRANSLATE("Session started %e %b %y [%R:%S]"), time_struct);
	SetStatus(message.c_str());

}


TalkView::~TalkView() {
    if(fMediaPicker.IsValid())fMediaPicker.SendMessage(B_QUIT_REQUESTED);
    if(fReactionPicker.IsValid())fReactionPicker.SendMessage(B_QUIT_REQUESTED);
	fAttachments.reset();
	if (fUploadTask) fUploadTask->Cancel();
	delete fAttachmentPanel;
	delete fUploadPoll;
    delete fDatePoll;
    delete fReadPoll;

	if (IsGroupChat() && JabberSpeak::Instance()->IsConnected())
		JabberSpeak::Instance()->SendGroupUnvitation(_group_room, _group_username);

    Avatars::Unwatch(this);
	TalkManager::Instance()->RemoveWindow(this);

	delete _idle_timer;
}


void TalkView::AttachedToWindow()
{
    fMediaAuthority = Upload::Profile::Load().authority;
	TalkManager::Instance()->StartWatchingAll(this);
    Avatars::Watch(this);
    if(auto* avatar=dynamic_cast<ConversationAvatar*>(FindView("conversation-avatar")))avatar->SetIdentity(IsGroupChat()?_group_room:_session->target().bare());
    if(!fDatePoll){BMessage tick('dPol');fDatePoll=new BMessageRunner(BMessenger(this),tick,60000000);}
    if(!fHistory) {
        BPath root;find_directory(B_USER_SETTINGS_DIRECTORY,&root);root.Append("MimisSendir");
        fHistory=std::make_unique<ChatHistory>(root.Path(),JabberSpeak::Instance()->GlooxClient()->jid().bare(),
            IsGroupChat()?_group_room:_session->target().bare(),IsGroupChat());
        RestoreHistory();
    }
	if (IsGroupChat()) {
		BlabberMainWindow::Instance()->StartWatching(this, kRoomInfo);
		auto* room = static_cast<gloox::MUCRoom*>(TalkManager::Instance()
			->IsExistingWindowToGroup(_group_room));
		if (room) {
			room->join();
			room->getRoomInfo();
		}
	}
	BMessage idleMessage('IDLE');
	if (IsGroupChat()) {
		// FIXME start this when receiving the MUC subject instead (that's when we're actually
		// joined to the MUC)
		_idle_timer = new BMessageRunner(this, idleMessage, kSelfPingDelay, -1);
	}

	fTimeline->SetTarget(this);
    SyncHistory();
    fJumpToLatest->SetTarget(this);
    if (!fReadPoll) { BMessage tick('rVis'); fReadPoll = new BMessageRunner(BMessenger(this), tick, 250000); }
	_people->SetTarget(this);
	fRaiseHand->SetTarget(this);
	fAttach->SetTarget(this);
    if(auto* media=dynamic_cast<BButton*>(FindView("emoji-media")))media->SetTarget(this);
    for(int32 i=0;i<fEmojiStrip->CountChildren();++i)if(auto* choice=dynamic_cast<BButton*>(fEmojiStrip->ChildAt(i)))choice->SetTarget(this);
	fCancelUpload->SetTarget(this);
    if(auto* cancel=dynamic_cast<BButton*>(FindView("cancel-reply")))cancel->SetTarget(this);
	fParticipants->SetTarget(this);
	if (auto* send = dynamic_cast<BButton*>(FindView("send-message"))) send->SetTarget(this);

	_status_view->SetTarget(this);
}


void TalkView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);

	BRect message_rect = fMessageInput->Frame();

	message_rect.OffsetTo(B_ORIGIN);

	message_rect.InsetBy(2.0, 2.0);

	fMessageInput->SetTextRect(message_rect);

	fTimeline->Invalidate();
	fTimelineScroller->Invalidate();
}


void TalkView::handleEvent(const gloox::Event& event)
{
	const auto* error = event.stanza()->error();
	if (error != nullptr) {
		fprintf(stderr, "Ping response with error for %s\n", GetGroupRoom().c_str());
		// Special case a few errors that don't indicate a connection issue according to XEP-0410
		if (error->error() == gloox::StanzaErrorServiceUnavailable
			|| error->error() == gloox::StanzaErrorFeatureNotImplemented) {
			// ping not implemented?
			// should not happen because we implement it, but if the server doesn't support
			// optimized self-ping, it may have asked another client connected to the same account.
			// In that case things won't work reliably, so stop sending pings.
			delete _idle_timer;
			_idle_timer = NULL;
			fIsPinging = false;
			return;
		}

		if (error->error() == gloox::StanzaErrorItemNotFound) {
			// User just changed nickname during the ping, try again later...
			_idle_timer->SetInterval(kSelfPingDelay);
			fIsPinging = false;
			return;
		}

		LockLooper();
		ShowError(error->error());
		UnlockLooper();

		// rejoin the room
		gloox::MUCRoom* room = (gloox::MUCRoom*)TalkManager::Instance()
			->IsExistingWindowToGroup(GetGroupRoom());
		if (room != nullptr) {
			// room may be null if we didn't ever succeed in joining it, in that case ignore the error
			fprintf(stderr, "Error on %s, re-join\n", GetGroupRoom().c_str());
			room->join();
		}
	} else {
		// TODO message back to the UI to display the lagmeter
		BString lagText;
		lagText.SetToFormat("Lag: %.2f", (system_time() - fPingSendTime) / 1000000.f);
		LockLooper();
		fLagMeter->SetText(lagText);
		fLagMeter->SetHighUIColor(B_PANEL_TEXT_COLOR);
        if(Window())Window()->PostMessage('tSta');
		UnlockLooper();
	}

	// Ping is processed, reset fPingSendTime
	fIsPinging = false;
	_idle_timer->SetInterval(kSelfPingDelay);
}


struct Iterator 
{
	using iterator_category = std::forward_iterator_tag;
	using difference_type   = std::ptrdiff_t;
	using value_type        = int;
	using pointer           = int;
	using reference         = int&;  // or also value_type&

	Iterator(pointer ptr) : m_ptr(ptr) {}

	reference operator*() const { return const_cast<reference>(m_ptr); }
	Iterator& operator++() { m_ptr++; return *this; }
	friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; }; 

	pointer m_ptr;
};


void TalkView::MessageReceived(BMessage *msg) {
    if(msg->what=='rVis'){
        const auto authority = Upload::Profile::Load().authority;
        if (authority != fMediaAuthority) {
            fMediaAuthority = authority;
            fAttachments.reset();
            if (fHistory) RebuildHistory();
        }
        UpdateReading();return;
    }
    if(msg->what=='dPol'){fTimeline->Invalidate();if(!IsHidden())SyncHistory();return;}
    if(msg->what=='rSet' && !fMessageInput->IsEditable()) { SetStatus("Cannot react until this conversation is ready.");return; }
    if(msg->what=='rInc' && !IsGroupChat() && std::string(msg->GetString("actor",""))!="self") {
        msg->RemoveName("name");msg->AddString("name",fFriendlyName.c_str());
    }
    if((msg->what=='rInc' || msg->what=='mCor' || msg->what=='mRed') && fLoadingArchive){if(fDeferredReactions.size()<128)fDeferredReactions.push_back(*msg);return;}
    if(msg->what=='rSet' && fLoadingArchive){SetStatus("Wait for history to finish loading before reacting.");return;}
    if(msg->what=='hDon') {
        fLoadingArchive=false;std::vector<BMessage> records;BMessage m;bool changed=false;
        const bool initial=!fArchiveSince;
        if(msg->GetBool("complete",false))fArchiveSince=fArchiveStarted;
        for(int i=0;msg->FindMessage("record",i,&m)==B_OK;++i)records.push_back(m);
        std::stable_sort(records.begin(),records.end(),[](const auto& a,const auto& b){return a.GetInt64("time",0)<b.GetInt64("time",0);});
        for(auto& item:records)if(item.what=='hRec') {
            if(!IsGroupChat()){item.RemoveName("name");item.AddString("name",item.GetBool("outgoing",false)?OurRepresentation().c_str():fFriendlyName.c_str());}
            else if(std::string(item.GetString("name",""))==_group_username){item.RemoveName("outgoing");item.AddBool("outgoing",true);}
            changed=fHistory->MergeArchive(item)||changed;
        }
        for(auto& item:records)if(item.what=='mCor') {
            const bool own=item.GetBool("outgoing",false) || (IsGroupChat() && std::string(item.GetString("name",""))==_group_username);
            changed=fHistory->Correct(item.GetString("replace",""),item.GetString("id",""),item.GetString("body",""),item.GetString("sender",""),own,item.GetInt64("time",0))||changed;
        }
        for (const auto& item : records) if (item.what=='mRed') {
            if (IsGroupChat() && std::string(item.GetString("name", ""))==_group_username) continue;
            changed=fHistory->MarkRead(item.GetString("id", ""),item.GetString("actor", "")) || changed;
        }
        // Rebuild once, chronologically, after the bounded archive batch. No outgoing replay.
        if(changed) {
            auto* bar=fTimelineScroller->ScrollBar(B_VERTICAL);float low,high;bar->GetRange(&low,&high);
            const float position=bar->Value();const bool bottom=position>=high-1;
            fLastMessageDay.clear();fTimelineDocument.SetTo(new TextDocument(),true);fTimeline->SetTextDocument(fTimelineDocument);
            if(fReactions)fReactions->Rebind(fTimelineDocument);
            if(fAttachments)fAttachments->Rebind(fTimelineDocument);
            RestoreHistory();
            if(!bottom)bar->SetValue(position);
        }
        for(auto& item:records)if(item.what=='rInc' && fReactions) {
            if(!IsGroupChat()){item.RemoveName("name");item.AddString("name",std::string(item.GetString("actor",""))=="self"?"You":fFriendlyName.c_str());}
            else if(std::string(item.GetString("name",""))==_group_username){item.RemoveName("actor");item.AddString("actor","self");}
            fReactions->Handle(item);
        }
        for(auto live:fDeferredReactions){if(live.what=='mCor' || live.what=='mRed')MessageReceived(&live);else if(fReactions)fReactions->Handle(live);}
        fDeferredReactions.clear();
        if(initial || changed || !msg->GetBool("complete",false) || !fHistory->Error().empty())
            SetStatus(fHistory->Error().empty()?msg->GetString("status","History loaded."):fHistory->Error());
        return;
    }
    if(msg->what=='mRed'){ReceiveRead(*msg);return;}
    if(fReactions && fReactions->Handle(*msg))return;
	if (fAttachments && fAttachments->HandleMessage(*msg)) return;
	switch(msg->what) {
        case 'mEnd': {
            fTimeline->Relayout();float low,high;auto* bar=fTimelineScroller->ScrollBar(B_VERTICAL);
            bar->GetRange(&low,&high);bar->SetValue(high);UpdateReading();break;
        }
        case 'mCtx': {
            auto* menu=new BPopUpMenu("Message");
            for(const auto& choice:std::vector<std::pair<const char*,uint32>>{{"Reply",'mRpl'},{"Edit…",'mEdt'},{"Forward…",'mFwd'},{"Copy",'mCpy'},{"React…",'rMor'}}) {
                if(choice.second=='mEdt' && (!fHistory || !fHistory->Editable(msg->GetString("id",""),IsGroupChat())))continue;
                auto* action=new BMessage(*msg);action->what=choice.second;
                auto* item=new BMenuItem(choice.first,action);menu->AddItem(item);item->SetTarget(this);
                if(choice.second=='mEdt')item->SetEnabled(fMessageInput->IsEditable() && fEditId.empty() && fReplyId.empty() && !fLoadingArchive);
                if(choice.second=='mRpl'||choice.second=='rMor')item->SetEnabled(fMessageInput->IsEditable() && *msg->GetString("id",""));
            }
            menu->Go(msg->GetPoint("point",BPoint()),true,true,true);break;
        }
        case 'mEdt': {
            const std::string id=msg->GetString("id","");
            if(!fMessageInput->IsEditable() || !fEditId.empty() || !fReplyId.empty() || fLoadingArchive
                || !fHistory || !fHistory->Editable(id,IsGroupChat()))break;
            auto original=fHistory->Record(id);
            const auto* delivery=fDeliveries.Find(original.GetString("id",""));
            if(delivery && (delivery->state==Chat::Delivery::Queued || delivery->state==Chat::Delivery::Failed)) {
                SetStatus("This message has not been sent. Retry the original message first.");break;
            }
            fEditSavedDraft=fMessageInput->Text();fEditId=original.GetString("id","");
            fMessageInput->SetText(original.GetString("failed-edit-body",original.GetString("body","")));
            fReplyLabel->SetText("Edit message. Enter saves; Cancel keeps the original.");
            fReplyLabel->SetToolTip(original.GetString("body",""));
            if(auto* cancel=dynamic_cast<BButton*>(FindView("cancel-reply")))cancel->SetLabel("Cancel edit");
            if(fReplyStrip->IsHidden())fReplyStrip->Show();
            fMessageInput->MakeFocus(true);break;
        }
        case 'mCor': {
            if(!fHistory)break;
            const std::string id=msg->GetString("id","");
            const bool own=msg->GetBool("outgoing",false);
            if(fHistory->Correct(msg->GetString("replace",""),id,msg->GetString("body",""),msg->GetString("sender",""),own,msg->GetInt64("time",real_time_clock_usecs())))RebuildHistory();
            if(own && fHistory->IsCorrection(id))SetStatus("Edit accepted by XMPP; delivery is not yet confirmed.");
            break;
        }
        case 'mCpy': {
            const std::string body=msg->GetString("body","");
            if(be_clipboard->Lock()){be_clipboard->Clear();be_clipboard->Data()->AddData("text/plain",B_MIME_TYPE,body.data(),body.size());be_clipboard->Commit();be_clipboard->Unlock();}
            break;
        }
        case 'mFwd': {
            BMessage forward(*msg);forward.what='fSel';forward.AddUInt64("generation",JabberSpeak::Instance()->ConnectionGeneration());
            Window()->PostMessage(&forward);break;
        }
        case 'mRpl': {
            if(!fMessageInput->IsEditable() || !fEditId.empty())break;
            fReplyId=msg->GetString("id","");fReplyTo=msg->GetString("sender","");fReplyBody=Chat::ReplyPrefix(msg->GetString("body",""));
            if(fReplyId.empty())break;
            const std::string label="Reply to "+std::string(msg->GetString("author",""))+": "+Conversations::Preview(msg->GetString("body",""));
            fReplyLabel->SetText(label.c_str());fReplyLabel->SetToolTip(msg->GetString("body",""));
            if(fReplyStrip->IsHidden())fReplyStrip->Show();
            fMessageInput->MakeFocus(true);break;
        }
        case 'mRcl': if(!fEditId.empty())EndEdit();fReplyId.clear();fReplyTo.clear();fReplyBody.clear();if(!fReplyStrip->IsHidden())fReplyStrip->Hide();break;
        case 'rMor': {
            if(!fMessageInput->IsEditable())break;
            if(fReactionPicker.IsValid())fReactionPicker.SendMessage(B_QUIT_REQUESTED);
            auto account=BlabberSettings::Instance()->Data("last-login").value_or("");
            auto* picker=new MediaPicker(BMessenger(this),gloox::JID(account).bare(),fFriendlyName,
                Window()->Frame().LeftTop()+BPoint(30,30),msg->GetString("id",""));
            fReactionPicker=BMessenger(picker);picker->Show();break;
        }
        case 'mPkr': {
            if(!fMessageInput->IsEditable())break;
            BMessage mode('mMod');mode.AddInt32("mode",msg->GetInt32("mode",0));
            if(fMediaPicker.IsValid()){fMediaPicker.SendMessage(&mode);BMessage activate('mAct');fMediaPicker.SendMessage(&activate);break;}
            auto account=BlabberSettings::Instance()->Data("last-login").value_or("");
            auto* picker=new MediaPicker(BMessenger(this),gloox::JID(account).bare(),fFriendlyName,Window()->Frame().LeftTop()+BPoint(30,30));
            fMediaPicker=BMessenger(picker);picker->PostMessage(&mode);picker->Show();break;
        }
        case 'eIns': {
            if(!fMessageInput->IsEditable())break;
            const char* emoji=msg->GetString("emoji","");
            int32 start,end;fMessageInput->GetSelection(&start,&end);
            fMessageInput->Delete(start,end);fMessageInput->Insert(start,emoji,strlen(emoji));
            fMessageInput->Select(start+strlen(emoji),start+strlen(emoji));fMessageInput->MakeFocus(true);break;
        }
        case 'aSta':SetStatus(msg->GetString("text",""));break;
		case B_COLORS_UPDATED: {
			// BTextView keeps explicit run colors when the system palette changes.
			const rgb_color textColor = ui_color(B_DOCUMENT_TEXT_COLOR);
			fMessageInput->SetFontAndColor(nullptr, 0, &textColor);
			fMessageInput->SetFontAndColor(0, fMessageInput->TextLength(), nullptr, 0, &textColor);
			BGroupView::MessageReceived(msg);
			break;
		}
		case 'pVis':
			if (IsGroupChat() && _people->CountItems() > 0) {
				bool hidden = fPeopleSplit->IsItemCollapsed(1);
				fPeopleSplit->SetItemCollapsed(1, !hidden);
				fPeopleSplit->SetSplitterSize(hidden ? 4 : 0);
			}
			break;
		case 'uSel':
		{
			if (!fMessageInput->IsEditable() || !fEditId.empty()) break;
			if (!fAttachmentPanel) {
				BMessenger target(this);
				BMessage selected('uRef');
				fAttachmentPanel = new BFilePanel(B_OPEN_PANEL, &target, nullptr,
					B_FILE_NODE, false, &selected);
			}
			fAttachmentPanel->Show();
			break;
		}
		case 'uRef':
		{
            struct Reply { BMessage* request; bool accepted=false; ~Reply() { if(request->HasInt64("media-token")) { BMessage result('gAck');result.AddBool("accepted",accepted);request->SendReply(&result); } } } reply{msg};
            auto lease = Giphy::Acquire(msg->GetInt64("media-token",0));
            if(msg->HasInt64("media-token") && !lease)break;
			if (!fMessageInput->IsEditable() || !fEditId.empty()) break;
			if (fUploadTask) break;
			entry_ref ref;
			if (msg->FindRef("refs", &ref) != B_OK) break;
			BPath path(&ref);
			if (path.InitCheck() != B_OK) break;
            if(lease && lease->path != path.Path())break;
			struct stat info;
			if (stat(path.Path(), &info) == 0 && info.st_size > (off_t)Upload::kMaxFileSize) {
				ModalAlertFactory::NonModalAlert(B_TRANSLATE("Filen er større enn 50 MiB"), B_TRANSLATE("OK"));
				break;
			}
            if (!JabberSpeak::Instance()->CanSendTo(IsGroupChat() ? _group_room : _session->target().full())) {
                SetStatus("Signal is not ready. Attachment was not queued.");
                break;
            }
			fUploadFilename = path.Leaf();
			fUploadTask = JabberSpeak::Instance()->QueueUpload(path.Path(),
				IsGroupChat() ? _group_room : _session->target().full(), IsGroupChat(), BMessenger(this), lease);
			if (!fUploadTask) {
				ModalAlertFactory::NonModalAlert(B_TRANSLATE("Connect before sending an attachment."), B_TRANSLATE("OK"));
				break;
			}
            reply.accepted=true;
			BMessage poll('uPol');
			fUploadPoll = new BMessageRunner(BMessenger(this), poll, 250000);
			fAttach->SetEnabled(false);
			fCancelUpload->SetEnabled(true);
			fUploadProgress->Reset(fUploadFilename.c_str(), B_TRANSLATE("Preparing…"));
			if (fUploadStrip->IsHidden()) fUploadStrip->Show();
			break;
		}
		case 'uPol':
		{
			// Recover the latest state if a busy looper dropped a nonblocking notice.
			if (!fUploadTask) break;
			BMessage notice;
			{
				std::lock_guard<std::mutex> lock(fUploadTask->noticeLock);
				notice = fUploadTask->latestNotice;
			}
			if (notice.what == Upload::kTransferNotice) MessageReceived(&notice);
			break;
		}
		case 'uCan':
			if (fUploadTask) {
				fUploadTask->Cancel();
				fCancelUpload->SetEnabled(false);
				fUploadProgress->Reset(B_TRANSLATE("Cancelling…"));
			}
			break;
		case Upload::kTransferNotice:
		{
			if (!fUploadTask || fUploadTask->token != msg->GetUInt64("token", 0)) break;
			BString state(msg->GetString("state", "error"));
			fUploadProgress->Reset(fUploadFilename.c_str(), msg->GetString("text", ""));
			const int64 size = msg->GetInt64("size", 0);
			const int64 sent = msg->GetInt64("sent", 0);
			if (size > 0) {
				const float percent = 100.0f * sent / size;
				BString label; label.SetToFormat("%.0f%%", percent);
				fUploadProgress->Update(percent, nullptr, label.String());
			}
			if (state == "sent" || state == "error" || state == "cancelled") {
                if (state == "sent") {
                    const auto id = msg->GetString("message-id", "");
                    fDeliveries.Add(id, msg->GetString("url", ""));
                    std::list<TextSpan> runs;
                    runs.emplace_back(msg->GetString("url", ""), CharacterStyle());
                    AddToTalk(OurRepresentation(), runs, LOCAL, true, 0, false, id, msg->GetString("url", ""));
                    DeliveryStatus(id, Chat::Delivery::Unconfirmed);
                }
				if (state == "error")
					ModalAlertFactory::NonModalAlert(msg->GetString("text", "Attachment failed"), B_TRANSLATE("OK"));
				if (!fUploadStrip->IsHidden()) fUploadStrip->Hide();
				fUploadTask.reset();
				delete fUploadPoll;
				fUploadPoll = nullptr;
				fAttach->SetEnabled(fMessageInput->IsEditable());
				fCancelUpload->SetEnabled(false);
			}
			break;
		}
		case JAB_CLOSE_TALKS:
		{
			RemoveSelf();
			delete this;
			break;
		}

		case 'IDLE':
		{
			// There is no ping running for the forst 2 minutes after joining a room. So the first
			// run will always get here (with fPingTime not yet set), in this case we just send
			// the first ping.
			if (fIsPinging) {
				// FIXME is it ok to do this from window thread? Or should
				// we go through main app?
				gloox::MUCRoom* room = (gloox::MUCRoom*)TalkManager::Instance()
					->IsExistingWindowToGroup(GetGroupRoom());
				room->join();
				fLagMeter->SetText("Lag: > 120.0");
				fLagMeter->SetHighUIColor(B_FAILURE_COLOR);
                if(Window())Window()->PostMessage('tSta');
			}

			// Nothing happened for 2 minutes, send a ping now and wait to get a reply
			_idle_timer->SetInterval(kSelfPingDelay);
			fPingSendTime = system_time();
			fIsPinging = true;

			// FIXME this should not be in a view class running in the window thread...
			gloox::ClientBase* client = JabberSpeak::Instance()->GlooxClient();
			gloox::JID jid(_group_room);
			jid.setResource(_group_username);
			client->xmppPing(jid, this);

			break;
		}

		case 'hlnk':
		{
			int32 buttons = msg->GetInt32("buttons", 0);
			BString url = msg->GetString("target");

			if (buttons & B_PRIMARY_MOUSE_BUTTON) {
				// load up browser!!
				char *argv[] = {const_cast<char *>(url.String()), NULL};
				if (!be_roster->IsRunning("text/html"))
					be_roster->Launch("text/html", 1, argv);
				else {
					BMessenger messenger("text/html");
					BMessage msg(B_NETPOSITIVE_OPEN_URL);
					msg.AddString("be:url", url);
					messenger.SendMessage(&msg);
				}
			} else {
				// popup menu to copy the URL (add it to the existing BTextView menu?)
				BPopUpMenu* menu = new BPopUpMenu(B_EMPTY_STRING, false, false);

				BMessage* copyMessage = new BMessage('hcpy');
				copyMessage->AddString("url", url);
				BMessage* openMessage = new BMessage('hope');
				openMessage->AddString("url", url);

                auto* copyItem = new BMenuItem("Copy link location", copyMessage);
                auto* openItem = new BMenuItem("Open in browser", openMessage);
                copyItem->SetTarget(BMessenger(this));
                openItem->SetTarget(BMessenger(this));
                menu->AddItem(copyItem);
                menu->AddItem(openItem);
                BPoint point; uint32 buttons;
                fTimeline->GetMouse(&point, &buttons);
                fTimeline->ConvertToScreen(&point);
                menu->SetAsyncAutoDestruct(true);
                menu->Go(point, true, true, true);
			}

			break;
		}

		// From popup menu on hyperlinks
		case 'hope':
		{
			// Open URL in browser
			BString url = msg->FindString("url");
			char *argv[] = {const_cast<char *>(url.String()), NULL};
			if (!be_roster->IsRunning("text/html"))
				be_roster->Launch("text/html", 1, argv);
			else {
				BMessenger messenger("text/html");
				BMessage msg(B_NETPOSITIVE_OPEN_URL);
				msg.AddString("be:url", url);
				messenger.SendMessage(&msg);
			}
			break;
		}
		// From popup menu on hyperlinks
		case 'hcpy':
		{
			be_clipboard->Lock();
			be_clipboard->Clear();
			BMessage* clip = be_clipboard->Data();
			BString string = msg->FindString("url");
			clip->AddData("text/plain", B_MIME_TYPE, string.String(), string.Length());
			be_clipboard->Commit();
			be_clipboard->Unlock();
			break;
		}

		case 'rqvc':
		{
			// Requext voice
			gloox::MUCRoom* room = (gloox::MUCRoom*)TalkManager::Instance()
				->IsExistingWindowToGroup(GetGroupRoom());
			room->setRole(_group_username.c_str(), gloox::RoleVisitor);
			break;
		}

		case kEditTopic:
		{
			gloox::MUCRoom* room = (gloox::MUCRoom*)TalkManager::Instance()
				->IsExistingWindowToGroup(GetGroupRoom());
			ChangeTopicWindow* changeSubject = new ChangeTopicWindow(*room,
				dynamic_cast<BTextToolTip*>(_status_view->ToolTip())->Text());
			changeSubject->Show();
			break;
		}

		case B_OBSERVER_NOTICE_CHANGE:
		{
            if(msg->GetInt32("be:observe_change_what",msg->GetInt32("be:observe_orig_what",0))==Avatars::kChanged){fTimeline->Invalidate();if(auto* avatar=FindView("conversation-avatar"))avatar->Invalidate();break;}
			// only for groupchat
			if (!IsGroupChat())
				break;

			switch(msg->GetInt32("be:observe_change_what", msg->GetInt32("be:observe_orig_what", 0)))
			{
				case kRoomInfo:
				{
					BString jid = msg->FindString("jid");
					if (jid == _group_room.c_str()) {
						RefreshHeader(msg->GetString("name", ""));
						uint32 flags = msg->FindInt32("flags");
						if (flags & gloox::FlagModerated) {
							// Not needed, we are only interested to know if the current user can talk
							// printf("Room %s is moderated\n", jid.String());
						}
					}
					break;
				}

				case JAB_GROUP_CHATTER_ONLINE:
				{
					if (GetGroupRoom() == msg->FindString("room")) {
						AddGroupChatter(msg->FindString("occupant-jid"),
							(gloox::MUCRoomAffiliation)msg->FindInt32("affiliation"),
							(gloox::MUCRoomRole)msg->FindInt32("role"),
							(gloox::Presence::PresenceType)msg->FindInt32("presence"),
							msg->FindString("real-jid"), msg->FindString("status"));
					} else {
						// TODO we shouldn't need to check the room here, right? If each TalkView
						// is only watching the corresponding TalkManager instance.
						// Currently there is a single TalkManager for all rooms and so every
						// view receives every notification...
					}
					break;
				}

				case JAB_GROUP_CHATTER_OFFLINE: {
					if (GetGroupRoom() == msg->FindString("room")) {
						RemoveGroupChatter(msg->FindString("occupant-jid"));

						BString newNick = msg->FindString("newNick");
						if (!newNick.IsEmpty()) {
							std::list<TextSpan> runs;
							BString nickChangeMessage;
							gloox::JID jid(msg->FindString("occupant-jid"));
							nickChangeMessage.SetToFormat(B_TRANSLATE("%s is now known as %s"),
								jid.resource().c_str(), newNick.String());
							runs.emplace_back(nickChangeMessage, sSystemMessagesStyle);
							AddToTalk("", runs, OTHER);
						}

						// if the message includes a "status" string, add a visible message with
						// that to the room. Otherwise, do nothing, to not flood the log with
						// join/part messages
						BString status = msg->FindString("status");
						if (!status.IsEmpty()) {
							std::list<TextSpan> runs;
							BString leftRoomMessage;
							gloox::JID jid(msg->FindString("occupant-jid"));
							leftRoomMessage.SetToFormat(B_TRANSLATE("%s has left the room: %s"),
								jid.resource().c_str(), status.String());
							runs.emplace_back(leftRoomMessage, sSystemMessagesStyle);
							AddToTalk("", runs, OTHER);
						}
					}
					break;
				}
				break;
			}
			break;
		}

		case JAB_SHOW_CHATLOG: {
			// just forward to main blabber window...
			BMessage *msgForward = new BMessage(*msg);
			BlabberMainWindow::Instance()->PostMessage(msgForward);
			break;
		}


		case BLAB_UPDATE_ROSTER: {
			// doesn't apply to groupchat
			if (IsGroupChat())
				break;

			// get new status
			JRoster::Instance()->Lock();
			UserID* user = JRoster::Instance()->FindUser(_session->target());
			if (!user) {
				JRoster::Instance()->Unlock();
				break;
			}
			UserID::online_status new_status = user->OnlineStatus();

			// if we have one, check their presence
			if (_current_status != new_status) {
				if (_current_status == UserID::ONLINE && new_status == UserID::OFFLINE) {
					SetStatus(B_TRANSLATE("This user is now offline."));
				} else if (_current_status == UserID::OFFLINE && new_status == UserID::ONLINE) {
					SetStatus(B_TRANSLATE("This user is now online."));
				}
			}

			_current_status = new_status;

			JRoster::Instance()->Unlock();

			break;
		}

        case 'rTry': {
            const auto* failed = fDeliveries.Find(msg->GetString("id", ""));
            if (!failed || failed->state != Chat::Delivery::Failed) break;
            if (fMessageInput->TextLength() != 0 || !fReplyId.empty() || !fEditId.empty()) {
                SetStatus("Finish or save your current draft before restoring the failed message.");
                break;
            }
            fReplyId=failed->replyId;fReplyTo=failed->replyTo;fReplyBody=failed->replyPrefix;
            fMessageInput->SetText(failed->text.substr(fReplyBody.size()).c_str());
            if(!fReplyId.empty()){fReplyLabel->SetText(failed->replyLabel.c_str());if(fReplyStrip->IsHidden())fReplyStrip->Show();}
            fMessageInput->MakeFocus(true);
            SetStatus("Failed text restored. Press Enter to explicitly try again.");
            break;
        }
		case JAB_CHAT_SENT:
		{
            if (!JabberSpeak::Instance()->IsConnected() || !fMessageInput->IsEditable()) break;
            if (!JabberSpeak::Instance()->CanSendTo(IsGroupChat() ? _group_room : _session->target().full())) {
                SetStatus("Signal is not ready. Your draft is retained; no message has been queued.");
                break;
            }
			BString message = fMessageInput->Text();

			// eliminate empty messages
			if (message.IsEmpty())
				break;

            if(!fEditId.empty()) {
                if(!fHistory || !fHistory->Editable(fEditId,IsGroupChat())){SetStatus("This message can no longer be edited.");break;}
                auto original=fHistory->Record(fEditId);
                if(std::string(message.String())==original.GetString("body","")){EndEdit();break;}
                const std::string target=original.GetString("wire-id",fEditId.c_str());
                if(target.empty()){SetStatus("Original message ID is unavailable. Nothing was sent.");break;}
                auto id=JabberSpeak::Instance()->QueueText(IsGroupChat()?_group_room:_session->target().full(),message.String(),IsGroupChat(),BMessenger(this),"","","",0,target,
                    [&](const std::string& request){return fHistory->Correct(fEditId,request,message.String(),original.GetString("sender",""),true,real_time_clock_usecs());});
                if(id.empty()){SetStatus("Edit not queued. Your edited text is retained.");break;}
                EndEdit();RebuildHistory();SetStatus("Edit sent. Delivery is not yet confirmed.");break;
            }
			if (_session == NULL) {
				// FIXME is it ok to do this from window thread? Or should
				// we go through main app?
				std::string roomName = GetGroupRoom();
				gloox::MUCRoom* room = (gloox::MUCRoom*)TalkManager::Instance()
					->IsExistingWindowToGroup(roomName);

				if (message.StartsWith("/part ")) {
					BookmarkManager::Instance().RemoveBookmark(roomName.c_str());
					TalkManager::Instance()->LeaveRoom(roomName, message.Remove(0, 6).String());
					return;
				}

				if (message.StartsWith("/nick ")) {
					TalkManager::Instance()->SetNickname(roomName, message.Remove(0, 6).String());
					// Reset message input box
					fMessageInput->ScrollToOffset(0);
					fMessageInput->SetText("");
					fMessageInput->MakeFocus(true);
					return;
				}

                (void)room;
            }
            std::string mention;
            if (IsGroupChat() && fLastCompletedNicknameIndex >= 0) {
                auto* person = dynamic_cast<PeopleListItem*>(_people->ItemAt(fLastCompletedNicknameIndex));
                if (person) mention = person->OccupantJID();
                fLastCompletedNicknameIndex = -1;
            }
            const std::string wire=fReplyBody+message.String();
            auto id = JabberSpeak::Instance()->QueueText(IsGroupChat() ? _group_room : _session->target().full(),
                wire, IsGroupChat(), BMessenger(this), mention, fReplyId, fReplyTo, Chat::Codepoints(fReplyBody));
            if (id.empty()) {
                SetStatus("Message not queued. Check connection or shorten the message; your draft is retained.");
                break;
            }
            fDeliveries.Add(id, wire, fReplyId, fReplyTo, fReplyBody, fReplyLabel->Text());
            std::list<TextSpan> runs;
            runs.emplace_back(wire.c_str(), CharacterStyle());
            AddToTalk(OurRepresentation(), runs, LOCAL, true, 0, false, id);
            fReplyId.clear();fReplyTo.clear();fReplyBody.clear();if(!fReplyStrip->IsHidden())fReplyStrip->Hide();
            DeliveryStatus(id, Chat::Delivery::Queued);
            Window()->PostMessage('mEnd',this);

			// Reset message input box
			fMessageInput->ScrollToOffset(0);
			fMessageInput->SetText("");
			fMessageInput->MakeFocus(true);

			break;
		}

		case JAB_OPEN_CHAT:
		{
			// Open private messages from participants list in a group chat
			msg->PrintToStream();
			const char* jidstring = msg->FindString("jid");
			gloox::JID jid(jidstring);
			TalkManager::Instance()->CreateTalkSession(&jid);

			Iterator it = std::lower_bound(Iterator(0), Iterator(_people->CountItems()), jid,
				[this](const Iterator& i, const gloox::JID& jid) {
					PeopleListItem *iterating_item = dynamic_cast<PeopleListItem *>(_people->ItemAt(i.m_ptr));
					return strcmp(iterating_item->Nickname().c_str(), jid.resource().c_str()) < 0;
				}
			);

			if (it.m_ptr < 0 || it.m_ptr >= _people->CountItems()) {
				debugger("Item not found?");
				break;
			}
			dynamic_cast<PeopleListItem*>(_people->ItemAt(it.m_ptr))->SetMarked(false);
			break;
		}

		case JAB_CLOSE_CHAT:
		{
			RemoveSelf();
			delete this;
			break;
		}

        case JAB_FOCUS_BUDDY:
		{
	         BlabberMainWindow::Instance()->Activate();
             break;
        }

        case kOwnGroupEcho: {
            const auto id = msg->GetString("id", "");
            if (fDeliveries.Find(id)) {
                DeliveryStatus(id, Chat::Delivery::Accepted);
                if(fReactions)fReactions->Alias(id,msg->GetString("reaction-id",""));
            } else {
                std::list<TextSpan> runs;
                runs.emplace_back(msg->GetString("content", ""), CharacterStyle());
                AddToTalk(OurRepresentation(), runs, LOCAL, true, 0, false, msg->GetString("reaction-id", id), msg->GetString("attachment", ""));

            }
            break;
        }
        case kMessageSubmitted:
            if(fHistory && fHistory->IsCorrection(msg->GetString("id","")))break;
            DeliveryStatus(msg->GetString("id", ""), Chat::Delivery::Unconfirmed);
            break;
        case kMessageReceipt:
            if(fHistory && fHistory->ConfirmCorrection(msg->GetString("id",""))){RebuildHistory();SetStatus("Edit delivered.");break;}
            DeliveryStatus(msg->GetString("id", ""), Chat::Delivery::Delivered);
            break;
        case kMessageDeliveryError: {
            const std::string id = msg->GetString("id", "");
            if(fHistory && fHistory->RejectCorrection(id)){RebuildHistory();SetStatus("Edit rejected. Original restored; choose Edit to recover your changed text.");break;}
            const auto* outgoing = fDeliveries.Find(id);
            if (outgoing && outgoing->state == Chat::Delivery::Delivered) break;
            DeliveryStatus(id, Chat::Delivery::Failed);
            ShowError(static_cast<gloox::StanzaError>(msg->GetInt32("condition",
                gloox::StanzaErrorUndefined)), outgoing != nullptr);
            break;
        }

		case kIncomingMessage:
		{
			if (BString(msg->GetString("content", "")).IsEmpty()
				&& BString(msg->GetString("attachment", "")).IsEmpty()) break;
			gloox::RosterManager* rm = JabberSpeak::Instance()->GlooxClient()->rosterManager();
			gloox::RosterItem* item = rm->getRosterItem(_session->target());
            const bool outgoing = msg->GetBool("outgoing", false);
            const std::string sender = outgoing ? OurRepresentation()
                : (item && !item->name().empty() ? item->name() : fFriendlyName);
            std::list<TextSpan> runs;
            runs.emplace_back(msg->GetString("content", ""), CharacterStyle());
            AddToTalk(sender, runs, outgoing ? LOCAL : MAIN_RECIPIENT, true,
                msg->GetInt64("time", 0), msg->GetBool("archive", false),
                msg->GetString("id", ""), msg->GetString("attachment", ""), "", "", msg->GetBool("markable", false));

			break;
		}

		case kHighlightRosterItem:
		{
			const char* who = msg->FindString("who");
			if (who) {
				Iterator it = std::lower_bound(Iterator(0), Iterator(_people->CountItems()), who,
					[this](const Iterator& i, const char* who) {
						PeopleListItem *iterating_item = dynamic_cast<PeopleListItem *>(_people->ItemAt(i.m_ptr));
						return strcmp(iterating_item->Nickname().c_str(), who);
					}
				);

				dynamic_cast<PeopleListItem*>(_people->ItemAt(it.m_ptr))->SetMarked(true);
			}
			BGroupView::MessageReceived(msg);
			break;
		}

		default:
			BGroupView::MessageReceived(msg);
			break;
	}
}


std::string TalkView::OurRepresentation() {
	// use friendly name if you have it
	std::string user = JabberSpeak::Instance()->CurrentRealName();

	return user;
}


void TalkView::ShowError(const gloox::StanzaError& error, bool deliveryFailure)
{
	BString errorString;
	switch (error) {
		case gloox::StanzaErrorBadRequest:
			errorString = "The sender has sent XML that is malformed or that cannot be processed";
			break;
		case gloox::StanzaErrorConflict:
			errorString = "Access cannot be granted because an existing resource or session exists "
				"with the same name or address";
			break;
		case gloox::StanzaErrorFeatureNotImplemented:
			errorString = "The feature requested is not implemented by the recipient or server and "
				"therefore cannot be processed";
			break;
		case gloox::StanzaErrorForbidden:
			errorString = "The requesting entity does not possess the required permissions to "
				"perform the action";
			break;
		case gloox::StanzaErrorGone:
			errorString = "The recipient or server can no longer be contacted at this address";
			break;
		case gloox::StanzaErrorInternalServerError:
			errorString = "The server could not process the stanza because of a misconfiguration "
				"or an otherwise-undefined internal server error";
			break;
		case gloox::StanzaErrorItemNotFound:
			errorString = "The addressed JID or item requested cannot be found";
			break;
		case gloox::StanzaErrorJidMalformed:
			errorString = "The sending entity has provided or communicated an XMPP address that "
				"does not adhere to the syntax defined in Addressing Scheme (Section 3)";
			break;
		case gloox::StanzaErrorNotAcceptable:
			errorString = "The recipient or server understands the request but is refusing to "
				"process it because it does not meet criteria defined by the recipient or server";
			break;
		case gloox::StanzaErrorNotAllowed:
			errorString = "The recipient or server does not allow any entity to perform the action";
			break;
		case gloox::StanzaErrorNotAuthorized:
			errorString = "The sender must provide proper credentials before being allowed to "
				"perform the action, or has provided impreoper credentials";
			break;
		case gloox::StanzaErrorNotModified:
			errorString = "The item requested has not changed since it was last requested";
			break;
		case gloox::StanzaErrorPaymentRequired:
			errorString = "The requesting entity is not authorized to access the requested service "
				"because payment is required";
			break;
		case gloox::StanzaErrorRecipientUnavailable:
			errorString = "The intended recipient is temporarily unavailable";
			break;
		case gloox::StanzaErrorRedirect:
			errorString = "The recipient or server is redirecting requests for this information to "
				"another entity";
			break;
		case gloox::StanzaErrorRegistrationRequired:
			errorString = "The requesting entity is not authorized to access the requested service "
				"because registration is required";
			break;
		case gloox::StanzaErrorRemoteServerNotFound:
			errorString = "A remote server or service specified as part or all of the JID of the "
				"intended recipient does not exist";
			break;
		case gloox::StanzaErrorRemoteServerTimeout:
			errorString = "A remote server or service specified as part or all of the JID of the "
				"intended recipient (or required to fulfill a request) could not be contacted "
				"within a reasonable amount of time";
			break;
		case gloox::StanzaErrorResourceConstraint:
			errorString = "The server or recipient lacks the system resources necessary to service "
				"the request";
			break;
		case gloox::StanzaErrorServiceUnavailable:
			errorString.SetToFormat("The server or recipient does not currently provide the "
					"requested service %s", fFriendlyName.c_str());
			break;
		case gloox::StanzaErrorSubscribtionRequired:
			errorString = "The requesting entity is not authorized to access the requested service "
				"because a subscription is required";
			break;
		case gloox::StanzaErrorUndefinedCondition:
			errorString = "The error condition is not one of those defined by the other conditions "
				"in this list; any error type may be associated with this condition";
			break;
		case gloox::StanzaErrorUnexpectedRequest:
			errorString = "The recipient or server understood the request but was not expecting it "
				"at this time (e.g., the request was out of order)";
			break;
		case gloox::StanzaErrorUnknownSender:
			errorString = "The stanza 'from' address specified by a connected client is not valid "
				"for the stream (e.g., the stanza does not include a 'from' address when multiple "
				"resources are bound to the stream)";
			break;
		case gloox::StanzaErrorUndefined:
			errorString = "The server did not provide an error reason.";
			break;
		default:
			errorString.SetToFormat("Unknown error %d", error);
			break;
	}

	if (deliveryFailure) errorString.Prepend("Sending failed. Delivery is not confirmed. ");
	SetStatus(errorString.String());

	std::list<TextSpan> runs;
	runs.emplace_back(errorString, sSystemMessagesStyle);
	AddToTalk("ERROR", runs, OTHER);
}


extern "C" void hsluv2rgb(double, double, double, rgb_color*);

rgb_color
ConsistentColor(const std::string& username)
{
	EVP_MD_CTX* hashctx = EVP_MD_CTX_new();
	const EVP_MD *hashptr = EVP_get_digestbyname("SHA1");
	EVP_DigestInit_ex(hashctx, hashptr, NULL);

	EVP_DigestUpdate(hashctx, username.c_str(), username.length());

	uint8_t buffer[256];
	unsigned int outlen;
	EVP_DigestFinal_ex(hashctx, buffer, &outlen);
	EVP_MD_CTX_free(hashctx);

	float angle = ((buffer[1] << 8) | (buffer[0])) * 360 / 65536.0;
	rgb_color out;
	const auto background=ui_color(B_DOCUMENT_BACKGROUND_COLOR);
    const bool dark=int(background.red)+background.green+background.blue<384;
    hsluv2rgb(angle, 85, dark?72:38, &out);
	return out;
}


bool TalkView::AddToTalk(std::string username, std::list<TextSpan>& runs,
	TalkView::user_type type, bool parseLinks, int64 timestamp, bool archive, const std::string& messageId, const std::string& suppliedAttachment, const std::string& senderJid, const std::string& wireId, bool markable)
{
	if (runs.empty()) return false;
    std::string rawBody;for(const auto& run:runs)rawBody+=run.Text().String();
    if (!suppliedAttachment.empty() && rawBody.find(suppliedAttachment) == std::string::npos) {
        rawBody += (rawBody.empty() ? "" : "\n") + suppliedAttachment;
        runs.emplace_back(("\n" + suppliedAttachment).c_str(), CharacterStyle());
    }
    const auto authority = Upload::Profile::Load().authority;
    const auto attachment = Upload::IsServiceUrl(suppliedAttachment, authority) ? suppliedAttachment
        : Upload::BodyAttachmentUrl(rawBody, authority);
    if(!attachment.empty() && rawBody.find(attachment)==std::string::npos)rawBody+=(rawBody.empty()?"":"\n")+attachment;
    const std::string sender=!senderJid.empty()?senderJid:type==LOCAL?(IsGroupChat()?_group_room+"/"+_group_username:Avatars::Account()):
        (IsGroupChat()?(fReplayingHistory?"":_group_room+"/"+username):_session->target().bare());
    for (auto& run : runs) run.SetText(Upload::DisplayAttachmentBody(run.Text().String(), attachment).c_str());
	if (type != OTHER) {
        if(!fReplayingHistory && fHistory && fHistory->Contains(messageId))return false;
		if (IsGroupChat() && type == MAIN_RECIPIENT)
			username = Conversations::FriendlyName(_group_room, username, false);
		if (!IsGroupChat() && type == MAIN_RECIPIENT)
			username = Conversations::FriendlyName(_session->target().bare(), username, false);
		if (timestamp == 0 && !archive) {
			struct timeval now; gettimeofday(&now, nullptr);
			timestamp = int64(now.tv_sec) * 1000000 + now.tv_usec;
		}
		std::string preview;
		for (const auto& run : runs) { preview += run.Text().String(); if (preview.size() >= 256) break; }
		if (preview.empty() && !attachment.empty()) preview = Upload::IsPreviewFilename(attachment) ? "Image" : Upload::AttachmentFilename(attachment);
		if (!preview.empty() && !fReplayingHistory) {
			BMessage activity('cAct');
			activity.AddString("jid", IsGroupChat() ? _group_room.c_str() : _session->target().bare().c_str());
			activity.AddString("name", fFriendlyName.c_str());
			activity.AddString("preview", Conversations::Preview(preview).c_str());
			activity.AddString("id", messageId.c_str());
			activity.AddBool("group", IsGroupChat());
			activity.AddBool("incoming", type == MAIN_RECIPIENT);
			activity.AddBool("visible", Window() && !IsHidden() && Window()->IsActive());
			activity.AddBool("archive", archive);
			activity.AddInt64("time", timestamp);
			activity.AddUInt64("generation", JabberSpeak::Instance()->ConnectionGeneration());
			BlabberMainWindow::Instance()->PostMessage(&activity);
		}
	}
	if(type!=OTHER && !fReplayingHistory && fHistory) {
        BMessage record('hRec');std::string body;for(const auto& run:runs)body+=run.Text().String();
        record.AddString("id",messageId.empty()?("local-"+std::to_string(system_time())).c_str():messageId.c_str());
        record.AddString("body",body.c_str());record.AddString("name",username.c_str());
        record.AddString("sender",sender.c_str());record.AddString("attachment",attachment.c_str());record.AddInt64("time",timestamp);
        if(!wireId.empty())record.AddString("wire-id",wireId.c_str());
        record.AddBool("markable",markable);
        record.AddBool("outgoing",type==LOCAL);fHistory->Put(record);
        if(!fHistory->Error().empty())SetStatus(fHistory->Error());
    }
    // We receive something so we know we're in the room. No need to send a ping then.
	// TODO if message from self, figure out when we sent it, and use that to update the lagmeter
	if (_idle_timer) {
		// Cancel the current ping timeout (if any and re-trig the timer)
		fIsPinging = false;
		_idle_timer->SetInterval(kSelfPingDelay);
	}

	// transform local identity
	if (IsGroupChat() && type == LOCAL)
		username = _group_username;

	// history (for quickly re-sending last sent messages)
	if (type == LOCAL && !fReplayingHistory) {
		// reset chat history index
		_chat_index = -1;

		// add latest (sending rich text is not supported so we know it will be a single run)
		_chat_history.push_front(runs.begin()->Text().String());

		// prune end
		if (_chat_history.size() > 50)
			_chat_history.pop_back();
	}

	// ignore empty messages
	if (runs.size() == 0)
		return false;

	// Figure out if the view is scrolled down before modifying it
	BScrollBar* vscroll = fTimelineScroller->ScrollBar(B_VERTICAL);
	float min, max;
	vscroll->GetRange(&min, &max);
	float scrollBottom = vscroll->Value();
	bool scrolledDown = (scrollBottom == max);

	// prune trailing whitespace
	// FIXME do this before computing Xhtml textruns, otherwise it will offset everything
#if 0
	while (!message.empty() && isspace(message[message.size() - 1]))
		message.erase(message.size() - 1);
#endif

	// Style for local user name
	CharacterStyle tr_thick_red;
	tr_thick_red.SetBold(true);
	tr_thick_red.SetForegroundColor(ConsistentColor(username));

	// Default style for everything else
	CharacterStyle charStyleThinBlack;
	charStyleThinBlack.SetForegroundColor(B_DOCUMENT_TEXT_COLOR);

	ParagraphStyle paragraphStyle;
    paragraphStyle.SetMessageContent(rawBody,sender,username);
	paragraphStyle.SetSpacingTop(type == OTHER ? 4 : 10);
	paragraphStyle.SetSpacingBottom(8);
	paragraphStyle.SetMessageKind(type == OTHER ? 0 : type == LOCAL ? 2 : 1);
	paragraphStyle.SetLineInset(0);
	paragraphStyle.SetFirstLineInset(0);
    if(type!=OTHER) {
        const time_t seconds=timestamp>0?timestamp/1000000:0;
        paragraphStyle.SetMessageTime(MessageFooter(messageId, timestamp));
        paragraphStyle.SetDeliveryMark(MessageDeliveryMark(messageId));
        const auto day=ChatDate::Day(seconds);
        if(day!=fLastMessageDay) {
            ParagraphStyle separator;separator.SetDateDivider(seconds);separator.SetSpacingTop(12);separator.SetSpacingBottom(8);
            Paragraph date(separator);date.Append(TextSpan(" ",charStyleThinBlack));fTimelineDocument->Append(date);
            fLastMessageDay=day;
        }
    }
    if(IsGroupChat() && type!=OTHER)paragraphStyle.SetAvatar(Avatars::Get(Avatars::Account(),type==LOCAL?Avatars::Account():_group_room+"/"+username),username);
    Paragraph paragraph(paragraphStyle);

	BString usernameString(username.c_str(), username.size());

	bool meMessage = false;
	// convert /me
	auto span = runs.begin();
	if (span->Text().StartsWith("/me ")) {
		span->Remove(0, 3);
		if(IsGroupChat())span->Insert(0, usernameString);
        span->SetStyle(sSystemMessagesStyle);
		meMessage = true;
	}


	if (type == MAIN_RECIPIENT) {
		CharacterStyle tra_thick_usercolor;
		tra_thick_usercolor.SetForegroundColor(ConsistentColor(username));
		tra_thick_usercolor.SetBold(true);

		if (!fReplayingHistory && !archive && (!IsGroupChat() || !BlabberSettings::Instance()->Tag("exclude-groupchat-sounds")))
			system_beep(kSoundNewMUCMessage);

		if (meMessage) {
			span->SetStyle(tra_thick_usercolor);
		} else if(IsGroupChat()) {
			paragraph.Append(TextSpan(usernameString, tra_thick_usercolor));
			paragraph.Append(TextSpan("\n", charStyleThinBlack));
		}
	} else if (type == LOCAL) {

		if (meMessage) {
			span->SetStyle(tr_thick_red);
		} else if(IsGroupChat()) {
			paragraph.Append(TextSpan(usernameString, tr_thick_red));
			paragraph.Append(TextSpan("\n", charStyleThinBlack));
		}
	}

    // Format quote fallback lines without changing the stored/wire message.
    bool firstContent = true;
    bool previousQuote = false;
    for (const auto& original : runs) {
        if (!parseLinks || original.Bitmap() || original.ClickMessage()) {
            paragraph.Append(original);
            firstContent = false;
            continue;
        }
        for (const auto& line : Chat::QuoteLines(original.Text().String())) {
            if (firstContent && line.quote) paragraph.Append(TextSpan("\n", charStyleThinBlack));
            if(previousQuote && !line.quote && !line.text.empty() && line.text[0]!='\n')paragraph.Append(TextSpan("\n",charStyleThinBlack));
            previousQuote=line.quote;
            CharacterStyle style(original.Style());
            if (line.quote) style.SetItalic(true);
            TextSpan formatted(line.text.c_str(), style);
            formatted.SetQuote(line.quote);
            GenerateHyperlinkText(formatted, paragraph);
            firstContent = false;
        }
    }

	fTimelineDocument->Append(paragraph);
    if(type!=OTHER && !messageId.empty()) {
        if(!fReactions)fReactions=std::make_unique<ReactionController>(BMessenger(this),*fTimeline,
            fTimelineDocument,IsGroupChat()?_group_room:_session->target().bare(),IsGroupChat(),fHistory.get());
        fReactions->SetBatch(fReplayingHistory);
        fReactions->Add(messageId,fTimelineDocument->CountParagraphs()-1);
    }
	AddAttachment(attachment);
	if(fReplayingHistory)return true;
	// FIXME relayout and invalidate only the newly added paragraph
	fTimeline->Relayout();

	if (scrolledDown) {
		BRect r(0, max, 10000, 0);

		// Get the new range
		vscroll->GetRange(&min, &max);
		r.bottom = max;
		// Scroll to the end of the new range
		vscroll->SetValue(max);

		// In case the view was not already filled to the bottom
		if (r.bottom == 0)
			fTimeline->Invalidate();
		else
			fTimeline->Invalidate(r);
	} else {
		fTimeline->Invalidate();
	}
	return true;
}


const gloox::JID& TalkView::GetUserID() {
	if (_session == NULL)
		debugger("Getting user ID not possible for group chat");

	return _session->target();
}


const std::string& TalkView::GetGroupRoom() {
	return _group_room;
}


const std::string& TalkView::GetGroupUsername() {
	return _group_username;
}


bool TalkView::NewlinesAllowed() {
	return false;
}


void TalkView::GenerateHyperlinkText(const TextSpan& span,
	Paragraph& paragraph)
{
    const BString& message = span.Text();
    const auto links = Chat::FindLinks(message.String());
    if (links.empty()) { paragraph.Append(span); return; }

	const CharacterStyle& standard = span.Style();

	// Derive the underline style from the base style
	CharacterStyle linkStyle(standard);
	linkStyle.SetForegroundColor(B_LINK_TEXT_COLOR);
	linkStyle.SetUnderline(UNDERLINE_SINGLE);

	// Insert two runs for each link: one with link style, and one restoring the standard style at
	// the end of the link
	int32 offset = 0;
	BString substring;
	for (auto linkIterator: links) {

		// The text before the link
		message.CopyInto(substring, offset, linkIterator.start - offset);
		TextSpan plain(substring, standard);
        plain.SetQuote(span.IsQuote());
        paragraph.Append(plain);

		// The text inside the link
		message.CopyInto(substring, linkIterator.start, linkIterator.end - linkIterator.start);
		TextSpan linkSpan(substring, linkStyle);
        linkSpan.SetQuote(span.IsQuote());
		linkSpan.SetCursor(BCursor(B_CURSOR_ID_FOLLOW_LINK));
		BMessage* message = new BMessage('hlnk');
		message->AddString("target", linkIterator.target.c_str());
		linkSpan.SetClickMessage(message);
		paragraph.Append(linkSpan);

		offset = linkIterator.end;
	}

	// Text after the last link
	message.CopyInto(substring, offset, message.Length() - offset);
	TextSpan plain(substring, standard);
    plain.SetQuote(span.IsQuote());
    paragraph.Append(plain);
}


static int compareStrings(const char* a, const char* b)
{
	// FIXME use ICU locale aware comparison instead
	int icompare = strcasecmp(a, b);
	if (icompare != 0)
		return icompare;

	// In case the names are case-insensitive-equal, still sort them in a
	// predictible way
	return strcmp(a, b);
}


void
TalkView::AddGroupChatter(std::string occupant_jid, gloox::MUCRoomAffiliation affiliation,
	gloox::MUCRoomRole role, gloox::Presence::PresenceType presence, BString fullJID,
	BString status)
{
	// We receive something so we know we're connected, no need to send a ping
	if (_idle_timer) {
		// Cancel the current ping timeout (if any and re-trig the timer)
		fIsPinging = false;
		_idle_timer->SetInterval(kSelfPingDelay);
	}

	// Find the right place to add the user
	int exactIndex = -1;
	int insertPosition = 0;
	PeopleListItem* exactItem = nullptr;
	if (_people->CountItems() > 0) {
		Iterator i = std::lower_bound(Iterator(0), Iterator(_people->CountItems()), occupant_jid,
			[this, &exactIndex, &exactItem](const Iterator& i, const std::string& occupant_jid) {
				PeopleListItem *iterating_item = dynamic_cast<PeopleListItem *>(_people->ItemAt(i.m_ptr));
				int compare = compareStrings(iterating_item->OccupantJID().c_str(), occupant_jid.c_str());
				if (compare == 0) {
					exactIndex = i.m_ptr;
					exactItem = iterating_item;
				}
				return compare < 0;
			}
		);
		insertPosition = i.m_ptr;
	}

	if (exactIndex >= 0) {
		// Found an existing entry in the list, update it
		exactItem->Set(affiliation, role, presence, status);
		_people->InvalidateItem(exactIndex);
	} else {
		// create a new entry
		PeopleListItem *people_item = new PeopleListItem(occupant_jid, affiliation, role, presence,
			fullJID, status);

		_people->AddItem(people_item, insertPosition);
	}

	RefreshHeader();
	gloox::JID selfJid(_group_room);
	selfJid.setResource(_group_username);
	if (occupant_jid == selfJid.full()) {
		if (role == gloox::RoleVisitor) {
			if (fMessageInput->IsEditable()) {
				fMessageInput->MakeEditable(false);
				fAttach->SetEnabled(false);
				if (fUploadTask) fUploadTask->Cancel();
				fMessageInput->MakeSelectable(false);
				fMessageInput->SetText(B_TRANSLATE("Request permission to talk" B_UTF8_ELLIPSIS));
				fMessageInput->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
				fRaiseHand->Show();
			}
		} else {
			if (!fMessageInput->IsEditable()) {
				fMessageInput->MakeEditable(true);
				fAttach->SetEnabled(!fUploadTask);
				fMessageInput->MakeSelectable(true);
				fMessageInput->SetText("");
				fMessageInput->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
				fRaiseHand->Hide();
			}
		}
	}
}


void
TalkView::RemoveGroupChatter(std::string occupant_jid)
{
	// We receive something so we know we're connected, no need to send a ping
	if (_idle_timer) {
		// Cancel the current ping timeout (if any and re-trig the timer)
		fIsPinging = false;
		_idle_timer->SetInterval(kSelfPingDelay);
	}

	// remove user
	for (int i=0; i < _people->CountItems(); ++i) {
		if (dynamic_cast<PeopleListItem *>(_people->ItemAt(i))->OccupantJID() == occupant_jid)
			delete _people->RemoveItem(i);
	}
	RefreshHeader();
}


// FIXME connect this with "last message correction" if available
void
TalkView::RevealPreviousHistory() {
	// boundary
	if (_chat_index == 49 || _chat_index == ((int)_chat_history.size() - 1))
		return;

	if (_chat_index == -1)
		_chat_buffer = fMessageInput->Text();

	// go back
	++_chat_index;

	// update text
	fMessageInput->SetText(_chat_history[_chat_index].c_str());
}


void
TalkView::RevealNextHistory() {
	// boundary
	if (_chat_index == -1)
		return;

	// go forward
	--_chat_index;

	// last buffer
	if (_chat_index == -1) {
		fMessageInput->SetText(_chat_buffer.c_str());
	} else {
		// update text
		fMessageInput->SetText(_chat_history[_chat_index].c_str());
	}
}


bool
TalkView::IsGroupChat()
{
	return !_group_room.empty();
}


void
TalkView::SetStatus(std::string message)
{
	_status_view->SetToolTip(message.c_str());

	// Don't allow newlines in the status view, there's no space for it and it breaks the layout
	size_t pos;
	if((pos = message.find('\n')) != std::string::npos) {
		message.resize(pos);
	}
	_status_view->SetText(message.c_str());
    if(Window())Window()->PostMessage('tSta');
}


std::optional<const std::string>
TalkView::_GetNextNickMatchingPrefix(const BString& prefix, bool reverse)
{
	if (_people->CountItems() == 0) return std::nullopt;
	int32 direction = reverse ? -1 : 1;

	if (fLastCompletedNicknameIndex >= _people->CountItems())
		fLastCompletedNicknameIndex = 0;
	if (fLastCompletedNicknameIndex < 0)
		fLastCompletedNicknameIndex = _people->CountItems() - 1;

	int32 firstTry = fLastCompletedNicknameIndex;

	do {
		// Determine the nick index to use next
		fLastCompletedNicknameIndex += direction;

		// Make sure to loop over as needed
		if (fLastCompletedNicknameIndex >= _people->CountItems())
			fLastCompletedNicknameIndex = 0;
		if (fLastCompletedNicknameIndex < 0)
			fLastCompletedNicknameIndex = _people->CountItems() - 1;

		// Finally get and insert the appropriate nickname
		PeopleListItem *iterating_item = dynamic_cast<PeopleListItem *>(_people->ItemAt(fLastCompletedNicknameIndex));
		const std::string& name = iterating_item->Nickname();

		if (BString(name.c_str()).ICompare(prefix, prefix.Length()) == 0)
			return std::make_optional<const std::string>(name);
	} while (fLastCompletedNicknameIndex != firstTry);

	return std::nullopt;
}


void
TalkView::TabComplete(bool reverse)
{
	// Compute some useful variables depending on the cursor position and previous completions
	int32 start;
	fMessageInput->GetSelection(&start, NULL);
	int32 lastNickEndPos = fLastStartOffset + fLastCompletedNicknameLength;

	// If we're starting a new completion, determine the "prefix" to filter the nicknames with
	if (start != lastNickEndPos) {
		fLastCompletedNicknameIndex =  -1;

		// New completion, determine the prefix
		fLastStartOffset = start - 1;
		while ((fLastStartOffset >= 0) && !isspace((char)fMessageInput->ByteAt(fLastStartOffset)))
			fLastStartOffset--;

		fLastStartOffset++;
		fPrefix = BString(fMessageInput->Text() + fLastStartOffset, start - fLastStartOffset);
	}

	std::optional<const std::string> name = _GetNextNickMatchingPrefix(fPrefix, reverse);

	if (name) {
		fMessageInput->Delete(fLastStartOffset, start);
		start = fLastStartOffset;

		fMessageInput->Insert(name->c_str());
		if (start == 0 || fMessageInput->ByteAt(start - 1) == '\n') {
			fMessageInput->Insert(": ");
			fLastCompletedNicknameLength = name->length() + 2;
		} else {
			fMessageInput->Insert(" ");
			fLastCompletedNicknameLength = name->length() + 1;
		}
	} else {
		//beep();
	}
}


void
TalkView::TextStyling(int8 byte) {
			uint32 sameProperties;
			BFont font;
			fMessageInput->GetFontAndColor(&font, &sameProperties);

			uint16 face = 0;

			if (!(font.Face() & B_REGULAR_FACE))
				face = font.Face();

			switch (byte) {
				case 0x62: {
					face ^= B_BOLD_FACE;
					break;
				}
				case 0x69: {
					face ^= B_ITALIC_FACE;
					break;
				}
				case 0x75: {
					face ^= B_UNDERSCORE_FACE;
					break;
				}
			}

			int32 startSelection;
			int32 endSelection;

			// why Oh WHY do you return void and not wether or not a selection exists :'(
			fMessageInput->GetSelection(&startSelection, &endSelection);
			font.SetFace(face);
			if (startSelection == endSelection)
				fMessageInput->SetFontAndColor(&font, B_FONT_FACE);
			else
				fMessageInput->SetFontAndColor(startSelection, endSelection, &font, B_FONT_FACE);
}


CharacterStyle& TalkView::SystemMessagesStyle()
{
	return sSystemMessagesStyle;
}

CharacterStyle TalkView::sSystemMessagesStyle;


void TalkView::AddAttachment(const std::string& url)
{
	if (url.empty()) return;
	if (!fAttachments) {
		auto profile = Upload::Profile::Load();
		fAttachments = std::make_unique<AttachmentController>(BMessenger(this),
			*fTimeline, fTimelineDocument, profile.authority, profile.caFile);
	}
	fAttachments->Add(url);
}

void TalkView::RefreshHeader(const char* name)
{
    const auto jid = IsGroupChat() ? _group_room : _session->target().bare();
    std::string title = name ? name : fFriendlyName;
    if (title.empty()) {
        if (IsGroupChat()) {
            const auto* bookmark = BookmarkManager::Instance().GetBookmark(jid.c_str());
            if (bookmark) title = bookmark->name;
        } else {
            JRoster::Instance()->Lock();
            const auto* user = JRoster::Instance()->FindUser(gloox::JID(jid));
            if (user) title = user->FriendlyName();
            JRoster::Instance()->Unlock();
        }
    }
    fFriendlyName = Conversations::FriendlyName(jid, title, IsGroupChat());
    fHeaderName->SetText(fFriendlyName.c_str());
    if (auto* avatar = dynamic_cast<ConversationAvatar*>(FindView("conversation-avatar"))) avatar->SetInitial(fFriendlyName.c_str());
    BString detail;
    if (IsGroupChat()) {
        if (_people->CountItems() == 1) detail = B_TRANSLATE("1 participant");
        else if (_people->CountItems() > 1) detail.SetToFormat("%ld participants", long(_people->CountItems()));
        else detail = B_TRANSLATE("Participant information is not available yet");
    } else detail = B_TRANSLATE("Direct conversation");
    fHeaderDetail->SetText(detail);
    fParticipants->SetEnabled(IsGroupChat() && _people->CountItems() > 0);
    if (!IsGroupChat() || _people->CountItems() == 0) {
        fPeopleSplit->SetItemCollapsed(1, true);
        fPeopleSplit->SetSplitterSize(0);
    }
    if (!IsGroupChat() && !fParticipants->IsHidden()) fParticipants->Hide();
}

void TalkView::DeliveryStatus(const std::string& id, Chat::Delivery state)
{
    if (fHistory && !fHistory->ReadLabel(id).empty()) return;
    const auto* message = fDeliveries.Find(id);
    if (!message) {
        // Receipts may arrive after reopening a conversation. Correlate them
        // against persisted outgoing history too, never the last visible text.
        if (fHistory && state == Chat::Delivery::Delivered
            && fHistory->Record(id).GetBool("outgoing", false)) {
            fHistory->Delivery(id, static_cast<int32>(state));
            RefreshMessageFooter(id);
            return;
        }
        if (state == Chat::Delivery::Failed)
            SetStatus("Send error with no matching message ID; delivery is unknown.");
        return;
    }
    if (state != Chat::Delivery::Queued && !fDeliveries.Update(id, state)) return;
    if(fHistory)fHistory->Delivery(id,static_cast<int32>(state));
    RefreshMessageFooter(id);
    const bool media = Upload::IsServiceUrl(message->text, Upload::Profile::Load().authority);
    std::string text = std::string(Chat::Deliveries::Label(state)) + ": "
        + (media ? Upload::AttachmentFilename(message->text) : Conversations::Preview(message->text));
    SetStatus(text);
    // Keep actionable failures next to the affected message. Routine transport
    // and receipt updates replace the footer instead of flooding the timeline.
    if (state != Chat::Delivery::Failed) return;
    std::list<TextSpan> runs;
    if (state == Chat::Delivery::Failed) text += media ? " [Choose the file again to retry]" : " [Restore text to retry]";
    TextSpan span(text.c_str(), sSystemMessagesStyle);
    if (state == Chat::Delivery::Failed && !media) {
        BMessage retry('rTry');
        retry.AddString("id", id.c_str());
        span.SetClickMessage(new BMessage(retry));

    }
    runs.push_back(span);
    AddToTalk("", runs, OTHER);
}

void TalkView::SyncHistory() {
    if(!fHistory || fLoadingArchive || !JabberSpeak::Instance()->IsConnected())return;
    // First read reconciles existing local history too. Later reads overlap the
    // last successful query, never a newest-message timestamp that could skip holes.
    fArchiveStarted=real_time_clock_usecs();
    const int64 since=fArchiveSince>60000000?fArchiveSince-60000000:0;
    fLoadingArchive=JabberSpeak::Instance()->QueueArchive(IsGroupChat()?_group_room:_session->target().bare(),IsGroupChat(),BMessenger(this),since);
    if(fLoadingArchive && !fArchiveSince)SetStatus("Checking for missing server history…");
}

void TalkView::RestoreHistory(bool reload) {
    auto records=reload?fHistory->Load():fHistory->Records();fReplayingHistory=true;
    for(const auto& m:records) {
        std::list<TextSpan> runs;runs.emplace_back(m.GetString("body",""),CharacterStyle());
        AddToTalk(m.GetString("name",""),runs,m.GetBool("outgoing",false)?LOCAL:MAIN_RECIPIENT,true,
            m.GetInt64("time",0),true,m.GetString("id",""),m.GetString("attachment",""),m.GetString("sender",""),m.GetString("wire-id",""),m.GetBool("markable",false));

        if(fReactions)fReactions->Alias(m.GetString("id",""),m.GetString("alias",""));
    }
    fReplayingHistory=false;
    if(fReactions)fReactions->SetBatch(false);
    fTimeline->Relayout();fTimeline->Invalidate();
    auto* bar=fTimelineScroller->ScrollBar(B_VERTICAL);float low,high;bar->GetRange(&low,&high);bar->SetValue(high);
}

void TalkView::InsertForwardedDraft(const std::string& text) {
    if(!fMessageInput->IsEditable())return;
    if(!fEditId.empty()){SetStatus("Finish or cancel the edit before forwarding a message.");return;}
    const std::string draft=std::string(fMessageInput->Text())+(fMessageInput->TextLength()?"\n\n":"")+"Forwarded message:\n"+text;
    if(draft.size()>65536){SetStatus("Forward is too long for this draft. Nothing was changed.");return;}
    fMessageInput->SetText(draft.c_str());fMessageInput->Select(draft.size(),draft.size());fMessageInput->MakeFocus(true);
    SetStatus("Forward added to your draft. Review the recipient and text, then press Send.");
}

void TalkView::EndEdit() {
    fEditId.clear();fMessageInput->SetText(fEditSavedDraft.c_str());fEditSavedDraft.clear();
    if(auto* cancel=dynamic_cast<BButton*>(FindView("cancel-reply")))cancel->SetLabel("Cancel reply");
    if(!fReplyStrip->IsHidden())fReplyStrip->Hide();
    fMessageInput->MakeFocus(true);
}
void TalkView::RebuildHistory() {
    auto* bar=fTimelineScroller->ScrollBar(B_VERTICAL);float low,high;bar->GetRange(&low,&high);
    const float position=bar->Value();const bool bottom=position>=high-1;
    fLastMessageDay.clear();fTimelineDocument.SetTo(new TextDocument(),true);fTimeline->SetTextDocument(fTimelineDocument);
    if(fReactions)fReactions->Rebind(fTimelineDocument);
    if(fAttachments)fAttachments->Rebind(fTimelineDocument);
    RestoreHistory(false);if(!bottom)bar->SetValue(position);
    const auto records=fHistory->Records();
    if(!records.empty()) {
        const auto& last=records.back();BMessage preview('cAct');preview.AddBool("edit",true);
        preview.AddString("jid",IsGroupChat()?_group_room.c_str():_session->target().bare().c_str());
        preview.AddString("id",last.GetString("id",""));preview.AddString("preview",last.GetString("body",""));
        preview.AddUInt64("generation",JabberSpeak::Instance()->ConnectionGeneration());
        BlabberMainWindow::Instance()->PostMessage(&preview);
    }
}

void TalkView::UpdateReading()
{
    if (IsHidden() || !Window()) { fReadTracker.Observe({}, system_time()); return; }
    auto* bar = fTimelineScroller->ScrollBar(B_VERTICAL);
    float low = 0, high = 0;
    if (bar) bar->GetRange(&low, &high);
    const bool moreBelow = bar && bar->Value() < high - 1;
    if (moreBelow && fJumpToLatest->IsHidden()) fJumpToLatest->Show();
    if (!moreBelow && !fJumpToLatest->IsHidden()) fJumpToLatest->Hide();

    auto* connection = JabberSpeak::Instance();
    const auto generation = connection->ConnectionGeneration();
    if (generation != fReadGeneration) { fReadTracker = {}; fReadGeneration = generation; }
    std::vector<std::string> visible;
    if (Window()->IsActive() && !Window()->IsMinimized() && !fLoadingArchive
        && !fReplayingHistory && fHistory && connection->IsConnected()) {
        for (int32 i = 0; i < fTimelineDocument->CountParagraphs(); ++i) {
            const auto& style = fTimelineDocument->ParagraphAtIndex(i).Style();
            const auto id = style.ReactionId();
            if (style.MessageKind() != 1 || id.empty() || fReadTracker.SubmittedAlready(id)
                || !fTimeline->ParagraphReadVisible(i)) continue;
            auto record = fHistory->Record(id);
            if (!record.GetBool("outgoing",true) && record.GetBool("markable",false)) visible.push_back(id);
        }
    }
    for (const auto& id : fReadTracker.Observe(visible, system_time())) {
        if (!connection->QueueDisplayed(IsGroupChat() ? _group_room : _session->target().bare(), id, IsGroupChat())) break;
        fReadTracker.Submitted(id);
    }
}

std::string TalkView::MessageFooter(const std::string& id, int64 timestamp) const {
    auto footer = ChatDate::Clock(timestamp > 0 ? timestamp / 1000000 : 0);
    if (!fHistory || id.empty()) return footer;
    const auto record = fHistory->Record(id);
    const std::string edit = record.GetString("edit-state", "");
    if (!edit.empty()) footer += edit == "unconfirmed" ? " · edited, unconfirmed" : " · edited";
    const auto read = fHistory->ReadLabel(id);
    if (!read.empty() && !_group_room.empty()) footer += " · " + read;
    return footer;
}

void TalkView::ReceiveRead(const BMessage& message) {
    if (!fHistory) return;
    const std::string id = message.GetString("id", "");
    if (!fHistory->MarkRead(id, message.GetString("actor", ""))) return;
    RefreshMessageFooter(id);
    SetStatus(fHistory->ReadLabel(id));
}

ParagraphStyle::DeliveryMark TalkView::MessageDeliveryMark(const std::string& id) const {
    using Mark = ParagraphStyle::DeliveryMark;
    if (!fHistory) return Mark::None;
    const auto record = fHistory->Record(id);
    if (!record.GetBool("outgoing", false)) return Mark::None;
    if (!fHistory->ReadLabel(id).empty()) return Mark::Read;
    const auto* live = fDeliveries.Find(id);
    // An old archive record with no transport evidence must not claim delivery.
    const int state = live ? static_cast<int>(live->state) : record.GetInt32("delivery", -1);
    switch (state) {
        case static_cast<int>(Chat::Delivery::Queued): return Mark::Sending;
        case static_cast<int>(Chat::Delivery::Unconfirmed):
        case static_cast<int>(Chat::Delivery::Accepted): return Mark::Sent;
        case static_cast<int>(Chat::Delivery::Delivered): return Mark::Delivered;
        case static_cast<int>(Chat::Delivery::Failed): return Mark::Failed;
        default: return Mark::None;
    }
}

void TalkView::RefreshMessageFooter(const std::string& id) {
    if (!fHistory) return;
    const auto record = fHistory->Record(id);
    const std::string original = record.GetString("id", "");
    if (original.empty()) return;
    auto* bar = fTimelineScroller->ScrollBar(B_VERTICAL);
    float low = 0, high = 0;
    bar->GetRange(&low, &high);
    const bool bottom = bar->Value() >= high - 1;
    for (int32 i = 0; i < fTimelineDocument->CountParagraphs(); ++i) {
        auto paragraph = fTimelineDocument->ParagraphAtIndex(i);
        auto style = paragraph.Style();
        auto target = fHistory->Record(style.ReactionId());
        if (original != target.GetString("id", "")) continue;
        style.SetMessageTime(MessageFooter(original, record.GetInt64("time", 0)));
        style.SetDeliveryMark(MessageDeliveryMark(original));
        paragraph.SetStyle(style);
        fTimelineDocument->SetParagraph(i, paragraph);
    }
    fTimeline->Relayout();
    fTimeline->Invalidate();
    if (bottom) { bar->GetRange(&low, &high); bar->SetValue(high); }
}
