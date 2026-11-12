// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "CredentialStore.h"
#include <KeyStore.h>
#include <Application.h>
#include <mutex>
#include <Looper.h>
#include <atomic>
#include <cstring>

namespace Credentials {
namespace {
std::atomic<int> pending{0};
std::atomic_bool stopping{false};
class Worker;
Worker* workerInstance = nullptr;
std::once_flag workerOnce;
class Worker final : public BLooper {
public:
    Worker() : BLooper("credential operations") {}
    void MessageReceived(BMessage* request) override {
        if (request->what != kResult) { BLooper::MessageReceived(request); return; }
        BMessenger target;
        request->FindMessenger("target", &target);
        BMessage reply(kResult);
        const char* operation = request->GetString("operation", "load");
        const char* account = request->GetString("account", "");
        reply.AddString("operation", operation);
        reply.AddString("account", account);
        reply.AddUInt64("request", request->GetUInt64("request", 0));
        reply.AddBool("initial", request->GetBool("initial", false));
        BKeyStore store;
        // Haiku returns B_BAD_VALUE for reads from a nonexistent keyring.
        // Create our independent ring before the first lookup, not after it.
        store.AddKeyring("MimisSendir");
        BPasswordKey existing;
        status_t status = B_OK;
        if (!strcmp(operation, "load")) {
            if (request->GetBool("initial", false)) {
                uint32 cookie = 0;
                BPasswordKey item;
                for (int count = 0; count < 256 && store.GetNextKey("MimisSendir", B_KEY_TYPE_PASSWORD,
                    B_KEY_PURPOSE_GENERIC, cookie, item) == B_OK; ++count)
                    reply.AddString("saved-account", item.Identifier());
            }
            status = store.GetKey("MimisSendir", B_KEY_TYPE_PASSWORD, account, existing);
            if ((status == B_ENTRY_NOT_FOUND || status == B_NAME_NOT_FOUND)
                && request->GetBool("import-legacy", false) && *account) {
                BPasswordKey legacy;
                status = store.GetKey("XMPP", B_KEY_TYPE_PASSWORD, account, legacy);
                if (status == B_OK) {
                    // Copy, never remove/change an upstream key. A new resource
                    // prevents the two clients from evicting each other's session.
                    store.AddKeyring("MimisSendir");
                    BPasswordKey copied(legacy.Password(), B_KEY_PURPOSE_GENERIC, account, "");
                    status = store.AddKey("MimisSendir", copied);
                    if (status == B_OK) status = store.GetKey("MimisSendir", B_KEY_TYPE_PASSWORD, account, existing);
                }
            }
            if (status == B_OK) {
                reply.AddString("password", existing.Password());
                reply.AddString("resource", existing.SecondaryIdentifier());
            }
        } else if (!strcmp(operation, "save")) {
            status = store.GetKey("MimisSendir", B_KEY_TYPE_PASSWORD, account, existing);
            if (status == B_OK && !strcmp(existing.Password(), request->GetString("password", ""))) {
                // An unchanged credential does not need remove/add permissions
                // on every login. Preserve its existing resource and metadata.
                status = B_OK;
            } else if (status == B_OK) {
                // No atomic replace API exists. Never delete the working key
                // before a replacement is durable; report the conflict.
                status = B_NAME_IN_USE;
            } else if (status == B_ENTRY_NOT_FOUND || status == B_NAME_NOT_FOUND) {
                store.AddKeyring("MimisSendir");
                BPasswordKey key(request->GetString("password", ""), B_KEY_PURPOSE_GENERIC,
                    account, request->GetString("resource", ""));
                status = store.AddKey("MimisSendir", key);
            }
        } else if (!strcmp(operation, "remove")) {
            status = store.GetKey("MimisSendir", B_KEY_TYPE_PASSWORD, account,
                request->GetString("resource", ""), existing);
            if (status == B_OK) status = store.RemoveKey("MimisSendir", existing);
        } else status = B_BAD_VALUE;
        reply.AddInt32("status", status);
        target.SendMessage(&reply, static_cast<BHandler*>(nullptr), 0);
        --pending;
        BMessage ready('bRdy');
        BMessenger(be_app).SendMessage(&ready, static_cast<BHandler*>(nullptr), 0);
    }
};
}
bool Submit(BMessage request, const BMessenger& target) {
    if (stopping.load()) return false;
    if (++pending > 4) { --pending; return false; }
    // This worker belongs to the process, not a window. A blocked OS prompt
    // cannot make a window destructor join it. BMessenger invalidates safely.
    std::call_once(workerOnce, [] { workerInstance = new Worker; workerInstance->Run(); });
    auto* worker = workerInstance;
    request.what = kResult;
    request.AddMessenger("target", target);
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
