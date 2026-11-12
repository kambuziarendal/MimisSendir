// Copyright 2026 Kambuzi EDB. Distributed under the MIT license.
#include "PreferencesWindow.h"
#include "BlabberSettings.h"
#include "JabberSpeak.h"
#include "Messages.h"
#include "config/PrivateProfile.h"
#include "support/SettingsValidation.h"
#include "ui/MainWindow.h"
#include <Messenger.h>
#include <Alert.h>
#include <Button.h>
#include <CheckBox.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>
#include <sys/stat.h>
#include <unistd.h>

PreferencesWindow* PreferencesWindow::_instance = nullptr;
PreferencesWindow* PreferencesWindow::Instance()
{
    if (!_instance) _instance = new PreferencesWindow();
    return _instance;
}
static BStringView* Note(const char* text) { return new BStringView(nullptr, text); }
BTextControl* PreferencesWindow::AddField(const char* key, const char* label,
    const char* fallback, Kind kind, int tab, bool optional)
{
    auto value = BlabberSettings::Instance()->Data(key).value_or(fallback);
    auto* control = new BTextControl(key, label, value.c_str(), nullptr);
    control->SetDivider(150);
    control->TextView()->SetMaxBytes(kind == Kind::Certificate ? 1023 : 253);
    fFields.push_back({key, control, value, kind, tab, optional});
    return control;
}
BCheckBox* PreferencesWindow::AddFlag(const char* key, const char* label)
{
    auto* control = new BCheckBox(key, label, nullptr);
    bool enabled = BlabberSettings::Instance()->Tag(key);
    control->SetValue(enabled ? B_CONTROL_ON : B_CONTROL_OFF);
    fFlags.push_back({key, control, enabled});
    return control;
}
PreferencesWindow::PreferencesWindow()
    : BWindow(BRect(0, 0, 580, 390), "MímisSendir settings", B_TITLED_WINDOW,
        B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
{
    auto* settings = BlabberSettings::Instance();
    fTabs = new BTabView("settings-tabs", B_WIDTH_FROM_LABEL);
    auto* account = new BGroupView("Account", B_VERTICAL);
    auto* connection = new BGroupView("Connection", B_VERTICAL);
    auto* bridge = new BGroupView("Bridge", B_VERTICAL);
    auto* uploads = new BGroupView("Uploads", B_VERTICAL);
    auto* messages = new BGroupView("Messages", B_VERTICAL);
    auto accountText = "XMPP account: " + settings->Data("last-login").value_or("Not configured");
    BLayoutBuilder::Group<>(account)
        .SetInsets(B_USE_DEFAULT_SPACING)
        .Add(Note(accountText.c_str()))
        .Add(AddField("last-realname", "Display name:", "", Kind::Text, 0, true))
        .Add(AddFlag("auto-login", "Sign in automatically when MímisSendir starts"))
        .Add(Note("Passwords are managed by Haiku KeyStore."))
        .Add(Note("To change your account or password, sign out and use the login screen."))
        .AddGlue();
    fPlain = new BCheckBox("plain-over-tls", "Use PLAIN authentication over TLS (compatibility)", nullptr);
    fInitialPlain = settings->Data("xmpp-auth-mechanism").value_or("") == "plain-over-tls";
    fPlain->SetValue(fInitialPlain ? B_CONTROL_ON : B_CONTROL_OFF);
    BLayoutBuilder::Group<>(connection)
        .SetInsets(B_USE_DEFAULT_SPACING)
        .Add(Note("Leave the server blank to discover it from your XMPP account."))
        .Add(AddField("xmpp-connect-host", "Server override:", "", Kind::Host, 1, true))
        .Add(AddField("xmpp-connect-port", "Override port:", "5222", Kind::Port, 1))
        .Add(AddField("xmpp-ca-file", "Trusted CA file:", "", Kind::Certificate, 1, true))
        .Add(fPlain)
        .Add(Note("TLS certificate verification stays enabled. Blank CA uses system trust."))
        .Add(Note("The port applies only when a server override is set."))
        .AddGlue();
    const bool connected = JabberSpeak::Instance()->IsConnected();
    const bool ready = JabberSpeak::Instance()->BridgeReady();
    BLayoutBuilder::Group<>(bridge)
        .SetInsets(B_USE_DEFAULT_SPACING)
        .Add(Note("Signal routing for the current XMPP account:"))
        .Add(AddField("signal-bridge-jid", "Bridge component:", PrivateProfile::kSignalDomain, Kind::Domain, 2))
        .Add(AddField("signal-contact-domain", "Contact domain:", PrivateProfile::kSignalDomain, Kind::Domain, 2))
        .Add(AddField("signal-group-domain", "Group domain:", PrivateProfile::kSignalDomain, Kind::Domain, 2))
        .Add(Note(connected ? "When opened: XMPP connected." : "When opened: XMPP disconnected."))
        .Add(Note(ready ? "When opened: bridge ready." : "When opened: bridge not ready."))
        .Add(Note("Use component domains, not web URLs. Other XMPP contacts are unaffected."))
        .Add(Note("Saving does not pair, restart or switch a Signal bridge."))
        .AddGlue();
    BLayoutBuilder::Group<>(uploads)
        .SetInsets(B_USE_DEFAULT_SPACING)
        .Add(AddField("upload-service", "Upload service:", PrivateProfile::kUploadService, Kind::Domain, 3))
        .Add(AddField("upload-authority", "Allowed HTTPS host:", PrivateProfile::kUploadAuthority, Kind::Authority, 3))
        .Add(AddField("upload-ca-file", "Trusted CA file:", "", Kind::Certificate, 3, true))
        .Add(Note("The service is an XMPP domain. The HTTPS host may include :port."))
        .Add(Note("Do not include https:// or a path. Only this HTTPS host is allowed."))
        .Add(Note("Blank CA uses system trust. Existing transfers are not changed."))
        .AddGlue();
    BLayoutBuilder::Group<>(messages)
        .SetInsets(B_USE_DEFAULT_SPACING)
        .Add(AddFlag("exclude-groupchat-sounds", "Mute incoming group-message sounds"))
        .Add(Note("Sound files and volume are configured in Haiku's Sounds preferences."))
        .Add(Note("Appearance follows Haiku's system colors, including dark palettes."))
        .Add(Note("Messages and downloaded media are stored locally on this computer."))
        .Add(Note("Saving settings does not delete history, media or account credentials."))
        .Add(Note("Settings location: ~/config/settings/jabber-for-beos/app-settings"))
        .AddGlue();
    for (auto* view : {account, connection, bridge, uploads, messages}) {
        view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
        fTabs->AddTab(view);
    }
    auto* save = new BButton("save-settings", "Save", new BMessage(JAB_OK));
    save->MakeDefault(true);
    BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .SetInsets(B_USE_WINDOW_SPACING)
        .Add(fTabs)
        .Add(Note("Account, connection, bridge and upload changes apply on the next sign-in."))
        .AddGroup(B_HORIZONTAL)
            .AddGlue()
            .Add(new BButton("cancel-settings", "Cancel", new BMessage(JAB_CANCEL)))
            .Add(save)
        .End();
    CenterOnScreen();
}
PreferencesWindow::~PreferencesWindow() { _instance = nullptr; }
void PreferencesWindow::Save()
{
    std::map<std::string, std::optional<std::string>> changes;
    for (const auto& field : fFields) {
        std::string value = field.control->Text();
        if (value == field.initial) continue;
        bool valid = false;
        if (value.empty() && field.optional) valid = true;
        else switch (field.kind) {
            case Kind::Text: valid = value.find_first_of("\r\n\t") == std::string::npos; break;
            case Kind::Host: valid = SettingsValidation::Host(value); break;
            case Kind::Port: valid = SettingsValidation::Port(value); break;
            case Kind::Domain: valid = SettingsValidation::Domain(value); break;
            case Kind::Authority: valid = SettingsValidation::Authority(value); break;
            case Kind::Certificate: {
                struct stat info;
                valid = !value.empty() && value.front() == '/' && stat(value.c_str(), &info) == 0
                    && S_ISREG(info.st_mode) && access(value.c_str(), R_OK) == 0;
                break;
            }
        }
        if (!valid) {
            fTabs->Select(field.tab);
            field.control->MakeFocus(true);
            const char* help = field.kind == Kind::Port ? "Enter a port from 1 to 65535."
                : field.kind == Kind::Certificate ? "Choose an existing readable CA file using its absolute path."
                : "Enter a valid value without spaces or a URL scheme. Use ASCII/punycode for domains.";
            std::string error = std::string(field.control->Label()) + "\n" + help;
            (new BAlert("Invalid setting", error.c_str(), "OK", nullptr, nullptr,
                B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go();
            return;
        }
        changes[field.key] = value.empty() ? std::nullopt : std::optional<std::string>(value);
    }
    for (const auto& flag : fFlags) {
        bool value = flag.control->Value() == B_CONTROL_ON;
        if (value != flag.initial) changes[flag.key] = value ? std::optional<std::string>("") : std::nullopt;
    }
    bool plain = fPlain->Value() == B_CONTROL_ON;
    if (plain != fInitialPlain)
        changes["xmpp-auth-mechanism"] = plain ? std::optional<std::string>("plain-over-tls") : std::nullopt;
    if (!changes.empty() && !BlabberSettings::Instance()->SaveChanges(changes)) {
        (new BAlert("Settings not saved", "Could not save settings. Your previous settings are unchanged."
            "\nCheck free disk space and write access, then try again.", "OK", nullptr, nullptr,
            B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go();
        return;
    }
    BMessage saved(JAB_SETTINGS_SAVED);
    saved.AddBool("display-name", changes.count("last-realname") != 0);
    saved.AddBool("auto-login", changes.count("auto-login") != 0);
    BMessenger(BlabberMainWindow::Instance()).SendMessage(&saved);
    PostMessage(B_QUIT_REQUESTED);
}
void PreferencesWindow::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case JAB_OK: Save(); break;
        case JAB_CANCEL: PostMessage(B_QUIT_REQUESTED); break;
        default: BWindow::MessageReceived(message); break;
    }
}
