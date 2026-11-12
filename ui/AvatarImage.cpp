// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "AvatarImage.h"
#include "support/ImageSafety.h"
#include <DataIO.h>
#include <TranslationUtils.h>
#include <algorithm>
#include <cmath>
namespace Avatars {
constexpr int kSide=96;
std::shared_ptr<BBitmap> DecodeAvatar(const void* bytes,size_t size) {
    if(size==0||size>1024*1024)return {};
    auto dimensions=Upload::InspectPreviewImage(static_cast<const unsigned char*>(bytes),size);
    if(!dimensions||dimensions->width>1024||dimensions->height>1024)return {};
    BMemoryIO input(bytes,size);std::unique_ptr<BBitmap> original(BTranslationUtils::GetBitmap(&input));
    if(!original||original->InitCheck()!=B_OK||(original->ColorSpace()!=B_RGBA32&&original->ColorSpace()!=B_RGB32))return {};
    auto out=std::make_shared<BBitmap>(BRect(0,0,kSide-1,kSide-1),B_RGBA32);
    if(out->InitCheck()!=B_OK)return {};
    const int width=original->Bounds().IntegerWidth()+1,height=original->Bounds().IntegerHeight()+1;
    const float crop=std::min(width,height),left=(width-crop)/2,top=(height-crop)/2;
    auto* source=static_cast<const uint8*>(original->Bits());auto* target=static_cast<uint8*>(out->Bits());
    for(int y=0;y<kSide;++y)for(int x=0;x<kSide;++x){
        float sx=std::clamp(left+(x+.5f)*crop/kSide-.5f,0.f,float(width-1));
        float sy=std::clamp(top+(y+.5f)*crop/kSide-.5f,0.f,float(height-1));
        int x0=int(sx),y0=int(sy),x1=std::min(x0+1,width-1),y1=std::min(y0+1,height-1);float dx=sx-x0,dy=sy-y0;
        auto* pixel=target+y*out->BytesPerRow()+x*4;
        for(int c=0;c<4;++c){
            float a=source[y0*original->BytesPerRow()+x0*4+c]*(1-dx)+source[y0*original->BytesPerRow()+x1*4+c]*dx;
            float b=source[y1*original->BytesPerRow()+x0*4+c]*(1-dx)+source[y1*original->BytesPerRow()+x1*4+c]*dx;
            pixel[c]=uint8(a*(1-dy)+b*dy);
        }
        if(original->ColorSpace()==B_RGB32)pixel[3]=255;
        float distance=std::hypot(x+0.5f-kSide/2.f,y+0.5f-kSide/2.f);
        pixel[3]=uint8(pixel[3]*std::clamp(kSide/2.f-distance,0.f,1.f));
    }
    return out;
}
}
