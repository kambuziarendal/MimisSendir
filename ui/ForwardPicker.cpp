// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "ForwardPicker.h"
#include "support/ForwardTargets.h"
#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>

namespace {
struct RecipientItem : BStringItem {
    Conversations::Summary recipient;
    explicit RecipientItem(const Conversations::Summary& value)
        : BStringItem((Conversations::FriendlyName(value.jid, value.name, value.group)
            + (value.group ? " (group)" : "")).c_str()), recipient(value) {}
};
void Clear(BListView* list) {
    while (auto* item = list->RemoveItem(int32(0))) delete item;
}
}
ForwardPicker::ForwardPicker(BMessenger target, BMessage request,
    std::vector<Conversations::Summary> targets, BPoint position)
    : BWindow(BRect(position.x, position.y, position.x + 420, position.y + 450),
        "Forward message", B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
      fTarget(target), fRequest(request), fTargets(std::move(targets))
{
    fSearch = new BTextControl("recipient-search", "Find:", "", new BMessage('fQry'));
    fSearch->SetModificationMessage(new BMessage('fQry'));
    fList = new BListView("forward-recipients", B_SINGLE_SELECTION_LIST);
    fList->SetSelectionMessage(new BMessage('fRow'));
    fList->SetInvocationMessage(new BMessage('fUse'));
    auto* scroll = new BScrollView("recipient-scroll", fList, 0, false, true);
    scroll->SetExplicitMinSize(BSize(300, 180));
    fOther = new BButton("other-contacts", "Other contacts…", new BMessage('fAll'));
    fOther->SetExplicitMinSize(BSize(fOther->StringWidth("Favorites and recent") + 24, B_SIZE_UNSET));
    fChoose = new BButton("choose-recipient", "Use recipient", new BMessage('fUse'));
    fStatus = new BStringView("recipient-status", "Choose a conversation. Nothing is sent yet.");
    BLayoutBuilder::Group<>(this, B_VERTICAL, 8).SetInsets(12)
        .Add(fSearch).Add(scroll).Add(fStatus)
        .AddGroup(B_HORIZONTAL, 8).Add(fOther).AddGlue()
        .Add(new BButton("Cancel", new BMessage(B_QUIT_REQUESTED))).Add(fChoose).End();
    AddShortcut(B_ESCAPE, B_NO_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
    AddShortcut('W', B_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
    SetDefaultButton(fChoose);
    Refresh();
}
ForwardPicker::~ForwardPicker() { Clear(fList); }
void ForwardPicker::Refresh()
{
    Clear(fList);
    auto targets = Conversations::ForwardChoices(fTargets, fAll, fSearch->Text());
    int section = -1;
    for (const auto& target : targets) {
        const int next = fAll ? 2 : target.favorite ? 0 : 1;
        if (section != next) {
            auto* title = new BStringItem(next == 0 ? "Favorites" : next == 1 ? "Recent conversations" : "Contacts and groups");
            title->SetEnabled(false); fList->AddItem(title); section = next;
        }
        fList->AddItem(new RecipientItem(target));
    }
    fChoose->SetEnabled(false);
    fOther->SetLabel(fAll ? "Favorites and recent" : "Other contacts…");
    fStatus->SetText(targets.empty() ? "No matching conversations." : "Choose a conversation. Nothing is sent yet.");
}
void ForwardPicker::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case 'fQry': Refresh(); break;
        case 'fAll': fAll = !fAll; fSearch->SetText(""); Refresh(); fSearch->MakeFocus(); break;
        case 'fRow': fChoose->SetEnabled(dynamic_cast<RecipientItem*>(fList->ItemAt(fList->CurrentSelection())) != nullptr); break;
        case 'fUse': {
            auto* item = dynamic_cast<RecipientItem*>(fList->ItemAt(fList->CurrentSelection()));
            if (!item) break;
            BMessage chosen(fRequest); chosen.what = 'fDst';
            chosen.RemoveName("jid"); chosen.AddString("jid", item->recipient.jid.c_str());
            chosen.RemoveName("group"); chosen.AddBool("group", item->recipient.group);
            fTarget.SendMessage(&chosen);
            PostMessage(B_QUIT_REQUESTED); break;
        }
        default: BWindow::MessageReceived(message); break;
    }
}
