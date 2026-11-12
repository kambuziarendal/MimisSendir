// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "ChatHistory.h"
#include <File.h>
#include <openssl/sha.h>
#include <algorithm>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
std::string Digest(const std::string& text) {
    unsigned char bytes[32]; SHA256(reinterpret_cast<const unsigned char*>(text.data()),text.size(),bytes);
    std::string out; for(auto b:bytes){char s[3];snprintf(s,3,"%02x",b);out+=s;}return out;
}
}
ChatHistory::ChatHistory(const std::string& settings,const std::string& account,const std::string& peer,bool group) : fGroup(group) {
    if(account.empty() || peer.empty()){fError="History needs an account and conversation.";return;}
    fPeer=peer;
    auto parent=settings+"/history-v1/"+Digest(account);
    fRoot=parent+"/"+Digest(peer);std::error_code ec;
    std::filesystem::create_directories(fRoot,ec);
    if(ec){fRoot.clear();fError="Could not create private message history.";return;}
    chmod((settings+"/history-v1").c_str(),0700);chmod(parent.c_str(),0700);chmod(fRoot.c_str(),0700);
}
std::vector<BMessage> ChatHistory::Load() {
    std::vector<BMessage> records;if(fRoot.empty())return records;
    std::error_code ec;
    for(const auto& file:std::filesystem::directory_iterator(fRoot,ec)) {
        if(file.path().extension()!=".msg" || file.is_symlink(ec) || !file.is_regular_file(ec))continue;
        auto size=file.file_size(ec);if(ec || size>256*1024){fError="Some history records could not be read; originals were preserved.";continue;}
        BFile input(file.path().c_str(),B_READ_ONLY);BMessage m;
        if(m.Unflatten(&input)!=B_OK || m.what!='hRec' || !*m.GetString("id","") || m.GetInt64("time",0)<=0) {
            fError="Some history records could not be read; originals were preserved.";continue;
        }
        records.push_back(m);
        if(records.size()>1000){auto oldest=std::min_element(records.begin(),records.end(),[](const auto& a,const auto& b){return a.GetInt64("time",0)<b.GetInt64("time",0);});records.erase(oldest);}
    }
    std::stable_sort(records.begin(),records.end(),[](const auto& a,const auto& b){return a.GetInt64("time",0)<b.GetInt64("time",0);});
    if(records.size()>1000)records.erase(records.begin(),records.end()-1000);
    fRecords.clear();for(const auto& m:records)fRecords[m.GetString("id","")]=m;
    return records;
}
std::string ChatHistory::Key(const std::string& id) const {
    if(id.empty())return {};
    if(fRecords.count(id))return id;
    for(const auto& [key,m]:fRecords)if(id==m.GetString("alias","") || id==m.GetString("wire-id","") || id==m.GetString("correction-id",""))return key;
    return {};
}
bool ChatHistory::Contains(const std::string& id) const { return !Key(id).empty(); }
bool ChatHistory::Save(const std::string& key,const BMessage& record) {
    if(fRoot.empty())return false;
    auto path=fRoot+"/"+Digest(key)+".msg";auto temporary=fRoot+"/.pending-XXXXXX";
    int fd=mkstemp(temporary.data());if(fd<0){fError="Message history could not be saved.";return false;}close(fd);
    BFile output(temporary.c_str(),B_WRITE_ONLY|B_ERASE_FILE);
    bool ok=record.Flatten(&output)==B_OK && output.Sync()==B_OK;output.Unset();
    if(ok)ok=rename(temporary.c_str(),path.c_str())==0;
    if(!ok){unlink(temporary.c_str());fError="Message history could not be saved. Existing records were preserved.";}
    return ok;
}
bool ChatHistory::Put(BMessage record) {
    const std::string id=record.GetString("id","");
    if(id.empty() || Contains(id) || record.FlattenedSize()>256*1024)return false;
    record.what='hRec';fRecords[id]=record;return Save(id,record);
}
bool ChatHistory::MergeArchive(BMessage record) {
    const int64 timestamp=record.GetInt64("time",0);
    if(timestamp<=0)return false;
    auto key=Key(record.GetString("id",""));
    if(key.empty())return Put(record);
    const auto& existing=fRecords.at(key);
    const bool upgradeMarkable = record.GetBool("markable",false) && !existing.GetBool("markable",false);
    if(existing.GetInt64("time",0)==timestamp && !upgradeMarkable)return false;
    // The archive's clock is authoritative for ordering. Preserve all local
    // content, aliases and reactions, including an optimistic pending change.
    BMessage merged(existing);merged.ReplaceInt64("time",timestamp);
    if(upgradeMarkable) { merged.RemoveName("markable"); merged.AddBool("markable",true); }
    if(!Save(key,merged))return false;
    fRecords[key]=merged;return true;
}
void ChatHistory::Alias(const std::string& original,const std::string& stable) {
    auto key=Key(original);if(key.empty() || stable.empty())return;
    auto& m=fRecords[key];if(stable==m.GetString("alias",""))return;
    m.RemoveName("alias");m.AddString("alias",stable.c_str());Save(key,m);
}
void ChatHistory::Reactions(const std::string& id,const Chat::ReactionState& state) {
    auto key=Key(id);if(key.empty())return;auto& record=fRecords[key];record.RemoveName("reaction");
    for(const auto& [actor,info]:state.actors){BMessage m;m.AddString("actor",actor.c_str());m.AddString("name",info.name.c_str());for(const auto& e:info.values)m.AddString("emoji",e.c_str());record.AddMessage("reaction",&m);}
    Save(key,record);
}
Chat::ReactionState ChatHistory::Reactions(const std::string& id) const {
    Chat::ReactionState state;auto key=Key(id);if(key.empty())return state;
    BMessage m;for(int i=0;fRecords.at(key).FindMessage("reaction",i,&m)==B_OK;++i){std::vector<std::string> values;const char* e;for(int n=0;m.FindString("emoji",n,&e)==B_OK;++n)values.emplace_back(e);state.Replace(m.GetString("actor",""),m.GetString("name",""),values);}
    return state;
}

BMessage ChatHistory::Record(const std::string& id) const {
    auto key=Key(id);return key.empty()?BMessage():fRecords.at(key);
}
bool ChatHistory::Editable(const std::string& id, bool group) const {
    auto m=Record(id);const std::string original=m.GetString("id","");
    return m.GetBool("outgoing",false) && !original.empty() && original.rfind("local-",0)!=0
        && original.rfind("archive-",0)!=0 && (!m.HasString("wire-id") || *m.GetString("wire-id",""))
        && (!group || original.rfind("renga-",0)==0 || *m.GetString("wire-id",""))
        && m.GetInt32("delivery",1)!=0 && m.GetInt32("delivery",1)!=4
        && !*m.GetString("attachment","") && *m.GetString("body","");
}
bool ChatHistory::Correct(const std::string& target,const std::string& id,const std::string& body,
    const std::string& sender,bool outgoing,int64 timestamp) {
    auto key=Key(target);if(key.empty() || id.empty() || id.size()>1024 || body.empty() || body.size()>65536)return false;
    const auto& old=fRecords.at(key);
    auto author=[this](const std::string& value) {
        // A room occupant is a distinct author. Direct resources belong to one bare JID.
        return fGroup
            ? value : value.substr(0,value.find('/'));
    };
    if(sender.empty() || author(sender)!=author(old.GetString("sender",""))
        || outgoing!=old.GetBool("outgoing",false) || *old.GetString("attachment","")
        || id==old.GetString("correction-id","") || timestamp<=old.GetInt64("edit-time",0))return false;
    BMessage m(old);
    if(!m.HasString("original-body"))m.AddString("original-body",old.GetString("body",""));
    m.RemoveName("previous-body");m.AddString("previous-body",old.GetString("body",""));
    m.RemoveName("previous-edit-time");m.AddInt64("previous-edit-time",old.GetInt64("edit-time",0));
    m.RemoveName("previous-edit-state");m.AddString("previous-edit-state",old.GetString("edit-state",""));
    m.RemoveName("failed-edit-body");
    m.RemoveName("correction-id");m.AddString("correction-id",id.c_str());
    m.RemoveName("edit-time");m.AddInt64("edit-time",timestamp);
    m.RemoveName("edit-state");m.AddString("edit-state",outgoing?"unconfirmed":"confirmed");
    m.RemoveName("body");m.AddString("body",body.c_str());
    if(!Save(key,m))return false;
    fRecords[key]=m;return true;
}
bool ChatHistory::IsCorrection(const std::string& id) const {
    if(id.empty())return false;
    for(const auto& [key,m]:fRecords)if(id==m.GetString("correction-id",""))return true;
    return false;
}
bool ChatHistory::RejectCorrection(const std::string& id) {
    if(id.empty())return false;
    for(auto& [key,record]:fRecords)if(id==record.GetString("correction-id","")
        && std::string(record.GetString("edit-state",""))=="unconfirmed") {
        BMessage m(record);m.RemoveName("failed-edit-body");m.AddString("failed-edit-body",m.GetString("body",""));
        const std::string previous=m.GetString("previous-body","");m.ReplaceString("body",previous.c_str());
        m.ReplaceInt64("edit-time",m.GetInt64("previous-edit-time",0));
        const std::string priorState=m.GetString("previous-edit-state","");m.ReplaceString("edit-state",priorState.c_str());
        m.RemoveName("correction-id");
        if(!Save(key,m))return false;
        record=m;return true;
    }
    return false;
}
bool ChatHistory::ConfirmCorrection(const std::string& id) {
    if(id.empty())return false;
    for(auto& [key,record]:fRecords)if(id==record.GetString("correction-id","")) {
        BMessage m(record);m.ReplaceString("edit-state","confirmed");
        if(!Save(key,m))return false;
        record=m;return true;
    }
    return false;
}

void ChatHistory::Delivery(const std::string& id,int32 state) {
    auto key=Key(id);if(key.empty())return;
    BMessage m(fRecords.at(key));m.RemoveName("delivery");m.AddInt32("delivery",state);
    if(Save(key,m))fRecords[key]=m;
}

std::vector<BMessage> ChatHistory::Records() const {
    std::vector<BMessage> records;records.reserve(fRecords.size());
    for(const auto& [id,m]:fRecords){(void)id;records.push_back(m);}
    std::stable_sort(records.begin(),records.end(),[](const auto& a,const auto& b){return a.GetInt64("time",0)<b.GetInt64("time",0);});
    return records;
}

// Exact message correlation: a later read marker is not evidence that an
// offscreen earlier message was seen. Never infer "everyone read" in a group.
bool ChatHistory::MarkRead(const std::string& id, const std::string& actor) {
    auto key = Key(id);
    if (key.empty() || actor.empty() || actor.size() > 3071) return false;
    if (fGroup) {
        const auto prefix = fPeer + "/";
        if (actor.compare(0, prefix.size(), prefix) != 0 || actor.size() == prefix.size()) return false;
    } else if (actor != fPeer) return false;
    BMessage record(fRecords.at(key));
    if (!record.GetBool("outgoing", false)) return false;
    const char* reader;
    int count = 0;
    while (record.FindString("read-by", count, &reader) == B_OK) {
        if (actor == reader) return false;
        ++count;
    }
    // Bound metadata without dropping an existing reader on overflow.
    if (count >= 256) return false;
    record.AddString("read-by", actor.c_str());
    if (record.FlattenedSize() > 256 * 1024) return false;
    if (!Save(key, record)) return false;
    fRecords[key] = record;
    return true;
}

std::string ChatHistory::ReadLabel(const std::string& id) const {
    auto record = Record(id);
    if (!record.GetBool("outgoing", false)) return {};
    const char* actor;
    int count = 0;
    while (record.FindString("read-by", count, &actor) == B_OK) ++count;
    if (!count) return {};
    if (!fGroup) return "Read";
    return "Read by " + std::to_string(count) + (count == 1 ? " participant" : " participants");
}
