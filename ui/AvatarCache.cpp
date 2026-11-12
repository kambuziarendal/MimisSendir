// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "AvatarCache.h"
#include "AvatarImage.h"
#include "jabber/BlabberSettings.h"
#include "jabber/JabberSpeak.h"
#include "support/ImageSafety.h"
#include <DataIO.h>
#include <File.h>
#include <FindDirectory.h>
#include <Looper.h>
#include <Path.h>
#include <String.h>
#include <TranslationUtils.h>
#include <openssl/sha.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <map>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
namespace Avatars {
namespace {
constexpr int kSide=96, kBytes=kSide*kSide*4;
std::atomic<unsigned> sPending{0};
std::string Hash(const std::string& s) {
    unsigned char hash[32]; SHA256(reinterpret_cast<const unsigned char*>(s.data()),s.size(),hash);
    std::string out;for(auto b:hash){char hex[3];snprintf(hex,3,"%02x",b);out+=hex;}return out;
}
class Cache final:public BLooper {
public:
    Cache():BLooper("avatar-cache"){Run();}
    std::shared_ptr<Image> Get(const std::string& account,const std::string& jid){
        if(account.empty()||jid.empty()||jid.size()>2048)return {};
        std::lock_guard<std::mutex> lock(fMutex);auto key=Hash(account)+"/"+Hash(jid);
        auto found=fImages.find(key);if(found!=fImages.end())return found->second;
        if(fImages.size()>=512)return {};
        auto image=std::make_shared<Image>();fImages[key]=image;fIdentities[key]={account,jid};
        BMessage request('avLd');request.AddString("account",account.c_str());request.AddString("jid",jid.c_str());PostMessage(&request);return image;
    }
    void MessageReceived(BMessage* message)override{
        struct Pending { bool incoming;~Pending(){if(incoming)--sPending;} } pending{message->what=='avIn'};
        std::string account=message->GetString("account",""),jid=message->GetString("jid","");
        if (message->what == 'avRf') {
            std::vector<std::string> peers;
            {
                std::lock_guard<std::mutex> lock(fMutex);
                for (const auto& [key, identity] : fIdentities)
                    if (identity.first == account && !fAliasSources.count(key)) peers.push_back(identity.second);
            }
            for (const auto& peer : peers) JabberSpeak::Instance()->QueueAvatar(account, peer);
            return;
        }
        if(account.empty()||jid.empty())return;
        auto key=Hash(account)+"/"+Hash(jid);auto image=Get(account,jid);if(!image)return;
        if(message->what=='avAl'){
            std::string real=message->GetString("real","");if(real.empty()||real==jid)return;
            auto source=Get(account,real);if(!source)return;
            auto sourceKey=Hash(account)+"/"+Hash(real);
            auto old=fAliasSources.find(key);
            const bool changed=old!=fAliasSources.end()&&old->second!=sourceKey;
            if(changed)fAliases[old->second].erase(key);
            fAliases[sourceKey].insert(key);fAliasSources[key]=sourceKey;
            if(auto bitmap=source->Get())Publish(key,image,bitmap,true);
            else if(changed)Publish(key,image,{},true);
            return;
        }
        if(message->what=='avLd'){
            auto path=Path(key);BFile input(path.c_str(),B_READ_ONLY);off_t size;
            if(input.GetSize(&size)==B_OK&&size==kBytes){
                auto bitmap=std::make_shared<BBitmap>(BRect(0,0,kSide-1,kSide-1),B_RGBA32);
                if(bitmap->InitCheck()==B_OK&&input.Read(bitmap->Bits(),kBytes)==kBytes)Publish(key,image,bitmap,false);
            }
            JabberSpeak::Instance()->QueueAvatar(account,jid);return;
        }
        if(message->what=='avIn'){
            if(fAliasSources.count(key))return;
            if(message->GetBool("remove",false)){Publish(key,image,{},true);return;}
            const void* bytes=nullptr;ssize_t size=0;
            if(message->FindData("image",B_RAW_TYPE,&bytes,&size)!=B_OK)return;
            if(size==0){Publish(key,image,{},true);return;}
            if(auto bitmap=DecodeAvatar(bytes,size))Publish(key,image,bitmap,true);
        }
    }
private:
    std::string Path(const std::string& key){BPath root;if(find_directory(B_USER_CACHE_DIRECTORY,&root)!=B_OK)return {};return std::string(root.Path())+"/Renga/avatars-v2/"+key;}
    void Save(const std::string& key,const std::shared_ptr<BBitmap>& bitmap){
        auto path=Path(key);if(path.empty())return;
        if(!bitmap){unlink(path.c_str());return;}
        std::error_code ec;auto parent=std::filesystem::path(path).parent_path();std::filesystem::create_directories(parent,ec);if(ec)return;
        chmod(parent.c_str(),0700);chmod(parent.parent_path().c_str(),0700);
        // Bound disk cache to 512 small thumbnails per account.
        size_t count=0;for(const auto& file:std::filesystem::directory_iterator(parent,ec)){(void)file;if(++count>=512&&!std::filesystem::exists(path))return;}
        auto temporary=path+".tmp-XXXXXX";int fd=mkstemp(temporary.data());if(fd<0)return;
        const auto wrote=write(fd,bitmap->Bits(),kBytes);bool ok=wrote==kBytes&&fsync(fd)==0;close(fd);
        if(ok)ok=rename(temporary.c_str(),path.c_str())==0;
        if(!ok)unlink(temporary.c_str());
    }
    void Publish(const std::string& key,const std::shared_ptr<Image>& image,const std::shared_ptr<BBitmap>& bitmap,bool save){
        {std::lock_guard<std::mutex> lock(image->mutex);image->bitmap=bitmap;}
        if(save)Save(key,bitmap);
        for(const auto& alias:fAliases[key]){
            std::shared_ptr<Image> target;{std::lock_guard<std::mutex> lock(fMutex);auto it=fImages.find(alias);if(it!=fImages.end())target=it->second;}
            if(target){{std::lock_guard<std::mutex> lock(target->mutex);target->bitmap=bitmap;}Save(alias,bitmap);}
        }
        SendNotices(kChanged);
    }
    std::map<std::string,std::pair<std::string,std::string>> fIdentities;
    std::mutex fMutex;std::map<std::string,std::shared_ptr<Image>> fImages;std::map<std::string,std::set<std::string>> fAliases;std::map<std::string,std::string> fAliasSources;
};
Cache& Instance(){static auto* cache=new Cache;return *cache;}
}
std::string Account(){return gloox::JID(BlabberSettings::Instance()->Data("last-login").value_or("")).bare();}
std::shared_ptr<Image> Get(const std::string& account,const std::string& jid){return Instance().Get(account,jid);}
void Receive(const std::string& account,const std::string& jid,const std::string& bytes){
    if(bytes.size()>1024*1024||account.empty()||jid.empty())return;
    if(sPending.fetch_add(1)>=16){--sPending;return;}
    BMessage message('avIn');message.AddString("account",account.c_str());message.AddString("jid",jid.c_str());if(bytes.empty())message.AddBool("remove",true);else message.AddData("image",B_RAW_TYPE,bytes.data(),bytes.size());if(Instance().PostMessage(&message)!=B_OK)--sPending;
}
void Alias(const std::string& account,const std::string& occupant,const std::string& real){
    if(account.empty()||occupant.empty()||real.empty())return;
    BMessage message('avAl');message.AddString("account",account.c_str());message.AddString("jid",occupant.c_str());message.AddString("real",gloox::JID(real).bare().c_str());Instance().PostMessage(&message);
}
void RefreshAccount(const std::string& account) {
    if(account.empty())return;
    BMessage message('avRf');message.AddString("account",account.c_str());Instance().PostMessage(&message);
}
void Watch(BHandler* handler){Instance().StartWatching(handler,kChanged);}
void Unwatch(BHandler* handler){Instance().StopWatching(handler,kChanged);}

}
