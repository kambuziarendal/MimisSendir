// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "network/HttpUpload.h"
#include "network/HttpTransfer.h"
#include <gloox/parser.h>
#include <gloox/taghandler.h>
#include <iostream>
#include <memory>
#include <stdexcept>

static int checks = 0;
static void Check(bool value, const char* description)
{
	++checks;
	if (!value) throw std::runtime_error(description);
}

class Parsed : public gloox::TagHandler {
public:
	std::unique_ptr<gloox::Tag> tag;
	void handleTag(gloox::Tag* value) override { tag.reset(value->clone()); }
};

static std::unique_ptr<gloox::Tag> Parse(std::string xml)
{
	Parsed parsed;
	gloox::Parser parser(&parsed);
	Check(parser.feed(xml) == -1, "XML parsed");
	Check(parsed.tag != nullptr, "XML delivered");
	return std::move(parsed.tag);
}

static void ProtocolTests()
{
	using namespace Upload;
	Check(ValidateFile("photo.png", kMaxFileSize, "image/png").empty(), "50 MiB accepted");
	Check(!ValidateFile("photo.png", kMaxFileSize + 1, "image/png").empty(), "over limit rejected");
	Check(ValidateFile("empty.txt", 0, "text/plain").empty(), "empty file accepted");
	for (const std::string name : {"", "..", "../secret", "dir/file", "dir\\file", "x\n.txt"})
		Check(!ValidateFile(name, 1, "text/plain").empty(), "unsafe filename rejected");
	Check(!ValidateFile("x", 1, "text/plain\r\nAuthorization: leak").empty(), "MIME injection rejected");
	Extension request("Blå & 'quote'.png", 42, "image/png");
	std::unique_ptr<gloox::Tag> tag(request.tag());
	Check(tag != nullptr, "valid request");
	auto parsed = Parse(tag->xml());
	Check(parsed->name() == "request" && parsed->xmlns() == kNamespace, "XEP-0363 namespace");
	Check(parsed->findAttribute("filename") == "Blå & 'quote'.png", "filename XML roundtrip");
	Check(parsed->findAttribute("size") == "42", "size serialized");
	Check(parsed->findAttribute("content-type") == "image/png", "MIME serialized");
	Check(Extension("bad", kMaxFileSize + 1, "image/png").tag() == nullptr, "oversized cannot serialize");
	const std::string xml = "<slot xmlns='urn:xmpp:http:upload:0'>"
		"<put url='https://upload.example.test/a?token=x&amp;y=2'>"
		"<header name='Authorization'>Bearer synthetic</header></put>"
		"<get url='https://upload.example.test/a'/></slot>";
	parsed = Parse(xml);
	Extension slot(parsed.get());
	Check(slot.IsSlot(), "valid slot parsed including inherited namespaces");
	Check(ValidateSlot(slot.GetSlot(), "upload.example.test").empty(), "slot accepted");
	std::unique_ptr<gloox::StanzaExtension> clone(slot.clone());
	Check(static_cast<Extension*>(clone.get())->GetSlot().putUrl == slot.GetSlot().putUrl, "clone preserves slot");
	for (const std::string url : {"http://upload.example.test/a", "file:///etc/passwd",
		"https://upload.example.test.evil/a", "https://upload.example.test@evil/a",
		"https://evil@upload.example.test/a", "https://upload.example.test/a\r\nX: 1",
		"https://upload.example.test/a#fragment", "https://upload.example.test\\@evil/a",
		"https://upload.example.test:444/a"})
		Check(!IsServiceUrl(url, "upload.example.test"), "unsafe URL rejected");
	for (const std::string malformed : {
		"<slot xmlns='wrong'><put url='a'/><get url='b'/></slot>",
		"<slot xmlns='urn:xmpp:http:upload:0'><put url='a'/></slot>",
		"<slot xmlns='urn:xmpp:http:upload:0'><put url='a'/><put url='b'/><get url='c'/></slot>",
		"<slot xmlns='urn:xmpp:http:upload:0'><put xmlns='wrong' url='a'/><get url='b'/></slot>",
		"<slot xmlns='urn:xmpp:http:upload:0'><put xmlns='' url='a'/><get url='b'/></slot>",
		"<slot xmlns='urn:xmpp:http:upload:0'><put url='a'><header name='Cookie'><x/></header></put><get url='b'/></slot>"}) {
		parsed = Parse(malformed);
		Check(!Extension(parsed.get()).IsSlot(), "malformed slot rejected");
	}
	Slot candidate = slot.GetSlot();
	for (const auto& pair : std::vector<std::pair<std::string, std::string>>{
		{"Host", "evil"}, {"Authorization", "x\r\nCookie: evil"}, {"Cookie", ""}}) {
		candidate.headers = {pair};
		Check(!ValidateSlot(candidate, "upload.example.test").empty(), "bad header rejected");
	}
	candidate.headers = {{"Cookie", "a"}, {"Cookie", "b"}};
	Check(!ValidateSlot(candidate, "upload.example.test").empty(), "duplicate header rejected");
	candidate.headers = {{"Cookie", "synthetic=1"}, {"Expires", "Wed, 21 Oct 2030 07:28:00 GMT"}};
	Check(ValidateSlot(candidate, "upload.example.test").empty(), "allowed headers accepted");
	std::cout << checks << " protocol checks passed\n";
}

int main(int argc, char** argv)
{
	try {
		if (argc == 1) { ProtocolTests(); return 0; }
		// Driver for the local HTTPS integration test. No production endpoints.
		if (argc != 7) return 2;
		const std::string authority = argv[1], path = argv[2], source = argv[3];
		const uint64_t size = std::stoull(argv[4]);
		const std::string ca = argv[5], mode = argv[6];
		auto closeFile = [](FILE* value) { fclose(value); };
		const bool download = mode.starts_with("get");
		std::unique_ptr<FILE, decltype(closeFile)> file(
			fopen(source.c_str(), download ? "r+b" : "rb"), closeFile);
		Upload::Slot slot{"https://" + authority + path, "https://" + authority + "/download", {}};
		slot.headers.emplace_back("Authorization", "Bearer synthetic");
		std::atomic_bool cancel{mode == "cancel" || mode == "get-cancel"};
		uint64_t last = 0;
		auto report = [&](uint64_t sent, uint64_t total) {
				Check(total == 0 || sent <= total, "progress bounded");
				last = sent;
				if (mode == "cancel-progress") cancel.store(true);
				if (mode == "get-cancel-progress" && sent > 0) cancel.store(true);
			};
		auto result = download
			? Upload::GetFile(slot.putUrl, authority, file.get(), cancel, report, ca == "system" ? "" : ca)
			: Upload::PutFile(slot, authority, file.get(), size, "application/octet-stream", cancel,
				report, ca == "system" ? "" : ca);
		std::cout << "success=" << result.success << " http=" << result.httpStatus
			<< " progress=" << last << " error=" << result.error << '\n';
		return result.success ? 0 : 1;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 3;
	}
}
