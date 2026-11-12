// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "Giphy.h"
#include "support/ServiceKeys.h"
#include <private/shared/Json.h>
#include <FindDirectory.h>
#include <Path.h>
#include <curl/curl.h>
#include <fstream>
#include <mutex>
#include <map>
#include <unistd.h>
#include <sys/stat.h>
namespace Giphy {
namespace {
std::mutex leasesLock;
std::map<uint64_t,std::weak_ptr<Transfer>> leases;
std::atomic<uint64_t> nextToken{1};
struct Receive { std::string bytes; FILE* file=nullptr; size_t size=0, limit=0; const std::atomic_bool* cancelled; };
size_t Write(char* p,size_t s,size_t n,void* data) {
    auto& r=*static_cast<Receive*>(data);
    if(r.cancelled->load() || (s && n>SIZE_MAX/s))return 0;
    size_t count=s*n;if(count>r.limit-r.size)return 0;
    if(r.file) { if(fwrite(p,1,count,r.file)!=count)return 0; }
    else { try { r.bytes.append(p,count); } catch(...) { return 0; } }
    r.size+=count;return count;
}
int Progress(void* data,curl_off_t,curl_off_t,curl_off_t,curl_off_t) {return static_cast<Receive*>(data)->cancelled->load()?1:0;}
bool Get(const std::string& url,Receive& receive,std::string& error) {
    static std::once_flag once;static CURLcode init=CURLE_FAILED_INIT;
    std::call_once(once,[]{init=curl_global_init(CURL_GLOBAL_DEFAULT);});
    if(init!=CURLE_OK){error="Could not initialize HTTPS.";return false;}
    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl(curl_easy_init(),curl_easy_cleanup);
    if(!curl){error="Could not initialize HTTPS.";return false;}
    auto* c=curl.get();curl_easy_setopt(c,CURLOPT_URL,url.c_str());
    curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"https");curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);
    curl_easy_setopt(c,CURLOPT_PROXY,"");curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,10L);curl_easy_setopt(c,CURLOPT_TIMEOUT,30L);
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,Write);curl_easy_setopt(c,CURLOPT_WRITEDATA,&receive);
    curl_easy_setopt(c,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(c,CURLOPT_XFERINFOFUNCTION,Progress);
    curl_easy_setopt(c,CURLOPT_XFERINFODATA,&receive);curl_easy_setopt(c,CURLOPT_USERAGENT,"mimissendir/0.1.0-beta.11");
    auto result=curl_easy_perform(c);long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);
    if(result==CURLE_OK && status==200)return true;
    error=receive.cancelled->load()?"Cancelled.":status==429?"GIPHY search limit reached. Try again later.":status==401||status==403?"GIPHY rejected the API key.":"Could not load GIPHY. Check the connection or try another GIF.";
    return false;
}
// Haiku beta6 BJson emits CESU-8 for escaped UTF-16 surrogate pairs.
// Normalize those pairs for native UTF-8 labels without changing media URLs.
std::string Label(const char* text) {
    std::string input=text, out;
    for(size_t i=0;i<input.size();++i) {
        auto c=[&](size_t n){return static_cast<unsigned char>(input[i+n]);};
        if(i+5<input.size() && c(0)==0xed && (c(1)&0xf0)==0xa0 && (c(2)&0xc0)==0x80
            && c(3)==0xed && (c(4)&0xf0)==0xb0 && (c(5)&0xc0)==0x80) {
            unsigned high=((c(1)&0x0f)<<6)|(c(2)&0x3f);
            unsigned low=((c(4)&0x0f)<<6)|(c(5)&0x3f);
            unsigned cp=0x10000+(high<<10)+low;
            out+=char(0xf0|(cp>>18));out+=char(0x80|((cp>>12)&63));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));i+=5;
        } else if(c(0)>=32) out+=input[i];
    }
    return out;
}
std::string Escape(const std::string& s) {
    const char hex[]="0123456789ABCDEF";std::string out;
    for(unsigned char c:s) {if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=c;else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}
    return out;
}
}
bool MediaURL(const std::string& url) {
    if(url.size()>4096||url.find_first_of("\r\n\t ")!=std::string::npos||url.compare(0,8,"https://")!=0)return false;
    auto slash=url.find('/',8);if(slash==std::string::npos)return false;
    auto host=url.substr(8,slash-8);
    return host=="media.giphy.com"||host=="media0.giphy.com"||host=="media1.giphy.com"||host=="media2.giphy.com"||host=="media3.giphy.com"||host=="media4.giphy.com"||host=="i.giphy.com";
}
std::vector<Item> Parse(const std::string& json,std::string& error) {
    std::vector<Item> items;BMessage root,data,meta;
    if(json.size()>2*1024*1024||BJson::Parse(json.data(),json.size(),root)!=B_OK||root.FindMessage("data",&data)!=B_OK||root.FindMessage("meta",&meta)!=B_OK||meta.GetDouble("status",0)!=200){error="Invalid GIPHY response.";return items;}
    for(unsigned i=0;i<24;++i) {
        BMessage item,images,rendition,user;
        if(data.FindMessage(std::to_string(i).c_str(),&item)!=B_OK)break;
        Item value;value.title=Label(item.GetString("title","GIF"));if(value.title.empty())value.title="GIF";
        if(value.title.size()>256)value.title.resize(256);
        if(item.FindMessage("user",&user)==B_OK)value.creator=Label(user.GetString("display_name",user.GetString("username","")));
        if(value.creator.size()>128)value.creator.resize(128);
        if(item.FindMessage("images",&images)==B_OK && images.FindMessage("fixed_height",&rendition)==B_OK)value.url=rendition.GetString("url","");
        if(!MediaURL(value.url))value.url.clear(); // Keep order and show unavailable entries.
        items.push_back(std::move(value));
    }
    return items;
}
std::vector<Item> Search(const std::string& query,unsigned offset,const std::atomic_bool& cancelled,std::string& error,const std::string& keyId) {
    if(query.size()>128||offset>480){error="Search is too long or the last page was reached.";return {};}
    if (cancelled.load()) { error="Cancelled."; return {}; }
    const auto key = ServiceKeys::Load(keyId, error);
    if (key.empty() || cancelled.load()) return {};
    std::string url="https://api.giphy.com/v1/gifs/"+std::string(query.empty()?"trending":"search")+"?api_key="+Escape(key)+"&limit=24&rating=g&lang=en&offset="+std::to_string(offset);
    if(!query.empty())url+="&q="+Escape(query);
    Receive r;r.limit=2*1024*1024;r.cancelled=&cancelled;
    if(!Get(url,r,error))return {};
    return Parse(r.bytes,error);
}
Transfer::~Transfer(){if(!path.empty())unlink(path.c_str());if(!directory.empty())rmdir(directory.c_str());}
std::shared_ptr<Transfer> Download(const std::string& url,const std::atomic_bool& cancelled,std::string& error) {
    if(!MediaURL(url)){error="This GIF is unavailable.";return {};}
    BPath base;if(find_directory(B_SYSTEM_TEMP_DIRECTORY,&base)!=B_OK){error="Temporary storage unavailable.";return {};}
    std::string pattern=std::string(base.Path())+"/renga-giphy-XXXXXX";
    std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    if(!mkdtemp(name.data())){error="Temporary storage unavailable.";return {};}
    auto result=std::make_shared<Transfer>();result->directory=name.data();result->path=result->directory+"/GIPHY.gif";
    FILE* file=fopen(result->path.c_str(),"w+b");
    if(!file){error="Temporary storage unavailable.";return {};}
    fchmod(fileno(file),0600);Receive r;r.file=file;r.limit=12*1024*1024;r.cancelled=&cancelled;
    bool ok=Get(url,r,error);if(fflush(file)!=0)ok=false;
    rewind(file);char magic[6]={};bool gif=fread(magic,1,6,file)==6&&(std::string(magic,6)=="GIF89a"||std::string(magic,6)=="GIF87a");fclose(file);
    if(!ok||!gif){if(error.empty())error="This GIF could not be loaded.";return {};}
    result->token=nextToken.fetch_add(1);std::lock_guard<std::mutex> lock(leasesLock);
    for(auto it=leases.begin();it!=leases.end();)if(it->second.expired())it=leases.erase(it);else++it;
    leases[result->token]=result;return result;
}
std::shared_ptr<Transfer> Acquire(uint64_t token) {std::lock_guard<std::mutex> lock(leasesLock);auto it=leases.find(token);return it==leases.end()?nullptr:it->second.lock();}
}
