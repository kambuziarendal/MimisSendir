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
#include <FindDirectory.h>
#include <filesystem>
#include <sys/stat.h>
#include "support/ProfileMigration.h"
#include "support/CredentialStore.h"
#include "support/ServiceKeys.h"
#include "ui/AvatarCache.h"

#include "BlabberApp.h"

#include "BlabberSettings.h"
#include "JabberSpeak.h"
#include "MessageRepeater.h"
#include "SoundSystem.h"

#include "ui/AboutWindow.h"
#include "ui/MainWindow.h"
#include "ui/ModalAlertFactory.h"

const char* kAppMIMEType = "application/x-vnd.Kambuzi-MimisSendir";

namespace {
void PrepareProfile() {
    namespace fs = std::filesystem;
    BPath settings;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &settings) != B_OK) std::exit(EXIT_FAILURE);
    const fs::path parent(settings.Path()), profile = parent / "MimisSendir";
    try {
        const auto state = fs::symlink_status(profile);
        if (state.type() != fs::file_type::not_found) {
            if (state.type() == fs::file_type::directory) return;
            throw std::runtime_error("The MímisSendir profile location is not a directory. It was not changed.");
        }
        const auto legacy = parent / "jabber-for-beos";
        if (fs::symlink_status(legacy).type() != fs::file_type::not_found) {
            auto* question = new BAlert("MímisSendir profile",
                "An earlier MímisSendir or Renga profile was found. Copy its settings, history and media "
                "to a separate MímisSendir profile? The original will stay unchanged. "
                "Close Renga and earlier MímisSendir versions before copying. "
                "Haiku may ask permission to copy the saved account password.",
                "Quit", "Start fresh", "Copy profile");
            const int32 choice = question->Go();
            if (choice == 0) std::exit(EXIT_SUCCESS);
            if (choice == 2) {
                if (be_roster->IsRunning("application/x-vnd.Haiku-Jabber"))
                    throw std::runtime_error("Close Renga or the earlier MímisSendir first, then start MímisSendir again. Neither profile was changed.");
                std::string error;
                if (!ProfileMigration::Copy(legacy.string(), profile.string(), error))
                    throw std::runtime_error(error);
                auto* imported = BlabberSettings::Instance();
                // Import only the selected profile's account, never enumerate the old keyring.
                imported->SetData("import-legacy-credential",
                    imported->Data("last-login").value_or("").c_str());
                if (!imported->WriteToFile())
                    throw std::runtime_error("The profile was copied, but the credential import preference could not be saved. Check free space before restarting.");
                return;
            }
        }
        if (mkdir(profile.c_str(), 0700) != 0)
            throw std::runtime_error("Could not create the MímisSendir profile. Check free space and permissions.");
    } catch (const std::exception& error) {
        (new BAlert("MímisSendir profile", error.what(), "Quit"))->Go();
        std::exit(EXIT_FAILURE);
    }
}
}

BlabberApp::BlabberApp()
	: BApplication(kAppMIMEType)
{
    PrepareProfile();
    for(const char* type : {"application/x-vnd.Be.URL.sgnl","application/x-vnd.Be.URL.mimissendir-stickers"}) {
        BMimeType mime(type);if(!mime.IsInstalled())mime.Install();
        char preferred[B_MIME_TYPE_LENGTH]={};
        if(mime.GetPreferredApp(preferred)!=B_OK||!*preferred||std::string(preferred)==kAppMIMEType
            || std::string(preferred)=="application/x-vnd.Kambuzi-RengaStickerLink")
            mime.SetPreferredApp("application/x-vnd.Kambuzi-MimisSendirStickerLink");
    }
    // Re-register the executable the user actually started. A development or
    // predecessor path may otherwise remain cached by Haiku's MIME database.
    app_info runningApp;
    if (GetAppInfo(&runningApp) == B_OK) {
        BMimeType appMime(kAppMIMEType);
        appMime.SetAppHint(&runningApp.ref);
        // Refresh only our independent MIME identity, never upstream Renga's
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
    BMimeType packFile("application/x-vnd.Kambuzi-MimisSendirSticker");
    if (!packFile.IsInstalled()) packFile.Install();
    packFile.SetShortDescription("MímisSendir sticker pack link");
    packFile.SetPreferredApp(kAppMIMEType);
    BMessage extensions;
    extensions.AddString("extensions", "mimissendir-sticker");
    packFile.SetFileExtensions(&extensions);
    packFile.SetSnifferRule("0.9 [0] ('MIMISSENDIR-STICKER-LINK 1')");

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
    Credentials::Shutdown();
    ServiceKeys::Shutdown();
    Avatars::Shutdown();
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
	if (!JabberSpeak::Instance()->IsReadyForConnection()
        || !Credentials::Idle() || !ServiceKeys::Idle()) {
		fQuitPending = true;
		return false;
	}
	BookmarkManager::Instance().Disconnect();
	return BApplication::QuitRequested();
}



void BlabberApp::MessageReceived(BMessage* message)
{
	if (message->what == kConnectionStopped || message->what == 'bRdy') {
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
