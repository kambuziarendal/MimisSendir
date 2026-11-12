#include "network/ArchiveTags.h"
#include "network/Reactions.h"
#include "network/Reply.h"
#include "support/ReplyDraft.h"
#include "network/MessagePresentation.h"
#include "support/ReactionState.h"
#include <iostream>
#include <gloox/client.h>
#include <gloox/messagesession.h>
#include <gloox/messagesessionhandler.h>
#include <gloox/messagehandler.h>
#include <memory>
#include <stdexcept>
struct WireProbe : gloox::MessageSessionHandler, gloox::MessageHandler {
    int contents=0,errors=0,reactions=0,corrections=0;
    void handleMessageSession(gloox::MessageSession* session) override { session->registerMessageHandler(this); }
    void handleMessage(const gloox::Message& m,gloox::MessageSession*) override {
        auto kind=Chat::Classify(m);
        if(kind==Chat::MessageKind::Content)++contents;
        if(kind==Chat::MessageKind::DeliveryError)++errors;
        if(kind==Chat::MessageKind::Reaction)++reactions;
        if(kind==Chat::MessageKind::Correction)++corrections;
    }
};
int main() {
    int checks=0;auto check=[&](bool ok,const char* label){if(!ok)throw std::runtime_error(label);++checks;};
    gloox::Message correction(gloox::Message::Chat,gloox::JID("bob@fixture.test"),"Corrected æ 🌞");
    correction.setID("edit-2");correction.addExtension(new Chat::Correction("original<&"));
    std::unique_ptr<gloox::Tag> correctedWire(correction.tag());
    auto* replace=correctedWire->findChild("replace");
    check(replace && replace->xmlns()=="urn:xmpp:message-correct:0" && replace->findAttribute("id")=="original<&","correction targets exact original ID");
    check(correctedWire->findChild("body")->cdata()=="Corrected æ 🌞" && correctedWire->findAttribute("id")=="edit-2","correction body and new stanza ID survive serialization");
    check(Chat::Classify(correction)==Chat::MessageKind::Correction,"correction never appended as ordinary content");
    gloox::Message deniedEdit(gloox::Message::Error,gloox::JID("bob@fixture.test"),"Corrected æ 🌞");deniedEdit.addExtension(new Chat::Correction("original<&"));
    check(Chat::Classify(deniedEdit)==Chat::MessageKind::DeliveryError,"echoed correction error is not content or edit");
    const auto prefix=Chat::ReplyPrefix("Original 🌞\nwith link https://example.test/?a=1&b=2");
    gloox::Message reply(gloox::Message::Chat,gloox::JID("bob@fixture.test"),prefix+"My reply");
    reply.addExtension(new Chat::Reply("original<&", "bob@fixture.test/phone"));
    reply.addExtension(new Chat::ReplyFallback(Chat::Codepoints(prefix)));
    std::unique_ptr<gloox::Tag> replyWire(reply.tag());
    auto* replyTag=replyWire->findChild("reply");auto* fallback=replyWire->findChild("fallback");
    check(replyTag && replyTag->xmlns()=="urn:xmpp:reply:0" && replyTag->findAttribute("id")=="original<&", "reply references exact original ID");
    check(replyTag->findAttribute("to")=="bob@fixture.test/phone", "reply retains sender identity");
    check(fallback && fallback->findChild("body")->findAttribute("end")==std::to_string(Chat::Codepoints(prefix)), "fallback Unicode end range");
    check(replyWire->findChild("body")->cdata()==prefix+"My reply", "wire body retains original fallback and authored reply");
    Chat::Reactions send("m<&42",{"👍","❤️"});std::unique_ptr<gloox::Tag> tag(send.tag());
    Chat::Reactions parsed(tag.get());check(parsed.id()=="m<&42" && parsed.values().size()==2,"wire roundtrip");
    check(tag->xml().find("m&lt;&amp;42")!=std::string::npos,"id escaped");
    gloox::Tag archived("message");archived.setXmlns("jabber:client");new gloox::Tag(&archived,"body","Saved text");
    std::unique_ptr<gloox::Tag> cloned(archived.clone());
    check(Chat::ArchiveChild(cloned.get(),"body","jabber:client")!=nullptr,"cloned archive body inherits namespace");
    gloox::Tag foreignBody("message");foreignBody.setXmlns("jabber:client");auto* badBody=new gloox::Tag(&foreignBody,"body","foreign");badBody->setXmlns("urn:wrong");
    check(Chat::ArchiveChild(&foreignBody,"body","jabber:client")==nullptr,"foreign archive child namespace rejected");
    check(Chat::ArchiveSender("alice@fixture.test","alice@fixture.test","bob@fixture.test",false),"own archive accepted");
    check(!Chat::ArchiveSender("bob@fixture.test","alice@fixture.test","bob@fixture.test",false),"peer cannot inject archive envelopes");
    check(!Chat::ArchiveSender("","alice@fixture.test","room@fixture.test",true),"group archive requires room sender");
    check(Chat::ArchivePeer("bob@fixture.test","alice@fixture.test","alice@fixture.test","bob@fixture.test",false),"incoming archive peer validated");
    check(Chat::ArchivePeer("alice@fixture.test","bob@fixture.test","alice@fixture.test","bob@fixture.test",false),"outgoing archive peer validated");
    check(!Chat::ArchivePeer("eve@fixture.test","alice@fixture.test","alice@fixture.test","bob@fixture.test",false),"unrelated archived peer rejected");
    Chat::Reactions remove("m1",{});std::unique_ptr<gloox::Tag> empty(remove.tag());
    check(Chat::Reactions(empty.get()).id()=="m1" && Chat::Reactions(empty.get()).values().empty(),"empty set removal");
    gloox::Tag foreign("reactions");foreign.setXmlns("urn:evil");foreign.addAttribute("id","m1");
    check(Chat::Reactions(&foreign).id().empty(),"foreign namespace");
    gloox::Tag huge("reactions");huge.setXmlns("urn:xmpp:reactions:0");huge.addAttribute("id","m1");new gloox::Tag(&huge,"reaction",std::string(65,'x'));
    check(Chat::Reactions(&huge).id().empty(),"oversized emoji rejected, not removal");
    gloox::Tag many("reactions");many.setXmlns("urn:xmpp:reactions:0");many.addAttribute("id","m1");
    for(int i=0;i<17;++i)new gloox::Tag(&many,"reaction",std::to_string(i));
    check(Chat::Reactions(&many).id().empty(),"reaction set cap");
    gloox::Message msg(gloox::Message::Chat,gloox::JID("peer@fixture.test"));msg.addExtension(send.clone());
    check(Chat::Classify(msg)==Chat::MessageKind::Reaction,"bodyless reaction is not ignored");
    gloox::Message err(gloox::Message::Error,gloox::JID("peer@fixture.test"));err.addExtension(send.clone());
    check(Chat::Classify(err)==Chat::MessageKind::DeliveryError,"echoed reaction error not incoming update");
    Chat::ReactionState state;
    check(state.Replace("alice","Alice",{"👍","👍"}),"incoming update");
    check(state.Counts()["👍"]==1,"duplicate emoji counts once per actor");
    state.Replace("bob","Bob",{"👍"});check(state.Counts()["👍"]==2,"multiple actors aggregate");
    state.Replace("alice","Alice",{"❤️"});check(state.Counts()["👍"]==1 && state.Counts()["❤️"]==1,"replace only one actor");
    state.Replace("alice","Alice",{});check(state.Counts().count("❤️")==0 && state.Counts()["👍"]==1,"remove only one actor");
    state.Replace("bob","Bob",{"👍"});check(state.Counts()["👍"]==1,"duplicate stanza idempotent");
    check(!state.Replace("evil","Evil",{"\n"}),"control data rejected");
    for(int i=0;i<127;++i)state.Replace(std::to_string(i),"Person",{"👍"});
    check(!state.Replace("overflow","Person",{"👍"}),"bounded actors");
    state.Replace("0","Person",{});check(state.Replace("overflow","Person",{"👍"}),"removal frees capacity");
    Chat::ReactionState other;check(other.Counts().empty(),"conversation state isolated");
    gloox::Message group(gloox::Message::Groupchat,gloox::JID("alice@fixture.test"));group.setFrom(gloox::JID("room@signal.fixture.test/Bob"));group.setID("sender-id");
    gloox::Tag sid("stanza-id");sid.setXmlns("urn:xmpp:sid:0");sid.addAttribute("by","room@signal.fixture.test");sid.addAttribute("id","room-id");group.addExtension(new Chat::StableId(&sid));
    check(Chat::ReactionId(group,true)=="room-id","room-assigned ID");check(Chat::ReactionId(group,false)=="sender-id","direct uses message ID");
    check(Upload::AttachmentType("REPORT.PDF")=="PDF document","type case insensitive");
    check(Upload::AttachmentDisplayName("https://upload.test/57fdcffd9ba644418cb5233222433606b.pdf")=="Received file.pdf","hash file readable title");
    check(Upload::DisplayAttachmentBody("See this\nhttps://upload.test/a.pdf","https://upload.test/a.pdf")=="See this","document caption preserved");
    check(Upload::DisplayAttachmentBody("https://other.test","https://upload.test/a.pdf")=="https://other.test","unrelated URL retained");
    WireProbe wire;
    gloox::Client client(gloox::JID("alice@fixture.test"),"synthetic-no-network");
    client.registerMessageSessionHandler(&wire);
    client.registerStanzaExtension(new Chat::Reactions());
    client.registerStanzaExtension(new Chat::Correction());
    gloox::Tag incoming("message");incoming.addAttribute("type","chat");incoming.addAttribute("from","bob@fixture.test/device");new gloox::Tag(&incoming,"body","hello");
    client.handleTag(&incoming);check(wire.contents==1,"real gloox creates Chat-only session");
    gloox::Tag rejected("message");rejected.addAttribute("type","error");rejected.addAttribute("from","bob@fixture.test/device");rejected.addAttribute("id","reaction-request");
    client.handleTag(&rejected);check(wire.errors==0,"reproduce missing error fallback in pinned gloox");
    client.registerMessageHandler(&wire);client.handleTag(&rejected);
    check(wire.errors==1,"fallback delivers bodyless error once");
    client.handleTag(&incoming);check(wire.contents==2,"fallback does not duplicate normal session messages");
    gloox::Tag reactionMessage("message");reactionMessage.addAttribute("type","chat");reactionMessage.addAttribute("from","bob@fixture.test/another-device");reactionMessage.addChild(send.tag());
    client.handleTag(&reactionMessage);check(wire.reactions==1,"wire parser routes reaction once across resources");
    gloox::Tag editMessage("message");editMessage.addAttribute("type","chat");editMessage.addAttribute("from","bob@fixture.test/device");editMessage.addAttribute("id","edit-wire");
    new gloox::Tag(&editMessage,"body","Corrected 🌞");editMessage.addChild(Chat::Correction("original-wire").tag());
    client.handleTag(&editMessage);check(wire.corrections==1 && wire.contents==2,"real parser delivers correction once without new bubble");
    gloox::Tag foreignEdit("message");foreignEdit.addAttribute("type","chat");foreignEdit.addAttribute("from","bob@fixture.test/device");new gloox::Tag(&foreignEdit,"body","Ordinary text");
    auto* foreignReplace=new gloox::Tag(&foreignEdit,"replace");foreignReplace->setXmlns("urn:wrong");foreignReplace->addAttribute("id","original-wire");
    client.handleTag(&foreignEdit);check(wire.corrections==1 && wire.contents==3,"foreign replace namespace is not a correction");
    std::cout<<checks<<" reaction and attachment checks passed\n";
}
