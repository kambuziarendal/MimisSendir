// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "ArchiveManager.h"
#include "ArchiveTags.h"
#include "Reactions.h"
#include "UploadManager.h"
#include "HttpUpload.h"
#include "support/ConversationStore.h"
#include <gloox/tag.h>
#include <ctime>
namespace Chat {
namespace {
class Mam : public gloox::StanzaExtension {
public:
    static constexpr int ID=gloox::ExtUser+43;
    std::shared_ptr<gloox::Tag> payload;
    explicit Mam(const gloox::Tag* tag=nullptr):StanzaExtension(ID),payload(tag?tag->clone():nullptr){}
    const std::string& filterString() const override {static const std::string f="/message/result[@xmlns='urn:xmpp:mam:2']|/iq/fin[@xmlns='urn:xmpp:mam:2']";return f;}
    gloox::StanzaExtension* newInstance(const gloox::Tag* t) const override {return new Mam(t);}
    gloox::StanzaExtension* clone() const override{return new Mam(payload.get());}
    gloox::Tag* tag() const override{return payload?payload->clone():nullptr;}
};

}
ArchiveManager::ArchiveManager(gloox::Client& client):fClient(client){fClient.registerStanzaExtension(new Mam());}
bool ArchiveManager::Queue(const std::string& peer,bool group,const BMessenger& target,int64 since){
    std::lock_guard<std::mutex> lock(fLock);if(fStopped || fQueue.size()>=16 || peer.empty())return false;
    auto r=std::make_shared<Request>();r->peer=peer;r->group=group;r->target=target;r->since=since;fQueue.push_back(r);return true;
}
void ArchiveManager::Send(const std::shared_ptr<Request>& r){
    r->id=fClient.getID();r->deadline=system_time()+15000000;r->pages++;
    gloox::Tag query("query");query.setXmlns("urn:xmpp:mam:2");query.addAttribute("queryid",r->id);
    if(!r->group || r->since>0){auto* form=new gloox::Tag(&query,"x");form->setXmlns("jabber:x:data");form->addAttribute("type","submit");
        auto* type=new gloox::Tag(form,"field");type->addAttribute("var","FORM_TYPE");type->addAttribute("type","hidden");new gloox::Tag(type,"value","urn:xmpp:mam:2");
        if(!r->group){auto* peer=new gloox::Tag(form,"field");peer->addAttribute("var","with");new gloox::Tag(peer,"value",r->peer);}
        if(r->since>0){time_t seconds=r->since/1000000;struct tm utc{};gmtime_r(&seconds,&utc);char stamp[32];strftime(stamp,sizeof(stamp),"%Y-%m-%dT%H:%M:%SZ",&utc);
            auto* start=new gloox::Tag(form,"field");start->addAttribute("var","start");new gloox::Tag(start,"value",stamp);}}
    auto* set=new gloox::Tag(&query,"set");set->setXmlns("http://jabber.org/protocol/rsm");new gloox::Tag(set,"max","50");new gloox::Tag(set,"before",r->before);
    gloox::IQ iq(gloox::IQ::Set,r->group?gloox::JID(r->peer):gloox::JID(),r->id);iq.addExtension(new Mam(&query));
    fActive[r->id]=r;fClient.send(iq,this,43);
}
void ArchiveManager::Finish(std::shared_ptr<Request> r,const char* status){
    fActive.erase(r->id);r->result.AddString("status",status);r->target.SendMessage(&r->result,static_cast<BHandler*>(nullptr),0);
}
void ArchiveManager::Tick(){
    if(!fClient.authed())return;
    {std::lock_guard<std::mutex> lock(fLock);while(!fQueue.empty() && fActive.size()<4){auto r=fQueue.front();fQueue.pop_front();Send(r);}}
    std::vector<std::shared_ptr<Request>> expired;for(const auto& [id,r]:fActive){(void)id;if(system_time()>r->deadline)expired.push_back(r);}
    for(auto& r:expired)Finish(r,"Archive request timed out. Local history is preserved.");
}
void ArchiveManager::Disconnected(){
    std::lock_guard<std::mutex> lock(fLock);fStopped=true;
    while(!fActive.empty())Finish(fActive.begin()->second,"Disconnected. Local history is preserved.");
    for(auto& r:fQueue){r->result.AddString("status","Disconnected. Local history is preserved.");r->target.SendMessage(&r->result,static_cast<BHandler*>(nullptr),0);}fQueue.clear();
}
bool ArchiveManager::Handle(const gloox::Message& message){
    auto* ext=message.findExtension<Mam>(Mam::ID);if(!ext)return false;

    auto* result=ext->payload.get();if(!result)return true;
    auto it=fActive.find(result->findAttribute("queryid"));if(it==fActive.end())return true;auto r=it->second;
    auto own=fClient.jid().bare();
    if(!ArchiveSender(message.from().bare(),own,r->peer,r->group))return true;
    auto* forward=ArchiveChild(result,"forwarded","urn:xmpp:forward:0");auto* msg=ArchiveChild(forward,"message","jabber:client");auto* delay=ArchiveChild(forward,"delay","urn:xmpp:delay");
    if(!msg || !delay || r->count>=1000 || r->result.FlattenedSize()>4*1024*1024)return true;
    gloox::JID from(msg->findAttribute("from")),to(msg->findAttribute("to"));bool sent=from.bare()==own;
    if(!ArchivePeer(from.bare(),to.bare(),own,r->peer,r->group))return true;
    BMessage item;auto id=msg->findAttribute("id");
    if(r->group){auto* sid=ArchiveChild(msg,"stanza-id","urn:xmpp:sid:0");if(sid && sid->findAttribute("by")==r->peer)id=sid->findAttribute("id");}
    if(id.empty())id="archive-"+result->findAttribute("id");
    item.AddString("id",id.c_str());item.AddInt64("time",Conversations::ArchiveTime(delay->findAttribute("stamp")));
    item.AddString("sender",from.full().c_str());item.AddBool("outgoing",sent);item.AddString("name",r->group?from.resource().c_str():sent?"You":"Contact");
    if(auto* reaction=ArchiveChild(msg,"reactions","urn:xmpp:reactions:0")){
        Reactions parsed(reaction);if(parsed.id().empty())return true;
        item.what='rInc';item.AddBool("archived",true);item.RemoveName("id");item.AddString("id",parsed.id().c_str());item.AddString("actor",sent?"self":r->group?from.full().c_str():from.bare().c_str());
        for(const auto& e:parsed.values())item.AddString("emoji",e.c_str());
    } else {
        item.what='hRec';auto* body=ArchiveChild(msg,"body","jabber:client");std::string text=body?body->cdata():"";
        if(text.size()>65536)return true;
        auto* oob=ArchiveChild(msg,"x","jabber:x:oob");auto* url=ArchiveChild(oob,"url","jabber:x:oob");auto attachment=url?url->cdata():text;
        if(Upload::IsServiceUrl(attachment,Upload::Profile::Load().authority))item.AddString("attachment",attachment.c_str());
        if(text.empty() && !item.HasString("attachment"))return true;
        item.AddString("body",text.c_str());
    }
    r->count++;r->result.AddMessage("record",&item);return true;
}
void ArchiveManager::handleIqID(const gloox::IQ& iq,int){
    auto it=fActive.find(iq.id());if(it==fActive.end())return;auto r=it->second;
    auto expected=r->group?r->peer:fClient.jid().bare();if(!iq.from().bare().empty() && iq.from().bare()!=expected)return;
    auto* ext=iq.findExtension<Mam>(Mam::ID);auto* fin=ext?ext->payload.get():nullptr;
    if(iq.subtype()==gloox::IQ::Error || !fin){Finish(r,"Server archive unavailable. Local history is preserved.");return;}
    auto* set=ArchiveChild(fin,"set","http://jabber.org/protocol/rsm");auto* first=ArchiveChild(set,"first","http://jabber.org/protocol/rsm");
    bool done=fin->findAttribute("complete")=="true" || fin->findAttribute("complete")=="1" || !first || first->cdata().empty() || first->cdata()==r->before;
    if(!done && r->pages<20 && r->count<1000 && r->result.FlattenedSize()<4*1024*1024){fActive.erase(it);r->before=first->cdata();Send(r);return;}
    r->result.AddBool("complete",done);
    Finish(r,done?"Available server history restored. New messages are saved locally.":"Recent archive restored (up to 1000 messages). New messages are saved locally.");
}
}
