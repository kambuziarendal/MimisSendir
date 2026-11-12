// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "support/ChatHistory.h"
#include "ui/ColorEmoji.h"
#include <Application.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
static int checks=0;
static void Check(bool ok,const char* text){if(!ok){fprintf(stderr,"FAIL: %s\n",text);exit(1);}++checks;}
int main(){
    BApplication app("application/x-vnd.Kambuzi-HistoryTest");
    auto glyph=ColorEmoji("❤️");Check(bool(glyph),"local colored reaction artwork");
    std::weak_ptr<BBitmap> weak=glyph;glyph.reset();Check(weak.expired(),"emoji cache cannot outlive UI and app_server connection");
    char pattern[]="/tmp/renga-history-XXXXXX";std::string root=mkdtemp(pattern);
    ChatHistory history(root,"alice@fixture.test","bob@fixture.test");Check(history.Load().empty(),"empty store");
    BMessage m('hRec');m.AddString("id","one");m.AddString("body","Hei 🌞\n> quoted text");m.AddString("name","Alice");m.AddInt64("time",123000000);m.AddBool("outgoing",true);m.AddString("attachment","https://upload.fixture.test/test.pdf");
    Check(history.Put(m),"persist full message");Check(!history.Put(m),"idempotent replay");
    Chat::ReactionState state;state.Replace("bob","Bob",{"❤️"});state.Replace("self","You",{"👍"});history.Reactions("one",state);history.Alias("one","room-stable");
    ChatHistory reopened(root,"alice@fixture.test","bob@fixture.test");auto records=reopened.Load();
    Check(records.size()==1,"restart preserves message count");Check(records[0].GetBool("outgoing",false),"direction preserved");
    Check(std::string(records[0].GetString("body",""))==m.GetString("body",""),"unicode and quotes preserved");
    Check(std::string(records[0].GetString("attachment",""))==m.GetString("attachment",""),"attachment metadata preserved");
    Check(reopened.Contains("room-stable"),"stable alias persisted");Check(reopened.Reactions("room-stable").Counts().size()==2,"reactions restored");
    state.Replace("self","You",{});reopened.Reactions("room-stable",state);
    ChatHistory third(root,"alice@fixture.test","bob@fixture.test");third.Load();Check(third.Reactions("one").Counts().size()==1,"reaction removal persisted");
    // A nonempty cache may contain a newer live message while an older one
    // was missed offline. Reconciliation must insert the hole, not truncate
    // at the newest local timestamp or overwrite reactions on duplicates.
    BMessage recent(m);recent.ReplaceString("id","three");recent.ReplaceInt64("time",125000000);
    Check(third.Put(recent),"newer live message precedes catch-up");
    BMessage missing(m);missing.ReplaceString("id","two");missing.ReplaceInt64("time",124000000);
    Check(third.MergeArchive(missing),"catch-up inserts missing middle message in nonempty cache");
    Check(!third.Put(m),"overlap does not replace existing message");
    Check(third.Reactions("one").Counts().size()==1,"catch-up preserves saved reactions");
    ChatHistory caughtUp(root,"alice@fixture.test","bob@fixture.test");auto merged=caughtUp.Load();
    Check(merged.size()==3 && std::string(merged[1].GetString("id",""))=="two", "gap restored chronologically after restart");
    Check(!caughtUp.Put(missing) && caughtUp.Load().size()==3,"repeated catch-up has no duplicates");
    BMessage clockFix(m);clockFix.ReplaceString("id","room-stable");clockFix.ReplaceInt64("time",122500000);
    clockFix.ReplaceString("body","Do not replace local content");
    Check(caughtUp.MergeArchive(clockFix),"archive time corrects a known stable alias");
    Check(!caughtUp.MergeArchive(clockFix),"matching archive timestamp is a no-op");
    clockFix.AddBool("markable",true);
    Check(caughtUp.MergeArchive(clockFix),"archive upgrades read metadata without a timestamp change");
    Check(caughtUp.Record(clockFix.GetString("id","")).GetBool("markable",false),"read opt-in survives archive merge");
    Check(!caughtUp.MergeArchive(clockFix),"repeated read metadata is a no-op");
    auto corrected=caughtUp.Load();
    Check(corrected[0].GetInt64("time",0)==122500000 && std::string(corrected[0].GetString("body",""))==m.GetString("body",""),"authoritative ordering preserves message content");
    Check(caughtUp.Reactions("one").Counts().size()==1,"clock reconciliation preserves reactions");
    ChatHistory other(root,"eve@fixture.test","bob@fixture.test");Check(other.Load().empty(),"account isolation");
    ChatHistory group(root,"alice@fixture.test","room@signal.fixture.test");Check(group.Load().empty(),"conversation isolation");
    for(const auto& file:std::filesystem::recursive_directory_iterator(root))if(file.is_regular_file()){
        struct stat st{};stat(file.path().c_str(),&st);Check((st.st_mode&0777)==0600,"private records");
        std::ofstream partial(file.path().parent_path()/"broken.msg");partial<<"partial";partial.close();break;
    }
    ChatHistory damaged(root,"alice@fixture.test","bob@fixture.test");Check(damaged.Load().size()==3 && !damaged.Error().empty(),"partial record preserved; good history still loads");
    ChatHistory edits(root,"alice@fixture.test","carol@fixture.test");
    BMessage text('hRec');text.AddString("id","renga-own");text.AddString("body","Original 🌞");text.AddString("sender","alice@fixture.test");text.AddBool("outgoing",true);text.AddInt64("time",100);
    Check(edits.Put(text) && edits.Editable("renga-own",false),"own text editable");
    edits.Delivery("renga-own",4);Check(!edits.Editable("renga-own",false),"failed message is not sent text");
    edits.Delivery("renga-own",0);Check(!edits.Editable("renga-own",false),"queued message is not editable");
    edits.Delivery("renga-own",1);Check(edits.Editable("renga-own",false),"submitted text can be corrected");
    Check(!history.Editable("one",false),"attachments are not editable");
    BMessage incoming(text);incoming.ReplaceString("id","incoming");incoming.ReplaceString("sender","carol@fixture.test");incoming.ReplaceBool("outgoing",false);edits.Put(incoming);
    Check(!edits.Editable("incoming",false),"other author's text not editable");
    BMessage unidentified(text);unidentified.ReplaceString("id","archive-missing-id");unidentified.AddString("wire-id","");edits.Put(unidentified);
    Check(!edits.Editable("archive-missing-id",false),"archive without sender ID must never turn an edit into a new message");
    edits.Reactions("renga-own",state);edits.Alias("renga-own","stable-own");
    Check(!edits.Correct("renga-own","attack","Wrong","carol@fixture.test",false,200),"peer cannot replace outgoing body");
    Check(!edits.Correct("renga-own","attack","Wrong","carol@fixture.test",true,200),"direction flag cannot bypass author");
    Check(!edits.Correct("renga-own","empty","","alice@fixture.test",true,200),"empty correction rejected");
    Check(edits.Correct("stable-own","edit-one","Changed æ 🌞","alice@fixture.test/device",true,200),"own correction through stable alias");
    ChatHistory editRestart(root,"alice@fixture.test","carol@fixture.test");auto editRows=editRestart.Load();
    Check(editRows.size()==3 && std::string(editRestart.Record("renga-own").GetString("body",""))=="Changed æ 🌞","edit survives restart without duplicate");
    Check(editRestart.Record("renga-own").GetInt64("time",0)==100 && editRestart.Reactions("renga-own").Counts()==state.Counts(),"original order and reactions retained");
    Check(!editRestart.Correct("renga-own","edit-one","Changed æ 🌞","alice@fixture.test",true,201),"echo duplicate idempotent");
    Check(editRestart.RejectCorrection("edit-one"),"correlated failure restores previous body after restart");
    auto restored=editRestart.Record("renga-own");
    Check(std::string(restored.GetString("body",""))=="Original 🌞" && std::string(restored.GetString("failed-edit-body",""))=="Changed æ 🌞","rejected draft retained separately from original");
    Check(!editRestart.RejectCorrection("unrelated"),"unrelated failure ignored");
    Check(editRestart.Correct("renga-own","edit-two","Second edit","alice@fixture.test",true,300) && editRestart.ConfirmCorrection("edit-two"),"receipt confirms exact edit");
    Check(!editRestart.RejectCorrection("edit-two"),"late error cannot roll back confirmed edit");
    Check(editRestart.Correct("renga-own","edit-three","Third edit","alice@fixture.test",true,400),"repeated edit still targets original");
    Check(!editRestart.RejectCorrection("edit-two") && editRestart.RejectCorrection("edit-three"),"stale failure does not roll back newer edit");
    Check(std::string(editRestart.Record("renga-own").GetString("body",""))=="Second edit","latest failed edit rolls back to previous successful edit");
    Check(!editRestart.Correct("renga-own","archive-old","Old edit","alice@fixture.test",true,200),"stale archive cannot overwrite newer edit");
    Check(editRestart.Correct("incoming","peer-edit","Corrected incoming","carol@fixture.test/phone",false,500),"incoming correction requires original author");
    ChatHistory room(root,"alice@fixture.test","room@conference.fixture.test",true);
    incoming.ReplaceString("id","server-id");incoming.AddString("wire-id","sender-id");incoming.ReplaceString("sender","room@conference.fixture.test/Bob");room.Put(incoming);
    Check(!room.Correct("sender-id","evil","Wrong","room@conference.fixture.test/Eve",false,500),"room occupants remain distinct authors");
    Check(room.Correct("sender-id","good","Corrected room","room@conference.fixture.test/Bob",false,500),"room correction resolves original sender ID not canonical reaction ID");
    std::filesystem::remove_all(root);printf("%d native history checks passed\n",checks);
}
