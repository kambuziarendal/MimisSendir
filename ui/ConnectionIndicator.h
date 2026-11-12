// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once

#include <View.h>
#include "ActionIcon.h"
#include <array>

// Compact connection state. Shape and color both distinguish readiness;
// the complete status stays available as a tooltip, without a footer sentence.
class ConnectionIndicator final : public BView {
public:
    enum State { Offline, Connecting, Limited, Ready };
    ConnectionIndicator() : BView("connection-status",B_WILL_DRAW) {
        SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
        const char* names[] = {"status-offline", "status-connecting", "status-limited", "status-ready"};
        for (int i = 0; i < 4; ++i) fIcons[i] = LoadActionIcon(names[i], 24);
        SetExplicitMinSize(BSize(26,26));
        SetExplicitMaxSize(BSize(26,26));
        SetState(Offline,"Disconnected");
    }
    void SetState(State state,const char* detail) {
        fState=state;SetToolTip(detail);Invalidate();
    }
    void SetDetail(const char* detail) { SetToolTip(detail); }
    void Draw(BRect) override {
        PushState();SetHighUIColor(B_PANEL_BACKGROUND_COLOR);FillRect(Bounds());
        DrawActionIcon(this, fIcons[fState].get(), BRect(1, 1, 24, 24));
        PopState();
    }
private:
    State fState=Offline;
    std::array<std::shared_ptr<BBitmap>, 4> fIcons;
};
