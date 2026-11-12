// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Messenger.h>
#include <deque>
#include <map>
#include <memory>
#include "support/ReactionState.h"
#include "support/ChatHistory.h"
#include "ui/textview/TextDocumentView.h"

// All access is on the owning chat looper. Network calls only enter a bounded queue.
class ReactionController {
public:
    ReactionController(BMessenger target,TextDocumentView& timeline,const TextDocumentRef& document,
        std::string recipient,bool group,ChatHistory* history = nullptr);
    void Add(const std::string& id,int32 paragraph);
    void Rebind(const TextDocumentRef& document);
    void SetBatch(bool batch) { fBatch = batch; }
    void Alias(const std::string& original,const std::string& stable);
    bool Handle(const BMessage& message);
private:
    struct Record { int32 paragraph; std::string id, latest; Chat::ReactionState state; };
    struct Pending { std::shared_ptr<Record> record; std::vector<std::string> previous; };
    void Render(Record& record);
    void Status(const char* text);
    BMessenger fTarget;
    TextDocumentView& fTimeline;
    TextDocumentRef fDocument;
    std::string fRecipient;
    bool fGroup;
    ChatHistory* fHistory;
    bool fBatch = false;
    std::map<std::string,std::shared_ptr<Record>> fMessages;
    std::deque<std::string> fOrder;
    std::map<std::string,Pending> fPending;
    std::deque<std::string> fPendingOrder;
    std::deque<BMessage> fEarly;
};
