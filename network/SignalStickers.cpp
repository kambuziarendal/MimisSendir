// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "SignalStickers.h"
#include "HttpTransfer.h"
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/hmac.h>
#include <openssl/crypto.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <set>
#include <stdexcept>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace SignalStickers {
namespace {
bool Hex(const std::string& value,size_t size) {
    return value.size()==size && std::all_of(value.begin(),value.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');});
}
Bytes Unhex(const std::string& value) {
    auto nibble=[](char c){return c>='0'&&c<='9'?c-'0':(c|32)-'a'+10;};
    Bytes out;for(size_t i=0;i<value.size();i+=2)out.push_back(nibble(value[i])*16+nibble(value[i+1]));return out;
}
struct Reader {
    const Bytes& bytes;size_t at=0;
    uint64_t Number() {
        uint64_t value=0;
        for(int shift=0;shift<64;shift+=7) {
            if(at>=bytes.size())throw 1;
            auto c=bytes[at++];if(shift==63&&(c&254))throw 1;
            value|=uint64_t(c&127)<<shift;if(!(c&128))return value;
        }
        throw 1;
    }
    Bytes Data() {auto size=Number();if(size>bytes.size()-at)throw 1;Bytes result(bytes.begin()+at,bytes.begin()+at+size);at+=size;return result;}
    void Skip(unsigned wire) {if(wire==0)Number();else if(wire==2)Data();else if(wire==1||wire==5){size_t n=wire==1?8:4;if(n>bytes.size()-at)throw 1;at+=n;}else throw 1;}
};
std::string Text(const Bytes& bytes) {
    if(bytes.size()>4096)throw 1;
    std::string text(bytes.begin(),bytes.end());
    for(auto& c:text)if(static_cast<unsigned char>(c)<32)c=' ';
    return text;
}
std::string Base(const Link& link){return "https://cdn-ca.signal.org/stickers/"+link.id+"/";}
}
std::optional<Link> ParseLink(const std::string& text) {
    if(text.size()>1024)return {};
    const std::vector<std::string> prefixes={"sgnl://addstickers/?","sgnl://addstickers?","https://signal.art/addstickers/#","renga-stickers://addstickers/?"};
    std::string query;
    for(const auto& prefix:prefixes)if(text.compare(0,prefix.size(),prefix)==0){query=text.substr(prefix.size());break;}
    if(query.empty())return {};
    Link link;std::set<std::string> keys;
    for(size_t at=0;at<query.size();) {
        size_t end=query.find('&',at);if(end==std::string::npos)end=query.size();
        auto field=query.substr(at,end-at);auto equal=field.find('=');if(equal==std::string::npos)return {};
        auto key=field.substr(0,equal),value=field.substr(equal+1);
        if(!keys.insert(key).second)return {};
        if(key=="pack_id")link.id=value;else if(key=="pack_key")link.key=value;else return {};
        at=end+1;
    }
    if(!Hex(link.id,32)||!Hex(link.key,64))return {};
    std::transform(link.id.begin(),link.id.end(),link.id.begin(),::tolower);
    return link;
}
std::optional<Link> ParseLinkFile(const std::string& text) {
    const std::string header = "RENGA-STICKER-LINK 1\n";
    if (text.size() > 1100 || text.compare(0, header.size(), header) != 0)
        return {};
    auto url = text.substr(header.size());
    if (!url.empty() && url.back() == '\n') url.pop_back();
    return ParseLink(url);
}

std::optional<Link> ReadLinkFile(const std::string& path, std::string& error) {
    error = "Could not read this sticker link. Download the .renga-sticker file again and open it in MímisSendir.";
    // Do not block on special files or follow links supplied as pack files.
    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    if (fd < 0) return {};
    struct stat st;
    std::array<char, 1101> data{};
    ssize_t size = -1;
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0 && st.st_size <= 1100)
        size = read(fd, data.data(), data.size());
    close(fd);
    if (size <= 0 || size > 1100) return {};
    auto link = ParseLinkFile(std::string(data.data(), size));
    if (!link) error = "Invalid sticker link file. Only a valid Signal pack ID and key are accepted. Nothing was imported.";
    else error.clear();
    return link;
}

Bytes Decrypt(const Bytes& encrypted,const std::string& key,std::string& error) {
    error.clear();
    if(!Hex(key,64)||encrypted.size()<64||encrypted.size()>12*1024*1024||(encrypted.size()-48)%16){error="Invalid encrypted sticker.";return {};}
    auto master=Unhex(key);std::array<unsigned char,64> derived{};std::array<unsigned char,32> salt{},mac{};
    std::unique_ptr<EVP_PKEY_CTX,decltype(&EVP_PKEY_CTX_free)> hkdf(EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF,nullptr),EVP_PKEY_CTX_free);
    size_t derivedSize=derived.size();const unsigned char info[]="Sticker Pack";
    if(!hkdf||EVP_PKEY_derive_init(hkdf.get())<=0||EVP_PKEY_CTX_set_hkdf_md(hkdf.get(),EVP_sha256())<=0
        ||EVP_PKEY_CTX_set1_hkdf_salt(hkdf.get(),salt.data(),salt.size())<=0||EVP_PKEY_CTX_set1_hkdf_key(hkdf.get(),master.data(),master.size())<=0
        ||EVP_PKEY_CTX_add1_hkdf_info(hkdf.get(),info,sizeof(info)-1)<=0||EVP_PKEY_derive(hkdf.get(),derived.data(),&derivedSize)<=0) {error="Could not prepare sticker decryption.";return {};}
    unsigned int macSize=0;
    if(!HMAC(EVP_sha256(),derived.data()+32,32,encrypted.data(),encrypted.size()-32,mac.data(),&macSize)
        ||macSize!=32||CRYPTO_memcmp(mac.data(),encrypted.data()+encrypted.size()-32,32)) {error="Sticker authentication failed. Nothing was imported.";return {};}
    std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> cipher(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);
    Bytes result(encrypted.size());int first=0,last=0;
    if(!cipher||EVP_DecryptInit_ex(cipher.get(),EVP_aes_256_cbc(),nullptr,derived.data(),encrypted.data())!=1
        ||EVP_DecryptUpdate(cipher.get(),result.data(),&first,encrypted.data()+16,encrypted.size()-48)!=1
        ||EVP_DecryptFinal_ex(cipher.get(),result.data()+first,&last)!=1) {error="Sticker decryption failed.";return {};}
    result.resize(first+last);return result;
}
std::optional<Pack> ParseManifest(const Bytes& bytes,const Link& link,std::string& error) {
    error.clear();if(bytes.empty()||bytes.size()>1024*1024){error="Invalid sticker manifest size.";return {};}
    try {
        Pack pack;pack.link=link;Reader reader{bytes};std::set<uint32_t> ids;
        while(reader.at<bytes.size()) {
            auto tag=reader.Number();auto number=tag>>3,wire=tag&7;if(!number)throw 1;
            if((number==1||number==2)&&wire==2) {auto value=Text(reader.Data());if(number==1)pack.title=value;else pack.author=value;}
            else if(number==4&&wire==2) {
                auto data=reader.Data();Reader item{data};Sticker sticker{0,""};bool seen=false;
                while(item.at<data.size()) {
                    auto field=item.Number();auto n=field>>3,w=field&7;if(!n)throw 1;
                    if(n==1&&w==0){if(seen)throw 1;auto id=item.Number();if(id>65535)throw 1;sticker.id=id;seen=true;}
                    else if(n==2&&w==2)sticker.emoji=Text(item.Data());else item.Skip(w);
                }
                if(!ids.insert(sticker.id).second||pack.stickers.size()>=200)throw 1;
                pack.stickers.push_back(sticker);
            } else reader.Skip(wire);
        }
        if(pack.stickers.empty())throw 1;
        if(pack.title.empty())pack.title="Signal sticker pack";
        return pack;
    } catch(...) {error="Invalid or oversized sticker manifest.";return {};}
}
Bytes FetchHTTPS(const std::string& url,size_t limit,const std::atomic_bool& cancelled,std::string& error) {
    error.clear();
    if(url.rfind("https://cdn-ca.signal.org/stickers/",0)!=0){error="Unsupported sticker host.";return {};}
    std::unique_ptr<FILE,decltype(&fclose)> file(tmpfile(),fclose);
    if(!file){error="Could not create temporary download.";return {};}
    auto result=Upload::GetFile(url,"cdn-ca.signal.org",file.get(),cancelled,{}, {},limit);
    if(!result.success){error=result.error;return {};}
    if(result.bytes>limit){error="Sticker download is too large.";return {};}
    Bytes bytes(result.bytes);rewind(file.get());if(fread(bytes.data(),1,bytes.size(),file.get())!=bytes.size()){error="Could not read sticker download.";return {};}
    return bytes;
}
std::optional<Pack> Load(const Link& link,const std::atomic_bool& cancelled,std::string& error,Fetch fetch) {
    auto encrypted=fetch(Base(link)+"manifest.proto",1024*1024,cancelled,error);if(!error.empty())return {};
    auto data=Decrypt(encrypted,link.key,error);if(!error.empty())return {};
    return ParseManifest(data,link,error);
}
Bytes Download(const Pack& pack,uint32_t id,const std::atomic_bool& cancelled,std::string& error,Fetch fetch) {
    if(std::none_of(pack.stickers.begin(),pack.stickers.end(),[&](const auto& item){return item.id==id;})){error="Unknown sticker.";return {};}
    auto encrypted=fetch(Base(pack.link)+"full/"+std::to_string(id),12*1024*1024,cancelled,error);if(!error.empty())return {};
    return Decrypt(encrypted,pack.link.key,error);
}
}
