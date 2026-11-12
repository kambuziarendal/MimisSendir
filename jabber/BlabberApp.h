//////////////////////////////////////////////////
// Blabber [BlabberApp.h]
//     Application control for BeOS.
//////////////////////////////////////////////////

#ifndef BLABBER_APP_H
#define BLABBER_APP_H

#ifndef _APPLICATION_H
	#include <app/Application.h>
#endif

#define APP_VERSION "0.1.0-beta.5"

namespace SignalStickers { struct Link; }
class BlabberMainWindow;

class BlabberApp : public BApplication {
public:
	 BlabberApp();
	~BlabberApp();

	void AboutRequested() override;
	void RefsReceived(BMessage* message) override;
	void ArgvReceived(int32 argc, char** argv) override;
	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;
private:
	void OpenStickerPack(const SignalStickers::Link& link);
	void OpenStickerFile(const char* path);
	bool fQuitPending = false;
	BMessenger fStickerPackWindow;
	
};

#endif
