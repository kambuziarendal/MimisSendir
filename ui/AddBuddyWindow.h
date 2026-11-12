#ifndef ADD_BUDDY_WINDOW_H
#define ADD_BUDDY_WINDOW_H

#include <interface/OptionPopUp.h>
#include <interface/StringView.h>
#include <interface/TextControl.h>
#include <interface/View.h>
#include <interface/Window.h>

class AddBuddyWindow : public BWindow {
public:
	static AddBuddyWindow	*Instance(BRect frame);
							~AddBuddyWindow();

	void					MessageReceived(BMessage *msg);
	bool					QuitRequested();

	void					AddNewUser();

protected:
							AddBuddyWindow(BRect frame);

private:
	static AddBuddyWindow	*fInstance;

	BTextControl			*fNickName;
	BOptionPopUp			*fServiceSelection;
	BTextControl			*fHandle;
	BStringView				*fHelpText;

};

#endif /* ADD_BUDDY_WINDOW_H */
