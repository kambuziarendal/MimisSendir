#include "support/ConversationStore.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace Conversations;
static int checks;
static void Check(bool ok) { ++checks; if (!ok) std::abort(); }
static Summary Msg(std::string jid, std::string name, int64_t time, bool group = false) {
    Summary m; m.jid=jid; m.name=name; m.activity=time; m.group=group; m.preview="Synthetic text"; return m;
}
int main() {
    Store s; s.Account("alice@example.test");
    Check(s.Recent().empty()); // Roster alone never populates recents.
    Check(s.Record(Msg("b@x/r", "Bob", 100), true, false));
    Check(s.Find("b@x")->unread == 1);
    Check(s.Record(Msg("g@x", "Pizza crew", 200, true), false, true));
    Check(s.Recent()[0].group);
    s.Rename("g@x", "");
    Check(s.Find("g@x")->name == "Pizza crew");
    s.Rename("g@x", "Kitchen crew");
    Check(s.Find("g@x")->name == "Kitchen crew");
    Check(s.Record(Msg("b@x", "Bob", 300), false, true));
    Check(s.Recent()[0].jid == "b@x" && s.Recent()[0].unread == 0);
    Check(!s.Record(Msg("g@x", "Pizza crew", 50, true), true, false, true));
    Check(s.Recent()[0].jid == "b@x");
    s.Record(Msg("a@x", "Aaron", 300), true, false);
    s.Record(Msg("c@x", "Aaron", 300), true, false);
    Check(s.Recent()[0].jid == "a@x" && s.Recent()[1].jid == "c@x");
    s.Read("a@x"); Check(s.Find("a@x")->unread == 0);
    auto dup = Msg("d@x", "Dora", 400); dup.lastMessageId="msg-1";
    s.Record(dup, true, false); dup.activity=500;
    Check(!s.Record(dup, true, false)); Check(s.Find("d@x")->unread == 1);
    Check(FriendlyName("deadbeef@x", "deadbeef", true).starts_with("Signal group "));
    Check(FriendlyName("one@x", "", true) != FriendlyName("two@x", "", true));
    Check(FriendlyName("one@x", "", true) == FriendlyName("one@x", "", true));
    Check(FriendlyName("a@x", "Pizza crew", true) == "Pizza crew");
    Check(ArchiveTime("2026-11-09T08:00:00Z") == 1794211200000000LL);
    Check(ArchiveTime("2026-02-30T08:00:00Z") == 0);
    Check(ArchiveTime("bad") == 0);
    Check(ArchiveTime("2026-11-09T08:00:00.000123456Z") == 1794211200000123LL);
    Check(ArchiveTime("2026-11-09T08:00:00junkZ") == 0);
    Check(ArchiveTime("2026-11-09T08:00:00.123Z") == 1794211200123000LL);
    std::string path="/tmp/renga-recents-test-" + std::to_string(getpid());
    Check(s.Save(path)); Store restored; restored.Account("alice@example.test");
    Check(restored.Load(path)); Check(restored.Recent().size() == 5);
    Check(restored.Recent()[0].jid == "d@x" && restored.Recent()[0].unread == 1);
    Store other; other.Account("other@example.test"); Check(!other.Load(path)); Check(other.Recent().empty());
    { std::ofstream bad(path); bad << "RSGREC1"; }
    Check(!restored.Load(path)); Check(restored.Recent().size() == 5);
    for (int i=0; i<400; ++i) s.Record(Msg(std::to_string(i)+"@x", "Contact", 1000+i), true, false);
    Check(s.Recent().size() == Store::kLimit); Check(s.Find("b@x") == nullptr);
    Check(s.Save(path)); Check(restored.Load(path)); Check(restored.Recent().size() == Store::kLimit);
    // Favorites work before the first message and survive retention, new
    // activity, restart and account switching without fabricating unread data.
    auto contact = Msg("pin@x/resource", "Pinned contact", 0);
    Check(s.SetFavorite(contact, true));
    Check(s.Find("pin@x")->favorite && s.Find("pin@x")->activity == 0);
    Check(s.Find("pin@x")->unread == 0 && s.Recent().size() == Store::kLimit);
    Check(s.SetFavorite(Msg("room@x", "A favorite group", 0, true), true));
    Check(s.Recent()[0].jid == "room@x" && s.Recent()[0].group);
    Check(s.Save(path)); Check(restored.Load(path));
    Check(restored.Find("pin@x")->favorite && restored.Find("room@x")->favorite);
    Check(!other.Load(path) && other.Recent().empty());
    Check(s.Record(Msg("pin@x", "Pinned contact", 3000), true, false));
    Check(s.Find("pin@x")->favorite && s.Find("pin@x")->unread == 1);
    Check(s.Recent()[0].jid == "room@x"); // Favorites stay alphabetical, not activity-driven.
    Check(s.SetFavorite(Msg("room@x", "", 0), false));
    Check(s.Find("room@x") == nullptr); // No artificial recent conversation remains.
    Check(s.SetFavorite(contact, false));
    Check(s.Find("pin@x") && !s.Find("pin@x")->favorite && s.Find("pin@x")->activity == 3000);
    Check(s.SetFavorite(Msg("room@x", "Crew", 0, true), true));
    s.Rename("room@x", "New crew"); Check(s.Find("room@x")->name == "New crew");
    for (int i=0; i<400; ++i) s.Record(Msg(std::to_string(i)+"@x", "Contact", 4000+i), true, false);
    Check(s.Find("room@x") && s.Find("room@x")->favorite);
    Check(s.Recent().size() == Store::kLimit);
    for (int i=0; i<63; ++i) Check(s.SetFavorite(Msg("fav"+std::to_string(i)+"@x", "Favorite", 0), true));
    Check(!s.SetFavorite(Msg("overflow@x", "Too many", 0), true));
    Check(s.SetFavorite(Msg("room@x", "Crew", 0, true), true)); // Idempotent at capacity.
    Check(!s.SetFavorite(Msg("", "Invalid", 0), true));
    Check(s.Save(path)); Check(restored.Load(path));
    Check(restored.Recent()[0].favorite);
    restored.Account("other@example.test"); Check(restored.Recent().empty());
    // Read a real v1 record without a favorite byte, preserving upgrade compatibility.
    {
        std::ofstream legacy(path, std::ios::binary);
        auto str = [&legacy](const std::string& v) { uint32_t n=v.size(); legacy.write(reinterpret_cast<const char*>(&n),4); legacy.write(v.data(),n); };
        legacy.write("RSGREC1",7); str("alice@example.test");
        uint32_t count=1, unread=2; int64_t activity=1234; uint8_t group=1;
        legacy.write(reinterpret_cast<const char*>(&count),4);
        str("legacy@x"); str("Legacy crew"); str("Hello"); str(""); str("old-id");
        legacy.write(reinterpret_cast<const char*>(&activity),8);
        legacy.write(reinterpret_cast<const char*>(&unread),4);
        legacy.write(reinterpret_cast<const char*>(&group),1);
    }
    restored.Account("alice@example.test"); Check(restored.Load(path));
    Check(restored.Find("legacy@x")->group && restored.Find("legacy@x")->unread == 2 && !restored.Find("legacy@x")->favorite);
    Check(restored.Save(path)); Store upgraded; upgraded.Account("alice@example.test"); Check(upgraded.Load(path));
    Check(upgraded.Find("legacy@x")->activity == 1234);
    Check(!upgraded.Save(path + "/missing-directory"));
    Check(upgraded.Find("legacy@x")->activity == 1234);
    // Hide is presentation-only, account-local, persistent and reset by new activity.
    Store hidden;hidden.Account("alice@example.test");
    auto last=Msg("hide@x","Hidden chat",100);last.lastMessageId="last";
    Check(hidden.Record(last,true,false));Check(hidden.SetFavorite(last,true));
    Check(hidden.Hide("hide@x",200));
    Check(hidden.Find("hide@x")->favorite && hidden.Find("hide@x")->unread==1);
    hidden.Read("hide@x");hidden.Rename("hide@x","Renamed");
    Check(hidden.Find("hide@x")->hiddenAt==200);
    Check(hidden.Save(path));Store hiddenRestart;hiddenRestart.Account("alice@example.test");
    Check(hiddenRestart.Load(path) && hiddenRestart.Find("hide@x")->hiddenAt==200);
    Check(!other.Load(path));
    last.activity=300;Check(!hiddenRestart.Record(last,true,false));
    Check(hiddenRestart.Find("hide@x")->hiddenAt==200); // Duplicate does not reveal.
    auto oldArchive=Msg("hide@x","Hidden chat",150);oldArchive.lastMessageId="archive-old";
    Check(hiddenRestart.Record(oldArchive,true,false,true));
    Check(hiddenRestart.Find("hide@x")->hiddenAt==200); // Backfill predates hiding.
    auto newArchive=Msg("hide@x","Hidden chat",250);newArchive.lastMessageId="archive-new";
    Check(hiddenRestart.Record(newArchive,true,false,true));
    Check(hiddenRestart.Find("hide@x")->hiddenAt==0 && hiddenRestart.Find("hide@x")->favorite);
    Check(hiddenRestart.Hide("hide@x",400));
    auto outgoing=Msg("hide@x","Hidden chat",240);outgoing.lastMessageId="outgoing";
    Check(hiddenRestart.Record(outgoing,false,true)); // Live send despite clock skew.
    Check(hiddenRestart.Find("hide@x")->hiddenAt==0);
    Check(hiddenRestart.Hide("hide@x",400));
    auto incoming=Msg("hide@x","Hidden chat",500);incoming.lastMessageId="incoming";
    Check(hiddenRestart.Record(incoming,true,false));Check(hiddenRestart.Find("hide@x")->hiddenAt==0);
    Check(!hiddenRestart.Hide("unknown@x",500) && !hiddenRestart.Hide("hide@x",0));
    Check(hiddenRestart.Hide("hide@x",600));Check(!hiddenRestart.Save(path+"/bad-directory"));
    Check(hiddenRestart.Find("hide@x")->hiddenAt==600);
    // A v2 favorite upgrades without introducing a hide flag or dropping metadata.
    {
        std::ofstream legacy(path,std::ios::binary);
        auto str=[&](const std::string& v){uint32_t n=v.size();legacy.write(reinterpret_cast<const char*>(&n),4);legacy.write(v.data(),n);};
        legacy.write("RSGREC2",7);str("alice@example.test");uint32_t count=1,unread=2;int64_t activity=1234;uint8_t group=1,favorite=1;
        legacy.write(reinterpret_cast<const char*>(&count),4);str("legacy@x");str("Legacy crew");str("Hello");str("");str("old-id");
        legacy.write(reinterpret_cast<const char*>(&activity),8);legacy.write(reinterpret_cast<const char*>(&unread),4);
        legacy.write(reinterpret_cast<const char*>(&group),1);legacy.write(reinterpret_cast<const char*>(&favorite),1);
    }
    Check(hiddenRestart.Load(path));Check(hiddenRestart.Find("legacy@x")->favorite && !hiddenRestart.Find("legacy@x")->hiddenAt);
    Check(hiddenRestart.Save(path));Check(hiddenRestart.Load(path));
    Check(hiddenRestart.Find("legacy@x")->activity==1234 && hiddenRestart.Find("legacy@x")->unread==2);
    unlink(path.c_str());
    std::cout << checks << " conversation checks passed\n";
}
