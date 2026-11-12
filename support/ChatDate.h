// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <ctime>
#include <string>
namespace ChatDate {
inline std::string Day(time_t value) {
    if(value<=0)return "earlier";
    tm local{};localtime_r(&value,&local);char text[32];strftime(text,sizeof(text),"%Y-%m-%d",&local);return text;
}
inline std::string Label(time_t value,time_t now=time(nullptr)) {
    if(value<=0)return "Earlier";
    if(Day(value)==Day(now))return "Today";
    tm yesterday{};localtime_r(&now,&yesterday);--yesterday.tm_mday;yesterday.tm_isdst=-1;
    if(Day(value)==Day(mktime(&yesterday)))return "Yesterday";
    tm local{};localtime_r(&value,&local);char text[32];strftime(text,sizeof(text),"%d.%m.%Y",&local);return text;
}
inline std::string Clock(time_t value) {
    if(value<=0)return {};
    tm local{};localtime_r(&value,&local);char text[16];strftime(text,sizeof(text),"%H:%M",&local);return text;
}
}
