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
ChatHistory::ChatHistory(const std::string& settings,const std::string& account,const std::string& peer) {
    if(account.empty() || peer.empty()){fError="History needs an account and conversation.";return;}
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
    for(const auto& [key,m]:fRecords)if(id==m.GetString("alias",""))return key;
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
    if(existing.GetInt64("time",0)==timestamp)return false;
    // The archive's clock is authoritative for ordering. Preserve all local
    // content, aliases and reactions, including an optimistic pending change.
    BMessage merged(existing);merged.ReplaceInt64("time",timestamp);
    if(!Save(key,merged))return false;
    fRecords[key]=merged;return true;
}
void ChatHistory::Alias(const std::string& original,const std::string& stable) {
    auto key=Key(original);if(key.empty() || stable.empty())return;
    auto& m=fRecords[key];m.RemoveName("alias");m.AddString("alias",stable.c_str());Save(key,m);
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
