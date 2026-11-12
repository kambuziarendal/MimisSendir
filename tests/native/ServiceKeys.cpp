// Synthetic-only native KeyStore acceptance. Never print secret values.
#include "support/ServiceKeys.h"
#include <Application.h>
#include <cassert>
#include <cstdio>
#include <string>
class Test final : public BApplication {
public:
    Test() : BApplication("application/x-vnd.Kambuzi-ServiceKeyTests") {}
    void ReadyToRun() override {
        assert(ServiceKeys::ValidGiphyKey("test_123-ABC"));
        for (auto bad : {"", "with space", "key\n", "a&b"}) assert(!ServiceKeys::ValidGiphyKey(bad));
        assert(!ServiceKeys::ValidGiphyKey(std::string(129, 'x')));
        assert(!ServiceKeys::ValidIdentifier("giphy-not-an-id"));
        std::string error;
        assert(ServiceKeys::Load("", error).empty() && !error.empty());
        BMessage request; request.AddString("operation", "store");
        request.AddString("key", "FixtureGiphyKeyNotForNetwork123");
        assert(ServiceKeys::Submit(request, BMessenger(this)));
    }
    void MessageReceived(BMessage* result) override {
        if (result->what != ServiceKeys::kResult) { BApplication::MessageReceived(result); return; }
        assert(result->GetInt32("status", B_ERROR) == B_OK);
        assert(!result->HasString("key") && !result->HasString("password"));
        if (std::string(result->GetString("operation", "")) == "store") {
            id = result->GetString("id", ""); assert(ServiceKeys::ValidIdentifier(id));
            std::string error;
            assert(ServiceKeys::Load(id, error) == "FixtureGiphyKeyNotForNetwork123" && error.empty());
            BMessage request; request.AddString("operation", "remove"); request.AddString("id", id.c_str());
            assert(ServiceKeys::Submit(request, BMessenger(this)));
        } else {
            std::string error; assert(ServiceKeys::Load(id, error).empty() && !error.empty());
            puts("Service keys: validation, private save/read/delete, secret-free replies passed");
            PostMessage(B_QUIT_REQUESTED);
        }
    }
    std::string id;
};
int main() { Test test; test.Run(); ServiceKeys::Shutdown(); }
