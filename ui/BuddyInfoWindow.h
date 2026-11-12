//////////////////////////////////////////////////
// Blabber [BuddyInfoWindow.h]
//////////////////////////////////////////////////

#pragma once

#include <interface/Box.h>
#include <interface/StringView.h>
#include <interface/Window.h>

#include "../jabber/UserID.h"

class BGridLayout;
class BarberPole;
class PictureView;

class BuddyInfoWindow : public BWindow
{
public:
					BuddyInfoWindow(UserID *querying_user);
					~BuddyInfoWindow();

	void			Show() override;
	void			MessageReceived(BMessage*) override;

private:
	BGridLayout*	fDetails;
	PictureView*	fAvatar;
	BarberPole*		fSpinner;
	BString fJID;
};

