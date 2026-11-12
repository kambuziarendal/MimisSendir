// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "ReactionController.h"
#include "ColorEmoji.h"
#include "ReactionSpans.h"
#include "jabber/JabberSpeak.h"
#include "ui/TalkView.h"
#include <PopUpMenu.h>
#include <MenuItem.h>
#include <ScrollBar.h>
#include <algorithm>

namespace {
class EmojiMenuItem : public BMenuItem {
public:
    EmojiMenuItem(const char* label,BMessage* action,const std::string& emoji)
        : BMenuItem(label,action),fImage(ColorEmoji(emoji)) {}
    void GetContentSize(float* width,float* height) override { BMenuItem::GetContentSize(width,height);*width+=26;*height=std::max(*height,22.0f); }
    void DrawContent() override {
        BPoint origin=ContentLocation();
        if(fImage){Menu()->PushState();Menu()->SetDrawingMode(B_OP_ALPHA);Menu()->DrawBitmap(fImage.get(),BPoint(origin.x,Frame().top+2));Menu()->PopState();}
        font_height metrics;Menu()->GetFontHeight(&metrics);
        const float baseline=Frame().top+(Frame().Height()-metrics.ascent-metrics.descent)/2+metrics.ascent;
        Menu()->MovePenTo(BPoint(origin.x+26,baseline));Menu()->DrawString(Label());
    }
private:std::shared_ptr<BBitmap> fImage;
};
}
ReactionController::ReactionController(BMessenger target,TextDocumentView& timeline,
    const TextDocumentRef& document,std::string recipient,bool group,ChatHistory* history)
    : fTarget(target),fTimeline(timeline),fDocument(document),fRecipient(std::move(recipient)),fGroup(group),fHistory(history) {}

void ReactionController::Status(const char* text) {
    BMessage message('aSta');message.AddString("text",text);fTarget.SendMessage(&message,static_cast<BHandler*>(nullptr),0);
}
void ReactionController::Rebind(const TextDocumentRef& document) {
    fDocument=document;
    for(auto& [id,record]:fMessages){(void)id;record->paragraph=-1;}
    for(auto& [id,pending]:fPending){(void)id;pending.record->paragraph=-1;}
}
void ReactionController::Add(const std::string& id,int32 paragraph) {
    if(id.empty() || id.size()>1024)return;
    if(auto it=fMessages.find(id);it!=fMessages.end()) {
        if(it->second->paragraph<0){it->second->paragraph=paragraph;Render(*it->second);}
        return;
    }
    if(fOrder.size()>=512) {
        auto it=fMessages.find(fOrder.front());
        if(it!=fMessages.end()) {
            auto record=it->second;
            for(auto a=fMessages.begin();a!=fMessages.end();) {
                if(a->second==record)a=fMessages.erase(a);else ++a;
            }
        }
        fOrder.pop_front();
    }
    std::shared_ptr<Record> record;
    for(auto& [request,pending]:fPending){(void)request;if(pending.record->id==id){record=pending.record;break;}}
    if(!record){record=std::make_shared<Record>();record->id=id;if(fHistory)record->state=fHistory->Reactions(id);}
    record->paragraph=paragraph;
    fMessages[id]=record;fOrder.push_back(id);Render(*record);
    auto early=std::move(fEarly);fEarly.clear();
    for(const auto& message:early)Handle(message);
}
void ReactionController::Alias(const std::string& original,const std::string& stable) {
    auto it=fMessages.find(original);
    if(it==fMessages.end() || stable.empty() || stable.size()>1024 || stable==original)return;
    if(fMessages.count(stable) && fMessages[stable]!=it->second)return;
    // Only the current and original IDs need aliases.
    if(it->second->id!=original)fMessages.erase(it->second->id);
    if(fHistory)fHistory->Alias(original,stable);
    it->second->id=stable;fMessages[stable]=it->second;Render(*it->second);
    auto early=std::move(fEarly);fEarly.clear();for(const auto& message:early)Handle(message);
}
void ReactionController::Render(Record& record) {
    if(record.paragraph<0 || record.paragraph>=fDocument->CountParagraphs())return;
    auto paragraph=WithoutReactions(fDocument->ParagraphAtIndex(record.paragraph));
    std::string ownEmoji;
    auto own=record.state.actors.find("self");
    if(own!=record.state.actors.end()&&!own->second.values.empty())ownEmoji=*own->second.values.begin();
    auto style=paragraph.Style();style.SetReactionId(record.id);
    style.SetOwnReaction(ownEmoji,ColorEmoji(ownEmoji));
    std::vector<ParagraphStyle::ReactionBadge> badges;
    for(const auto& [emoji,count]:record.state.Counts()) {
        // The footer shows at most three distinct icons and an overflow count.
        // Keep all totals for resizing, but decode only the visible candidates.
        badges.push_back({emoji,count,badges.size()<3?ColorEmoji(emoji):nullptr});
    }
    style.SetReactionBadges(std::move(badges));paragraph.SetStyle(style);
    auto* bar=fTimeline.ScrollBar(B_VERTICAL);float low=0,high=0;if(bar)bar->GetRange(&low,&high);
    bool bottom=bar && bar->Value()>=high-1;
    fDocument->SetParagraph(record.paragraph,paragraph);
    if(fBatch)return;
    fTimeline.Relayout();fTimeline.Invalidate();
    if(bottom){bar->GetRange(&low,&high);bar->SetValue(high);}
}
bool ReactionController::Handle(const BMessage& m) {
    if(m.what==kMessageSubmitted || m.what==kMessageDeliveryError) {
        auto it=fPending.find(m.GetString("id",""));if(it==fPending.end())return false;
        if(m.what==kMessageSubmitted){if(fHistory)fHistory->Reactions(it->second.record->id,it->second.record->state);Status("Reaction sent to XMPP; delivery unconfirmed.");}
        else {
            if(it->second.record->latest==it->first) {
                it->second.record->state.Replace("self","You",it->second.previous);if(fHistory)fHistory->Reactions(it->second.record->id,it->second.record->state);Render(*it->second.record);
                it->second.record->latest.clear();
            }
            Status("Reaction could not be sent. Choose it again to retry.");fPending.erase(it);
        }
        return true;
    }
    if(m.what!='rInc' && m.what!='rMnu' && m.what!='rSet')return false;
    std::string id=m.GetString("id","");auto it=fMessages.find(id);
    if(it==fMessages.end()) {
        if(m.what=='rInc' && !id.empty() && id.size()<=1024) {
            if(fEarly.size()>=64)fEarly.pop_front();
            fEarly.push_back(m);
        }
        return true;
    }
    auto record=it->second;
    if(m.what=='rInc') {
        std::vector<std::string> values;const char* value=nullptr;
        for(int32 i=0;m.FindString("emoji",i,&value)==B_OK && i<17;++i)values.emplace_back(value);
        const std::string actor=m.GetString("actor","");
        // Archive replay must not replace an unconfirmed local reaction whose
        // correlated error may still arrive after the timeline is rebuilt.
        if(m.GetBool("archived",false) && actor=="self" && !record->latest.empty())return true;
        if(record->state.Replace(actor,m.GetString("name","Contact"),values)) {
            if(actor=="self")record->latest.clear();
            if(fHistory)fHistory->Reactions(record->id,record->state);
            Render(*record);
        }
    } else if(m.what=='rMnu') {
        auto* menu=new BPopUpMenu("Reactions",false,false);
        for(const auto& [actor,info]:record->state.actors) {
            (void)actor;std::string label=info.name+": ";for(const auto& emoji:info.values)label+=emoji+" ";
            auto* entry=new BMenuItem(label.c_str(),nullptr);entry->SetEnabled(false);menu->AddItem(entry);
        }
        if(!record->state.actors.empty())menu->AddSeparatorItem();
        const std::pair<const char*,const char*> options[]={{"👍","Like"},{"❤️","Love"},{"😂","Laugh"},{"😮","Surprised"},{"😢","Sad"},{"🙏","Thanks"},{"🔥","Fire"},{"🎉","Celebrate"},{"👎","Dislike"},{"✅","Done"},{"","Remove my reaction"}};
        for(const auto& [emoji,label]:options) {
            auto* action=new BMessage('rSet');action->AddString("id",record->id.c_str());action->AddString("emoji",emoji);
            auto* entry=new EmojiMenuItem(label,action,emoji);entry->SetTarget(fTarget);
            auto own=record->state.actors.find("self");entry->SetMarked(own!=record->state.actors.end() && own->second.values.count(emoji));menu->AddItem(entry);
        }
        auto* more=new BMessage('rMor');more->AddString("id",record->id.c_str());
        auto* moreItem=new BMenuItem("More emojis…",more);moreItem->SetTarget(fTarget);menu->AddItem(moreItem);
        BPoint point;uint32 buttons;fTimeline.GetMouse(&point,&buttons);fTimeline.ConvertToScreen(&point);
        menu->SetAsyncAutoDestruct(true);menu->Go(point,true,true,true);
    } else {
        const std::string emoji=m.GetString("emoji","");
        auto request=JabberSpeak::Instance()->QueueReaction(fRecipient,record->id,emoji,fGroup,fTarget);
        if(request.empty()){Status("Not connected or Signal not ready. Reaction was not sent.");return true;}
        std::vector<std::string> previous;
        if(auto own=record->state.actors.find("self");own!=record->state.actors.end())previous.assign(own->second.values.begin(),own->second.values.end());
        if(fPendingOrder.size()>=128){fPending.erase(fPendingOrder.front());fPendingOrder.pop_front();}
        fPendingOrder.push_back(request);fPending[request]={record,previous};record->latest=request;
        record->state.Replace("self","You",emoji.empty()?std::vector<std::string>{}:std::vector<std::string>{emoji});
        Render(*record);Status("Sending reaction…");
    }
    return true;
}
