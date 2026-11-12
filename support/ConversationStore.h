// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Conversations {
struct Summary {
    std::string jid, name, preview, avatar, lastMessageId;
    int64_t activity = 0; // UTC microseconds, never presence or window-open time.
    uint32_t unread = 0;
    int64_t hiddenAt = 0; // Local hide action; no history or contact deletion.
    bool group = false;
    bool favorite = false;
};
std::string FriendlyName(const std::string& jid, const std::string& name, bool group);
std::string Preview(const std::string& text);
int64_t ArchiveTime(const std::string& stamp);
class Store {
public:
    static constexpr size_t kLimit = 256;
    void Account(const std::string& account) { fAccount = account; fItems.clear(); }
    bool Record(Summary message, bool incoming, bool visible, bool archive = false);
    bool SetFavorite(Summary item, bool favorite);
    bool CorrectPreview(const std::string& jid, const std::string& id, const std::string& body);
    bool Hide(const std::string& jid, int64_t when);
    void Read(const std::string& jid);
    void Rename(const std::string& jid, const std::string& name);
    std::vector<Summary> Recent() const;
    const Summary* Find(const std::string& jid) const;
    bool Save(const std::string& path) const;
    bool Load(const std::string& path);
private:
    std::string fAccount;
    std::map<std::string, Summary> fItems;
};
}
