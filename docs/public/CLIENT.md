# Client installation and use

Public releases are early betas. Download from the [GitHub releases page](https://github.com/kambuziarendal/MimisSendir/releases).

## Requirements

- Haiku x86_64. The current package requires R1/beta6 hrev59866_79 or newer;
  this is the tested packaging baseline, not a promise that every newer nightly works.
- An XMPP account, its password, a reachable server and valid TLS certificates.
- For Signal conversations: an operator-provided Slidgnal bridge and an existing
  Signal account linked through that bridge. Signal phone registration is not implemented here.
- Internet access for remote attachments and optional online sticker/GIF services.

No Windows, Linux desktop, macOS, ARM or 32-bit Haiku client is currently supported.
The bridge has a separate Linux amd64 requirement.

## Install, upgrade and uninstall

Install the published **HPKG** from the GitHub release page. Check the exact version, architecture and SHA-256 against the publisher's
manifest. Haiku's package manager must resolve dependencies; do not copy random
shared libraries into the application directory. Restart the application after upgrading.

From beta 8, MímisSendir and upstream Renga can be installed side by side. They have independent application IDs, settings, caches, sound events and password keyrings. MímisSendir does not replace the upstream package or change its profile. Never delete a profile to solve an upgrade problem.

On first launch, if the legacy `~/config/settings/jabber-for-beos/` profile exists, choose **Copy profile**, **Start fresh** or **Quit**. Close Renga and earlier MímisSendir versions before copying. The import copies settings, message history, receipts, roster/sidebar state and imported media into `~/config/settings/MimisSendir/`. It never merges into or overwrites an existing destination. An interrupted/failed copy is not activated; the original remains untouched. Copies containing symlinks or special files are refused.

If you choose to copy, the saved password for the profile's last account is copied from the `XMPP` keyring into the separate `MimisSendir` keyring when Haiku allows access. The original key is never removed or changed. If access is denied, enter the password yourself. Other accounts can be added normally. A new XMPP resource avoids evicting an upstream client's session. Subsequent use and account removal affect only the MímisSendir keyring. Avatar caches are rebuilt rather than shared.

Before upgrading, close the application and back up
`~/config/settings/MimisSendir/` (and `jabber-for-beos/` before the first import). This includes local conversation data;
protect it as private material. Passwords use Haiku's KeyStore and are not made
portable merely by copying that directory. Avatar cache is under
`~/config/cache/MimisSendir/avatars-v2/`; it is not the authoritative message store.

Uninstall through Haiku's package manager. Removing the app does not unlink a
Signal device, remove server-side history or intentionally erase the local profile.
Unlink Signal only through the operator's documented process or Signal's linked-device UI.

## Connect

Enter the full XMPP address (`alice@chat.example.org`) and account password in
the login window. The address is not a Signal telephone number. Use the operator's
connection details, not Kambuzi's private service. Never disable TLS verification.

Use **Settings** for account, connection, Signal routing, uploads and group-message
sounds. Save applies only edited fields; Cancel or closing the window discards edits.
Connection, bridge and upload changes apply at the next sign-in. Saving neither
restarts nor pairs a bridge. Passwords remain in KeyStore.

For operator-managed advanced configuration, profiles live in `~/config/settings/MimisSendir/app-settings`.
Close the application, back up this file, and merge only the required elements from
[the example](examples/app-settings.xml), keeping the existing `<container>` root
and unrelated settings. XML must remain well formed; malformed settings can be
replaced by the current parser. Do not put passwords into XML.

| Setting | Meaning |
| --- | --- |
| `xmpp-connect-host`, `xmpp-connect-port` | Optional explicit TCP endpoint; the account JID remains unchanged. |
| `xmpp-ca-file` | Optional CA PEM file for a private CA; normal public certificates need no override. |
| `signal-bridge-jid` | Bridge component JID used for status. |
| `signal-contact-domain`, `signal-group-domain` | Exact domains treated as Signal traffic for readiness checks. |
| `upload-service` | XEP-0363 upload-service JID, not an HTTPS URL. |
| `upload-authority` | Allowed upload hostname, optionally with port. |
| `upload-ca-file` | Optional CA PEM file for upload TLS. |

`xmpp-auth-mechanism=plain-over-tls` is an explicit compatibility option, not a
recommendation to weaken authentication. TLS remains required. Only enable it
when the operator has established why the normal mechanism is insufficient.

Public builds have empty service defaults. Configure Signal domains and upload endpoints in Settings if needed. Ordinary XMPP contacts outside configured Signal domains are not gated by bridge readiness. Broader interoperability is not yet established.

## Everyday use

- The three large top-left icons select Conversations, Contacts and Groups.
  Hover for the original text labels. They support keyboard activation.
- The sidebar toggle is prepared at the bottom-left, leaving the chat title aligned.
- The header's person-and-list icon opens Participants; Groups uses three people.
- Scroll the name list with the mouse wheel. Edge indicators appear only when more
  content exists above/below. The message history retains its own scrollbar.
- Use the attachment action for files and the media picker for emoji, GIFs and stickers.
  Only send to a conversation you intended. A timeout is not proof that delivery failed.
- A hidden conversation is not necessarily deleted on the server or other devices.
- Server history restoration depends on the server's archive and retention settings.
  A readiness/status indication is not proof of end-to-end Signal delivery.

Beta 4 adds right-click **Edit…** on your sent text messages and reactions in the timestamp footer. Beta 5 adds a jump-to-latest button beside the composer when scrolled up. Beta 6 shows **Read** beside the timestamp when the peer reports displaying that exact outgoing message. Groups show **Read by N participants** for received participant markers, not an “everyone” claim. A delivery receipt or reaction alone never means read. Read status persists locally across restart, including room-assigned ID aliases; server-archived displayed markers are applied when available. Receipts for unknown IDs or another conversation are ignored. Messages without a read receipt remain unconfirmed, not necessarily unread.

Beta 7 replaces the direct-chat **Read** text with compact status checks beside the timestamp:

- One muted check: submitted to XMPP or accepted by the bridge, not proof of Signal delivery.
- Two muted checks: a matching delivery receipt was received.
- Two white checks: a matching displayed/read marker was received. A dark outline keeps white checks visible in light themes.
- A small clock: still queued. An exclamation mark: sending failed. Old history with no transport evidence has no guessed status.

Group read counts remain visible so a read by one participant is not presented as everyone having read. Historical and duplicate group mentions no longer raise fresh system notifications or roster highlights on reconnect; genuinely new mentions still notify.

Incoming messages with read-marker support are acknowledged after a short, stable visible dwell in the active conversation. Hidden, minimized, inactive or clipped content is not acknowledged just because it was received. Both sides and any Signal bridge must support forwarding the markers. Signal privacy settings may suppress receipts. Native synthetic verification does not establish live Signal behavior for every bridge, especially group receipts.

## Stickers and GIFs

Direct installation from Signal Stickers works in **WebPositive** (user-verified).
The tested Firefox-on-Haiku workaround uses a downloaded `.mimissendir-sticker` link file
from the supplied installer page: click the pack, then open the download in the client,
review the pack and import. Do not claim that Signal Stickers' own install button
works in Firefox. Packs are downloaded, validated and imported, not bundled as an
unlicensed collection with the public application.

Local GIFs work without a GIPHY key. Online GIPHY search is optional and currently
uses your key from Settings → GIPHY; the private key must never be distributed in
a public HPKG. No provider key is included. Provider terms,
request limits and attribution apply. Sticker links may contain pack decryption
keys; do not paste them into public bug reports.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Cannot connect | JID/password, endpoint, DNS, clock, certificate chain and server logs. Do not bypass TLS. |
| XMPP connected, Signal unavailable | Bridge component and linked-device state. Do not automatically relink. |
| Group shows an initial instead of a picture | The bridge may have no group avatar; this is not proof of a client renderer failure. |
| Attachment fails | Upload JID/authority, TLS, size/quota, expiration, bridge version and Signal upload-length patch. |
| Old messages absent | Archive retention, local profile and account used. Do not overwrite current data with an old backup. |
| Duplicate delivery risk after timeout | Check the receiving device/history before resending. |
| GIF search unavailable | Online-key configuration, rate limit or network. Local GIFs are independent. |
| Firefox sticker button does nothing | Use WebPositive or the tested downloaded-link workflow. |

See [support](SUPPORT.md) for a privacy-preserving report.

Older `.renga-sticker` files can still be opened explicitly in MímisSendir. Their old file association is not taken over. New links use `.mimissendir-sticker`. Existing Signal-protocol handlers selected for other applications are preserved.

## GIPHY API key

Open **Settings → GIPHY**, paste your own key into the masked field, and choose **Save key**. The key is stored in Haiku KeyStore (`MimisSendir-Services`), not the XML profile. Only a non-secret key identifier is kept in settings. Saving a replacement stores it before changing the active selection, then removes the previous key. A denied save leaves the previous key working. **Remove key** deletes the selected key and disables online search. These two buttons apply immediately; the main Settings Save/Cancel buttons concern the other settings.

The saved key is never displayed or copied into the edit field. Haiku may request permission to access KeyStore. GIPHY search uses verified HTTPS without redirects and sends the query and API key to GIPHY; neither is written to application logs. A profile backup alone does not contain the key. No shared key or legacy system-file fallback is included.

## Images and video (beta 10)

When upload fields are blank, the client discovers XEP-0363 services advertised by
its authenticated account server. Automatic discovery accepts only a service in
that account server's own DNS domain. The discovered host is cached per account
for offline history. Explicit settings take precedence. If the HTTPS endpoint is
a separate CDN or uses a nonstandard port, enter its exact host in Settings → Uploads.
Neither arbitrary message links nor room members can add trusted download hosts.

Old URL-only records from that host are recognized again when history is displayed.
PNG, JPEG, GIF and WebP keep their inline previews. **Play video** downloads the
clip on demand (maximum 50 MiB), then plays it inside the message bubble. Controls
provide play/pause, replay, 10-second backward/forward seeking, mute and close.
Seeking while paused keeps the video paused. There is no autoplay. Only one video
plays at a time across conversations; leaving a conversation pauses its video.
Closing the conversation or app stops its player. The private downloaded file is
retained until the decoder finishes closing, then normal cache cleanup applies.

Codecs depend on the installed Haiku media add-ons. Inline playback supports
videos up to 4096 × 2160 pixels and 24 hours, with a preview up to 480 × 270 pixels.
The player decodes incrementally with a bounded audio buffer, not the whole clip
in memory. Unsupported files or unavailable audio output show an error. Use the
attachment menu's **Open in MediaPlayer** or **Save file** as a fallback. Cancel
stops downloads, and failed downloads and player initialization remain retryable.

The optional external MediaPlayer keeps its private temporary file after the chat
closes. Closing MímisSendir during external playback can leave that file until the
system temporary directory is cleaned. No file is executed or automatically
opened as another application type.
