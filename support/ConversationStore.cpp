// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "ConversationStore.h"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

namespace Conversations {
std::string FriendlyName(const std::string& jid, const std::string& name, bool group)
{
    const auto local = jid.substr(0, jid.find('@'));
    const bool opaque = name.size() >= 24 && std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_' || c == '+' || c == '/' || c == '=';
    });
    if (name.empty() || name == jid || name == local || name.find('@') != std::string::npos || opaque)
    {
        if (!group) return "Signal contact";
        uint32_t token = 2166136261u;
        for (unsigned char c : jid) { token ^= c; token *= 16777619u; }
        char label[40];
        snprintf(label, sizeof(label), "Signal group %08x", token);
        return label;
    }
    return Preview(name);
}
std::string Preview(const std::string& text)
{
    std::string result;
    for (unsigned char c : text) {
        if (result.size() >= 256) break;
        if (c < 32 || c == 127) c = ' ';
        result += char(c);
    }
    // Do not truncate inside a UTF-8 sequence.
    if (result.size() < text.size()) {
        size_t start = result.size();
        while (start > 0 && (static_cast<unsigned char>(result[start - 1]) & 0xc0) == 0x80) --start;
        if (start > 0 && static_cast<unsigned char>(result[start - 1]) >= 0xc0) --start;
        result.resize(start);
    }
    return result;
}
int64_t ArchiveTime(const std::string& stamp)
{
    if (stamp.size() < 20 || stamp[4] != '-' || stamp[7] != '-' || stamp[10] != 'T'
        || stamp[13] != ':' || stamp[16] != ':' || stamp.back() != 'Z') return 0;
    for (size_t i = 0; i < 19; ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
        if (!std::isdigit(static_cast<unsigned char>(stamp[i]))) return 0;
    }
    if (stamp.size() > 20) {
        if (stamp[19] != '.' || stamp.size() < 22 || stamp.size() > 30) return 0;
        for (size_t i = 20; i + 1 < stamp.size(); ++i)
            if (!std::isdigit(static_cast<unsigned char>(stamp[i]))) return 0;
    }
    int y, m, d, h, n, s;
    if (stamp.size() < 20 || stamp.back() != 'Z' ||
        sscanf(stamp.c_str(), "%d-%d-%dT%d:%d:%d", &y, &m, &d, &h, &n, &s) != 6 ||
        y < 1970 || y > 2200 || m < 1 || m > 12 || d < 1 || d > 31 ||
        h < 0 || h > 23 || n < 0 || n > 59 || s < 0 || s > 59) return 0;
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (d > days[m-1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) return 0;
    y -= m <= 2;
    const int era = y / 400;
    const unsigned year = unsigned(y - era * 400);
    const unsigned day = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const int64_t epochDays = era * 146097 + year * 365 + year / 4 - year / 100 + day - 719468;
    int64_t fraction=0;
    if(stamp.size()>20)for(size_t i=0;i<6;++i){fraction*=10;if(20+i<stamp.size()-1)fraction+=stamp[20+i]-'0';}
    return ((epochDays * 24 + h) * 3600 + n * 60 + s) * 1000000 + fraction;
}
bool Store::Record(Summary message, bool incoming, bool visible, bool archive)
{
    message.hiddenAt = 0;
    message.favorite = false; // Only an explicit local user action can pin a chat.
    message.jid = message.jid.substr(0, message.jid.find('/'));
    if (message.jid.empty() || message.jid.size() > 1024 || message.activity <= 0) return false;
    message.name = FriendlyName(message.jid, message.name, message.group);
    message.preview = Preview(message.preview);
    message.avatar = Preview(message.avatar);
    message.lastMessageId = Preview(message.lastMessageId);
    auto found = fItems.find(message.jid);
    if (found != fItems.end()) {
        const auto& old = found->second;
        message.favorite = old.favorite;
        if (!message.lastMessageId.empty() && message.lastMessageId == old.lastMessageId) return false;
        const bool reveal=old.hiddenAt && (!archive || message.activity>old.hiddenAt);
        if (message.activity < old.activity || (archive && message.activity == old.activity)) {
            if(reveal){found->second.hiddenAt=0;return true;}
            return false;
        }
        message.hiddenAt=reveal?0:old.hiddenAt;
        message.unread = visible ? 0 : std::min(9999u, old.unread + uint32_t(incoming && !archive));
    } else message.unread = incoming && !visible && !archive ? 1 : 0;
    fItems[message.jid] = message;
    auto ordered = Recent();
    for (size_t i = kLimit; i < ordered.size(); ++i) fItems.erase(ordered[i].jid);
    return true;
}
bool Store::CorrectPreview(const std::string& jid,const std::string& id,const std::string& body) {
    auto found=fItems.find(jid);
    if(id.empty() || found==fItems.end() || found->second.lastMessageId!=id)return false;
    const auto preview=Preview(body);if(found->second.preview==preview)return false;
    found->second.preview=preview;return true;
}
bool Store::SetFavorite(Summary item, bool favorite)
{
    item.jid = item.jid.substr(0, item.jid.find('/'));
    if (item.jid.empty() || item.jid.size() > 1024) return false;
    auto found = fItems.find(item.jid);
    if (!favorite) {
        if (found == fItems.end()) return true;
        if (found->second.activity == 0) fItems.erase(found);
        else found->second.favorite = false;
        return true;
    }
    size_t count = 0;
    for (const auto& entry : fItems) count += entry.second.favorite;
    if (count >= 64 && (found == fItems.end() || !found->second.favorite)) return false;
    if (found != fItems.end()) found->second.favorite = true;
    else {
        Summary pinned;
        pinned.jid = item.jid;
        pinned.group = item.group;
        pinned.name = FriendlyName(item.jid, item.name, item.group);
        pinned.favorite = true;
        fItems[pinned.jid] = pinned;
        auto ordered = Recent();
        for (size_t i = kLimit; i < ordered.size(); ++i) fItems.erase(ordered[i].jid);
    }
    return true;
}
bool Store::Hide(const std::string& jid, int64_t when) {
    auto found=fItems.find(jid);
    if(found==fItems.end() || when<=0)return false;
    found->second.hiddenAt=when;
    return true;
}
void Store::Read(const std::string& jid) { auto i = fItems.find(jid); if (i != fItems.end()) i->second.unread = 0; }
void Store::Rename(const std::string& jid, const std::string& name) {
    auto i = fItems.find(jid);
    if (i != fItems.end()) {
        const auto title = FriendlyName(jid, name, i->second.group);
        if (title != FriendlyName(jid, "", i->second.group) || i->second.name.empty())
            i->second.name = title;
    }
}
const Summary* Store::Find(const std::string& jid) const { auto i = fItems.find(jid); return i == fItems.end() ? nullptr : &i->second; }
std::vector<Summary> Store::Recent() const {
    std::vector<Summary> out;
    for (const auto& [jid, summary] : fItems) out.push_back(summary);
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        if (a.favorite != b.favorite) return a.favorite;
        if (!a.favorite && a.activity != b.activity) return a.activity > b.activity;
        if (a.name != b.name) return a.name < b.name;
        return a.jid < b.jid;
    });
    return out;
}
namespace {
bool WriteString(FILE* f, const std::string& value) {
    uint32_t len = value.size();
    return fwrite(&len, 4, 1, f) == 1 && fwrite(value.data(), 1, len, f) == len;
}
bool ReadString(FILE* f, std::string& value, uint32_t limit) {
    uint32_t len;
    if (fread(&len, 4, 1, f) != 1 || len > limit) return false;
    value.resize(len);
    return fread(value.data(), 1, len, f) == len;
}
}
bool Store::Save(const std::string& path) const {
    std::string temporary = path + ".XXXXXX";
    int fd = mkstemp(temporary.data());
    if (fd < 0) return false;
    FILE* f = fdopen(fd, "wb");
    if (!f) { close(fd); unlink(temporary.c_str()); return false; }
    bool ok = fwrite("RSGREC3", 1, 7, f) == 7 && WriteString(f, fAccount);
    uint32_t count = fItems.size();
    ok = ok && fwrite(&count, 4, 1, f) == 1;
    for (const auto& item : Recent()) {
        uint8_t group = item.group;
        uint8_t favorite = item.favorite;
        ok = ok && WriteString(f, item.jid) && WriteString(f, item.name) && WriteString(f, item.preview)
            && WriteString(f, item.avatar) && WriteString(f, item.lastMessageId)
            && fwrite(&item.activity, 8, 1, f) == 1 && fwrite(&item.unread, 4, 1, f) == 1 && fwrite(&group, 1, 1, f) == 1
            && fwrite(&favorite, 1, 1, f) == 1 && fwrite(&item.hiddenAt, 8, 1, f) == 1;
    }
    ok = fflush(f) == 0 && ok;
    ok = fsync(fd) == 0 && ok;
    ok = fclose(f) == 0 && ok;
    if (ok) ok = rename(temporary.c_str(), path.c_str()) == 0;
    if (!ok) unlink(temporary.c_str());
    return ok;
}
bool Store::Load(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    struct stat statbuf;
    char magic[7]; std::string account; uint32_t count = 0;
    bool ok = fstat(fileno(f), &statbuf) == 0 && statbuf.st_size <= 600000 &&
        fread(magic, 1, 7, f) == 7 && (memcmp(magic, "RSGREC1", 7) == 0 || memcmp(magic, "RSGREC2", 7) == 0 || memcmp(magic, "RSGREC3", 7) == 0) &&
        ReadString(f, account, 1024) && account == fAccount && fread(&count, 4, 1, f) == 1 && count <= kLimit;
    std::map<std::string, Summary> items;
    size_t favoriteCount = 0;
    for (uint32_t i = 0; ok && i < count; ++i) {
        Summary item; uint8_t group = 0, favorite = 0;
        ok = ReadString(f, item.jid, 1024) && ReadString(f, item.name, 256) && ReadString(f, item.preview, 256)
            && ReadString(f, item.avatar, 256) && ReadString(f, item.lastMessageId, 256)
            && fread(&item.activity, 8, 1, f) == 1 && fread(&item.unread, 4, 1, f) == 1 && fread(&group, 1, 1, f) == 1;
        if (ok && magic[6] >= '2') ok = fread(&favorite, 1, 1, f) == 1;
        if (ok && magic[6] == '3') ok = fread(&item.hiddenAt, 8, 1, f) == 1;
        ok = ok && item.hiddenAt >= 0 && !item.jid.empty() && (item.activity > 0 || (favorite == 1 && item.activity == 0))
            && item.unread <= 9999 && group <= 1 && favorite <= 1 && !items.count(item.jid);
        item.group = group;
        item.favorite = favorite;
        favoriteCount += item.favorite;
        ok = ok && favoriteCount <= 64;
        item.name = FriendlyName(item.jid, item.name, item.group);
        items[item.jid] = item;
    }
    ok = ok && fgetc(f) == EOF;
    fclose(f);
    if (ok) fItems = std::move(items);
    return ok;
}
}
