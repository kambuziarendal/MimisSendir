// Synthetic metadata only. No real contacts or message content.
#include "network/AttachmentMetadata.h"
#include "network/UploadDiscovery.h"
#include "network/MessagePresentation.h"
#include "network/DeliveryReceipt.h"
#include <gloox/oob.h>
#include <gloox/tag.h>
#include <cstdlib>
#include <memory>
#include <iostream>
static int count;
static void Check(bool ok, const char* name) {
	++count;
	if (!ok) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}
int main() {
	Check(Upload::DiscoveryHost("upload.example.test", "example.test"), "same-domain discovery allowed");
    for (const char* host : {"example.test.evil", "evil.test", "user@upload.example.test", "upload.example.test/resource", "upload.example.test:443"})
        Check(!Upload::DiscoveryHost(host,"example.test"), "untrusted discovery target rejected");
    Check(!Upload::DiscoveryHost("127.0.0.1", "127.0.0.1"), "numeric discovery rejected");
    Check(Upload::IsVideoFilename("https://upload.example.test/clip.MP4?sig=abc"), "MP4 playback recognized");
    Check(!Upload::IsVideoFilename("https://upload.example.test/x.exe?name=clip.mp4"), "query cannot spoof video type");
    Check(!Upload::IsPreviewFilename("https://upload.example.test/clip.mp4"), "video never automatically downloads");
    Check(Upload::BodyAttachmentUrl(" \nhttps://upload.example.test/clip.mp4\n", "upload.example.test") == "https://upload.example.test/clip.mp4", "legacy URL-only history recovered");
    Check(Upload::BodyAttachmentUrl("https://upload.example.test/clip.mp4", "").empty(), "unconfigured history never auto-fetches");
    const std::string host = "upload.example.test";
	const std::string url = "https://" + host + "/slot/picture.png";
	gloox::Message message(gloox::Message::Chat, gloox::JID("peer@example.test"), "Synthetic caption");
	message.addExtension(new gloox::OOB(url, "Synthetic image", false));
	Check(Upload::AttachmentUrl(message, host) == url, "OOB recognized with real gloox");
	Check(Upload::AttachmentBody(message) == "Synthetic caption\n" + url, "caption and OOB preserved");
	Check(Upload::AttachmentUrl(message, "other.example.test").empty(), "other authority blocked");
	gloox::Message body(gloox::Message::Chat, gloox::JID("peer@example.test"), "\n " + url + " \t");
	Check(Upload::AttachmentUrl(body, host) == url, "standalone body fallback trimmed");
	gloox::Message mixed(gloox::Message::Chat, gloox::JID("peer@example.test"), "Discuss " + url);
	Check(Upload::AttachmentUrl(mixed, host).empty(), "ordinary prose does not auto-fetch");
	gloox::Message duplicate(gloox::Message::Chat, gloox::JID("peer@example.test"), url);
	duplicate.addExtension(new gloox::OOB(url, "", false));
	Check(Upload::AttachmentBody(duplicate) == url, "no duplicate body URL");
	for (const auto& invalid : {"http://upload.example.test/a.png", "https://evil.example/a.png",
		"https://upload.example.test.evil/a.png", "https://user@upload.example.test/a.png",
		"file:///boot/home/private.png", "https://upload.example.test/a.png#fragment"}) {
		gloox::Message bad(gloox::Message::Chat, gloox::JID("peer@example.test"), invalid);
		Check(Upload::AttachmentUrl(bad, host).empty(), "unsafe source rejected");
	}
	Check(Upload::IsPreviewFilename(url), "PNG auto preview candidate");
	Check(Upload::IsPreviewFilename("https://upload.example.test/a.JPEG?token=x"), "JPEG query preview candidate");
	Check(!Upload::IsPreviewFilename("https://upload.example.test/a.svg"), "SVG never auto-decoded");
	Check(!Upload::IsPreviewFilename("https://upload.example.test/a.exe?name=a.png"), "query cannot change file type");
	Check(Upload::AttachmentFilename("https://upload.example.test/a/../") == "attachment", "empty leaf fallback");
	Check(Upload::AttachmentFilename("https://upload.example.test/%2fboot%2fhome") == "_2fboot_2fhome", "no path percent decoding");
	Check(Upload::AttachmentFilename("https://upload.example.test/..").find('/') == std::string::npos, "no parent path name");
	Check(Upload::AttachmentFilename("https://upload.example.test/" + std::string(200, 'a')).size() == 100, "bounded filename");
	gloox::Tag x("x"); x.setXmlns("jabber:x:oob"); new gloox::Tag(&x, "url", url);
	gloox::Message parsed(gloox::Message::Chat, gloox::JID("peer@example.test"));
	parsed.addExtension(new gloox::OOB(&x));
	Check(Upload::AttachmentUrl(parsed, host) == url, "wire OOB parsing");
	Check(Chat::Classify(message) == Chat::MessageKind::Content, "ordinary chat remains visible");
	Check(Chat::Classify(parsed) == Chat::MessageKind::Content, "OOB-only attachment remains visible");
	gloox::Message control(gloox::Message::Chat, gloox::JID("peer@example.test"));
	Check(Chat::Classify(control) == Chat::MessageKind::Ignore, "bodyless typing/receipt creates no reply");
	gloox::Message notice(gloox::Message::Normal, gloox::JID("peer@example.test"), "Service notice");
	Check(Chat::Classify(notice) == Chat::MessageKind::Content, "normal service text remains visible");
	gloox::Message failure(gloox::Message::Error, gloox::JID("peer@example.test"));
	Check(Chat::Classify(failure) == Chat::MessageKind::DeliveryError, "empty error is not empty contact reply");
	gloox::Message echoed(gloox::Message::Error, gloox::JID("peer@example.test"), "Original outgoing text");
	echoed.addExtension(new gloox::OOB(url, "", false));
	Check(Chat::Classify(echoed) == Chat::MessageKind::DeliveryError, "error echo cannot become incoming content or attachment");
	gloox::Message embedded(gloox::Message::Chat, gloox::JID("peer@example.test"), "Original outgoing text");
	embedded.addExtension(new gloox::Error(gloox::StanzaErrorTypeWait, gloox::StanzaErrorRecipientUnavailable));
	Check(Chat::Classify(embedded) == Chat::MessageKind::DeliveryError, "error extension takes priority over body");
	Check(embedded.error()->error() == gloox::StanzaErrorRecipientUnavailable, "delivery error condition retained");
	gloox::Message headline(gloox::Message::Headline, gloox::JID("peer@example.test"), "Metadata event");
	Check(Chat::Classify(headline) == Chat::MessageKind::Ignore, "headline is not a contact reply");
    gloox::Tag received("received"); received.setXmlns("urn:xmpp:receipts");
    received.addAttribute("id", "sent-message-42");
    Chat::Receipt receipt(&received);
    Check(receipt.id() == "sent-message-42", "receipt references original message ID");
    gloox::Tag foreign("received"); foreign.setXmlns("urn:foreign"); foreign.addAttribute("id", "sent-message-42");
    Check(Chat::Receipt(&foreign).id().empty(), "foreign receipt namespace ignored");
    Chat::Receipt request(true);
    std::unique_ptr<gloox::Tag> requestTag(request.tag());
    Check(requestTag->name() == "request" && requestTag->xmlns() == "urn:xmpp:receipts", "outgoing request is real XEP0184");
    control.addExtension(receipt.clone());
    Check(Chat::Classify(control) == Chat::MessageKind::Ignore, "delivery receipt cannot become chat bubble");
	Check(Upload::IsPreviewFilename("https://upload.example.test/a.gif"), "GIF preview candidate");
    Check(Upload::IsPreviewFilename("https://upload.example.test/a.webp"), "WebP preview candidate");
    Check(Upload::DisplayAttachmentBody(url, url).empty(), "image-only body hides URL");
    Check(Upload::DisplayAttachmentBody("Caption\n" + url, url) == "Caption", "image caption retained");
    Check(Upload::DisplayAttachmentBody("hello", "") == "hello", "ordinary text untouched");
    Check(Upload::DisplayAttachmentBody("https://upload.example.test/a.pdf", "https://upload.example.test/a.pdf") .empty(), "document URL hidden by attachment card");
    std::cout << count << " attachment metadata checks passed\n";
}
