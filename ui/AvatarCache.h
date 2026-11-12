// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Bitmap.h>
#include <Handler.h>
#include <View.h>
#include <String.h>
#include <memory>
#include <mutex>
#include <string>
namespace Avatars {
constexpr uint32 kChanged = 'avCh';
struct Image {
    std::mutex mutex;
    std::shared_ptr<BBitmap> bitmap;
    std::shared_ptr<BBitmap> Get() { std::lock_guard<std::mutex> lock(mutex); return bitmap; }
};
std::string Account();
std::shared_ptr<Image> Get(const std::string& account, const std::string& jid);
void Receive(const std::string& account, const std::string& jid, const std::string& bytes);
void Alias(const std::string& account, const std::string& occupant, const std::string& realJid);
void RefreshAccount(const std::string& account);
// Call after windows and network callbacks have drained.
void Shutdown();
void Watch(BHandler* handler);
void Unwatch(BHandler* handler);
inline void Draw(BView* view,BRect frame,const std::shared_ptr<Image>& image,const std::string& name){
    view->PushState();
    if(auto bitmap=image?image->Get():nullptr){view->SetDrawingMode(B_OP_ALPHA);view->SetBlendingMode(B_PIXEL_ALPHA,B_ALPHA_OVERLAY);view->DrawBitmap(bitmap.get(),bitmap->Bounds(),frame,B_FILTER_BITMAP_BILINEAR);}
    else {view->SetHighUIColor(B_PANEL_BACKGROUND_COLOR);view->FillEllipse(frame);view->SetHighUIColor(B_PANEL_TEXT_COLOR);
        BString initial(name.c_str());initial.TruncateChars(1);BFont font(be_bold_font);font.SetSize(frame.Height()*.44f);view->SetFont(&font);font_height metrics;font.GetHeight(&metrics);
        view->DrawString(initial,BPoint(frame.left+(frame.Width()-font.StringWidth(initial))/2,frame.top+(frame.Height()+metrics.ascent-metrics.descent)/2));}
    view->PopState();
}
}
