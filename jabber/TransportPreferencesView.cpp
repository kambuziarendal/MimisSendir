//////////////////////////////////////////////////
// Blabber [MessagesPreferencesView.cpp]
//////////////////////////////////////////////////

#include "TransportPreferencesView.h"

#include <cstdio>

#include "AgentList.h"
#include "BlabberSettings.h"
#include "JabberSpeak.h"
#include "Messages.h"

#include "ui/ModalAlertFactory.h"

#include <GridLayout.h>
#include <LayoutBuilder.h>

TransportPreferencesView::TransportPreferencesView()
	: BView ("transport preferences", B_WILL_DRAW)
{
	_current_transport_registered = false;
	_curr_transport               = "";

	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BGridLayout* layout = new BGridLayout();
	SetLayout(layout);

	// transport selection
	_agent_entries = new BPopUpMenu("<select a service>");

	_agent_list = new BMenuField("agent_registrations", "Online service: ", _agent_entries);

	_username = new BTextControl("username", "Username: ", NULL, NULL);
	_username->SetDivider(_username->Divider() - 3);

	_password = new BTextControl("password", "Password: ", NULL, NULL);
	_password->TextView()->HideTyping(true);
	_password->SetDivider(_password->Divider() - 3);

	rgb_color note = ui_color(B_PANEL_TEXT_COLOR);
	BFont black_9(be_plain_font);
	black_9.SetSize(9.0);

	_transport_id_info = new BTextView(NULL, &black_9, &note, B_WILL_DRAW);
	_transport_id_info->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	_transport_id_info->MakeEditable(false);
	_transport_id_info->MakeSelectable(false);
	_transport_id_info->SetText("Please enter all usernames and passwords EXACTLY as you'd type them with their respective clients.");

	_register = new BButton("register", "Register", new BMessage(REGISTER_TRANSPORT));
	_unregister = new BButton("register", "Unregister", new BMessage(UNREGISTER_TRANSPORT));

	BTextView *enter_note = new BTextView(NULL, &black_9, &note, B_WILL_DRAW);
	enter_note->SetViewColor(216, 216, 216, 255);
	enter_note->MakeEditable(false);
	enter_note->MakeSelectable(false);
	enter_note->SetText("Note: Transports serve as the means by which XMPP "
		"communicates with external chat systems such as IRC. They "
		"are add-on components of the XMPP server you are logged on and "
		"not part of MímisSendir.");

	BLayoutBuilder::Grid<>(this)
		.SetInsets(B_USE_WINDOW_SPACING)
		.AddMenuField(_agent_list, 0, 0, B_ALIGN_LEFT, 1, 3)
		.AddTextControl(_username, 0, 1, B_ALIGN_LEFT, 1, 3)
		.AddTextControl(_password, 0, 2, B_ALIGN_LEFT, 1, 3)
		.Add(_transport_id_info, 0, 3, 4)
		.Add(_register, 1, 4)
		.Add(_unregister, 2, 4)
		.Add(enter_note, 0, 5, 4)
	.End();
}


TransportPreferencesView::~TransportPreferencesView()
{
}


void TransportPreferencesView::AttachedToWindow()
{
	_register->SetTarget(this);
	_register->SetEnabled(false);

	_unregister->SetTarget(this);
	_unregister->SetEnabled(false);
}


void TransportPreferencesView::MessageReceived(BMessage *msg)
{
	AgentList *agents = AgentList::Instance();

	switch (msg->what) {
		case UNREGISTER_TRANSPORT: {
			if (agents->GetAgentByService(_curr_transport)) {
				JabberSpeak::Instance()->UnregisterWithAgent(_curr_transport);
			}

			break;
		}

		case TRANSPORT_UPDATE: {
			Agent *agent = const_cast<Agent *>(agents->GetAgentByService(_curr_transport));

			if (agent) {
				_username->SetText(agent->Username().c_str());
				_password->SetText(agent->Password().c_str());
			}

			break;
		}

	}

	// buttons enabled
	if (agents->GetAgentByService(_curr_transport)) {
		_current_transport_registered = agents->GetAgentByService(_curr_transport)->IsRegistered();
	}

	// BUGBUG workaround for an outrageous hack! ;)
	if (_register && _unregister) {
		if (_current_transport_registered) {
			_register->SetEnabled(false);
			_unregister->SetEnabled(true);
		} else {
			_register->SetEnabled(true);
			_unregister->SetEnabled(false);
		}
	}
}

void TransportPreferencesView::UpdateFile() {
}
