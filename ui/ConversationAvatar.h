// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <String.h>
#include "AvatarCache.h"
#include <View.h>

class ConversationAvatar final : public BView {
public:
    explicit ConversationAvatar(const char* name)
        : BView("conversation-avatar", B_WILL_DRAW) {
        SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
        SetExplicitMinSize(BSize(42,42)); SetExplicitMaxSize(BSize(42,42));
        SetInitial(name);
    }
    void SetInitial(const char* name) { fInitial = name; fInitial.TruncateChars(1); Invalidate(); }
    void SetIdentity(const std::string& jid){fImage=Avatars::Get(Avatars::Account(),jid);Invalidate();}
    void Draw(BRect) override { Avatars::Draw(this,Bounds().InsetByCopy(2,2),fImage,fInitial.String()); }
private:
    BString fInitial;
    std::shared_ptr<Avatars::Image> fImage;
};
