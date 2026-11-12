// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "AttachmentMetadata.h"
#include "HttpUpload.h"
#include <gloox/oob.h>
#include <algorithm>
#include <cctype>
namespace Upload {
std::string AttachmentBody(const gloox::Message& message)
{
	std::string body = message.body();
	const auto* oob = message.findExtension<gloox::OOB>(gloox::ExtOOB);
	if (oob && oob->url().size() <= 8192 && oob->url().starts_with("https://")
		&& oob->url().find_first_of("\r\n\t ") == std::string::npos
		&& body.find(oob->url()) == std::string::npos) {
		if (!body.empty()) body += "\n";
		body += oob->url();
	}
	return body;
}
std::string AttachmentUrl(const gloox::Message& message, const std::string& authority)
{
	const auto* oob = message.findExtension<gloox::OOB>(gloox::ExtOOB);
	if (oob && IsServiceUrl(oob->url(), authority)) return oob->url();
	std::string body = message.body();
	const auto first = body.find_first_not_of(" \r\n\t");
	if (first == std::string::npos) return {};
	body = body.substr(first, body.find_last_not_of(" \r\n\t") - first + 1);
	return IsServiceUrl(body, authority) ? body : std::string();
}
std::string AttachmentFilename(const std::string& url)
{
	const std::string path = url.substr(0, url.find_first_of("?#"));
	std::string filename = path.substr(path.find_last_of('/') + 1);
	// Never percent-decode a remote path into a local filename or trust MIME names.
	if (filename.empty() || filename == "." || filename == "..") filename = "attachment";
	for (char& c : filename) {
		if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_')) c = '_';
	}
	if (filename.size() > 100) filename.resize(100);
	return filename;
}
std::string AttachmentType(const std::string& filename) {
    auto name=filename;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
    auto dot=name.find_last_of('.');auto ext=dot==std::string::npos?"":name.substr(dot+1);
    if(ext=="pdf")return "PDF document";
    if(ext=="zip" || ext=="gz" || ext=="7z")return "Archive";
    if(ext=="mp3" || ext=="ogg" || ext=="m4a" || ext=="wav")return "Audio";
    if(ext=="mp4" || ext=="mov" || ext=="webm")return "Video";
    if(ext=="txt" || ext=="md")return "Text document";
    if(ext=="doc" || ext=="docx" || ext=="odt")return "Document";
    if(ext=="xls" || ext=="xlsx" || ext=="ods")return "Spreadsheet";
    return "File attachment";
}
std::string AttachmentDisplayName(const std::string& url) {
    auto name=AttachmentFilename(url);auto dot=name.find_last_of('.');
    auto stem=name.substr(0,dot);
    // Bridges sometimes supply only a content hash. Do not pretend it is a meaningful title.
    if(stem.size()>=24 && std::all_of(stem.begin(),stem.end(),[](unsigned char c){return std::isxdigit(c);}))
        return "Received file"+(dot==std::string::npos?"":name.substr(dot));
    return name;
}
bool IsPreviewFilename(const std::string& url)
{
	auto name = AttachmentFilename(url);
	std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
	return name.ends_with(".png") || name.ends_with(".jpg") || name.ends_with(".jpeg")
        || name.ends_with(".gif") || name.ends_with(".webp");
}
std::string DisplayAttachmentBody(const std::string& body,const std::string& attachment) {
    if(attachment.empty())return body;
    std::string result=body;size_t at=0;
    while((at=result.find(attachment,at))!=std::string::npos)result.erase(at,attachment.size());
    auto first=result.find_first_not_of(" \r\n\t");
    return first==std::string::npos ? "" : result.substr(first,result.find_last_not_of(" \r\n\t")-first+1);
}
}
