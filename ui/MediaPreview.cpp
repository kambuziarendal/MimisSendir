// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "MediaPreview.h"
#include "support/AnimatedImage.h"
#include "support/ImageSafety.h"
#include "network/HttpUpload.h"
#include <DataIO.h>
#include <TranslationUtils.h>
#include <algorithm>
#include <cstring>
#include <cmath>
namespace {
std::shared_ptr<BBitmap> MakeThumbnail(FILE* file, bool conversation)
{
	std::vector<unsigned char> prefix(Upload::kPreviewHeaderLimit);
	rewind(file);
	size_t read = fread(prefix.data(), 1, prefix.size(), file);
	auto dimensions = Upload::InspectPreviewImage(prefix.data(), read);
	if (!dimensions) return {};
	// BTranslationUtils accepts a BPositionIO, not FILE. Decode via bounded in-memory
	// bytes only after preflight; the HTTP download is already capped at 50 MiB.
	if (fseek(file, 0, SEEK_END) != 0) return {};
	long size = ftell(file);
	if (size < 0 || static_cast<uint64_t>(size) > Upload::kMaxFileSize) return {};
	std::vector<unsigned char> bytes(static_cast<size_t>(size));
	rewind(file);
	if (fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) return {};
	BMemoryIO input(bytes.data(), bytes.size());
	std::unique_ptr<BBitmap> decoded(BTranslationUtils::GetBitmap(&input));
	if (!decoded || decoded->InitCheck() != B_OK) return {};
	const BRect bounds = decoded->Bounds();
	const uint32 width = bounds.IntegerWidth() + 1, height = bounds.IntegerHeight() + 1;
	if (width != dimensions->width || height != dimensions->height
		|| static_cast<uint64_t>(width) * height > Upload::kPreviewPixelLimit
		|| decoded->BitsLength() <= 0
		|| static_cast<uint64_t>(decoded->BitsLength()) > 4 * Upload::kPreviewPixelLimit
		|| static_cast<uint64_t>(decoded->BitsLength()) < static_cast<uint64_t>(decoded->BytesPerRow()) * height
		|| (decoded->ColorSpace() != B_RGB32 && decoded->ColorSpace() != B_RGBA32)
		|| decoded->BytesPerRow() < static_cast<int32>(width * 4)) return {};
	const float scale = std::min({1.0f, (conversation ? 1024.0f : 240.0f) / width, (conversation ? 768.0f : 180.0f) / height});
	const uint32 thumbWidth = std::max(1U, static_cast<uint32>(width * scale));
	const uint32 thumbHeight = std::max(1U, static_cast<uint32>(height * scale));
	auto thumb = std::make_shared<BBitmap>(BRect(0, 0, thumbWidth - 1, thumbHeight - 1), B_RGBA32);
	if (thumb->InitCheck() != B_OK) return {};
	const auto* pixels = static_cast<const unsigned char*>(decoded->Bits());
	auto* output = static_cast<unsigned char*>(thumb->Bits());
	for (uint32 y = 0; y < thumbHeight; ++y) {
		for (uint32 x = 0; x < thumbWidth; ++x) {
            // Bilinear sampling keeps text and edges legible at chat size.
            const float sx=std::clamp((x+0.5f)*width/thumbWidth-0.5f,0.0f,float(width-1));
            const float sy=std::clamp((y+0.5f)*height/thumbHeight-0.5f,0.0f,float(height-1));
            const uint32 x0=uint32(sx),y0=uint32(sy),x1=std::min(x0+1,width-1),y1=std::min(y0+1,height-1);
            const float fx=sx-x0,fy=sy-y0;
            const unsigned char* samples[]={pixels+y0*decoded->BytesPerRow()+x0*4,pixels+y0*decoded->BytesPerRow()+x1*4,pixels+y1*decoded->BytesPerRow()+x0*4,pixels+y1*decoded->BytesPerRow()+x1*4};
            const float weights[]={(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy};
            auto* to=output+y*thumb->BytesPerRow()+x*4;
            float alpha=0,channels[3]={};
            for(int i=0;i<4;++i) {
                const float a=decoded->ColorSpace()==B_RGB32?255.0f:samples[i][3];alpha+=a*weights[i];
                for(int c=0;c<3;++c)channels[c]+=samples[i][c]*a*weights[i];
            }
            for(int c=0;c<3;++c)to[c]=alpha>0?uint8_t(std::clamp(std::lround(channels[c]/alpha),0L,255L)):0;
            to[3]=uint8_t(std::clamp(std::lround(alpha),0L,255L));
		}
	}
	return thumb;
}
}
MediaPreview LoadMediaPreview(FILE* file, bool conversation) {
    MediaPreview result;
    auto animation = Media::DecodeAnimation(file,conversation?480:240,conversation?360:180);
    if (!animation.frames.empty()) {
        for (const auto& frame : animation.frames) {
            auto bitmap = std::make_shared<BBitmap>(BRect(0,0,animation.width-1,animation.height-1),B_RGBA32);
            if (bitmap->InitCheck()!=B_OK) return {};
            auto* pixels=static_cast<uint8_t*>(bitmap->Bits());
            for(uint32_t y=0;y<animation.height;++y)for(uint32_t x=0;x<animation.width;++x) {
                const auto* src=frame.rgba.data()+(y*animation.width+x)*4;
                auto* dst=pixels+y*bitmap->BytesPerRow()+x*4;
                dst[0]=src[2];dst[1]=src[1];dst[2]=src[0];dst[3]=src[3];
            }
            result.frames.push_back(bitmap); result.delays.push_back(frame.delayMs);
        }
    } else if (auto bitmap=MakeThumbnail(file,conversation)) {
        result.frames.push_back(bitmap);result.delays.push_back(100);
    }
    return result;
}
