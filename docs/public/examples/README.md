# Configuration templates

All domains are reserved examples. These templates contain no working credentials.
Do not start them unchanged. They are the reviewable topology for a new isolated
operator installation, not an accepted turnkey production deployment.

- `app-settings.xml`: merge selected fields into the closed client's existing XML.
- `slidgnal.conf.example`: pinned Slidge setting names; replace the component secret.
- `compose.yaml.example`: Linux host networking, loopback component, no published ports.
- `prosody.cfg.lua.example`: dedicated server skeleton with TLS, upload and MAM.

Replace domains consistently, configure trusted certificates/renewals, provision
private state and secrets, select the combined patched image digest, and validate
both parsers before activation. The precise Slidge delegated stanza privileges are
still a release gate. Do not grant unrestricted component privileges as a shortcut.
Keep completed files outside Git. A configuration's syntax passing is not evidence
that DNS, TLS, permissions, registration or message delivery work.
