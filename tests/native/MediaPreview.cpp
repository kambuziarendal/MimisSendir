// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "ui/MediaPreview.h"
#include "ui/AvatarImage.h"
#include <fstream>
#include <iterator>
#include <thread>
#include <Application.h>
#include <cstdio>
#include <cstdlib>
static int checks=0;
static void Check(bool value,const char* name){if(!value){fprintf(stderr,"FAIL: %s\n",name);exit(1);}++checks;}
int main(){
    BApplication app("application/x-vnd.Kambuzi-MediaQualityTests");
    for(const char* path:{"tests/media/clarity.png","tests/media/clarity.jpg"}) {
        FILE* file=fopen(path,"rb");Check(file!=nullptr,"open synthetic high-resolution image");
        auto picker=LoadMediaPreview(file);auto chat=LoadMediaPreview(file,true);fclose(file);
        Check(picker.frames.size()==1&&chat.frames.size()==1,"static image decodes once");
        Check(picker.frames[0]->Bounds()==BRect(0,0,239,134),"picker keeps small preview");
        Check(chat.frames[0]->Bounds()==BRect(0,0,1023,575),"chat retains 1024px image with correct aspect");
    }
    FILE* file=fopen("resources/gifs/Enig.gif","rb");Check(file!=nullptr,"open animated image");
    auto animation=LoadMediaPreview(file,true);fclose(file);
    Check(animation.frames.size()==24&&animation.frames[0]->Bounds()==BRect(0,0,191,191),"chat animation retains original detail");
    file=fopen("tests/media/clarity-animation.gif","rb");Check(file!=nullptr,"open larger animation");
    auto largeAnimation=LoadMediaPreview(file,true);fclose(file);
    Check(largeAnimation.frames.size()==24,"all larger animation frames retained");
    const auto bounds=largeAnimation.frames[0]->Bounds();
    Check(bounds.Width()>239 && bounds.Width()<360 && bounds.Width()==bounds.Height(),"animation scales within memory budget while retaining more detail");
    size_t bytes=0;for(const auto& frame:largeAnimation.frames)bytes+=frame->BitsLength();
    Check(bytes<=8*1024*1024,"larger animation remains below 8 MiB frame budget");
    for(const char* path:{"tests/media/avatar.png","tests/media/avatar.jpg"}) {
        std::ifstream input(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(input)),{});
        auto avatar=Avatars::DecodeAvatar(bytes.data(),bytes.size());Check(bool(avatar),"PNG/JPEG profile image decodes");
        Check(avatar->Bounds()==BRect(0,0,95,95)&&avatar->BitsLength()==96*96*4,"avatar memory bounded to 96 pixels");
        auto* pixels=static_cast<unsigned char*>(avatar->Bits());Check(pixels[3]==0&&pixels[48*avatar->BytesPerRow()+48*4+3]==255,"avatar is circular with opaque center");
        Check(!Avatars::DecodeAvatar(bytes.data(),10),"truncated profile image rejected");
    }
    std::ifstream oversized("tests/media/clarity.png",std::ios::binary);std::string large((std::istreambuf_iterator<char>(oversized)),{});
    Check(!Avatars::DecodeAvatar(large.data(),large.size()),"oversized profile dimensions rejected before decoding");
    Check(!Avatars::DecodeAvatar(nullptr,0),"empty profile produces fallback");
    std::string excessive(1024*1024+1,'x');Check(!Avatars::DecodeAvatar(excessive.data(),excessive.size()),"oversized profile payload rejected");
    bool workerComplete=false;
    std::thread worker([&]{FILE* file=fopen("tests/media/avatar.png","rb");auto preview=LoadMediaPreview(file,true);fclose(file);workerComplete=preview.frames.size()==1;});worker.join();
    Check(workerComplete,"native image decoder completes on attachment worker thread");
    printf("%d native image-quality checks passed\n",checks);
}
