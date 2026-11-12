// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "AnimatedImage.h"
#include <gif_lib.h>
#include <webp/demux.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <memory>

namespace Media {
namespace {
constexpr uint64_t kCanvas = 1024 * 1024, kPixels = 16 * 1024 * 1024, kFrames = 96;
bool Bounded(uint64_t w, uint64_t h, uint64_t frames) {
    return w && h && w <= 4096 && h <= 4096 && w * h <= kCanvas
        && frames && frames <= kFrames && w * h * frames <= kPixels;
}
uint32_t LE16(const uint8_t* p) { return p[0] | uint32_t(p[1]) << 8; }
bool GifPreflight(const uint8_t* p, size_t size) {
    if (size < 13 || (memcmp(p,"GIF87a",6) && memcmp(p,"GIF89a",6))) return false;
    const uint32_t w=LE16(p+6), h=LE16(p+8);
    if (!Bounded(w,h,1)) return false;
    size_t at=13, frames=0;
    if (p[10] & 128) at += 3u << ((p[10]&7)+1);
    auto blocks=[&]() {
        while (at < size) { size_t n=p[at++]; if (!n) return true; if (n>size-at) return false; at+=n; }
        return false;
    };
    while (at < size) {
        auto tag=p[at++];
        if (tag==0x3b) return Bounded(w,h,frames);
        if (tag==0x21) { if (at==size) return false; ++at; if (!blocks()) return false; }
        else if (tag==0x2c) {
            if (size-at<9) return false;
            uint32_t x=LE16(p+at), y=LE16(p+at+2), fw=LE16(p+at+4), fh=LE16(p+at+6);
            if (!fw || !fh || x+fw>w || y+fh>h || !Bounded(w,h,++frames)) return false;
            auto flags=p[at+8]; at+=9;
            if (flags&128) at+=3u<<((flags&7)+1);
            if (at>=size) return false;
            if (p[at]<2 || p[at]>8) return false;
            ++at; if (!blocks()) return false;
        } else return false;
    }
    return false;
}
void AddFrame(Animation& out, const uint8_t* rgba, uint32_t w, uint32_t h, int delay, uint32_t maxWidth, uint32_t maxHeight, uint32_t frames) {
    const double scale=std::min({1.0,double(maxWidth)/w,double(maxHeight)/h,std::sqrt(double(8*1024*1024)/(4.0*w*h*frames))});
    out.width=std::max(1u,uint32_t(w*scale)); out.height=std::max(1u,uint32_t(h*scale));
    if (uint64_t(out.width)*out.height*4*(out.frames.size()+1)>8*1024*1024) throw 1;
    Frame frame; frame.delayMs=std::clamp(delay,50,10000); frame.rgba.resize(out.width*out.height*4);
    for (uint32_t y=0;y<out.height;++y) for (uint32_t x=0;x<out.width;++x)
        memcpy(frame.rgba.data()+(y*out.width+x)*4,rgba+((uint64_t(y)*h/out.height)*w+uint64_t(x)*w/out.width)*4,4);
    out.frames.push_back(std::move(frame));
}
struct Input { const uint8_t* data; size_t size, at=0; };
int ReadGif(GifFileType* gif,GifByteType* data,int count) {
    auto* input=static_cast<Input*>(gif->UserData);
    size_t n=std::min(size_t(std::max(0,count)),input->size-input->at);
    memcpy(data,input->data+input->at,n); input->at+=n; return int(n);
}
Animation Gif(const uint8_t* data,size_t size,uint32_t maxWidth,uint32_t maxHeight) {
    if (!GifPreflight(data,size)) return {};
    Input input{data,size}; int error=0;
    auto close=[](GifFileType* g){int e=0;if(g)DGifCloseFile(g,&e);};
    std::unique_ptr<GifFileType,decltype(close)> gif(DGifOpen(&input,ReadGif,&error),close);
    if (!gif || DGifSlurp(gif.get())!=GIF_OK || !Bounded(gif->SWidth,gif->SHeight,gif->ImageCount)) return {};
    uint32_t w=gif->SWidth,h=gif->SHeight;
    std::vector<uint8_t> canvas(w*h*4,0), previous;
    Animation out;
    for (int i=0;i<gif->ImageCount;++i) {
        const auto& image=gif->SavedImages[i];const auto& d=image.ImageDesc;
        GraphicsControlBlock control{}; control.TransparentColor=NO_TRANSPARENT_COLOR;
        DGifSavedExtensionToGCB(gif.get(),i,&control);
        auto* colors=d.ColorMap?d.ColorMap:gif->SColorMap;
        if (!colors || !image.RasterBits) return {};
        if (i == 0 && control.TransparentColor == NO_TRANSPARENT_COLOR && gif->SColorMap
            && gif->SBackGroundColor < gif->SColorMap->ColorCount) {
            const auto color = gif->SColorMap->Colors[gif->SBackGroundColor];
            for (size_t at = 0; at < canvas.size(); at += 4) {
                canvas[at] = color.Red; canvas[at + 1] = color.Green;
                canvas[at + 2] = color.Blue; canvas[at + 3] = 255;
            }
        }
        if (control.DisposalMode==DISPOSE_PREVIOUS) previous=canvas;
        for (int y=0;y<d.Height;++y) for (int x=0;x<d.Width;++x) {
            int index=image.RasterBits[y*d.Width+x];
            if(index==control.TransparentColor)continue;
            if(index>=colors->ColorCount)return {};
            auto* pixel=canvas.data()+((y+d.Top)*w+x+d.Left)*4;
            pixel[0]=colors->Colors[index].Red;pixel[1]=colors->Colors[index].Green;
            pixel[2]=colors->Colors[index].Blue;pixel[3]=255;
        }
        AddFrame(out,canvas.data(),w,h,control.DelayTime*10,maxWidth,maxHeight,gif->ImageCount);
        if(control.DisposalMode==DISPOSE_PREVIOUS)canvas.swap(previous);
        else if(control.DisposalMode==DISPOSE_BACKGROUND) {
            uint8_t background[4]={0,0,0,0};
            if(control.TransparentColor==NO_TRANSPARENT_COLOR && gif->SColorMap
                && gif->SBackGroundColor<gif->SColorMap->ColorCount) {
                auto c=gif->SColorMap->Colors[gif->SBackGroundColor];
                background[0]=c.Red;background[1]=c.Green;background[2]=c.Blue;background[3]=255;
            }
            for(int y=0;y<d.Height;++y)for(int x=0;x<d.Width;++x)
                memcpy(canvas.data()+((y+d.Top)*w+x+d.Left)*4,background,4);
        }
    }
    return out;
}
Animation WebP(const uint8_t* data,size_t size,uint32_t maxWidth,uint32_t maxHeight) {
    WebPData input{data,size};
    auto demux=std::unique_ptr<WebPDemuxer,decltype(&WebPDemuxDelete)>(WebPDemux(&input),WebPDemuxDelete);
    if (!demux) return {};
    if(!Bounded(WebPDemuxGetI(demux.get(),WEBP_FF_CANVAS_WIDTH),WebPDemuxGetI(demux.get(),WEBP_FF_CANVAS_HEIGHT),WebPDemuxGetI(demux.get(),WEBP_FF_FRAME_COUNT)))return {};
    WebPAnimDecoderOptions options; if(!WebPAnimDecoderOptionsInit(&options))return {};
    options.color_mode=MODE_RGBA; options.use_threads=0;
    auto decoder=std::unique_ptr<WebPAnimDecoder,decltype(&WebPAnimDecoderDelete)>(WebPAnimDecoderNew(&input,&options),WebPAnimDecoderDelete);
    WebPAnimInfo info;
    if(!decoder || !WebPAnimDecoderGetInfo(decoder.get(),&info))return {};
    Animation out; int previous=0;
    while(WebPAnimDecoderHasMoreFrames(decoder.get())) {
        uint8_t* pixels=nullptr;int timestamp=0;
        if(!WebPAnimDecoderGetNext(decoder.get(),&pixels,&timestamp))return {};
        AddFrame(out,pixels,info.canvas_width,info.canvas_height,timestamp-previous,maxWidth,maxHeight,info.frame_count);previous=timestamp;
    }
    return out;
}
}
Animation DecodeAnimation(const uint8_t* data,size_t size,uint32_t maxWidth,uint32_t maxHeight) {
    if(!maxWidth||!maxHeight||maxWidth>1024||maxHeight>1024)return {};
    if(!data || size>kFileLimit || size<12)return {};
    try {
        if(!memcmp(data,"GIF8",4))return Gif(data,size,maxWidth,maxHeight);
        if(!memcmp(data,"RIFF",4) && !memcmp(data+8,"WEBP",4))return WebP(data,size,maxWidth,maxHeight);
    } catch (...) { return {}; }
    return {};
}
Animation DecodeAnimation(FILE* file,uint32_t maxWidth,uint32_t maxHeight) {
    if(!file || fseek(file,0,SEEK_END))return {};
    long size=ftell(file);if(size<12 || size>long(kFileLimit))return {};
    std::vector<uint8_t> data(size);rewind(file);
    if(fread(data.data(),1,data.size(),file)!=data.size())return {};
    return DecodeAnimation(data.data(),data.size(),maxWidth,maxHeight);
}
}
