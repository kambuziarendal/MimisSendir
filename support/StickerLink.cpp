// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include <Application.h>
#include <Roster.h>
#include <Alert.h>
// A short-lived protocol handler makes BRoster return B_OK to browsers even
// when Renga is already running. WebPositive treats B_ALREADY_RUNNING as an
// unhandled protocol and would otherwise submit the URL to its search engine.
class StickerLink final : public BApplication {
public:
    StickerLink():BApplication("application/x-vnd.Kambuzi-RengaStickerLink"){}
    void ArgvReceived(int32 count,char** args) override {
        if(count<2)return;
        const char* link=args[1];
        const status_t result=be_roster->Launch("application/x-vnd.Haiku-Jabber",1,&link);
        if(result!=B_OK && result!=B_ALREADY_RUNNING)
            (new BAlert("MímisSendir","Could not open MímisSendir. Start MímisSendir once, then try the sticker link again.","OK"))->Go();
    }
    void ReadyToRun() override {PostMessage(B_QUIT_REQUESTED);}
};
int main(){StickerLink app;app.Run();return 0;}
