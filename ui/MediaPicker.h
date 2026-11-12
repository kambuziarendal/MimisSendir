// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Window.h>
#include <Messenger.h>
#include <memory>
#include "support/MediaLibrary.h"
class BMenuField; class BButton; class BTextControl; class BStringView; class BListView;
class BCardLayout; class BFilePanel; class BMessageRunner; class PreviewView;
class MediaPicker final : public BWindow {
public:
    MediaPicker(BMessenger target, const std::string& account, const std::string& conversation, BPoint position, std::string reactionId = "");
    ~MediaPicker();
    void MessageReceived(BMessage*) override;
private:
    struct Job;
    struct SearchJob;
    void Refresh();
    void SearchGiphy();
    void PreviewSelection();
    void Poll();
    BMessenger fTarget;
    std::string fReactionId;
    Media::Library fLibrary;
    BTextControl* fSearch;
    BButton* fEmoji[24];
    BMenuField* fCategory;
    BButton* fEditFavorites;
    std::string fCategoryName;
    bool fEditingFavorites = false;
    BButton *fPrevious, *fNext, *fImport, *fRemove, *fSend, *fSearchOnline, *fOnlinePrevious, *fOnlineNext;
    BStringView* fStatus;
    BView* fAttribution;
    BListView* fList;
    BCardLayout* fCards;
    PreviewView* fPreview;
    BFilePanel* fPanel = nullptr;
    BMessageRunner* fTimer;
    std::vector<Media::Entry> fEntries;
    std::vector<size_t> fFiltered;
    std::shared_ptr<Job> fJob;
    std::shared_ptr<SearchJob> fSearchJob;
    unsigned fOffset = 0;
    bool fOnlineControls = false;
    std::string fPreviewPath;
    size_t fFrame = 0, fPage = 0;
    bigtime_t fNextFrame = 0;
    int fMode = 0;
};
