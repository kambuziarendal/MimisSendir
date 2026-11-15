// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include "ConversationStore.h"
#include <strings.h>
#include <cstring>

namespace Conversations {
inline std::vector<Summary> ForwardChoices(std::vector<Summary> targets, bool other,
    const std::string& search = {})
{
    targets.erase(std::remove_if(targets.begin(), targets.end(), [&](const auto& item) {
        if (item.jid.empty() || (!other && !item.favorite && item.activity <= 0)) return true;
        const auto name = FriendlyName(item.jid, item.name, item.group);
        return !search.empty() && strcasestr(name.c_str(), search.c_str()) == nullptr;
    }), targets.end());
    std::stable_sort(targets.begin(), targets.end(), [other](const auto& a, const auto& b) {
        if (!other && a.favorite != b.favorite) return a.favorite;
        if (!other && a.activity != b.activity) return a.activity > b.activity;
        const int names = strcasecmp(a.name.c_str(), b.name.c_str());
        if (names != 0) return names < 0;
        if (a.group != b.group) return a.group < b.group;
        return a.jid < b.jid;
    });
    return targets;
}
}
