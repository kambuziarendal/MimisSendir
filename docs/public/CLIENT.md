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

The package conflicts with upstream Renga and retains the existing application/profile identity. Upgrades preserve history/settings. Side-by-side installation is not supported. Never delete a profile to solve an upgrade problem.

Before upgrading, close the application and back up
`~/config/settings/jabber-for-beos/`. This includes local conversation data;
protect it as private material. Passwords use Haiku's KeyStore and are not made
portable merely by copying that directory. Avatar cache is under
`~/config/cache/Renga/avatars-v2/`; it is not the authoritative message store.

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

For operator-managed advanced configuration, profiles live in `~/config/settings/jabber-for-beos/app-settings`.
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

Beta 4 adds right-click **Edit…** on your sent text messages and reactions in the timestamp footer. Beta 5 adds a jump-to-latest button beside the composer when scrolled up. Read receipts are not implemented as a working end-to-end feature in the current beta.

## Stickers and GIFs

Direct installation from Signal Stickers works in **WebPositive** (user-verified).
The tested Firefox-on-Haiku workaround uses a downloaded `.renga-sticker` link file
from the supplied installer page: click the pack, then open the download in the client,
review the pack and import. Do not claim that Signal Stickers' own install button
works in Firefox. Packs are downloaded, validated and imported, not bundled as an
unlicensed collection with the public application.

Local GIFs work without a GIPHY key. Online GIPHY search is optional and currently
needs a separately configured key; the private key must never be distributed in
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
