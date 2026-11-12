// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "network/Giphy.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iterator>
static int count=0;
static void Check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}++count;}
int main(int argc,char**){
    if(argc>1) {
        std::string json((std::istreambuf_iterator<char>(std::cin)),std::istreambuf_iterator<char>()), error;
        auto items=Giphy::Parse(json,error);size_t usable=0;for(const auto& item:items)if(!item.url.empty())++usable;
        printf("Live response parsed: items=%zu usable=%zu error=%d\n",items.size(),usable,!error.empty());
        return error.empty()&&usable>0?0:1;
    }
    Check(Giphy::MediaURL("https://media0.giphy.com/media/abc/giphy.gif?cid=one&rid=giphy.gif"),"signed media query retained");
    for(const char* url:{"http://media.giphy.com/a.gif","https://media.giphy.com.evil/a.gif","https://media.giphy.com@evil/a.gif","https://127.0.0.1/a.gif","https://media.giphy.com:444/a.gif","https://media.giphy.com/a.gif\n"})Check(!Giphy::MediaURL(url),"reject untrusted authority");
    std::string error;
    auto items=Giphy::Parse(R"({"meta":{"status":200},"data":[{"title":"Happy \ud83d\ude00","images":{"fixed_height":{"url":"https://media.giphy.com/media/a/giphy.gif?cid=x&rid=y"}},"user":{"display_name":"Artist"}},{"title":"Unavailable","images":{"fixed_height":{"url":"https://bad.example/a.gif"}}}]})",error);
    Check(error.empty()&&items.size()==2,"parse actual array representation");
    Check(items[0].title=="Happy 😀"&&items[0].creator=="Artist","unicode and artist attribution");
    Check(items[0].url=="https://media.giphy.com/media/a/giphy.gif?cid=x&rid=y","retain all query parameters");
    Check(items[1].url.empty(),"unsafe entry retained in order but disabled");
    error.clear();Check(Giphy::Parse("{broken",error).empty()&&!error.empty(),"reject malformed response");
    error.clear();Check(Giphy::Parse(R"({"meta":{"status":429},"data":[]})",error).empty()&&!error.empty(),"reject API failure");
    Check(!Giphy::Acquire(999999),"expired or unknown lease cannot send");
    std::atomic_bool cancel{false};error.clear();Check(!Giphy::Download("https://evil.invalid/test",cancel,error)&&!error.empty(),"reject before external request");
    printf("%d GIPHY protocol checks passed\n",count);
}
