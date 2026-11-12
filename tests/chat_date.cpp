#include "support/ChatDate.h"
#include <cstdlib>
#include <iostream>
int checks=0;void Check(bool ok){++checks;if(!ok)std::exit(1);}
time_t At(int year,int month,int day,int hour=12){tm value{};value.tm_year=year-1900;value.tm_mon=month-1;value.tm_mday=day;value.tm_hour=hour;value.tm_isdst=-1;return mktime(&value);}
int main(){setenv("TZ","Europe/Oslo",1);tzset();
Check(ChatDate::Label(At(2026,11,10,0),At(2026,11,10,23))=="Today");
Check(ChatDate::Label(At(2026,11,9,23),At(2026,11,10,0))=="Yesterday");
Check(ChatDate::Label(At(2026,12,31),At(2027,1,1))=="Yesterday");
Check(ChatDate::Label(At(2026,3,28,23),At(2026,3,29,23))=="Yesterday");
Check(ChatDate::Label(At(2026,10,24,0),At(2026,10,25,23))=="Yesterday");
Check(ChatDate::Label(At(2026,11,8),At(2026,11,10))=="08.11.2026");
Check(ChatDate::Label(0)=="Earlier");Check(ChatDate::Clock(0).empty());Check(ChatDate::Clock(At(2026,11,10,14))=="14:00");
Check(ChatDate::Day(At(2026,11,10,0))==ChatDate::Day(At(2026,11,10,23)));
std::cout<<checks<<" date separator checks passed\n";}
