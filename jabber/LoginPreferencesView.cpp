//////////////////////////////////////////////////
// Blabber [LoginpreferencesView.h]
//////////////////////////////////////////////////

#include "LoginPreferencesView.h"

#include "BlabberSettings.h"

#include <LayoutBuilder.h>


LoginPreferencesView::LoginPreferencesView()
	: BGroupView ("Login preferences", B_VERTICAL)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	_surrounding_options = new BBox("Options");
	_surrounding_options->SetLabel("Options");

	_surrounding = new BBox("Slash syntax");
	_surrounding->SetLabel("/ Syntax");

	_surrounding_groupchat = new BBox("Group chat options");
	_surrounding_groupchat->SetLabel("Group chat options");

	_surrounding_chatlog = new BBox("Chat log options");
	_surrounding_chatlog->SetLabel("Chat log options");

	BLayoutBuilder::Group<>(this)
		.SetInsets(B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING)
		.Add(_surrounding_options)
		.Add(_surrounding)
		.Add(_surrounding_groupchat)
		.Add(_surrounding_chatlog)
	.End();

	_show_timestamp = new BCheckBox(NULL, "Attach timestamps to all messages.", NULL);
	_show_all_chat = new BCheckBox(NULL, "Convert all incoming ICQ-style messages to AOL-style chat.", NULL);
	_double_click = new BCheckBox(NULL, "Double-clicking on a user starts a chat.", NULL);

	BGroupView* grp = new BGroupView("group", B_VERTICAL);

	_surrounding_options->AddChild(grp);

	BLayoutBuilder::Group<>(grp, B_VERTICAL, 0)
		.SetInsets(B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING)
		.Add(_show_timestamp)
		.Add(_show_all_chat)
		.Add(_double_click)
	.End();

	_auto_login = new BCheckBox(NULL, "Disallow /alert from interrupting me.", NULL);
	_focus_on_chat = new BCheckBox(NULL, "Prevent new chat windows from interrupting me.", NULL);

	grp = new BGroupView("group", B_VERTICAL);

	_surrounding->AddChild(grp);

	BLayoutBuilder::Group<>(grp, B_VERTICAL, 0)
		.SetInsets(B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING)
		.Add(_auto_login)
		.Add(_focus_on_chat)
	.End();

	_name = new BTextControl(NULL, "Channel name:", NULL, NULL);

	grp = new BGroupView("group", B_VERTICAL);
	_surrounding_groupchat->AddChild(grp);
	BLayoutBuilder::Group<>(grp, B_VERTICAL, 0)
		.SetInsets(B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING)
		.Add(_name)
	.End();

	_autoopen_chatlog = new BCheckBox(NULL, "Auto-open chat log", NULL);
	_chatlog_path = new BTextControl(NULL, "Path:", NULL, NULL);

	grp = new BGroupView("group", B_VERTICAL);
	_surrounding_chatlog->AddChild(grp);
	BLayoutBuilder::Group<>(grp, B_VERTICAL, 0)
		.SetInsets(B_USE_WINDOW_SPACING, 0)
		.Add(_autoopen_chatlog)
		.Add(_chatlog_path)
	.End();
}


LoginPreferencesView::~LoginPreferencesView()
{
}


void LoginPreferencesView::AttachedToWindow()
{
	_auto_login->SetValue(BlabberSettings::Instance()->Tag("suppress-alert"));
	_focus_on_chat->SetValue(BlabberSettings::Instance()->Tag("suppress-chat-focus"));

	_show_timestamp->SetValue(BlabberSettings::Instance()->Tag("show-timestamp"));
	_show_all_chat->SetValue(BlabberSettings::Instance()->Tag("convert-messages-to-chat"));
	_double_click->SetValue(BlabberSettings::Instance()->Tag("enable-double-click"));

	std::optional<std::string> s = BlabberSettings::Instance()->Data("channel-name");
	if (s)
		_name->SetText(s->c_str());
	else
		_name->SetText("");

	_autoopen_chatlog->SetValue(BlabberSettings::Instance()->Tag("autoopen-chatlog"));
	s = BlabberSettings::Instance()->Data("chatlog-path");
	if (s)
		_chatlog_path->SetText(s->c_str());
	else
		_chatlog_path->SetText("");
}

void LoginPreferencesView::UpdateFile()
{
	BlabberSettings::Instance()->SetTag("suppress-alert", _auto_login->Value());
	BlabberSettings::Instance()->SetTag("suppress-chat-focus", _focus_on_chat->Value());
	BlabberSettings::Instance()->SetTag("show-timestamp", _show_timestamp->Value());
	BlabberSettings::Instance()->SetTag("convert-messages-to-chat", _show_all_chat->Value());
	BlabberSettings::Instance()->SetTag("enable-double-click", _double_click->Value());
	BlabberSettings::Instance()->SetData("channel-name", _name->Text());
	BlabberSettings::Instance()->SetTag("autoopen-chatlog", _autoopen_chatlog->Value());
	BlabberSettings::Instance()->SetData("chatlog-path", _chatlog_path->Text());
}
