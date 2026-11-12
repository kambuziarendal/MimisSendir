// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <gloox/stanzaextension.h>
#include <gloox/tag.h>

namespace Upload {
inline constexpr uint64_t kMaxFileSize = 50ULL * 1024 * 1024;
inline constexpr const char* kNamespace = "urn:xmpp:http:upload:0";

struct Slot {
	std::string putUrl;
	std::string getUrl;
	std::vector<std::pair<std::string, std::string>> headers;
};

// The configured authority is exact, including a non-default port if needed.
// Redirects, userinfo, fragments and non-HTTPS URLs are intentionally disallowed.
bool IsServiceUrl(const std::string& url, const std::string& authority);
std::string ValidateFile(const std::string& filename, uint64_t size,
	const std::string& contentType);
std::string ValidateSlot(const Slot& slot, const std::string& authority);

class Extension final : public gloox::StanzaExtension {
public:
	explicit Extension(const gloox::Tag* tag = nullptr);
	Extension(std::string filename, uint64_t size, std::string contentType);
	const std::string& filterString() const override;
	gloox::Tag* tag() const override;
	gloox::StanzaExtension* newInstance(const gloox::Tag* tag) const override;
	gloox::StanzaExtension* clone() const override;
	bool IsSlot() const { return fValidSlot; }
	const Slot& GetSlot() const { return fSlot; }
private:
	bool fRequest = false;
	bool fValidSlot = false;
	std::string fFilename;
	uint64_t fSize = 0;
	std::string fContentType;
	Slot fSlot;
};
}
