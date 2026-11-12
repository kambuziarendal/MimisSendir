// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
namespace Chat {
class ReactionState {
public:
    struct Actor { std::string name; std::set<std::string> values; };
    // View-scoped and bounded. Empty sets remove only this actor's reactions.
    bool Replace(const std::string& actor, const std::string& name, const std::vector<std::string>& values) {
        if(actor.empty() || actor.size()>2048 || name.size()>1024 || values.size()>16)return false;
        for(const auto& v:values)if(v.empty() || v.size()>64 || v.find_first_of("\r\n\t")!=std::string::npos)return false;
        if(values.empty()){actors.erase(actor);return true;}
        if(!actors.count(actor) && actors.size()>=128)return false;
        actors[actor]={name,{values.begin(),values.end()}};return true;
    }
    std::map<std::string,size_t> Counts() const {
        std::map<std::string,size_t> result;
        for(const auto& [key,actor]:actors){(void)key;for(const auto& e:actor.values)++result[e];}
        return result;
    }
    std::map<std::string,Actor> actors;
};
}
