// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "HttpUpload.h"
#include "Extensions.h"
#include <algorithm>
#include <set>

namespace Upload {
static bool HasControl(const std::string& value)
{
	return std::any_of(value.begin(), value.end(), [](unsigned char c) {
		return c < 32 || c == 127;
	});
}

static std::string Namespace(const gloox::Tag* tag)
{
	// gloox's parser retains xmlns as attributes in some released versions.
	// Resolve inheritance explicitly, including an explicit xmlns='' reset.
	const std::string prefix = tag->prefix();
	const std::string attribute = prefix.empty() ? "xmlns" : "xmlns:" + prefix;
	for (auto* node = tag; node; node = node->parent()) {
		if (node->hasAttribute(attribute)) return node->findAttribute(attribute);
		const auto ns = node->xmlns(prefix);
		if (!ns.empty()) return ns;
	}
	return {};
}

bool IsServiceUrl(const std::string& url, const std::string& authority)
{
	if (authority.empty() || HasControl(authority)
		|| authority.find_first_of("/@?#\\ ") != std::string::npos)
		return false;
	const std::string prefix = "https://" + authority + "/";
	return url.size() <= 8192 && url.compare(0, prefix.size(), prefix) == 0
		&& !HasControl(url) && url.find_first_of("#\\ ") == std::string::npos;
}

std::string ValidateFile(const std::string& filename, uint64_t size,
	const std::string& contentType)
{
	if (size > kMaxFileSize)
		return "Files must not exceed 50 MiB.";
	if (filename.empty() || filename.size() > 255 || HasControl(filename)
		|| filename == "." || filename == ".."
		|| filename.find_first_of("/\\") != std::string::npos)
		return "Invalid attachment filename.";
	if (contentType.empty() || contentType.size() > 255 || HasControl(contentType)
		|| contentType.find('/') == std::string::npos)
		return "Invalid attachment content type.";
	return {};
}

std::string ValidateSlot(const Slot& slot, const std::string& authority)
{
	if (!IsServiceUrl(slot.putUrl, authority) || !IsServiceUrl(slot.getUrl, authority))
		return "Upload service returned an unexpected or insecure URL.";
	if (slot.headers.size() > 3)
		return "Upload service returned too many headers.";
	std::set<std::string> names;
	for (const auto& [name, value] : slot.headers) {
		if (name != "Authorization" && name != "Cookie" && name != "Expires")
			return "Upload service returned an unsupported header.";
		if (!names.insert(name).second || value.empty() || value.size() > 8192
			|| HasControl(value))
			return "Upload service returned an invalid header.";
	}
	return {};
}

Extension::Extension(const gloox::Tag* tag)
	: gloox::StanzaExtension(ExtHttpUpload)
{
	if (!tag || tag->name() != "slot" || Namespace(tag) != kNamespace)
		return;
	auto puts = tag->findChildren("put");
	auto gets = tag->findChildren("get");
	if (puts.size() != 1 || gets.size() != 1)
		return;
	const auto* put = puts.front();
	if (Namespace(put) != kNamespace || Namespace(gets.front()) != kNamespace)
		return;
	fSlot.putUrl = put->findAttribute("url");
	fSlot.getUrl = gets.front()->findAttribute("url");
	for (const auto* child : put->children()) {
		if (child->name() != "header" || Namespace(child) != kNamespace
			|| !child->children().empty())
			return;
		fSlot.headers.emplace_back(child->findAttribute("name"), child->cdata());
	}
	fValidSlot = !fSlot.putUrl.empty() && !fSlot.getUrl.empty();
}

Extension::Extension(std::string filename, uint64_t size, std::string contentType)
	: gloox::StanzaExtension(ExtHttpUpload), fRequest(true),
	fFilename(std::move(filename)), fSize(size), fContentType(std::move(contentType))
{
}

const std::string& Extension::filterString() const
{
	static const std::string filter = "/iq/slot[@xmlns='urn:xmpp:http:upload:0']";
	return filter;
}

gloox::Tag* Extension::tag() const
{
	if (!fRequest || !ValidateFile(fFilename, fSize, fContentType).empty())
		return nullptr;
	auto* result = new gloox::Tag("request");
	result->setXmlns(kNamespace);
	result->addAttribute("filename", fFilename);
	result->addAttribute("size", std::to_string(fSize));
	result->addAttribute("content-type", fContentType);
	return result;
}

gloox::StanzaExtension* Extension::newInstance(const gloox::Tag* tag) const
{
	return new Extension(tag);
}

gloox::StanzaExtension* Extension::clone() const
{
	return new Extension(*this);
}
}
