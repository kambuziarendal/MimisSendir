// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Message.h>
#include <Messenger.h>
#include <Window.h>
#include "support/ConversationStore.h"
class BListView; class BButton; class BTextControl; class BStringView;

class ForwardPicker final : public BWindow {
public:
    ForwardPicker(BMessenger target, BMessage request,
        std::vector<Conversations::Summary> targets, BPoint position);
    ~ForwardPicker() override;
    void MessageReceived(BMessage*) override;
private:
    void Refresh();
    BMessenger fTarget;
    BMessage fRequest;
    std::vector<Conversations::Summary> fTargets;
    BListView* fList;
    BTextControl* fSearch;
    BButton *fOther, *fChoose;
    BStringView* fStatus;
    bool fAll = false;
};
