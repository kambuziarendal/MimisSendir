// Optional acceptance against explicitly supplied local sticker folders.
#include "support/MediaLibrary.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <iostream>
#include <unistd.h>
static std::string Read(const std::string& path) {std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
int main(int argc,char** argv) {
    if(argc<2)return 2;
    std::vector<std::string> folders;std::set<std::string> originals;
    for(int i=1;i<argc;++i) {
        folders.emplace_back(argv[i]);
        for(const auto& entry:std::filesystem::directory_iterator(argv[i])) {
            auto ext=entry.path().extension();
            if(ext==".webp"||ext==".gif"||ext==".png")originals.insert(Read(entry.path().string()));
        }
    }
    char root[]="/tmp/renga-pack-test-XXXXXX";if(!mkdtemp(root))return 2;
    Media::Library library(root,"pack-acceptance@example.test");
    const auto result=library.ImportSelection(folders,true);
    bool ok=result.failed==0&&result.error.empty()&&result.paths.size()==originals.size();
    for(const auto& path:result.paths)ok=originals.count(Read(path))&&ok;
    Media::Library reopened(root,"pack-acceptance@example.test"),other(root,"other@example.test");
    ok=ok&&reopened.List(true).size()==originals.size()&&other.List(true).empty();
    const auto repeat=reopened.ImportSelection(folders,true);
    ok=ok&&repeat.failed==0&&repeat.paths.size()==originals.size()&&reopened.List(true).size()==originals.size();
    std::cout<<result.paths.size()<<" unique images, "<<result.failed<<" rejected. Original bytes, restart, account isolation and repeat import: "<<(ok?"PASS":"FAIL")<<'\n';
    if(!result.error.empty())std::cerr<<result.error<<'\n';
    std::filesystem::remove_all(root);return ok?0:1;
}
