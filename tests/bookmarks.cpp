// Real gloox bookmark parser, synthetic payloads, no connections.
#include <gloox/client.h>
#include <gloox/pubsubitem.h>
#include <gloox/error.h>
#include "network/bookmark2storage.h"
#include <cstdlib>
#include <iostream>
static int checks;
static void Check(bool ok) { ++checks; if (!ok) std::abort(); }
struct Handler : gloox::BookmarkHandler {
 int calls=0; gloox::ConferenceList rooms;
 void handleBookmarks(const gloox::BookmarkList&, const gloox::ConferenceList& list) override { ++calls; rooms=list; }
};
int main() {
 gloox::Client client(gloox::JID("synthetic@example.test"), "unused");
 gloox::Bookmark2Storage storage(&client); Handler handler; storage.registerBookmarkHandler(&handler);
 gloox::PubSub::Item missing; missing.setID("missing@example.test");
 gloox::PubSub::Item other; other.setID("other@example.test"); other.setPayload(new gloox::Tag("other"));
 gloox::PubSub::Item valid; valid.setID("room@example.test");
 auto* conference=new gloox::Tag("conference");conference->addAttribute("name", "Synthetic group");
 conference->addAttribute("autojoin", "true");new gloox::Tag(conference, "nick", "Synthetic Alice"); valid.setPayload(conference);
 gloox::PubSub::ItemList items{&missing,&other,&valid};
 storage.handleItems("", gloox::JID(), "", items, nullptr);
 Check(handler.calls==1); Check(handler.rooms.size()==1);
 Check(handler.rooms.front().name=="Synthetic group"); Check(handler.rooms.front().autojoin);
 Check(handler.rooms.front().nick=="Synthetic Alice");
 gloox::Error error(gloox::StanzaErrorTypeCancel,gloox::StanzaErrorForbidden);
 storage.handleItems("",gloox::JID(),"",{},&error);Check(handler.calls==1);Check(handler.rooms.size()==1);
 storage.handleItems("",gloox::JID(),"",{},nullptr);Check(handler.calls==2);Check(handler.rooms.empty());
 std::cout << checks << " bookmark snapshot checks passed\n";
}
