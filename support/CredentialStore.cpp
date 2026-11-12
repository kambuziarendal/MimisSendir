// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "CredentialStore.h"
#include <KeyStore.h>
#include <Looper.h>
#include <atomic>
#include <cstring>

namespace Credentials {
namespace {
std::atomic<int> pending{0};
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
        BPasswordKey existing;
        status_t status = B_OK;
        if (!strcmp(operation, "load")) {
            if (request->GetBool("initial", false)) {
                uint32 cookie = 0;
                BPasswordKey item;
                for (int count = 0; count < 256 && store.GetNextKey("XMPP", B_KEY_TYPE_PASSWORD,
                    B_KEY_PURPOSE_GENERIC, cookie, item) == B_OK; ++count)
                    reply.AddString("saved-account", item.Identifier());
            }
            status = store.GetKey("XMPP", B_KEY_TYPE_PASSWORD, account, existing);
            if (status == B_OK) {
                reply.AddString("password", existing.Password());
                reply.AddString("resource", existing.SecondaryIdentifier());
            }
        } else if (!strcmp(operation, "save")) {
            status = store.GetKey("XMPP", B_KEY_TYPE_PASSWORD, account, existing);
            if (status == B_OK && !strcmp(existing.Password(), request->GetString("password", ""))) {
                // An unchanged credential does not need remove/add permissions
                // on every login. Preserve its existing resource and metadata.
                status = B_OK;
            } else if (status == B_OK) {
                // No atomic replace API exists. Never delete the working key
                // before a replacement is durable; report the conflict.
                status = B_NAME_IN_USE;
            } else if (status == B_ENTRY_NOT_FOUND || status == B_NAME_NOT_FOUND) {
                store.AddKeyring("XMPP");
                BPasswordKey key(request->GetString("password", ""), B_KEY_PURPOSE_GENERIC,
                    account, request->GetString("resource", ""));
                status = store.AddKey("XMPP", key);
            }
        } else if (!strcmp(operation, "remove")) {
            status = store.GetKey("XMPP", B_KEY_TYPE_PASSWORD, account,
                request->GetString("resource", ""), existing);
            if (status == B_OK) status = store.RemoveKey("XMPP", existing);
        } else status = B_BAD_VALUE;
        reply.AddInt32("status", status);
        --pending;
        target.SendMessage(&reply, static_cast<BHandler*>(nullptr), 0);
    }
};
}
bool Submit(BMessage request, const BMessenger& target) {
    if (++pending > 4) { --pending; return false; }
    // This worker belongs to the process, not a window. A blocked OS prompt
    // cannot make a window destructor join it. BMessenger invalidates safely.
    static Worker* worker = [] { auto* result = new Worker; result->Run(); return result; }();
    request.what = kResult;
    request.AddMessenger("target", target);
    if (worker->PostMessage(&request) != B_OK) { --pending; return false; }
    return true;
}
}
