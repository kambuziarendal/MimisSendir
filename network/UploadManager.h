// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once

#include <Messenger.h>
#include <Message.h>
#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <gloox/iqhandler.h>
#include "support/UploadCancellation.h"
#include "Giphy.h"

namespace gloox { class Client; }
namespace Upload {
inline constexpr uint32 kTransferNotice = 'uNtc';

// Loaded once per connection. Defaults are isolated from protocol/transport code.
struct Profile {
	std::string service;
	std::string authority;
	std::string caFile; // Optional private/test CA bundle; validation stays enabled.
	static Profile Load();
};

struct Task {
	std::atomic_bool cancelled{false};
	std::atomic_bool done{false};
	std::atomic<uint64_t> sent{0};
	std::string path, recipient, filename, type, id, error, url, messageId;
	bool group = false;
	bool success = false;
	uint64_t token = 0;
	uint64_t size = 0;
	int64_t modifiedSeconds = 0;
	int64_t modifiedNanoseconds = 0;
	int64_t deadline = 0;
	int64_t lastNotice = 0;
	BMessenger target;
	std::shared_ptr<FILE> file;
	std::shared_ptr<Giphy::Transfer> mediaLease;
	SendCommitGate messageGate;
	std::mutex noticeLock;
	BMessage latestNotice;
	void Cancel() { messageGate.Cancel(); cancelled = true; }
};

class Manager final : public gloox::IqHandler {
public:
	Manager(gloox::Client& client, Profile profile);
	~Manager();
	// UI thread. Queues only; never accesses gloox or opens the file.
	std::shared_ptr<Task> Queue(const std::string& path, const std::string& recipient,
		bool group, const BMessenger& target, std::shared_ptr<Giphy::Transfer> mediaLease = {});
	// Connection thread only.
	void Connected();
	void Tick();
	void Disconnected();
	bool handleIq(const gloox::IQ&) override { return false; }
	void handleIqID(const gloox::IQ& iq, int context) override;
private:
	void Request(const std::shared_ptr<Task>& task);
	static void Notice(const std::shared_ptr<Task>& task, const char* state,
		const std::string& text, const std::string& url = {});
	gloox::Client& fClient;
	Profile fProfile;
	std::mutex fQueueLock;
	std::atomic_bool fConnected{false};
	std::vector<std::shared_ptr<Task>> fQueue;
	std::vector<std::shared_ptr<Task>> fActive;
};
}
