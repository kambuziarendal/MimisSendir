// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Message.h>
#include <map>
#include <string>
#include <vector>
#include "ReactionState.h"

// Private, account-and-conversation scoped records. Each replacement is atomic.
// Older records remain on disk; the newest 1000 are loaded into the timeline.
class ChatHistory {
public:
    ChatHistory(const std::string& settings, const std::string& account, const std::string& peer);
    std::vector<BMessage> Load();
    bool Contains(const std::string& id) const;
    bool Put(BMessage record);
    bool MergeArchive(BMessage record);
    void Alias(const std::string& original, const std::string& stable);
    void Reactions(const std::string& id, const Chat::ReactionState& state);
    Chat::ReactionState Reactions(const std::string& id) const;
    const std::string& Error() const { return fError; }
    bool Empty() const { return fRecords.empty(); }
private:
    bool Save(const std::string& key, const BMessage& record);
    std::string Key(const std::string& id) const;
    std::string fRoot, fError;
    std::map<std::string,BMessage> fRecords;
};
