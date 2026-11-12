// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "MediaLibrary.h"
#include "AnimatedImage.h"
#include "ImageSafety.h"
#include <openssl/sha.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <set>
#include <fstream>
#include <mutex>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Media {
namespace {
std::string Hash(const void* bytes,size_t size) {
    unsigned char hash[SHA256_DIGEST_LENGTH]; SHA256(static_cast<const unsigned char*>(bytes),size,hash);
    std::string out; for(auto c:hash){char hex[3];snprintf(hex,3,"%02x",c);out+=hex;}return out;
}
bool Supported(const std::string& name,bool stickers) {
    auto ext=std::filesystem::path(name).extension().string();
    return stickers ? ext==".png"||ext==".webp"||ext==".gif" : ext==".gif"||ext==".webp";
}
}
Library::Library(std::string root,const std::string& account,std::string builtinDirectory) : fBuiltin(std::move(builtinDirectory)) {
    if(!account.empty())fRoot=root+"/media-"+Hash(account.data(),account.size());
}
std::vector<Entry> Library::List(bool stickers) const {
    std::vector<Entry> entries; if(fRoot.empty())return entries;
    std::error_code error;
    for(const auto& file:std::filesystem::directory_iterator(fRoot+(stickers?"/stickers":"/gifs"),error)) {
        if(entries.size()>=(stickers?512:48))break;
        auto name=file.path().filename().string();
        if(name.empty()||name[0]=='.'||!Supported(name,stickers)||file.is_symlink(error)||!file.is_regular_file(error))continue;
        auto stem=file.path().stem().string();auto dash=stem.find_last_of('-');
        if(dash!=std::string::npos)stem.resize(dash);
        entries.push_back({stem + file.path().extension().string(),file.path().string()});
    }
    if(!stickers && !fBuiltin.empty())for(const auto& file:std::filesystem::directory_iterator(fBuiltin,error)) {
        if(entries.size()>=64)break;
        if(file.path().extension()==".gif" && file.is_regular_file(error) && !file.is_symlink(error))entries.push_back({file.path().stem().string(),file.path().string()});
    }
    std::sort(entries.begin(),entries.end(),[](const auto&a,const auto&b){return a.name==b.name?a.path<b.path:a.name<b.name;});
    return entries;
}
std::string Library::Import(const std::string& source,bool stickers,std::string& error) const {
    error.clear();if(fRoot.empty()){error="Sign in before importing media.";return {};}
    int fd=open(source.c_str(),O_RDONLY|O_NONBLOCK);struct stat st{};
    if(fd<0||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<=0||st.st_size>off_t(kFileLimit)) {
        if(fd>=0)close(fd);
        error="Choose a readable image no larger than 12 MiB.";return {};
    }
    std::vector<uint8_t> bytes(st.st_size);size_t done=0;
    while(done<bytes.size()){ssize_t n=read(fd,bytes.data()+done,bytes.size()-done);if(n<=0)break;done+=n;}close(fd);
    if(done!=bytes.size()){error="Could not read the complete file.";return {};}
    std::string ext;
    if(bytes.size()>=6&&(!memcmp(bytes.data(),"GIF87a",6)||!memcmp(bytes.data(),"GIF89a",6)))ext=".gif";
    else if(bytes.size()>=12&&!memcmp(bytes.data(),"RIFF",4)&&!memcmp(bytes.data()+8,"WEBP",4))ext=".webp";
    else if(stickers&&bytes.size()>=8&&!memcmp(bytes.data(),"\x89PNG\r\n\x1a\n",8))ext=".png";
    if(ext.empty()||(ext==".png"?!Upload::InspectPreviewImage(bytes.data(),bytes.size()):DecodeAnimation(bytes.data(),bytes.size()).frames.empty())) {
        error="This image cannot be used. Choose a bounded GIF/WebP animation or a PNG sticker.";return {};
    }
    auto directory=fRoot+(stickers?"/stickers":"/gifs");std::error_code ec;
    std::filesystem::create_directories(directory,ec);
    if(ec){error="Could not create the media library.";return {};}
    chmod(fRoot.c_str(),0700);chmod(directory.c_str(),0700);
    auto suffix="-"+Hash(bytes.data(),bytes.size()).substr(0,32)+ext;
    auto entries=List(stickers);
    entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const auto& e){return !IsImported(e.path);}),entries.end());
    for(const auto& e:entries)if(e.path.ends_with(suffix))return e.path;
    uint64_t total=bytes.size();for(const auto&e:entries){auto n=std::filesystem::file_size(e.path,ec);if(!ec)total+=n;}
    if(entries.size()>=(stickers?512:48)||total>128*1024*1024){error="The local media library is full.";return {};}
    std::string name=std::filesystem::path(source).stem().string();
    for(char& c:name)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='-'&&c!='_')c='_';
    if(name.empty())name="image";
    if(name.size()>40)name.resize(40);
    auto path=directory+"/"+name+suffix;std::string temporary=directory+"/.import-XXXXXX";
    fd=mkstemp(temporary.data());if(fd<0){error="Could not save the imported image.";return {};}
    done=0;while(done<bytes.size()){ssize_t n=write(fd,bytes.data()+done,bytes.size()-done);if(n<=0)break;done+=n;}
    bool ok=done==bytes.size()&&fsync(fd)==0;ok=close(fd)==0&&ok;
    // BFS has no hard links. Publish the complete private file atomically.
    // The destination includes its content digest; concurrent identical
    // imports may replace only an identical library copy, never the source.
    if(ok)ok=rename(temporary.c_str(),path.c_str())==0;
    unlink(temporary.c_str());
    if(!ok){error="Could not save the imported image.";return {};}
    return path;
}
ImportResult Library::ImportSelection(const std::vector<std::string>& sources, bool stickers,
    const std::atomic_bool* cancelled) const {
    ImportResult result;
    std::set<std::string> files, imported;
    auto fail = [&](const std::string& reason) { ++result.failed; if(result.error.empty()) result.error=reason; };
    if (sources.size()>512) { fail("Select at most 512 files or folders."); return result; }
    for (const auto& source : sources) {
        if (cancelled && *cancelled) { result.error="Import cancelled. Completed copies are kept."; return result; }
        std::error_code error;
        auto status=std::filesystem::symlink_status(source,error);
        if(error || std::filesystem::is_symlink(status)) { fail("A selected source is missing or is a symbolic link."); continue; }
        if(std::filesystem::is_directory(status)) {
            std::filesystem::directory_iterator iterator(source,error), end;
            if(error) { fail("A selected folder could not be opened."); continue; }
            for(; iterator!=end; iterator.increment(error)) {
                if(error) break;
                const auto& entry=*iterator;
                auto name=entry.path().filename().string();
                auto itemStatus=entry.symlink_status(error);
                if(error) break;
                if(!name.empty() && name[0]!='.' && Supported(name,stickers)
                    && std::filesystem::is_regular_file(itemStatus)) files.insert(entry.path().string());
                if(files.size()>512) break;
            }
            if(error) fail("A selected folder could not be read completely.");
        } else if(std::filesystem::is_regular_file(status)) files.insert(source);
        else fail("Choose ordinary files or folders.");
        if(files.size()>512) { fail("Select at most 512 images at once."); return result; }
    }
    if(files.empty()) { if(result.error.empty()) fail("No supported images found. Select a folder containing PNG, WebP or GIF files."); return result; }
    for(const auto& file : files) {
        if(cancelled && *cancelled) { result.error="Import cancelled. Completed copies are kept."; break; }
        std::string error;
        auto path=Import(file,stickers,error);
        if(path.empty()) fail(error);
        else if(imported.insert(path).second) result.paths.push_back(path);
    }
    return result;
}
bool Library::IsImported(const std::string& path) const {
    auto parent=std::filesystem::path(path).parent_path().string();
    return !fRoot.empty() && (parent==fRoot+"/gifs" || parent==fRoot+"/stickers");
}
bool Library::Remove(const std::string& path, bool stickers) const {
    // Only the library copy can be removed, never an arbitrary source path.
    for (const auto& entry : List(stickers))
        if (entry.path == path) return unlink(path.c_str()) == 0;
    return false;
}
namespace {
std::mutex favoritesLock;
bool ReadFavorites(const std::string& root, std::set<std::string>& values) {
    if(root.empty())return false;
    const auto path=root+"/emoji-favorites.txt";
    struct stat st{};
    if(lstat(path.c_str(),&st)!=0)return errno==ENOENT;
    if(!S_ISREG(st.st_mode)||st.st_size>32768)return false;
    std::ifstream file(path);if(!file)return false;
    std::string line;
    while(std::getline(file,line)) {
        if(line.size()>128||values.size()>=256)return false;
        if(std::any_of(Emojis().begin(),Emojis().end(),[&](const auto& e){return line==e.text;}))values.insert(line);
    }
    return !file.bad();
}
}
std::vector<std::string> Library::FavoriteEmojis() const {
    std::lock_guard<std::mutex> guard(favoritesLock);
    std::set<std::string> values;if(!ReadFavorites(fRoot,values))return {};
    return {values.begin(),values.end()};
}
bool Library::SetFavoriteEmoji(const std::string& emoji,bool favorite,std::string& error) const {
    std::lock_guard<std::mutex> guard(favoritesLock);error.clear();
    if(fRoot.empty()||!std::any_of(Emojis().begin(),Emojis().end(),[&](const auto& e){return emoji==e.text;})) {
        error="Choose an emoji while signed in.";return false;
    }
    std::set<std::string> values;
    if(!ReadFavorites(fRoot,values)){error="Could not read the saved favorites. They have not been changed.";return false;}
    if(favorite)values.insert(emoji);else values.erase(emoji);
    if(values.size()>256){error="Choose up to 256 favorite emojis.";return false;}
    std::error_code ec;std::filesystem::create_directories(fRoot,ec);
    if(ec){error="Could not create the favorites folder.";return false;}
    chmod(fRoot.c_str(),0700);
    std::string data;for(const auto& value:values)data+=value+"\n";
    std::string temporary=fRoot+"/.emoji-XXXXXX";int fd=mkstemp(temporary.data());
    if(fd<0){error="Could not save favorites.";return false;}
    size_t done=0;while(done<data.size()){ssize_t n=write(fd,data.data()+done,data.size()-done);if(n<=0)break;done+=n;}
    bool ok=done==data.size()&&fsync(fd)==0;ok=close(fd)==0&&ok;
    if(ok)ok=rename(temporary.c_str(),(fRoot+"/emoji-favorites.txt").c_str())==0;
    unlink(temporary.c_str());if(!ok)error="Could not save favorites. Previous favorites are kept.";
    return ok;
}
const std::vector<Emoji>& Emojis() {
    static const std::vector<Emoji> all={
#include "EmojiData.inc"
    };return all;
}
}
