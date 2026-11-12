#include "network/DeliveryReceipt.h"
// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "UploadManager.h"
#include "HttpUpload.h"
#include "HttpTransfer.h"
#include "Extensions.h"
#include "jabber/BlabberSettings.h"
#include "config/PrivateProfile.h"

#include <Message.h>
#include <OS.h>
#include <gloox/client.h>
#include <gloox/iq.h>
#include <gloox/message.h>
#include <gloox/oob.h>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace Upload {
Profile Profile::Load()
{
	auto* settings = BlabberSettings::Instance();
	return {settings->Data("upload-service").value_or(PrivateProfile::kUploadService),
		settings->Data("upload-authority").value_or(PrivateProfile::kUploadAuthority),
		settings->Data("upload-ca-file").value_or("")};
}

Manager::Manager(gloox::Client& client, Profile profile)
	: fClient(client), fProfile(std::move(profile))
{
	fClient.registerStanzaExtension(new Extension());
	fClient.registerStanzaExtension(new gloox::OOB(nullptr));
}

Manager::~Manager()
{
	fClient.removeIDHandler(this);
	// Workers own only shared Task + FILE, never this manager or a view.
	for (const auto& task : fActive)
		task->Cancel();
}

void Manager::Notice(const std::shared_ptr<Task>& task, const char* state,
	const std::string& text, const std::string& url)
{
	BMessage message(kTransferNotice);
	message.AddUInt64("token", task->token);
	message.AddString("state", state);
	message.AddString("text", text.c_str());
	message.AddString("path", task->path.c_str());
	message.AddString("message-id", task->messageId.c_str());
	message.AddInt64("sent", task->sent.load());
	message.AddInt64("size", task->size);
	if (!url.empty()) message.AddString("url", url.c_str());
	{
		std::lock_guard<std::mutex> lock(task->noticeLock);
		task->latestNotice = message;
	}
	// Never block the networking thread on a closed/busy window's message queue.
	task->target.SendMessage(&message, static_cast<BHandler*>(nullptr), 0);
}

std::shared_ptr<Task> Manager::Queue(const std::string& path,
	const std::string& recipient, bool group, const BMessenger& target, std::shared_ptr<Giphy::Transfer> mediaLease)
{
	static std::atomic<uint64_t> nextToken{1};
	auto task = std::make_shared<Task>();
	task->token = nextToken.fetch_add(1);
	task->path = path;
	task->mediaLease = std::move(mediaLease);
	task->recipient = recipient;
	task->group = group;
	task->target = target;
	std::lock_guard<std::mutex> lock(fQueueLock);
	if (!fConnected || fQueue.size() >= 8) {
		task->done = true;
		Notice(task, "error", "Not connected or upload queue is full. Try again when connected.");
	} else {
		fQueue.push_back(task);
	}
	return task;
}

void Manager::Connected()
{
	std::lock_guard<std::mutex> lock(fQueueLock);
	fConnected = true;
}

void Manager::Disconnected()
{
	std::vector<std::shared_ptr<Task>> queued;
	{
		std::lock_guard<std::mutex> lock(fQueueLock);
		fConnected = false;
		queued.swap(fQueue);
	}
	queued.insert(queued.end(), fActive.begin(), fActive.end());
	for (const auto& task : queued) {
		task->Cancel();
		Notice(task, "error", "Disconnected. Attachment was not sent; reconnect and retry.");
	}
	fActive.clear();
	fClient.removeIDHandler(this);
}

void Manager::Request(const std::shared_ptr<Task>& task)
{
	// Keep this exact file handle through the slot wait and transfer. Replacing the
	// selected pathname cannot silently upload a different file of the same size.
	int fd = open(task->path.c_str(), O_RDONLY | O_NONBLOCK);
	struct stat st;
	if (fd < 0 || fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
		if (fd >= 0) close(fd);
		Notice(task, "error", "Choose a readable regular file.");
		return;
	}
	FILE* file = fdopen(fd, "rb");
	if (!file) {
		close(fd);
		Notice(task, "error", "Could not open the selected file.");
		return;
	}
	task->file = std::shared_ptr<FILE>(file, fclose);
	task->filename = task->path.substr(task->path.find_last_of('/') + 1);
	task->size = st.st_size;
	task->modifiedSeconds = st.st_mtim.tv_sec;
	task->modifiedNanoseconds = st.st_mtim.tv_nsec;
	task->type = "application/octet-stream";
	std::string suffix = task->filename.substr(task->filename.find_last_of('.') + 1);
	std::transform(suffix.begin(), suffix.end(), suffix.begin(), [](unsigned char c) { return std::tolower(c); });
	if (suffix == "png") task->type = "image/png";
	else if (suffix == "jpg" || suffix == "jpeg") task->type = "image/jpeg";
	else if (suffix == "gif") task->type = "image/gif";
	else if (suffix == "webp") task->type = "image/webp";
	else if (suffix == "heic") task->type = "image/heic";
	else if (suffix == "pdf") task->type = "application/pdf";
	auto error = ValidateFile(task->filename, task->size, task->type);
	if (!error.empty()) { Notice(task, "error", error); return; }
	gloox::JID service(fProfile.service);
	if (!service || !service.username().empty() || !service.resource().empty()
		|| !IsServiceUrl("https://" + fProfile.authority + "/", fProfile.authority)) {
		Notice(task, "error", "Invalid upload service profile.");
		return;
	}
	task->id = fClient.getID();
	task->deadline = system_time() + 30000000;
	fActive.push_back(task);
	gloox::IQ request(gloox::IQ::Get, service, task->id);
	request.addExtension(new Extension(task->filename, task->size, task->type));
	Notice(task, "waiting", "Requesting upload slot...");
	fClient.send(request, this, 0);
}

void Manager::Tick()
{
	std::vector<std::shared_ptr<Task>> queued;
	{
		std::lock_guard<std::mutex> lock(fQueueLock);
		if (!fConnected) return;
		queued.swap(fQueue);
	}
	for (const auto& task : queued) {
		if (!fConnected) Notice(task, "error", "Disconnected. Attachment was not sent.");
		else if (task->cancelled) Notice(task, "cancelled", "Upload cancelled.");
		else if (fActive.size() >= 8) Notice(task, "error", "Too many uploads. Try again later.");
		else Request(task);
	}
	// send() can synchronously disconnect and clear fActive. Iterate a snapshot.
	const auto active = fActive;
	for (const auto& task : active) {
		if (!fConnected) break;
		bool remove = false;
		if (task->cancelled) {
			Notice(task, "cancelled", "Upload cancelled. No attachment message sent.");
			remove = true;
		} else if (task->done.load()) {
			if (task->success) {
				// Exactly one cancel/commit wins; never make UI closure wait on send().
				if (task->messageGate.TryCommit()) {
					gloox::Message message(task->group ? gloox::Message::Groupchat : gloox::Message::Chat,
						gloox::JID(task->recipient), task->url);
					task->messageId = fClient.getID();
                    message.setID(task->messageId);
                    if (!task->group) message.addExtension(new Chat::Receipt(true));
                    message.addExtension(new gloox::OOB(task->url, task->filename, false));
					fClient.send(message);
					if (fConnected)
						Notice(task, "sent", "Uploaded; attachment message sent (delivery not confirmed).", task->url);
				} else Notice(task, "cancelled", "Upload cancelled.");
			} else Notice(task, "error", task->error);
			remove = true;
		} else if (task->deadline != 0 && system_time() > task->deadline) {
			Notice(task, "error", "Upload service did not answer within 30 seconds.");
			remove = true;
		} else if (task->deadline == 0 && system_time() - task->lastNotice > 250000) {
			task->lastNotice = system_time();
			Notice(task, "uploading", "Uploading...");
		}
		if (remove) fActive.erase(std::remove(fActive.begin(), fActive.end(), task), fActive.end());
	}
	// gloox removes handlers on successful replies, not on our own timeouts.
	if (fActive.empty()) fClient.removeIDHandler(this);
}

void Manager::handleIqID(const gloox::IQ& iq, int)
{
	auto found = std::find_if(fActive.begin(), fActive.end(), [&](const auto& task) {
		return task->id == iq.id() && task->deadline != 0;
	});
	if (found == fActive.end()) return;
	auto task = *found;
	if (task->cancelled) return;
	// Exact service JID, not just bare domain; reject forged or late responses.
	if (iq.from().full() != fProfile.service || system_time() > task->deadline) {
		task->error = "Upload slot reply did not match the requested service or arrived too late.";
		task->done = true;
		return;
	}
	const auto* extension = iq.findExtension<Extension>(ExtHttpUpload);
	if (iq.subtype() != gloox::IQ::Result || !extension || !extension->IsSlot()) {
		task->error = "Upload service rejected the request or returned an invalid slot.";
		task->done = true;
		return;
	}
	Slot slot = extension->GetSlot();
	auto error = ValidateSlot(slot, fProfile.authority);
	if (!error.empty()) { task->error = error; task->done = true; return; }
	task->deadline = 0;
	const std::string authority = fProfile.authority;
	const std::string caFile = fProfile.caFile;
	try {
		std::thread([task, slot, authority, caFile]() {
			struct stat st;
			if (fstat(fileno(task->file.get()), &st) != 0 || st.st_size < 0
				|| static_cast<uint64_t>(st.st_size) != task->size
				|| st.st_mtim.tv_sec != task->modifiedSeconds
				|| st.st_mtim.tv_nsec != task->modifiedNanoseconds) {
				task->error = "File changed. Select it again.";
				task->done = true;
				return;
			}
			auto result = PutFile(slot, authority, task->file.get(), task->size, task->type, task->cancelled,
				[task](uint64_t sent, uint64_t) { task->sent = sent; }, caFile);
			// In-place edits during the PUT also invalidate the outgoing message.
			if (result.success && (fstat(fileno(task->file.get()), &st) != 0
				|| st.st_size < 0 || static_cast<uint64_t>(st.st_size) != task->size
				|| st.st_mtim.tv_sec != task->modifiedSeconds
				|| st.st_mtim.tv_nsec != task->modifiedNanoseconds)) {
				result.success = false;
				result.error = "File changed during upload. No attachment message sent.";
			}
			task->success = result.success;
			task->error = result.error;
			task->url = slot.getUrl;
			task->done = true;
		}).detach();
	} catch (const std::system_error&) {
		task->error = "Could not start upload worker. Try again.";
		task->done = true;
	}
}
}
