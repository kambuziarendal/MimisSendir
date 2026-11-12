// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <gloox/tag.h>
#include <string>
namespace Chat {
// Pinned gloox clones may detach unqualified children from their parent.
// Resolve their default namespace from the already-validated enclosing element.
inline const gloox::Tag* ArchiveChild(const gloox::Tag* parent,const char* name,const char* ns) {
    if(parent)for(const auto* child:parent->children())
        if(child->name()==name && (child->xmlns()==ns || (child->xmlns().empty() && child->prefix().empty() && parent->xmlns()==ns)))return child;
    return nullptr;
}
inline bool ArchiveSender(const std::string& sender,const std::string& own,const std::string& peer,bool group) {
    return group?sender==peer:sender.empty() || sender==own;
}
inline bool ArchivePeer(const std::string& from,const std::string& to,const std::string& own,const std::string& peer,bool group) {
    return group?from==peer:(from==own && to==peer) || (from==peer && to==own);
}
}
