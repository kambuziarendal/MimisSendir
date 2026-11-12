//////////////////////////////////////////////////
// Blabber [TalkWindow.h]
//     A session with another user.
//////////////////////////////////////////////////

#pragma once

#include <gloox/eventhandler.h>
#include <gloox/message.h>

#include "network/UploadManager.h"
#include "support/MessageDelivery.h"
#include "support/ReadTracker.h"
#include "support/ChatHistory.h"
#include <StatusBar.h>
#include <SplitView.h>

#include <cstdio>
#include <deque>
#include <optional>
#include <string>

#include <interface/Box.h>
#include <interface/Button.h>
#include <interface/CheckBox.h>
#include <GroupView.h>
#include <interface/ListView.h>
#include <interface/MenuBar.h>
#include <interface/MenuItem.h>
#include <interface/ScrollView.h>
#include <interface/StringView.h>
#include <String.h>
#include <interface/TextView.h>
#include <interface/Window.h>
#include <storage/FilePanel.h>



#include "jabber/UserID.h"

#include "ui/EditingFilter.h"
#include "ui/MainWindow.h"
#include "ui/StatusView.h"
#include "ui/textview/TextDocumentView.h"

class AttachmentController;
class ReactionController;

enum {
	kIncomingMessage = 'imsg',
	kMessageDeliveryError = 'merr',
    kMessageSubmitted = 'msub',
    kMessageReceipt = 'mrcp',
    kOwnGroupEcho = 'mech'
};


struct Hyperlink
{
	Hyperlink(int32 s, int32 e, BString t)
		: start(s), end(e), target(t) {}
	Hyperlink(int32 s, int32 e, std::string t)
		: start(s), end(e), target(t.c_str()) {}
	int32 start;
	int32 end;
	BString target;
};


/** Compute XEP-0372 "consistent color", used for nicknames in the chat list */
rgb_color ConsistentColor(const std::string& username);


class TalkView : public BGroupView, public gloox::EventHandler {
public:
	enum                 user_type {MAIN_RECIPIENT, LOCAL, OTHER};

public:
						TalkView(const gloox::JID *user, std::string group_room,
							std::string group_username,
							gloox::MessageSession* session);
						~TalkView();

	void				AttachedToWindow() override;
	void InsertForwardedDraft(const std::string& text);
	void AddAttachment(const std::string& url);
	void                 FrameResized(float width, float height) override;
	void                 MessageReceived(BMessage *msg) override;

	std::string          OurRepresentation();

	/** Add something to the timeline.
	 *
	 * Prepend the username and timestamp as needed (depending on the user_type) with the given
	 * formatting. Do the sepacial formatting for /me commands. Also parse hyperlinks if requested.
	 */
	bool                 AddToTalk(std::string username, std::list<TextSpan>& spans,
							TalkView::user_type type, bool parseLinks = false, int64 timestamp = 0,
		bool archive = false, const std::string& messageId = "", const std::string& attachment = "", const std::string& senderJid = "", const std::string& wireId = "", bool markable = false);

	void                 ShowError(const gloox::StanzaError& error, bool deliveryFailure = false);

	bool                 NewlinesAllowed();

	const gloox::JID&    GetUserID();
	const std::string&   GetGroupRoom();
	const std::string&   GetGroupUsername();

	/* Insert span into paragraph with standard style.
	 * Detect any hyperlinks in the message, style them with the same style, but underlined and
	 * using the hyperlink color.
	 */
	void                 GenerateHyperlinkText(const TextSpan& span,
							Paragraph& paragraph);

	void                 AddGroupChatter(std::string username, gloox::MUCRoomAffiliation,
							gloox::MUCRoomRole, gloox::Presence::PresenceType, BString fullJID,
							BString status);
	void                 RemoveGroupChatter(std::string username);

	void                 RevealPreviousHistory();
	void                 RevealNextHistory();

	void                 TextStyling(int8 byte);

	bool				IsGroupChat();

	void				SetStatus(std::string message);
	void RefreshHeader(const char* name = nullptr);
    StatusView* ConversationStatus() const { return _status_view; }
    StatusView* LagStatus() const { return fLagMeter; }

	/** Try to complete the text in fMessageInput from names of the MUC participants matching the
	 * prefix */
	void				TabComplete(bool reverse);


	static CharacterStyle& SystemMessagesStyle();
private:
	// gloox EventHandler
	void handleEvent(const gloox::Event&) override;

	std::optional<const std::string> _GetNextNickMatchingPrefix(const BString& prefix, bool reverse);

private:
    BString				originalWindowTitle;

    Chat::Deliveries fDeliveries;
    void DeliveryStatus(const std::string& id, Chat::Delivery state);
	std::string            _group_room;
	std::string            _group_username;
	UserID::online_status  _current_status;
	gloox::MessageSession* _session;

	// GUI
	StatusView*			_status_view;
	BStringView* fHeaderName;
	BStringView* fHeaderDetail;
	BButton* fParticipants;
	BSplitView* fPeopleSplit;
	std::string fFriendlyName;

	BScrollView*		fTimelineScroller;
	TextDocumentView*	fTimeline;
	TextDocumentRef		fTimelineDocument;
    std::string fLastMessageDay;
    BMessageRunner* fDatePoll=nullptr;
    BMessageRunner* fReadPoll=nullptr;
    BButton* fJumpToLatest=nullptr;
    Chat::ReadTracker fReadTracker;
    uint64 fReadGeneration=0;
    void UpdateReading();
    std::string MessageFooter(const std::string& id, int64 timestamp) const;
    void ReceiveRead(const BMessage& message);
    void RefreshMessageFooter(const std::string& id);
    ParagraphStyle::DeliveryMark MessageDeliveryMark(const std::string& id) const;

	BScrollView*		fMessageInputScroller;
	BTextView*			fMessageInput;
	BButton*			fRaiseHand;
	std::unique_ptr<AttachmentController> fAttachments;
    std::unique_ptr<ReactionController> fReactions;
    std::unique_ptr<ChatHistory> fHistory;
    bool fReplayingHistory = false;
    bool fLoadingArchive = false;
    std::deque<BMessage> fDeferredReactions;
    void RestoreHistory(bool reload=true);
    void RebuildHistory();
    void EndEdit();
    std::string fEditId, fEditSavedDraft;
    void SyncHistory();
    int64 fArchiveSince=0;
    int64 fArchiveStarted=0;
	BButton* fAttach;
    BMessenger fMediaPicker;
    BMessenger fReactionPicker;
	BButton* fCancelUpload;
	BStatusBar* fUploadProgress;
	BGroupView* fUploadStrip;
    BGroupView* fEmojiStrip;
    BGroupView* fReplyStrip;
    BStringView* fReplyLabel;
    std::string fReplyId, fReplyTo, fReplyBody;
	std::string fUploadFilename;
	BFilePanel* fAttachmentPanel = nullptr;
	BMessageRunner* fUploadPoll = nullptr;
	std::shared_ptr<Upload::Task> fUploadTask;

	BListView*			_people;

	BScrollView*		_scrolled_people_pane;

	// user history
	std::deque<std::string>  _chat_history;
	std::string              _chat_buffer;
	int                 _chat_index;

	BMessageRunner     *_idle_timer;
	bigtime_t          fPingSendTime;
	bool               fIsPinging;

	StatusView*        fLagMeter;



	// nickname tab completion
	/// The initial "prefix" to filter the list of nicknames
	BString            fPrefix;

	/// The list of nicknames to pick from (it is all the _people right now)
	/// The index in the list of the last or next inserted item
	int32              fLastCompletedNicknameIndex = -1;

	/// The offset at which the word being completed starts
	int32              fLastStartOffset = 0;
	int32              fLastCompletedNicknameLength = 0;

	static CharacterStyle     sSystemMessagesStyle;
};
