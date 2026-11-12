#include "network/SignalStickers.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
using namespace SignalStickers;
using namespace std::string_literals;
int checks=0;
void Check(bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}}
Bytes Hex(const std::string& text){Bytes b;for(size_t i=0;i<text.size();i+=2)b.push_back(std::stoi(text.substr(i,2),nullptr,16));return b;}
int main(){
    const std::string id(32,'a'),key(64,'0');
    auto link=ParseLink("sgnl://addstickers/?pack_id="+id+"&pack_key="+key);
    Check(bool(link),"standard browser link accepted");
    for(auto prefix:{"sgnl://addstickers?","https://signal.art/addstickers/#","renga-stickers://addstickers/?"})
        Check(bool(ParseLink(std::string(prefix)+"pack_key="+key+"&pack_id="+id)),"alternate link and field ordering");
    for(auto bad:{"http://signal.art/addstickers/#","https://signal.art.evil/addstickers/#","file:///tmp/","sgnl://other/?"})
        Check(!ParseLink(std::string(bad)+"pack_id="+id+"&pack_key="+key),"unrecognized origin rejected");
    auto url="sgnl://addstickers/?pack_id="+id+"&pack_key="+key;
    Check(!ParseLink(url+"&pack_id="+id),"duplicate key rejected");
    Check(!ParseLink(url+"&other=1"),"extra fields rejected");
    Check(!ParseLink(url.substr(0,url.size()-1)),"short key rejected");
    const auto fileText = "RENGA-STICKER-LINK 1\n" + url + "\n";
    Check(bool(ParseLinkFile(fileText)), "native pack file accepted");
    Check(bool(ParseLinkFile(fileText.substr(0, fileText.size()-1))), "pack file without final newline accepted");
    for (const auto& bad : {url, "RENGA-STICKER-LINK 2\n" + url,
         fileText + url, fileText + "\0"s, std::string(1101, 'x'),
         "RENGA-STICKER-LINK 1\nhttps://evil.test/" + url,
         "RENGA-STICKER-LINK 1\n" + url + "&command=launch"})
        Check(!ParseLinkFile(bad), "malformed pack file rejected");
    std::string fileError;
    char directory[] = "/tmp/renga-link-test-XXXXXX";
    Check(mkdtemp(directory) != nullptr, "temporary test directory");
    std::string path = std::string(directory) + "/pack.renga-sticker";
    {std::ofstream out(path); out << fileText;}
    Check(bool(ReadLinkFile(path, fileError)) && fileError.empty(), "pack file read and validated");
    Check(!ReadLinkFile(path + ".missing", fileError) && !fileError.empty(), "missing file explains failure");
    Check(!ReadLinkFile(directory, fileError), "directory refused");
    std::string alias = path + ".symlink";
    Check(symlink(path.c_str(), alias.c_str()) == 0, "test symlink created");
    Check(!ReadLinkFile(alias, fileError), "symlink refused");
    unlink(alias.c_str());
    Check(mkfifo(alias.c_str(), 0600) == 0, "test FIFO created");
    Check(!ReadLinkFile(alias, fileError), "FIFO refused without blocking");
    unlink(alias.c_str());
    {std::ofstream out(path); out << std::string(1101, 'x');}
    Check(!ReadLinkFile(path, fileError), "oversized file refused");
    unlink(path.c_str()); rmdir(directory);
    std::string error;auto encrypted=Hex("00000000000000000000000000000000d6b8ea2867f18e8737528fd1cf19c96e660b17fcb36ff6c6f7614b30eb5a7c2d0f4fbf673176407ec90659979c7a9f2a53c2f8304c497e80ae6692be69d6ed6a");
    auto data=Decrypt(encrypted,key,error);Check(error.empty()&&!data.empty(),"independent Python crypto fixture decrypts");
    auto pack=ParseManifest(data,*link,error);Check(pack&&pack->title=="Test"&&pack->author=="Kambuzi"&&pack->stickers.size()==2&&pack->stickers[0].id==0&&pack->stickers[1].id==1,"protobuf default zero ID and metadata");
    auto tampered=encrypted;tampered[17]^=1;Check(Decrypt(tampered,key,error).empty()&&!error.empty(),"tampered ciphertext rejected");
    tampered=encrypted;tampered.back()^=1;Check(Decrypt(tampered,key,error).empty()&&!error.empty(),"tampered MAC rejected");
    Check(Decrypt(encrypted,std::string(64,'1'),error).empty(),"wrong key rejected");
    encrypted.pop_back();Check(Decrypt(encrypted,key,error).empty(),"truncation rejected");
    auto duplicate=data;duplicate.insert(duplicate.end(),{0x22,0});Check(!ParseManifest(duplicate,*link,error),"duplicate default ID rejected");
    Check(!ParseManifest(Bytes{0x22,0xff},*link,error),"truncated varint rejected");
    Check(!ParseManifest(Bytes{0},*link,error),"zero field rejected");
    Check(!ParseManifest(Bytes{0x0a,4,'T'},*link,error),"truncated text rejected");
    std::atomic_bool cancelled{false};int fetches=0;
    auto fetch=[&](const std::string&,size_t,const std::atomic_bool&,std::string& err){++fetches;err="cancelled";return Bytes{};};
    Check(!Load(*link,cancelled,error,fetch)&&fetches==1,"transport failure not retried");
    fetches=0;Check(Download(*pack,123,cancelled,error,fetch).empty()&&fetches==0,"unknown sticker does not fetch");
    Check(FetchHTTPS("https://other.example/stickers/x",100,cancelled,error).empty(),"network host restricted");
    std::cout<<checks<<" Signal sticker checks passed\n";
}
