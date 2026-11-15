// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "support/ForwardTargets.h"
#include <cstdio>
#include <cstdlib>
int main() {
    int checks=0;
    auto check=[&](bool ok,const char* text){if(!ok){fprintf(stderr,"FAIL %s\n",text);std::exit(1);}++checks;printf("PASS %s\n",text);};
    Conversations::Summary favorite,old,recent,other,duplicate;
    favorite.jid="fav@fixture.test";favorite.name="Zeta";favorite.favorite=true;favorite.activity=1;
    old.jid="old@fixture.test";old.name="Alfa";old.activity=2;
    recent.jid="recent@fixture.test";recent.name="Beta";recent.activity=9;
    other.jid="other@fixture.test";other.name="New Contact";
    duplicate=old;duplicate.jid="second-alfa@fixture.test";duplicate.activity=3;
    std::vector<Conversations::Summary> targets{old,other,recent,favorite,duplicate};
    auto top=Conversations::ForwardChoices(targets,false);
    check(top.size()==4,"first page contains favorites and existing conversations only");
    check(top[0].jid==favorite.jid,"favorite precedes newer conversations");
    check(top[1].jid==recent.jid && top[2].jid==duplicate.jid && top[3].jid==old.jid,"recent conversations ordered by activity");
    auto all=Conversations::ForwardChoices(targets,true);
    check(all.size()==5 && all[0].name=="Alfa" && all[1].name=="Alfa","other contacts page includes all destinations alphabetically");
    check(all[0].jid!=all[1].jid,"equal display names never merge recipient identities");
    auto search=Conversations::ForwardChoices(targets,true,"new");
    check(search.size()==1 && search[0].jid==other.jid,"other contacts are searchable by readable name");
    check(Conversations::ForwardChoices(targets,false,"new").empty(),"search cannot silently broaden first page to unrelated contacts");
    check(Conversations::FriendlyName("long-opaque-id@signal.fixture.test","Readable Name",false)=="Readable Name","display label excludes transport identifier");
    check(Conversations::ForwardChoices({},false).empty(),"empty roster remains empty");
    printf("%d forwarding checks passed\n",checks);
}
