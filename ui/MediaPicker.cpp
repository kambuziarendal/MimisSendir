#include "jabber/BlabberSettings.h"
// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "MediaPicker.h"
#include "MediaPreview.h"
#include "ColorEmoji.h"
#include "network/Giphy.h"
#include <TranslationUtils.h>
#include <Application.h>
#include <Button.h>
#include <MenuField.h>
#include <PopUpMenu.h>
#include <MenuItem.h>
#include <set>
#include <CardLayout.h>
#include <FilePanel.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <GridView.h>
#include <GridLayout.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MessageRunner.h>
#include <Path.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <algorithm>
#include <atomic>
#include <thread>

namespace {
std::string BuiltinGIFs() { BPath p;find_directory(B_SYSTEM_DATA_DIRECTORY,&p);p.Append("mimissendir/gifs");return p.Path(); }
std::string Settings() { BPath p;find_directory(B_USER_SETTINGS_DIRECTORY,&p);p.Append("MimisSendir");return p.Path(); }
std::string Lower(std::string text) { for(auto& c:text)c=std::tolower(static_cast<unsigned char>(c));return text; }
}
class PreviewView final : public BView {
public:
    PreviewView():BView("media-preview",B_WILL_DRAW) { SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);SetExplicitMinSize(BSize(245,190)); }
    std::shared_ptr<BBitmap> bitmap;
    void Draw(BRect) override {
        SetHighUIColor(B_DOCUMENT_BACKGROUND_COLOR);FillRect(Bounds());
        if(bitmap){SetDrawingMode(B_OP_ALPHA);DrawBitmap(bitmap.get(),BPoint((Bounds().Width()-bitmap->Bounds().Width())/2,(Bounds().Height()-bitmap->Bounds().Height())/2));SetDrawingMode(B_OP_COPY);}
    }
};
class AttributionView final : public BView {
    std::unique_ptr<BBitmap> logo;
public:
    AttributionView():BView("giphy-attribution",B_WILL_DRAW) {
        SetViewUIColor(B_PANEL_BACKGROUND_COLOR);BPath path;find_directory(B_SYSTEM_DATA_DIRECTORY,&path);path.Append("mimissendir/giphy/powered-by.png");
        logo.reset(BTranslationUtils::GetBitmap(path.Path()));SetExplicitMinSize(BSize(200,42));SetExplicitMaxSize(BSize(200,42));
    }
    void Draw(BRect) override { if(logo) {SetDrawingMode(B_OP_ALPHA);DrawBitmap(logo.get(),BPoint(0,0));SetDrawingMode(B_OP_COPY);}else DrawString("Powered By GIPHY",BPoint(4,25)); }
};
struct MediaPicker::SearchJob {
    std::atomic_bool done{false}, cancelled{false};
    std::vector<Giphy::Item> items;
    std::string error;
};
struct MediaPicker::Job {
    std::atomic_bool done{false}, cancelled{false};
    std::shared_ptr<Giphy::Transfer> transfer;
    std::string path,error;
    MediaPreview preview;
    bool import=false;
    size_t imported=0, failed=0;
};
MediaPicker::MediaPicker(BMessenger target,const std::string& account,const std::string& conversation,BPoint position,std::string reactionId)
    :BWindow(BRect(position.x,position.y,position.x+470,position.y+415),"Emoji, GIFs and stickers",B_TITLED_WINDOW,
        B_ASYNCHRONOUS_CONTROLS|B_AUTO_UPDATE_SIZE_LIMITS|B_NOT_ZOOMABLE),fTarget(target),fReactionId(std::move(reactionId)),fLibrary(Settings(),account,BuiltinGIFs())
{
    auto* root=new BGroupView(B_VERTICAL,8);
    BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(root);
    fSearch=new BTextControl("media-search","Search:","",new BMessage('mFil'));
    fSearch->SetModificationMessage(new BMessage('mFil'));
    auto* grid=new BGridView(4,4);
    BFont emoji(be_plain_font);emoji.SetFamilyAndStyle("Noto Emoji",nullptr);emoji.SetSize(23);
    for(int i=0;i<24;++i) {
        fEmoji[i]=new BButton("emoji", "",new BMessage('mEmj'));
        fEmoji[i]->SetFont(&emoji);
        fEmoji[i]->SetExplicitMinSize(BSize(48,45));
        fEmoji[i]->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,B_SIZE_UNLIMITED));
        fEmoji[i]->SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH,B_ALIGN_USE_FULL_HEIGHT));
        grid->GridLayout()->AddView(fEmoji[i],i%6,i/6);
    }
    fPrevious=new BButton("previous","Previous",new BMessage('mPre'));
    fNext=new BButton("next","Next",new BMessage('mNxt'));
    auto* categories = new BPopUpMenu("All emoji");
    std::vector<std::string> names{"All emoji", "Favorites"};
    for (const auto& entry : Media::Emojis())
        if (std::find(names.begin(), names.end(), entry.category) == names.end()) names.emplace_back(entry.category);
    for (const auto& name : names) {
        auto* action = new BMessage('mCat'); action->AddString("category", name.c_str());
        auto* item = new BMenuItem(name.c_str(), action); categories->AddItem(item);
        if (name == "All emoji") item->SetMarked(true);
    }
    fCategory = new BMenuField("category", "Category:", categories);
    fEditFavorites = new BButton("edit-favorites", "Edit favorites", new BMessage('mFav'));
    auto* emojiPage=new BGroupView(B_VERTICAL);
    BLayoutBuilder::Group<>(emojiPage).AddGroup(B_HORIZONTAL).Add(fCategory).AddGlue().Add(fEditFavorites).End().Add(grid).AddGroup(B_HORIZONTAL).Add(fPrevious).AddGlue().Add(fNext).End();
    fList=new BListView("media-list");fList->SetSelectionMessage(new BMessage('mSel'));
    fList->SetExplicitMinSize(BSize(145,190));
    fPreview=new PreviewView();
    fImport=new BButton("import-media","Import…",new BMessage('mImp'));
    fImport->SetToolTip("Choose images, or select their folder to import a complete local pack.");
    fRemove=new BButton("remove-media","Remove",new BMessage('mRem'));
    fRemove->SetToolTip("Remove this library copy. The original file is kept.");
    fSend=new BButton("send-media","Send",new BMessage('mSnd'));fSend->SetEnabled(false);
    fSearchOnline=new BButton("giphy-search","Search",new BMessage('gSrc'));
    fOnlinePrevious=new BButton("giphy-previous","Previous",new BMessage('gPre'));
    fOnlineNext=new BButton("giphy-next","Next",new BMessage('gNxt'));
    fAttribution=new AttributionView();
    fSearchOnline->Hide();fOnlinePrevious->Hide();fOnlineNext->Hide();fAttribution->Hide();
    auto* mediaPage=new BGroupView(B_VERTICAL);
    BLayoutBuilder::Group<>(mediaPage).AddGroup(B_HORIZONTAL)
        .Add(new BScrollView("media-scroll",fList,0,false,true),1).Add(fPreview,1).End()
        .AddGroup(B_HORIZONTAL).Add(fImport).Add(fRemove).Add(fSearchOnline).Add(fOnlinePrevious).Add(fOnlineNext).AddGlue().Add(fSend).End()
        .Add(fAttribution);
    auto* cards=new BView("media-cards",0,new BCardLayout());cards->SetExplicitMinSize(BSize(420,340));fCards=static_cast<BCardLayout*>(cards->GetLayout());
    fCards->AddView(emojiPage);fCards->AddView(mediaPage);
    fStatus=new BStringView("media-status","");fStatus->SetExplicitMinSize(BSize(100,B_SIZE_UNSET));
    fStatus->SetTruncation(B_TRUNCATE_END);
    auto mode=[](int n){auto* m=new BMessage('mMod');m->AddInt32("mode",n);return m;};
    BLayoutBuilder::Group<>(root).SetInsets(10)
        .Add(new BStringView("media-recipient",("To: " + conversation).c_str()))
        .AddGroup(B_HORIZONTAL).Add(new BButton("emoji-tab","Emoji",mode(0)))
            .Add(new BButton("gif-tab","Local GIFs",mode(1))).Add(new BButton("giphy-tab","GIPHY",mode(3))).Add(new BButton("sticker-tab","Stickers",mode(2))).End()
        .Add(fSearch).Add(cards).Add(fStatus);
    if(!fReactionId.empty()) {
        SetTitle("React to message");
        FindView("giphy-tab")->Hide();FindView("gif-tab")->Hide();FindView("sticker-tab")->Hide();
        fSearch->SetToolTip("Choose an emoji to send a reaction to the selected message.");
    }
    Refresh();BMessage tick('mPol');fTimer=new BMessageRunner(BMessenger(this),tick,50000);
}
MediaPicker::~MediaPicker(){if(fJob)fJob->cancelled=true;if(fSearchJob)fSearchJob->cancelled=true;delete fTimer;delete fPanel;}
void MediaPicker::Refresh() {
    fCards->SetVisibleItem(fMode==0?0:1);
    const bool online=fMode==3;
    if(online!=fOnlineControls) {
        if(online) {fImport->Hide();fRemove->Hide();fSearchOnline->Show();fOnlinePrevious->Show();fOnlineNext->Show();fAttribution->Show();}
        else {fImport->Show();fRemove->Show();fSearchOnline->Hide();fOnlinePrevious->Hide();fOnlineNext->Hide();fAttribution->Hide();}
        fOnlineControls=online;
        InvalidateLayout(true);
        Layout(true);
    }
    fSearch->SetMessage(new BMessage(online?'gSrc':'mFil'));
    fOnlinePrevious->SetEnabled(fOffset>0 && (!fSearchJob||fSearchJob->done));
    fOnlineNext->SetEnabled(fEntries.size()==24 && fOffset<480 && (!fSearchJob||fSearchJob->done));
    if(fMode==3) { fStatus->SetText("Search GIPHY, then select a GIF to preview. Empty search shows trending GIFs.");return; }
    auto query=Lower(fSearch->Text());
    if(fMode==0) {
        fFiltered.clear();const auto& all=Media::Emojis();
        const auto saved = fLibrary.FavoriteEmojis();
        const std::set<std::string> favorites(saved.begin(), saved.end());
        for (size_t i = 0; i < all.size(); ++i) {
            const auto& e = all[i];
            if (fCategoryName == "Favorites" && !favorites.count(e.text)) continue;
            if (!fCategoryName.empty() && fCategoryName != "Favorites" && fCategoryName != e.category) continue;
            if (query.empty() || Lower(std::string(e.keywords) + " " + e.category).find(query) != std::string::npos || query == e.text) fFiltered.push_back(i);
        }
        std::stable_partition(fFiltered.begin(), fFiltered.end(), [&](size_t i) { return favorites.count(all[i].text) != 0; });
        if(fPage*24>=fFiltered.size())fPage=0;
        for(size_t i=0;i<24;++i) {
            bool exists=fPage*24+i<fFiltered.size();fEmoji[i]->SetEnabled(exists);
            auto* message=new BMessage('mEmj');
            if (exists) {
                const auto& e = all[fFiltered[fPage*24+i]];
                auto icon = ColorEmoji(e.text);
                const bool colored = icon && fEmoji[i]->SetIcon(icon.get()) == B_OK;
                if (!colored) fEmoji[i]->SetIcon(nullptr);
                BFont labelFont(be_plain_font);
                if(!colored){labelFont.SetFamilyAndStyle("Noto Emoji",nullptr);labelFont.SetSize(23);}
                fEmoji[i]->SetFont(&labelFont);
                const bool favorite = favorites.count(e.text);
                fEmoji[i]->SetLabel(colored ? (favorite ? "★" : "") : e.text);
                const std::string tip = std::string(e.keywords) + (favorite ? " · Favorite" : "") + (fEditingFavorites ? " · Click to toggle favorite" : "");
                fEmoji[i]->SetToolTip(tip.c_str());
                message->AddString("emoji", e.text);
            } else {
                fEmoji[i]->SetIcon(nullptr);
                fEmoji[i]->SetLabel("");
                fEmoji[i]->SetToolTip("");
            }
            fEmoji[i]->SetMessage(message);
        }
        fPrevious->SetEnabled(fPage>0);fNext->SetEnabled((fPage+1)*24<fFiltered.size());
        std::string status = fEditingFavorites ? "Click emoji to add/remove favorites. " : "Favorites first. ";
        status += std::to_string(fFiltered.size()) + " emoji · Page " + std::to_string(fPage + 1) + "/" + std::to_string(std::max(size_t(1), (fFiltered.size() + 23) / 24));
        if (fFiltered.empty()) status = "No emoji here. Choose All emoji, edit favorites or change your search.";
        fStatus->SetText(status.c_str());
    } else {
        fList->SetSelectionMessage(nullptr);
        while(fList->CountItems())delete fList->RemoveItem(int32(0));
        fEntries=fLibrary.List(fMode==2);
        fEntries.erase(std::remove_if(fEntries.begin(),fEntries.end(),[&](const auto& e){return Lower(e.name).find(query)==std::string::npos;}),fEntries.end());
        for(const auto& e:fEntries)fList->AddItem(new BStringItem(e.name.c_str()));
        fList->SetSelectionMessage(new BMessage('mSel'));
        fPreview->bitmap.reset();fPreview->Invalidate();fPreviewPath.clear();fSend->SetEnabled(false);
        fStatus->SetText(fMode==2?"Import PNG/WebP/GIF. Stickers are sent as images.":"Local GIF library. Preview and send, or import your own GIFs.");
    }
}
void MediaPicker::PreviewSelection() {
    fSend->SetEnabled(false);fRemove->SetEnabled(false);int32 index=fList->CurrentSelection();
    if(index<0||size_t(index)>=fEntries.size())return;
    if(fJob&&!fJob->done)return;
    auto path=fEntries[index].path;fRemove->SetEnabled(fLibrary.IsImported(path));if(fPreviewPath==path && fJob && !fJob->preview.frames.empty()){fSend->SetEnabled(true);return;}
    fPreviewPath=path;fPreview->bitmap.reset();fPreview->Invalidate();
    auto job=std::make_shared<Job>();job->path=path;fJob=job;fStatus->SetText("Loading preview…");
    const bool online=fMode==3;
    try { std::thread([job,online] {
        try{
            std::string local=job->path;
            if(online) {job->transfer=Giphy::Download(job->path,job->cancelled,job->error);if(job->transfer)local=job->transfer->path;else local.clear();}
            FILE* file=local.empty()?nullptr:fopen(local.c_str(),"rb");if(file){job->preview=LoadMediaPreview(file);fclose(file);}}
        catch(...){job->error="Could not load this image.";}
        if(job->preview.frames.empty()&&job->error.empty())job->error="Preview unavailable. Choose another image.";
        job->done=true;
    }).detach(); } catch(...){job->error="Could not start preview worker.";job->done=true;}
}
void MediaPicker::SearchGiphy() {
    if(fMode!=3)return;
    if(fSearchJob&&!fSearchJob->done)return;
    if(fJob){fJob->cancelled=true;}
    fJob.reset();fPreview->bitmap.reset();fPreview->Invalidate();fPreviewPath.clear();fSend->SetEnabled(false);
    fList->SetSelectionMessage(nullptr);while(fList->CountItems())delete fList->RemoveItem(int32(0));fList->SetSelectionMessage(new BMessage('mSel'));fEntries.clear();
    auto job=std::make_shared<SearchJob>();fSearchJob=job;std::string query=fSearch->Text();auto offset=fOffset;
    fSearchOnline->SetEnabled(false);fOnlineNext->SetEnabled(false);fOnlinePrevious->SetEnabled(false);fStatus->SetText("Searching GIPHY…");
    const auto keyId=BlabberSettings::Instance()->Data("giphy-key-id").value_or("");
    try {std::thread([job,query,offset,keyId]{try{job->items=Giphy::Search(query,offset,job->cancelled,job->error,keyId);}catch(...){job->error="GIPHY search could not complete.";}job->done=true;}).detach();}
    catch(...){job->error="Could not start GIPHY search.";job->done=true;}
}
void MediaPicker::Poll() {
    if(fSearchJob&&fSearchJob->done) {
        auto result=fSearchJob;fSearchJob.reset();fSearchOnline->SetEnabled(true);
        if(fMode==3&&!result->cancelled) {
            for(const auto& item:result->items) {fEntries.push_back({item.title,item.url});auto label=item.title+(item.creator.empty()?"":" • "+item.creator);fList->AddItem(new BStringItem(label.c_str()));}
            fOnlinePrevious->SetEnabled(fOffset>0);fOnlineNext->SetEnabled(fEntries.size()==24&&fOffset<480);
            fStatus->SetText(!result->error.empty()?result->error.c_str():fEntries.empty()?"No GIFs found. Try another search.":"Select a GIF to preview. Press Send when ready.");
        }
    }
    if(!fJob||!fJob->done)return;
    if(fJob->import) {
        auto result=fJob;fJob.reset();fImport->SetEnabled(true);Refresh();
        std::string summary=std::to_string(result->imported)+" images available in your local library.";
        if(result->failed) summary+=" "+std::to_string(result->failed)+" could not be imported.";
        if(!result->error.empty()) summary+=" "+result->error;
        fStatus->SetText(summary.c_str());fStatus->SetToolTip(summary.c_str());
        if(result->imported==1 && result->failed==0)
            for(size_t i=0;i<fEntries.size();++i)if(fEntries[i].path==result->path){fList->Select(i);break;}
        return;
    }
    int32 selected=fList->CurrentSelection();
    if(fMode==0||selected<0||size_t(selected)>=fEntries.size())return;
    if(fPreviewPath!=fEntries[selected].path){PreviewSelection();return;}
    if(!fJob->error.empty()){fStatus->SetText(fJob->error.c_str());return;}
    const auto& preview=fJob->preview;if(preview.frames.empty())return;
    if(!fPreview->bitmap){fFrame=0;fPreview->bitmap=preview.frames[0];fPreview->Invalidate();fNextFrame=system_time()+preview.delays[0]*1000;fSend->SetEnabled(true);fStatus->SetText("Ready. Press Send to share with this conversation.");}
    if(preview.frames.size()>1&&!IsHidden()&&system_time()>=fNextFrame){fFrame=(fFrame+1)%preview.frames.size();fPreview->bitmap=preview.frames[fFrame];fPreview->Invalidate();fNextFrame=system_time()+preview.delays[fFrame]*1000;}
}
void MediaPicker::MessageReceived(BMessage* message) {
    switch(message->what) {
        case 'mMod': {
            if(!fReactionId.empty()||(fJob&&fJob->import&&!fJob->done))break;
            if(fJob){fJob->cancelled=true;}
    fJob.reset();if(fSearchJob)fSearchJob->cancelled=true;
            fList->SetSelectionMessage(nullptr);while(fList->CountItems())delete fList->RemoveItem(int32(0));fList->SetSelectionMessage(new BMessage('mSel'));fEntries.clear();
            fPreview->bitmap.reset();fPreview->Invalidate();fPreviewPath.clear();fSend->SetEnabled(false);
            fMode=message->GetInt32("mode",0);fPage=0;fOffset=0;Refresh();break;
        }
        case 'mFil':if(fMode!=3){fPage=0;Refresh();}else{fOffset=0;fOnlineNext->SetEnabled(false);fOnlinePrevious->SetEnabled(false);}break;
        case 'gSrc':fOffset=0;SearchGiphy();break;
        case 'gPre':if(fOffset>=24){fOffset-=24;SearchGiphy();}break;
        case 'gNxt':if(fOffset<480){fOffset+=24;SearchGiphy();}break;
        case 'mPre':if(fPage)--fPage;Refresh();break;
        case 'mNxt':++fPage;Refresh();break;
        case 'mCat': fCategoryName=message->GetString("category", "");if(fCategoryName=="All emoji")fCategoryName.clear();fPage=0;Refresh();break;
        case 'mFav': fEditingFavorites=!fEditingFavorites;fEditFavorites->SetLabel(fEditingFavorites?"Done":"Edit favorites");Refresh();break;
        case 'mEmj': {
            if (fEditingFavorites) {
                const std::string emoji=message->GetString("emoji", "");
                const auto favorites=fLibrary.FavoriteEmojis();std::string error;
                const bool add=std::find(favorites.begin(),favorites.end(),emoji)==favorites.end();
                if(fLibrary.SetFavoriteEmoji(emoji,add,error))Refresh();else fStatus->SetText(error.c_str());
                break;
            }
            const char* text=message->GetString("emoji","");if(*text){BMessage insert(fReactionId.empty()?'eIns':'rSet');if(!fReactionId.empty())insert.AddString("id",fReactionId.c_str());insert.AddString("emoji",text);fTarget.SendMessage(&insert);PostMessage(B_QUIT_REQUESTED);}break;}
        case 'mSel':PreviewSelection();break;
        case 'mPol':Poll();break;
        case 'mRem': {
            if(fJob&&!fJob->done)break;
            int32 index=fList->CurrentSelection();
            if(index<0||size_t(index)>=fEntries.size())break;
            if(fLibrary.Remove(fEntries[index].path,fMode==2)){fJob.reset();Refresh();}
            else fStatus->SetText("Could not remove the library copy.");
            break;
        }
        case 'mImp': {
            if(fJob&&!fJob->done)break;
            delete fPanel;BMessenger target(this);BMessage chosen('mRef');chosen.AddBool("stickers",fMode==2);
            fPanel=new BFilePanel(B_OPEN_PANEL,&target,nullptr,B_FILE_NODE|B_DIRECTORY_NODE,true,&chosen);fPanel->Show();break;
        }
        case 'mRef': {
            if(fJob&&!fJob->done)break;
            std::vector<std::string> sources;
            entry_ref ref;
            for(int32 index=0;message->FindRef("refs",index,&ref)==B_OK;++index) {
                BPath path(&ref);if(path.InitCheck()==B_OK)sources.emplace_back(path.Path());
                if(sources.size()>512)break;
            }
            if(sources.empty())break;
            const bool stickers=message->GetBool("stickers",false);fMode=stickers?2:1;
            auto job=std::make_shared<Job>();job->import=true;fJob=job;fSend->SetEnabled(false);fImport->SetEnabled(false);
            fStatus->SetText("Importing into your local library…");auto library=fLibrary;
            try { std::thread([job,library,sources,stickers]{
                try {
                    auto result=library.ImportSelection(sources,stickers,&job->cancelled);
                    job->imported=result.paths.size();job->failed=result.failed;job->error=result.error;
                    if(!result.paths.empty())job->path=result.paths.front();
                } catch(...) {job->error="Could not finish importing. Completed copies are kept.";}
                job->done=true;
            }).detach();}
            catch(...){job->error="Could not start import worker.";job->done=true;}break;
        }
        case 'mSnd': {
            int32 index=fList->CurrentSelection();if(!fSend->IsEnabled()||index<0||size_t(index)>=fEntries.size())break;
            std::string path=fEntries[index].path;
            std::shared_ptr<Giphy::Transfer> transfer;
            if(fMode==3) {if(!fJob||!fJob->done||!fJob->transfer)break;transfer=fJob->transfer;path=transfer->path;}
            BEntry file(path.c_str());entry_ref ref;if(file.GetRef(&ref)!=B_OK)break;
            BMessage send('uRef');send.AddRef("refs",&ref);
            if(transfer) {
                fSend->SetEnabled(false);send.AddInt64("media-token",transfer->token);BMessage reply;
                auto status=fTarget.SendMessage(&send,&reply,500000,1500000);
                if(status!=B_OK){fStatus->SetText("Send status unknown. Check the conversation before trying again.");break;}
                if(!reply.GetBool("accepted",false)){fStatus->SetText("Not sent. Check the connection and active upload.");fSend->SetEnabled(true);break;}
            } else fTarget.SendMessage(&send);
            PostMessage(B_QUIT_REQUESTED);break;
        }
        case 'mAct':Activate();break;
        default:BWindow::MessageReceived(message);
    }
}
