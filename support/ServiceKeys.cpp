// Copyright 2026 Kambuzi EDB. Distributed under the MIT license.
#include "ServiceKeys.h"
#include <KeyStore.h>
#include <Application.h>
#include <mutex>
#include <Looper.h>
#include <atomic>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
namespace ServiceKeys {
namespace {
constexpr const char* kRing = "MimisSendir-Services";
std::atomic<int> pending{0};
std::atomic_bool stopping{false};
class Worker;
Worker* workerInstance = nullptr;
std::once_flag workerOnce;
std::string NewIdentifier() {
    unsigned char random[16];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return {};
    auto count = read(fd, random, sizeof random); close(fd);
    if (count != static_cast<ssize_t>(sizeof random)) return {};
    std::string id = "giphy-";
    for (auto byte : random) { char hex[3]; snprintf(hex, sizeof hex, "%02x", byte); id += hex; }
    return id;
}
class Worker final : public BLooper {
public:
    Worker() : BLooper("service key operations") {}
    void MessageReceived(BMessage* request) override {
        if (request->what != kResult) { BLooper::MessageReceived(request); return; }
        const std::string operation = request->GetString("operation", "");
        BMessage reply(kResult);
        reply.AddString("operation", operation.c_str());
        reply.AddString("context", request->GetString("context", ""));
        BKeyStore store;
        status_t result = B_BAD_VALUE;
        if (operation == "store") {
            const std::string key = request->GetString("key", "");
            const auto id = NewIdentifier();
            if (ValidGiphyKey(key) && ValidIdentifier(id)) {
                store.AddKeyring(kRing);
                BPasswordKey secret(key.c_str(), B_KEY_PURPOSE_GENERIC, id.c_str());
                result = store.AddKey(kRing, secret);
                if (result == B_OK) reply.AddString("id", id.c_str());
            }
        } else if (operation == "remove") {
            const std::string id = request->GetString("id", "");
            if (ValidIdentifier(id)) {
                BPasswordKey key;
                result = store.GetKey(kRing, B_KEY_TYPE_PASSWORD, id.c_str(), key);
                if (result == B_OK) result = store.RemoveKey(kRing, key);
                else if (result == B_ENTRY_NOT_FOUND || result == B_NAME_NOT_FOUND) result = B_OK;
            }
        }
        BMessenger target; request->FindMessenger("target", &target);
        reply.AddInt32("status", result);
        target.SendMessage(&reply, static_cast<BHandler*>(nullptr), 0);
        --pending;
        BMessage ready('bRdy');
        BMessenger(be_app).SendMessage(&ready, static_cast<BHandler*>(nullptr), 0);
    }
};
}
bool ValidGiphyKey(const std::string& key) {
    return !key.empty() && key.size() <= 128
        && key.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::string::npos;
}
bool ValidIdentifier(const std::string& id) {
    return id.size() == 38 && id.compare(0, 6, "giphy-") == 0
        && id.find_first_not_of("0123456789abcdef", 6) == std::string::npos;
}
std::string Load(const std::string& id, std::string& error) {
    if (!ValidIdentifier(id)) { error = "Add your GIPHY API key in Settings > GIPHY."; return {}; }
    BKeyStore store; BPasswordKey key;
    if (store.GetKey(kRing, B_KEY_TYPE_PASSWORD, id.c_str(), key) != B_OK) {
        error = "GIPHY key unavailable. Check Haiku KeyStore access or save a key in Settings > GIPHY."; return {};
    }
    std::string result = key.Password();
    if (!ValidGiphyKey(result)) { error = "The stored GIPHY key is invalid. Replace it in Settings > GIPHY."; return {}; }
    error.clear(); return result;
}
bool Submit(BMessage request, const BMessenger& target) {
    if (stopping.load()) return false;
    if (++pending > 2) { --pending; return false; }
    std::call_once(workerOnce, [] { workerInstance = new Worker; workerInstance->Run(); });
    auto* worker = workerInstance;
    request.what = kResult; request.AddMessenger("target", target);
    if (worker->PostMessage(&request) != B_OK) { --pending; return false; }
    return true;
}
bool Idle() { return pending.load() == 0; }
void Shutdown() {
    // The application waits asynchronously for pending OS prompts before
    // reaching its destructor. Stop the looper before Haiku tears down ports.
    stopping = true;
    if (workerInstance && workerInstance->Lock()) workerInstance->Quit();
    workerInstance = nullptr;
}

}
