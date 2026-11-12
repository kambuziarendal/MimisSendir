// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "StickerPackWindow.h"
#include "MediaPreview.h"
#include "support/MediaLibrary.h"
#include <Button.h>
#include <StringView.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <Path.h>
#include <FindDirectory.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <thread>
class StickerPackPreview final : public BView {
public:
    StickerPackPreview():BView("sticker-pack-preview",B_WILL_DRAW){SetViewUIColor(B_PANEL_BACKGROUND_COLOR);SetExplicitMinSize(BSize(245,190));}
    std::shared_ptr<BBitmap> bitmap;
    void Draw(BRect) override {
        SetHighUIColor(B_PANEL_BACKGROUND_COLOR);FillRect(Bounds());
        if(bitmap){SetDrawingMode(B_OP_ALPHA);DrawBitmap(bitmap.get(),BPoint((Bounds().Width()-bitmap->Bounds().Width())/2,(Bounds().Height()-bitmap->Bounds().Height())/2));SetDrawingMode(B_OP_COPY);}
    }
};
struct StickerPackWindow::Prepared {
    SignalStickers::Pack pack;
    std::string directory;
    MediaPreview preview;
    std::set<uint32_t> downloaded;
    size_t bytes=0;
    ~Prepared(){if(!directory.empty()){std::error_code error;std::filesystem::remove_all(directory,error);}}
    std::string File(uint32_t id) const {return directory+"/"+pack.link.id.substr(0,8)+"-"+std::to_string(id)+".webp";}
    bool Get(uint32_t id,const std::atomic_bool& cancelled,std::string& error) {
        if(downloaded.count(id))return true;
        auto data=SignalStickers::Download(pack,id,cancelled,error);if(!error.empty())return false;
        if(bytes+data.size()>128*1024*1024){error="This pack exceeds the local 128 MiB limit.";return false;}
        std::ofstream out(File(id),std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());out.close();
        if(!out){error="Could not save downloaded sticker.";return false;}
        bytes+=data.size();downloaded.insert(id);return true;
    }
};
struct StickerPackWindow::Job {
    std::atomic_bool done{false},cancelled{false};
    std::atomic_uint completed{0};
    std::shared_ptr<Prepared> prepared;
    std::string error;
    bool importing=false;
};
StickerPackWindow::StickerPackWindow(const SignalStickers::Link& link,const std::string& account)
    :BWindow(BRect(70,70,520,430),"Install Signal sticker pack",B_TITLED_WINDOW,
        B_ASYNCHRONOUS_CONTROLS|B_AUTO_UPDATE_SIZE_LIMITS|B_NOT_ZOOMABLE),fAccount(account) {
    fTitle=new BStringView("pack-title","Loading sticker pack…");fTitle->SetTruncation(B_TRUNCATE_END);
    fAuthor=new BStringView("pack-author","");fAuthor->SetTruncation(B_TRUNCATE_END);
    fStatus=new BStringView("pack-status","Reading the pack from Signal…");fStatus->SetTruncation(B_TRUNCATE_END);
    fPreview=new StickerPackPreview();fImport=new BButton("import-pack","Import",new BMessage('sImp'));fImport->SetEnabled(false);
    BLayoutBuilder::Group<>(this,B_VERTICAL,8).SetInsets(12)
        .Add(fTitle).Add(fAuthor).Add(fPreview)
        .Add(new BStringView("pack-account",("Local library: "+account).c_str()))
        .Add(fStatus).AddGroup(B_HORIZONTAL).AddGlue().Add(new BButton("close-pack","Close",new BMessage(B_QUIT_REQUESTED))).Add(fImport).End();
    auto job=std::make_shared<Job>();fJob=job;
    try {std::thread([job,link]{
        try {
            auto pack=SignalStickers::Load(link,job->cancelled,job->error);
            if(pack&&!job->cancelled) {
                auto prepared=std::make_shared<Prepared>();prepared->pack=std::move(*pack);
                BPath temp;if(find_directory(B_SYSTEM_TEMP_DIRECTORY,&temp)!=B_OK)throw 1;
                std::string pattern=std::string(temp.Path())+"/renga-signal-pack-XXXXXX";
                if(!mkdtemp(pattern.data()))throw 1;
                prepared->directory=pattern;
                const auto first=prepared->pack.stickers.front().id;
                if(prepared->Get(first,job->cancelled,job->error)) {
                    FILE* file=fopen(prepared->File(first).c_str(),"rb");
                    if(file){prepared->preview=LoadMediaPreview(file);fclose(file);}
                    job->prepared=prepared;
                }
            }
        }catch(...){job->error="Could not prepare this sticker pack.";}
        job->done=true;
    }).detach();}catch(...){job->error="Could not start the pack download.";job->done=true;}
    BMessage tick('sPol');fTimer=new BMessageRunner(BMessenger(this),tick,100000);
}
StickerPackWindow::~StickerPackWindow(){if(fJob)fJob->cancelled=true;delete fTimer;}
void StickerPackWindow::Import() {
    if(fJob||!fPrepared)return;
    fImport->SetEnabled(false);auto job=std::make_shared<Job>();job->importing=true;job->prepared=fPrepared;fJob=job;
    BPath settings;if(find_directory(B_USER_SETTINGS_DIRECTORY,&settings)!=B_OK){job->error="Could not locate your local library.";job->done=true;return;}settings.Append("jabber-for-beos");
    Media::Library library(settings.Path(),fAccount);
    try {std::thread([job,library]{
        try {
            for(const auto& sticker:job->prepared->pack.stickers) {
                if(job->cancelled)break;
                if(!job->prepared->Get(sticker.id,job->cancelled,job->error))break;
                auto saved=library.Import(job->prepared->File(sticker.id),true,job->error);
                if(saved.empty())break;
                ++job->completed;
            }
        }catch(...){job->error="Could not finish importing. Completed copies are kept.";}
        job->done=true;
    }).detach();}catch(...){job->error="Could not start the import.";job->done=true;}
}
void StickerPackWindow::Poll() {
    if(!fJob)return;
    if(!fJob->done) {
        if(fJob->importing)fStatus->SetText(("Importing "+std::to_string(fJob->completed.load())+" / "+std::to_string(fPrepared->pack.stickers.size())+"…").c_str());
        return;
    }
    auto job=fJob;fJob.reset();
    if(job->importing) {
        auto status=std::to_string(job->completed.load())+" stickers available under Stickers.";
        if(!job->error.empty())status+=" "+job->error;
        fStatus->SetText(status.c_str());fStatus->SetToolTip(status.c_str());return;
    }
    if(!job->error.empty()||!job->prepared){fStatus->SetText(job->error.c_str());fStatus->SetToolTip(job->error.c_str());return;}
    fPrepared=job->prepared;fTitle->SetText(fPrepared->pack.title.c_str());fTitle->SetToolTip(fPrepared->pack.title.c_str());
    fAuthor->SetText(fPrepared->pack.author.c_str());fAuthor->SetToolTip(fPrepared->pack.author.c_str());
    fImport->SetLabel(("Import "+std::to_string(fPrepared->pack.stickers.size())+" stickers").c_str());fImport->SetEnabled(true);
    fStatus->SetText("Import a local copy. No message will be sent.");
    if(!fPrepared->preview.frames.empty()){fPreview->bitmap=fPrepared->preview.frames.front();fPreview->Invalidate();}
}
void StickerPackWindow::MessageReceived(BMessage* message) {
    if(message->what=='sPol')Poll();else if(message->what=='sImp')Import();else if(message->what=='sAct')Activate();else BWindow::MessageReceived(message);
}
