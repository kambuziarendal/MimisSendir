/*
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include "GlooxHandler.h"

#include "bob.h"
#include "BobStore.h"
#include "Extensions.h"
#include "media.h"

#include "support/LogHandler.h"

#include <Catalog.h>
#include <GridView.h>
#include <LayoutBuilder.h>
#include <String.h>
#include <StringView.h>
#include <TextView.h>

#include <gloox/dataformitem.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "GlooxHandler"


GlooxHandler::GlooxHandler(gloox::Client* client)
	: BHandler("gloox connection")
	, fClient(client)
{
	fRegistration = new gloox::Registration(fClient);
	fRegistration->registerRegistrationHandler(this);
	fClient->registerConnectionListener(this);

	// BOB handling was in a former gloox 1.1 branch but was deleted?
	// So we have our own version, but we must register it here.
	gloox::BOB* bob = new gloox::BOB();
	bob->RegisterBobHandler(BobStore::Instance());
	fClient->registerStanzaExtension(bob);

	// And this one isn't in Gloox yet. TODO upstream it
	Media* media = new Media();
	media->RegisterMediaHandler(this);
	fClient->registerStanzaExtension(media);
}

void GlooxHandler::Run()
{
	thread_id thread = spawn_thread(GlooxThread, "gloox connection", B_LOW_PRIORITY, this);
	resume_thread(thread);
}


int32 GlooxHandler::GlooxThread(void* arg)
{
	GlooxHandler* object = (GlooxHandler*)arg;

	object->fClient->connect();

	return 0;
}


void GlooxHandler::handleRegistrationFields(const gloox::JID&, int, std::string)
{
	SendNotices(kRegistrationFields);
}


void GlooxHandler::handleAlreadyRegistered(const gloox::JID&)
{
	SendNotices(kAlreadyRegistered);
}


void GlooxHandler::handleRegistrationResult(const gloox::JID& from, gloox::RegistrationResult result)
{
	BMessage* message = new BMessage(kRegistrationResult);
	message->AddString("gloox::JID", from.full().c_str());
	message->AddInt32("gloox::RegistrationResult", result);
	SendNotices(kRegistrationResult, message);
}


void GlooxHandler::handleDataForm(const gloox::JID& from, const gloox::DataForm& form)
{
	BMessage message(kDataForm);

	BGridView* registrationForm = new BGridView("gloox::DataForm");

	int line = 0;

	if (!form.title().empty()) {
		BStringView* titleView = new BStringView("gloox::title", form.title().c_str());
		titleView->SetFont(be_bold_font);

		BLayoutBuilder::Grid<>(registrationForm).Add(titleView, 0, line++, 3, 1);
	}

	if (!form.instructions().empty()) {
		BString instructions;
		for (const auto& line: form.instructions()) {
			instructions << line.c_str() << "\n";
		}

		BTextView* instructionsView = new BTextView("gloox::instructions");
		instructionsView->MakeEditable(false);
		instructionsView->MakeSelectable(false);
		instructionsView->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		instructionsView->SetText(instructions);
		float charSize = instructionsView->StringWidth("W");
		instructionsView->SetExplicitMinSize(BSize(charSize * 30, charSize * 7));

		BLayoutBuilder::Grid<>(registrationForm).Add(instructionsView, 0, line++, 3, 1);
	}

	bool hasRequiredFields = false;

	for (auto f: form.fields()) {
		switch(f->type()) {
			case gloox::DataFormField::TypeTextSingle:
			case gloox::DataFormField::TypeTextPrivate:
			case gloox::DataFormField::TypeHidden:
			{
				BTextControl* fieldView = new BTextControl(f->name().c_str(),
					f->label().c_str(), f->value().c_str(), NULL);
				if (!f->description().empty()) {
					fieldView->SetToolTip(f->description().c_str());
				}
				BLayoutBuilder::Grid<>(registrationForm).Add(
					fieldView->CreateLabelLayoutItem(), 0, line);
				BLayoutBuilder::Grid<>(registrationForm).Add(
					fieldView->CreateTextViewLayoutItem(), 1, line);
				if (f->required() && f->type() != gloox::DataFormField::TypeHidden) {
					BLayoutBuilder::Grid<>(registrationForm).Add(
						new BStringView("gloox::requiredMarker", "*"), 2, line);
				}

				if (f->type() == gloox::DataFormField::TypeTextPrivate) {
					fieldView->TextView()->HideTyping(true);
				}
				if (f->type() == gloox::DataFormField::TypeHidden) {
					fieldView->Hide();
				}
				break;
			}
			case gloox::DataFormField::TypeFixed:
			{
				BStringView* fieldView = new BStringView(f->name().c_str(), f->value().c_str());
				if (!f->description().empty()) {
					fieldView->SetToolTip(f->description().c_str());
				}
				if (!f->label().empty()) {
					fprintf(stderr, "Fixed field without label\n");
				}
				BLayoutBuilder::Grid<>(registrationForm).Add(
					fieldView, 0, line, 2, 1);
				break;
			}
			default:
			{
				fprintf(stderr, "Unhandled form type %d for %s [%s] (%d)\n",
					f->type(), f->name().c_str(), f->label().c_str(), f->required());
				break;
			}
		}
		line++;
	}

	if (hasRequiredFields) {
		BLayoutBuilder::Grid<>(registrationForm).Add(
			new BStringView("footer", B_TRANSLATE("* required fields")), 0, line++);
	}
#if 1
	// TODO need to handle nested items if there are some (can anyone trigger this code? let us
	// know how!)
	for (const gloox::DataFormItem* i: form.items()) {
		fprintf(stderr, "-------------\n");
		for (auto f: i->fields()) {
			fprintf(stderr, "%s [%s] (%d)\n", f->name().c_str(),
				f->label().c_str(), f->required());
		}
	}
#endif

	registrationForm->Archive(&message);
	delete registrationForm;

	message.AddString("gloox::JID", from.full().c_str());
	SendNotices(kDataForm, &message);
}


void GlooxHandler::handleOOB(const gloox::JID&, const gloox::OOB& oob)
{
	BMessage message(kOOB);
	message.AddString("url", oob.url().c_str());
	message.AddString("desc", oob.desc().c_str());
	SendNotices(kOOB, &message);
}


void GlooxHandler::onConnect()
{
	// Ok, not very clean, the listener could tell us to do that instead...
	if (fRegistration)
		fRegistration->fetchRegistrationFields();
	SendNotices(kConnect);
}


void GlooxHandler::onDisconnect(gloox::ConnectionError error)
{
	BMessage message(kDisconnect);
	message.AddInt32("gloox::ConnectionError", error);
	if (error == gloox::ConnStreamError)
		message.AddInt32("gloox::StreamError", fClient->streamError());
	SendNotices(kDisconnect, &message);
}


bool GlooxHandler::onTLSConnect(const gloox::CertInfo& info)
{
	// TODO let the listener (if any?) decide what to do with invalid
	// certificates. Add the relevant info to the notice, and wait for a
	// reply before moving on. But what if the listener does not reply or if
	// there is no listener? 

	// TODO add the relevant fields to the notice so the listener can make a
	// decision
	SendNotices(kTLSConnect);
	return info.status == gloox::CertOk;
}


void GlooxHandler::handleMedia(const Media* media)
{
	BMessage message(kMedia);
	message.AddString("type", media->type().c_str());
	message.AddString("uri", media->uri().c_str());
	SendNotices(kMedia, &message);
}


void GlooxHandler::createAccount(gloox::DataForm* form)
{
	fRegistration->createAccount(form);
}
