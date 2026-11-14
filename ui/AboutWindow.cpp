//////////////////////////////////////////////////
// Blabber [AboutWindow.cpp]
//////////////////////////////////////////////////

#include "AboutWindow.h"
#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AboutWindow"


AboutWindow *AboutWindow::_instance = NULL;

AboutWindow *AboutWindow::Instance() {
	if (_instance == NULL) {
		// create window singleton
		_instance = new AboutWindow();
	}

	return _instance;
}

AboutWindow::AboutWindow()
	: BAboutWindow(B_TRANSLATE_SYSTEM_NAME("MímisSendir"), "application/x-vnd.Kambuzi-MimisSendir") {
	SetVersion("0.1.0-beta.14");
	// draw credits
	PopulateCredits();
}

AboutWindow::~AboutWindow() {
	_instance = NULL;
}

void AboutWindow::PopulateCredits() {
	AddDescription(B_TRANSLATE("An XMPP instant messaging client"));
    AddExtraInfo(B_TRANSLATE("MímisSendir is a fork of Renga. Original authors and MIT license retained."));
    AddExtraInfo(B_TRANSLATE("Developed by Kambuzi EDB with AI-assisted programming using OpenAI Codex through OpenClaw."));
	const char * authors[] = {
		"Kambuzi EDB (MímisSendir fork)",
		"Pascal Abresch",
		"Andrea Anzani",
		"John Blanco",
		"Adrien Destugues",
		"Frank Paul Silye",
		NULL
	};
	AddAuthors(authors);
	const char * special[] = {
		"Michele Blanco",
		NULL
	};
	AddSpecialThanks(special);
	const char * graphics[] = {
		"Michele Frau",
		NULL
	};
	AddText(B_TRANSLATE("Graphics"), graphics);
	AddExtraInfo(B_TRANSLATE("Based on Jabber for BeOS by John Blanco"));
	AddExtraInfo(B_TRANSLATE("Powered by Gloox by Jakob Schröter"));
}

