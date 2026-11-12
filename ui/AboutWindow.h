//////////////////////////////////////////////////
// Blabber [AboutWindow.h]
//     Quasi-reusable about window scrolling
//     credits!
//////////////////////////////////////////////////

#ifndef ABOUT_WINDOW_H
#define ABOUT_WINDOW_H

#include <interface/TextView.h>
#include <interface/Window.h>
#include <private/interface/AboutWindow.h>

class AboutWindow : public BAboutWindow {
public:
	static AboutWindow *Instance();

public:
	                     AboutWindow();
	                    ~AboutWindow();
	 
	void                 PopulateCredits();
 
private:
	static AboutWindow *_instance;
};

#endif

