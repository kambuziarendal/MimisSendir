// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "ColorEmoji.h"
#include <cstddef>
#include "ColorEmojiData.h"
#include <Application.h>
#include <Roster.h>
#include <File.h>
#include <Resources.h>
#include <algorithm>
#include <map>
#include <mutex>
#include <cstring>
#include <string_view>
#include <vector>

std::shared_ptr<BBitmap> ColorEmoji(const std::string& text) {
    static std::mutex lock;std::lock_guard<std::mutex> guard(lock);
    static std::map<std::string,std::weak_ptr<BBitmap>> cache;
    auto cached=cache.find(text);if(cached!=cache.end())if(auto image=cached->second.lock())return image;
    const auto* entry=std::lower_bound(std::begin(kEmojiPixels),std::end(kEmojiPixels),text,
        [](const auto& item,const auto& value){return std::string_view(item.text)<value;});
    if(entry==std::end(kEmojiPixels)||text!=entry->text)return {};
    // Keep only raw pixels across application lifetimes, never app_server objects.
    static std::vector<unsigned char> atlas;
    if(atlas.empty()&&be_app) {
        app_info info;
        if(be_app->GetAppInfo(&info)==B_OK) {
            BFile file(&info.ref,B_READ_ONLY);BResources resources(&file);
            size_t size=0;const auto* data=static_cast<const unsigned char*>(resources.LoadResource('EMJI',1,&size));
            if(data&&size==sizeof(kEmojiPixels)/sizeof(kEmojiPixels[0])*2304)atlas.assign(data,data+size);
        }
    }
    if(entry->offset+2304>atlas.size())return {};
    auto bitmap=std::make_shared<BBitmap>(BRect(0,0,23,23),B_RGBA32);
    if(bitmap->InitCheck()!=B_OK)return {};
    for(int y=0;y<24;++y)memcpy(static_cast<char*>(bitmap->Bits())+y*bitmap->BytesPerRow(),atlas.data()+entry->offset+y*96,96);
    cache[text]=bitmap;return bitmap;
}
