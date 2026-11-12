# Installer internals

Operator entrypoint: `../install.py`. Public guide: `../../docs/public/BRIDGE.md`.

- Linux amd64, root-owned private installation, existing local Prosody 13.
- Component-only deployment, host network, loopback authentication. No DNS/TLS, account or Signal pairing automation.
- `Dockerfile.combined` uses the public Slidgnal digest, verified Python/native inputs and checksum-pinned Go 1.26.10. Compiler and download scratch use BuildKit tmpfs.
- `launch.py` adds an authenticated-component heartbeat without message/account data. `health.py` checks freshness and process existence. Neither claims Signal readiness.
- Lifecycle operations use a recorded random Compose project and installation label. Local restore copies retain the installation identity, get a separate project and cannot start concurrently.
- Backup stops the component, copies the full private state/config/source, and resumes only if it was running. Restore is offline, refuses an existing destination and rejects archive links/traversal.
- Restored source files retain safe readability/executable modes for Docker COPY; private config and state remain owner-only.
- No automatic upgrades or rollback of databases. Do not share archives/configuration publicly.

Checks: `sudo python3 bridge/tests/installer.py`, Python overlay unit tests, final-image pinned adapter tests, native encrypted-upload regression, real isolated Prosody authentication and private backup/offline restore.
