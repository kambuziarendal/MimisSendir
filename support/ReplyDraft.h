// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <string>
#include <cstddef>
namespace Chat {
inline size_t Codepoints(const std::string& text) {
    size_t count=0;
    for(unsigned char c:text)if((c&0xc0)!=0x80)++count;
    return count;
}
inline std::string ReplyPrefix(const std::string& body) {
    // Bound the quoted excerpt without cutting a UTF-8 sequence.
    size_t end=std::min(body.size(),size_t(2048));
    while(end<body.size() && (static_cast<unsigned char>(body[end])&0xc0)==0x80)--end;
    std::string excerpt=body.substr(0,end);
    if(end<body.size())excerpt+="…";
    std::string prefix="> ";
    for(size_t i=0;i<excerpt.size();++i) {
        prefix+=excerpt[i];
        if(excerpt[i]=='\n' && i+1<excerpt.size())prefix+="> ";
    }
    if(prefix.back()!='\n')prefix+='\n';
    return prefix+'\n';
}
}
