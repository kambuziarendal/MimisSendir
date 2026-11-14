// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once

#include "ui/textview/TextDocumentView.h"
#include "ui/JumpToLatestButton.h"

// Anchor the overlay to visible document bounds, including synchronous scrolls.
// It is deliberately not a layout item: visibility never reflows messages.
class ConversationTimeline : public TextDocumentView {
public:
    explicit ConversationTimeline(const char* name = "Timeline")
        : TextDocumentView(name), fJump(new JumpToLatestButton())
    {
        fJump->ResizeTo(36, 36);
        fJump->Hide();
        AddChild(fJump);
    }

    BButton* JumpButton() const { return fJump; }

    void UpdateJumpButton(bool visible)
    {
        // History restoration and scrollbar range changes may settle after
        // layout. Re-anchor before exposing the control, even without input.
        PositionButton();
        if (visible && fJump->IsHidden()) fJump->Show();
        if (!visible && !fJump->IsHidden()) fJump->Hide();
    }

    void ScrollTo(BPoint position) override
    {
        TextDocumentView::ScrollTo(position);
        PositionButton();
    }

    void FrameResized(float width, float height) override
    {
        TextDocumentView::FrameResized(width, height);
        PositionButton();
    }

private:
    void PositionButton()
    {
        const BRect bounds = Bounds();
        fJump->MoveTo(bounds.right - fJump->Bounds().Width() - 12,
            bounds.bottom - fJump->Bounds().Height() - 12);
    }

    JumpToLatestButton* fJump;
};
