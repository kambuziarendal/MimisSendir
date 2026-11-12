# Security and privacy

Kambuzi EDB develops and publishes the fork. A self-hosted bridge operator is
responsible for that deployment's accounts, access, retention and operational privacy.
There is no bundled Kambuzi-hosted service and no promised public registration.

## Data and recipients

| Location/service | Data involved |
| --- | --- |
| Haiku client | Account settings, local message history, avatars and downloaded media; password in KeyStore. |
| XMPP server | Authentication, routing, roster, archives and uploaded files according to operator settings. |
| Signal bridge | Decrypted message processing, linked-device credentials, mappings, attachment/avatar caches. |
| Signal service | Requests/messages needed by the linked device. |
| Sticker service | Requested pack identifiers/keys and network metadata when importing packs. |
| GIPHY, when enabled | Search terms and request/network metadata; optional provider API key. |
| Distribution host/Spritz Depot | Package requests and metadata under their own privacy policies. |

The Signal bridge is an encryption endpoint. TLS between Haiku and XMPP does not
turn it into uninterrupted Signal end-to-end encryption. Do not advertise that property,
OMEMO support, audited security or universal XMPP compatibility without evidence.

Kambuzi EDB has not added an intentional telemetry/crash-upload service to this
fork. This is not a claim of an independent security audit. Server logs and third-party network services are separate from telemetry.

Attachment URLs may grant bearer access. Treat them as private even when served
over HTTPS. Local files and server backups are not automatically encrypted by the
app. OS account separation and encrypted storage are operator/user responsibilities.
Deleting a local conversation is not a promise to erase messages from other devices,
server archives, Signal, expired-file caches or backups.

## Public defaults

No private endpoints or API keys, no account preconfiguration, TLS verification on,
no open bridge registration, component secret restricted, no public component port,
bounded uploads/logs and explicit retention. Never bundle real conversation screenshots.
Use a synthetic account and synthetic artwork for catalog screenshots.

## Vulnerability reporting

Use [GitHub private vulnerability reporting](https://github.com/kambuziarendal/MimisSendir/security/advisories/new).
Do not post credentials, QR codes or exploitable details involving a live user account
in a public issue. Send affected version, platform, minimal reproduction and sanitized
impact through that private channel. Rotate exposed credentials with the operator;
removing them from the latest Git commit does not remove them from history.
