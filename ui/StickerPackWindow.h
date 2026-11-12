// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Window.h>
#include <memory>
#include "network/SignalStickers.h"
class BButton;class BStringView;class BMessageRunner;class StickerPackPreview;
class StickerPackWindow final : public BWindow {
public:
    StickerPackWindow(const SignalStickers::Link&,const std::string& account);
    ~StickerPackWindow();
    void MessageReceived(BMessage*) override;
private:
    struct Prepared;
    struct Job;
    void Poll();
    void Import();
    std::string fAccount;
    BStringView *fTitle,*fAuthor,*fStatus;
    BButton* fImport;
    StickerPackPreview* fPreview;
    BMessageRunner* fTimer;
    std::shared_ptr<Job> fJob;
    std::shared_ptr<Prepared> fPrepared;
};
