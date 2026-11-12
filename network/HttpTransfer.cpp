// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "HttpTransfer.h"
#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <memory>
#include <mutex>

namespace Upload {
namespace {
bool InitializeCurl()
{
	static std::once_flag init;
	static CURLcode initialized = CURLE_FAILED_INIT;
	std::call_once(init, [] { initialized = curl_global_init(CURL_GLOBAL_DEFAULT); });
	return initialized == CURLE_OK;
}

struct Context {
	FILE* file;
	uint64_t size;
	uint64_t read = 0;
	size_t responseBytes = 0;
	const std::atomic_bool& cancelled;
	const std::function<void(uint64_t, uint64_t)>& progress;
};

struct DownloadContext {
	FILE* file;
	uint64_t received = 0;
    uint64_t limit = kMaxFileSize;
	bool tooLarge = false;
	const std::atomic_bool& cancelled;
	const std::function<void(uint64_t, uint64_t)>& progress;
};

size_t Write(char* buffer, size_t size, size_t count, void* data)
{
	auto& c = *static_cast<DownloadContext*>(data);
	const size_t bytes = size * count;
	if (c.cancelled.load()) return 0;
	if (bytes > c.limit - c.received) { c.tooLarge = true; return 0; }
	const size_t written = fwrite(buffer, 1, bytes, c.file);
	c.received += written;
	return written;
}

int DownloadProgress(void* data, curl_off_t total, curl_off_t received, curl_off_t, curl_off_t)
{
	auto& c = *static_cast<DownloadContext*>(data);
	if (total > static_cast<curl_off_t>(c.limit)) { c.tooLarge = true; return 1; }
	if (c.cancelled.load()) return 1;
	if (c.progress) {
		try { c.progress(std::max<curl_off_t>(received, 0), std::max<curl_off_t>(total, 0)); }
		catch (...) { return 1; }
	}
	return 0;
}

size_t Read(char* buffer, size_t size, size_t count, void* data)
{
	auto& c = *static_cast<Context*>(data);
	if (c.cancelled.load()) return CURL_READFUNC_ABORT;
	const auto wanted = static_cast<size_t>(std::min<uint64_t>(size * count, c.size - c.read));
	if (wanted == 0) return 0;
	const size_t got = fread(buffer, 1, wanted, c.file);
	if (got == 0) return CURL_READFUNC_ABORT; // shortened or unreadable source
	c.read += got;
	return got;
}

size_t Discard(char*, size_t size, size_t count, void* data)
{
	auto& c = *static_cast<Context*>(data);
	const size_t bytes = size * count;
	if (bytes > 16384 - c.responseBytes) return 0;
	c.responseBytes += bytes;
	return bytes;
}

int Progress(void* data, curl_off_t, curl_off_t, curl_off_t, curl_off_t sent)
{
	auto& c = *static_cast<Context*>(data);
	if (c.cancelled.load()) return 1;
	if (c.progress) {
		try { c.progress(std::min<uint64_t>(std::max<curl_off_t>(sent, 0), c.size), c.size); }
		catch (...) { return 1; } // Never unwind through libcurl's C callbacks.
	}
	return 0;
}
}

TransferResult PutFile(const Slot& slot, const std::string& authority,
	FILE* file, uint64_t size, const std::string& contentType,
	const std::atomic_bool& cancelled,
	const std::function<void(uint64_t, uint64_t)>& progress, const std::string& caFile)
{
	TransferResult result;
	result.error = ValidateSlot(slot, authority);
	if (!result.error.empty()) return result;
	result.error = ValidateFile("attachment", size, contentType);
	if (!result.error.empty()) return result;
	if (cancelled.load()) { result.error = "Upload cancelled."; return result; }
	struct stat before {};
	if (!file || fstat(fileno(file), &before) != 0 || !S_ISREG(before.st_mode)
		|| before.st_size < 0 || static_cast<uint64_t>(before.st_size) != size
		|| fseeko(file, 0, SEEK_SET) != 0) {
		result.error = "Attachment is not a readable regular file or has changed.";
		return result;
	}
	if (!InitializeCurl()) { result.error = "Could not initialize HTTPS."; return result; }
	std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
	if (!curl) { result.error = "Could not initialize HTTPS."; return result; }
	curl_slist* rawHeaders = nullptr;
	auto append = [&](const std::string& value) {
		auto* next = curl_slist_append(rawHeaders, value.c_str());
		if (!next) return false;
		rawHeaders = next;
		return true;
	};
	bool ok = append("Content-Type: " + contentType) && append("Expect:");
	for (const auto& [name, value] : slot.headers) ok = ok && append(name + ": " + value);
	std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(rawHeaders, curl_slist_free_all);
	if (!ok) { result.error = "Could not allocate upload headers."; return result; }
	Context context{file, size, 0, 0, cancelled, progress};
	auto set = [&](CURLoption option, auto value) {
		if (curl_easy_setopt(curl.get(), option, value) != CURLE_OK) ok = false;
	};
	set(CURLOPT_URL, slot.putUrl.c_str());
	set(CURLOPT_UPLOAD, 1L);
	set(CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(size));
	set(CURLOPT_READFUNCTION, &Read);
	set(CURLOPT_READDATA, &context);
	set(CURLOPT_WRITEFUNCTION, &Discard);
	set(CURLOPT_WRITEDATA, &context);
	set(CURLOPT_HTTPHEADER, headers.get());
	set(CURLOPT_XFERINFOFUNCTION, &Progress);
	set(CURLOPT_XFERINFODATA, &context);
	set(CURLOPT_NOPROGRESS, 0L);
	set(CURLOPT_NOSIGNAL, 1L);
	set(CURLOPT_CONNECTTIMEOUT, 15L);
	set(CURLOPT_TIMEOUT, 300L);
	set(CURLOPT_LOW_SPEED_LIMIT, 1L);
	set(CURLOPT_LOW_SPEED_TIME, 30L);
	set(CURLOPT_FOLLOWLOCATION, 0L);
	set(CURLOPT_PROTOCOLS_STR, "https");
	set(CURLOPT_SSL_VERIFYPEER, 1L);
	set(CURLOPT_SSL_VERIFYHOST, 2L);
	if (!caFile.empty()) set(CURLOPT_CAINFO, caFile.c_str());
	if (!ok) { result.error = "Could not configure HTTPS upload."; return result; }
	const CURLcode status = curl_easy_perform(curl.get());
	curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &result.httpStatus);
	struct stat after {};
	if (cancelled.load()) result.error = "Upload cancelled.";
	else if (status != CURLE_OK) result.error = "HTTPS upload failed: " + std::string(curl_easy_strerror(status));
	else if (result.httpStatus != 201) result.error = "Upload rejected (HTTP " + std::to_string(result.httpStatus) + ").";
	else if (context.read != size || fstat(fileno(file), &after) != 0
		|| after.st_size != before.st_size || after.st_mtime != before.st_mtime)
		result.error = "Attachment changed during upload. No message was sent.";
	else { result.success = true; result.bytes = context.read; result.error.clear(); }
	return result;
}

TransferResult GetFile(const std::string& url, const std::string& authority,
	FILE* destination, const std::atomic_bool& cancelled,
	const std::function<void(uint64_t, uint64_t)>& progress, const std::string& caFile, uint64_t limit)
{
	TransferResult result;
	if (!IsServiceUrl(url, authority)) {
		result.error = "Attachment URL does not belong to the configured HTTPS service.";
		return result;
	}
	struct stat info {};
	if (!destination || fstat(fileno(destination), &info) != 0
		|| !S_ISREG(info.st_mode) || info.st_size != 0 || ftello(destination) != 0) {
		result.error = "Download destination must be a new empty regular file.";
		return result;
	}
	if (cancelled.load()) { result.error = "Download cancelled."; return result; }
	if (!InitializeCurl()) { result.error = "Could not initialize HTTPS."; return result; }
	std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
	if (!curl) { result.error = "Could not initialize HTTPS."; return result; }
	limit=std::min(limit,kMaxFileSize);
	DownloadContext context{destination, 0, limit, false, cancelled, progress};
	bool ok = true;
	auto set = [&](CURLoption option, auto value) {
		if (curl_easy_setopt(curl.get(), option, value) != CURLE_OK) ok = false;
	};
	set(CURLOPT_URL, url.c_str());
	set(CURLOPT_WRITEFUNCTION, &Write);
	set(CURLOPT_WRITEDATA, &context);
	set(CURLOPT_XFERINFOFUNCTION, &DownloadProgress);
	set(CURLOPT_XFERINFODATA, &context);
	set(CURLOPT_NOPROGRESS, 0L);
	set(CURLOPT_NOSIGNAL, 1L);
	set(CURLOPT_CONNECTTIMEOUT, 15L);
	set(CURLOPT_TIMEOUT, 300L);
	set(CURLOPT_LOW_SPEED_LIMIT, 1L);
	set(CURLOPT_LOW_SPEED_TIME, 30L);
	set(CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(limit));
	set(CURLOPT_FOLLOWLOCATION, 0L);
	set(CURLOPT_PROTOCOLS_STR, "https");
	set(CURLOPT_SSL_VERIFYPEER, 1L);
	set(CURLOPT_SSL_VERIFYHOST, 2L);
	if (!caFile.empty()) set(CURLOPT_CAINFO, caFile.c_str());
	if (!ok) { result.error = "Could not configure HTTPS download."; return result; }
	const CURLcode status = curl_easy_perform(curl.get());
	curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &result.httpStatus);
	const bool flushed = fflush(destination) == 0;
	if (cancelled.load()) result.error = "Download cancelled.";
	else if (context.tooLarge || status == CURLE_FILESIZE_EXCEEDED)
		result.error = "Download exceeds the allowed size limit.";
	else if (status != CURLE_OK) result.error = "HTTPS download failed: " + std::string(curl_easy_strerror(status));
	else if (result.httpStatus != 200) result.error = "Download rejected (HTTP " + std::to_string(result.httpStatus) + ").";
	else if (!flushed) result.error = "Could not save attachment.";
	else {
		char* contentType = nullptr;
		curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_TYPE, &contentType);
		if (contentType && std::char_traits<char>::length(contentType) <= 255)
			result.contentType = contentType;
		result.success = true;
		result.bytes = context.received;
	}
	if (!result.success) {
		// Never leave an error page or partial payload for an image decoder/opener.
		if (ftruncate(fileno(destination), 0) != 0)
			result.error = "Download failed; could not clear the temporary file.";
		fseeko(destination, 0, SEEK_SET);
	}
	return result;
}
}
