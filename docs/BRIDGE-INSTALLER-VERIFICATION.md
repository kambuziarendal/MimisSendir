# Guided bridge installer verification, 12 November 2026

Scope: new Linux amd64 Signal component on a prepared local Prosody server. No existing production configuration, state or containers were modified. DNS, trusted public TLS, phone pairing and live Signal delivery remain operator steps, not verified turnkey automation.

- Public digest-pinned Slidgnal 0.5.1 base, verified upstream Python/native source, checksum-pinned Go 1.26.10. Combined Python/native build completed without private images, toolchain mounts or live account data.
- Native encrypted-upload-length regression passed. 19 Python worker/queue tests and 7 final-image pinned adapter tests passed.
- 7 installer tests passed: private permissions and scoped allowlist, injection/recursive-destination rejection, existing-path refusal, offline backup/restore, unsafe archive rejection, recovery after backup write failure, concurrent-copy guard and operation locking.
- Actual wizard, source snapshot, Compose validation, immutable-ID build/start and authenticated-component heartbeat exercised against a fresh loopback-only Prosody 13 fixture. No Signal account was registered.
- Actual backup stopped and resumed only the test component. Restored five state files matched the archive before opening SQLite, and both restored databases passed integrity checks. Restored clones remained offline. Starting a second copy while the original ran was refused.
- Restored source modes remain readable for Docker COPY. Config/state remain private. Health was healthy again after backup/restart.
- Tested image: `sha256:2cef8eaf04055b4a0730ec144a033d691ff1c8c467c973a269b585971e142131`. Independently prepared source snapshots produced the same final image in this environment; this is not a cross-host reproducible-build guarantee.
- Production bridge image and original start time/restart count were unchanged. Owned test components were stopped; private fixture snapshots/backups retained locally.

The doctor checks image identity, authenticated XMPP transport and event-loop heartbeat. It does not verify Signal account pairing, message delivery, media delivery, public TLS or backup off-site encryption. The installer does not merge Prosody configuration or grant broader server permissions automatically.
