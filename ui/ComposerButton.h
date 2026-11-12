// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include "ui/ActionIconButton.h"

class ComposerButton final : public ActionIconButton {
public:
    enum Kind { Emoji, Attachment, Send };
    ComposerButton(const char* name, const char* label, BMessage* message, Kind kind)
        : ActionIconButton(name, label, message,
            kind == Emoji ? "emoji" : kind == Attachment ? "attachment" : "send", 28, 36) {}
};

#include "ui/ColorEmoji.h"
// A single-row picker item. Inserting an emoji only changes the draft.
class ComposerChoice final : public BButton {
public:
    ComposerChoice(const char* label,BMessage* action,bool emoji=false)
        : BButton(label,label,action),fImage(emoji?ColorEmoji(label):nullptr) {
        const BSize size(emoji?28:StringWidth(label)+16,28);
        SetExplicitMinSize(size);SetExplicitMaxSize(size);
    }
    void Draw(BRect) override {
        PushState();auto background=ui_color(B_PANEL_BACKGROUND_COLOR);
        SetHighColor(background);FillRect(Bounds());
        if(Value()==B_CONTROL_ON){SetHighColor(tint_color(background,B_DARKEN_1_TINT));FillRoundRect(Bounds(),5,5);}
        const BPoint center(Bounds().Width()/2,Bounds().Height()/2);
        if(fImage){SetDrawingMode(B_OP_ALPHA);DrawBitmap(fImage.get(),fImage->Bounds(),BRect(center.x-10,center.y-10,center.x+10,center.y+10));}
        else {SetHighUIColor(B_PANEL_TEXT_COLOR);BFont font;GetFont(&font);font_height metrics;font.GetHeight(&metrics);DrawString(Label(),BPoint(center.x-font.StringWidth(Label())/2,center.y+(metrics.ascent-metrics.descent)/2));}
        if(IsFocus()){SetHighUIColor(B_KEYBOARD_NAVIGATION_COLOR);StrokeLine(BPoint(5,Bounds().bottom),BPoint(Bounds().right-5,Bounds().bottom));}
        PopState();
    }
private:
    std::shared_ptr<BBitmap> fImage;
};
