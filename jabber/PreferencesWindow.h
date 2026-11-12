// Copyright 2026 Kambuzi EDB. Distributed under the MIT license.
#ifndef PREFERENCES_WINDOW_H
#define PREFERENCES_WINDOW_H
#include <Window.h>
#include <string>
#include <vector>
class BTextControl;
class BCheckBox;
class BStringView;
class BButton;
class BTabView;
class PreferencesWindow : public BWindow {
public:
    static PreferencesWindow* Instance();
    PreferencesWindow();
    ~PreferencesWindow() override;
    bool QuitRequested() override;
    void MessageReceived(BMessage* message) override;
private:
    enum class Kind { Text, Host, Port, Domain, Authority, Certificate };
    struct Field {
        const char* key;
        BTextControl* control;
        std::string initial;
        Kind kind;
        int tab;
        bool optional;
    };
    struct Flag { const char* key; BCheckBox* control; bool initial; };
    BTextControl* AddField(const char* key, const char* label, const char* fallback,
        Kind kind, int tab, bool optional = false);
    BCheckBox* AddFlag(const char* key, const char* label);
    void Save();
    void StoreGiphyKey();
    void RemoveGiphyKey();
    void GiphyResult(BMessage* message);
    void SetKeyBusy(bool busy);
    BTextControl* fGiphyKey;
    BStringView* fGiphyStatus;
    BButton* fStoreGiphy;
    BButton* fRemoveGiphy;
    bool fKeyBusy = false;
    std::string fGiphyKeyId;
    static PreferencesWindow* _instance;
    BTabView* fTabs;
    std::vector<Field> fFields;
    std::vector<Flag> fFlags;
    BCheckBox* fPlain;
    bool fInitialPlain;
};
#endif
