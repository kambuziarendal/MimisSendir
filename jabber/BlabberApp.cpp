//////////////////////////////////////////////////
// Blabber [BlabberApp.cpp]
//////////////////////////////////////////////////

#include <cstdlib>
#include <cstring>
#include <AppFileInfo.h>
#include <File.h>

#include <Beep.h>
#include <MimeType.h>
#include <Entry.h>
#include <Path.h>
#include <Roster.h>
#include "ui/StickerPackWindow.h"
#include "network/SignalStickers.h"
#include <InterfaceKit.h>

#include "BlabberApp.h"

#include "BlabberSettings.h"
#include "JabberSpeak.h"
#include "MessageRepeater.h"
#include "SoundSystem.h"

#include "ui/AboutWindow.h"
#include "ui/MainWindow.h"
#include "ui/ModalAlertFactory.h"

const char* kAppMIMEType = "application/x-vnd.Haiku-Jabber";

BlabberApp::BlabberApp()
	: BApplication(kAppMIMEType)
{
    for(const char* type : {"application/x-vnd.Be.URL.sgnl","application/x-vnd.Be.URL.renga-stickers"}) {
        BMimeType mime(type);if(!mime.IsInstalled())mime.Install();
        char preferred[B_MIME_TYPE_LENGTH]={};
        if(mime.GetPreferredApp(preferred)!=B_OK||!*preferred||std::string(preferred)==kAppMIMEType)
            mime.SetPreferredApp("application/x-vnd.Kambuzi-RengaStickerLink");
    }
    // Re-register the executable the user actually started. A development or
    // predecessor path may otherwise remain cached by Haiku's MIME database.
    app_info runningApp;
    if (GetAppInfo(&runningApp) == B_OK) {
        BMimeType appMime(kAppMIMEType);
        appMime.SetAppHint(&runningApp.ref);
        // Keep the established MIME/keyring identity, but refresh a predecessor's
        // cached app icon from the executable actually running after an upgrade.
        BFile executable(&runningApp.ref, B_READ_ONLY);
        BAppFileInfo info(&executable);
        uint8* icon = nullptr;
        size_t iconSize = 0;
        if (info.GetIcon(&icon, &iconSize) == B_OK) {
            uint8* cached = nullptr;
            size_t cachedSize = 0;
            if (appMime.GetIcon(&cached, &cachedSize) != B_OK
                || cachedSize != iconSize || std::memcmp(cached, icon, iconSize) != 0)
                appMime.SetIcon(icon, iconSize);
            std::free(cached);
        }
        std::free(icon);
    }
    BMimeType packFile("application/x-vnd.Kambuzi-RengaSticker");
    if (!packFile.IsInstalled()) packFile.Install();
    packFile.SetShortDescription("MímisSendir sticker pack link");
    packFile.SetPreferredApp(kAppMIMEType);
    BMessage extensions;
    extensions.AddString("extensions", "renga-sticker");
    packFile.SetFileExtensions(&extensions);
    packFile.SetSnifferRule("0.9 [0] ('RENGA-STICKER-LINK 1')");

	// launch the message repeater looper
	MessageRepeater::Instance()->Run();

	// display opening window
	BlabberMainWindow::Instance()->Show();

	// launch initial login process
	BlabberMainWindow::Instance()->PostMessage(kLoadCredentials);

	// TODO: System beeps don't allow default values
	// TODO: System beeps do not let us access the value or use something other than a file (for
	// example, to use executable resources)
	//
	// A workaround would be to check for system_beep returning an error, and then playing the
	// default sound from resources in that case. But then you can't separate the case of a
	// disabled sound event from a default one.
	//
	// TODO: System sound events can't be localized. There no internal event names or IDs.
	add_system_beep_event(kSoundNewMessage);
	add_system_beep_event(kSoundNewMUCMessage);
	add_system_beep_event(kSoundUserOnline);
	add_system_beep_event(kSoundUserOffline);
	add_system_beep_event(kSoundHighlight);
}

BlabberApp::~BlabberApp()
{
	// take down the message repeater
	MessageRepeater::Instance()->Lock();
	MessageRepeater::Instance()->Quit();

	// take down the network queue
	delete JabberSpeak::Instance();
	
	// clean up the settings module
	delete BlabberSettings::Instance();
}


void
BlabberApp::AboutRequested()
{
	AboutWindow::Instance()->Show();
	AboutWindow::Instance()->Activate();
}

bool
BlabberApp::QuitRequested()
{
	JabberSpeak::Instance()->SendDisconnect();
	if (!JabberSpeak::Instance()->IsReadyForConnection()) {
		fQuitPending = true;
		return false;
	}
	BookmarkManager::Instance().Disconnect();
	return BApplication::QuitRequested();
}



void BlabberApp::MessageReceived(BMessage* message)
{
	if (message->what == kConnectionStopped) {
		if (fQuitPending) PostMessage(B_QUIT_REQUESTED);
		return;
	}
	BApplication::MessageReceived(message);
}

void BlabberApp::OpenStickerPack(const SignalStickers::Link& link) {
    if (fStickerPackWindow.IsValid()) {
        fStickerPackWindow.SendMessage('sAct');
        (new BAlert("Sticker pack", "Finish or close the current sticker pack, then open the next link.", "OK"))->Go(nullptr);
        return;
    }
    auto account = gloox::JID(BlabberSettings::Instance()->Data("last-login").value_or("")).bare();
    if (account.empty()) {
        (new BAlert("Sticker pack", "Sign in to MímisSendir once, then open the sticker pack link again.", "OK"))->Go(nullptr);
        return;
    }
    auto* window = new StickerPackWindow(link, account);
    fStickerPackWindow = BMessenger(window);
    window->Show();
}

void BlabberApp::OpenStickerFile(const char* path) {
    std::string error;
    auto link = SignalStickers::ReadLinkFile(path, error);
    if (link) OpenStickerPack(*link);
    else (new BAlert("Sticker pack", error.c_str(), "OK"))->Go(nullptr);
}

void BlabberApp::RefsReceived(BMessage* message) {
    entry_ref ref;
    for (int32 index = 0; message->FindRef("refs", index, &ref) == B_OK; ++index) {
        BEntry entry(&ref);
        BPath path;
        if (entry.GetPath(&path) == B_OK) OpenStickerFile(path.Path());
        else (new BAlert("Sticker pack", "Could not locate this sticker link file. Download it again.", "OK"))->Go(nullptr);
    }
}

void BlabberApp::ArgvReceived(int32 argc, char** argv) {
    for (int32 index = 1; index < argc; ++index) {
        auto link = SignalStickers::ParseLink(argv[index]);
        if (link) OpenStickerPack(*link);
        else OpenStickerFile(argv[index]);
    }
}
