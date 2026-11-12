// Copyright 2026 Kambuzi EDB. Distributed under the MIT license.
#pragma once
#include "support/SettingsValidation.h"
#include <algorithm>
#include <cctype>
#include <string>
namespace Upload {
// Only authenticated account-server discovery may call this. Never infer trust
// from a received message, URL, room member, or a similarly named domain.
inline bool DiscoveryHost(const std::string& service, const std::string& domain) {
    if (!SettingsValidation::Domain(service) || !SettingsValidation::Domain(domain)
        || domain.find('.') == std::string::npos) return false;
    if (std::all_of(domain.begin(), domain.end(), [](unsigned char c) { return std::isdigit(c) || c == '.'; })) return false;
    return service == domain || (service.size() > domain.size()
        && service.ends_with("." + domain));
}
}
