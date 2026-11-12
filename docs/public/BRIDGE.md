# Self-hosted Signal bridge

## Guided Linux installation

The guided installer creates a **new, separate Signal component** on a Linux amd64
server with Docker Engine, Compose v2, Python 3.10+ and an existing local Prosody 13.
It does not change an existing bridge, create DNS records, install TLS certificates,
create XMPP accounts or register the Signal device automatically.

Prerequisites: your own XMPP account, working client TLS and HTTPS uploads on that
Prosody server. Keep external component TCP private on loopback. Allow about 3 GiB
of free disk and 4 GiB RAM for the first native build. No paid service is required.

Download the pinned installer source, then run the wizard:

```sh
git clone --depth 1 --branch bridge-v0.1.0-beta.1 https://github.com/kambuziarendal/MimisSendir.git
cd MimisSendir
sudo python3 bridge/install.py init /opt/mimissendir-bridge
sudo cat /opt/mimissendir-bridge/NEXT-STEPS.txt
```

The wizard asks for the existing domain/account, component name, HTTPS upload
hostname and local component port. It generates a random private component secret,
a single-account allowlist, private state directory and reviewed-source snapshot.
It refuses an existing destination. **It starts nothing and never edits Prosody.**
The next-steps file explains the two generated Prosody snippets. Review and merge
these into your existing configuration, preserving other hosts, modules and
privileges. Install the supplied pinned `mod_privilege.lua` with its license;
check Prosody configuration/certificates before reloading your own server.

Then build once and start:

```sh
sudo python3 /opt/mimissendir-bridge/source/install.py build /opt/mimissendir-bridge
sudo python3 /opt/mimissendir-bridge/source/install.py start /opt/mimissendir-bridge
sudo python3 /opt/mimissendir-bridge/source/install.py doctor /opt/mimissendir-bridge
```

The build verifies upstream Python/native source and Go toolchain checksums, applies
both patch sets, runs the encrypted-length upload regression, and records the exact
local image ID. Start uses that ID, without pulling a changed tag. The first build
needs Internet access and can take several minutes. Later starts reuse the image.
`doctor` verifies the running image and an authenticated XMPP-component heartbeat,
**not Signal pairing or end-to-end delivery**. Allow roughly a minute on first start.

Use Gajim or another XMPP client supporting gateway registration/ad-hoc commands to
register with the configured component and scan the linked-device QR from Signal
on your phone. The Haiku onboarding UI is not yet verified for this complete flow.
Never register or copy an already linked device into two simultaneously running
bridges. Test messages and attachments with an agreed test contact before relying
on the installation. The installer does not send messages itself.

### Stop, backup and offline restore

```sh
sudo python3 /opt/mimissendir-bridge/source/install.py stop /opt/mimissendir-bridge
sudo python3 /opt/mimissendir-bridge/source/install.py backup /opt/mimissendir-bridge /safe/private/bridge-backup.tar.gz
sudo python3 bridge/install.py restore /safe/private/bridge-backup.tar.gz /opt/mimissendir-bridge-restored
```

Backup briefly stops an active component for a consistent snapshot, then resumes
it, including after a recoverable archive-write failure. Use a new backup filename
outside the installation directory. The archive is mode 0600 and contains private
linked-device keys, message state and component credentials. Keep it private and
encrypt it before off-host storage. Prosody/account/upload backups remain separate.
Restore refuses an existing destination, rejects links/traversal, verifies the
retained source snapshot and **does not start the restored copy**. Stop the original
before starting the restored copy, including on another host. On the same Docker
daemon the installer also rejects concurrent copies of the same installation.
Rebuild from the retained source if restoring to a host without the recorded image.

Keep the source snapshot and image with backups. No automatic upgrades or database
rollback are attempted. A future bridge update needs a fresh compatibility check.
The old templates below remain reference material, not alternative copy-paste
instructions for an existing production server.

## Architecture and trust

Signal device ↔ Signal service ↔ Slidgnal/Slidge ↔ Prosody ↔ TLS ↔ Haiku client.

Signal encryption ends at the bridge. The server can process message contents,
attachments, account mappings and linked-device keys. This is **not** uninterrupted
Signal end-to-end encryption to the Haiku application. Choose a server/operator
you trust. Do not connect to a third-party bridge on the assumption it cannot read messages.

## Version baseline

| Component | Preparation baseline |
| --- | --- |
| Prosody | 13.0.1, Debian Linux |
| Slidgnal | 0.5.1, Linux amd64 |
| Slidge | 0.5.2, inside the pinned image |
| Base OCI image | `codeberg.org/slidge/slidgnal:v0.5.1-amd64@sha256:ea4aaf58ad5eab65214ebfb43bfe4178b30baa90251c78f54b57e1cd79756982` |
| Python | 3.13 in that image |
| Native Signal dependency | vendored mautrix-signal v0.2608.0 |

Our Python overlay fixes startup event ordering, isolates inaccessible groups,
bounds correlation maps and separates native metadata/action workers. Read-only
retry is bounded; sending is not automatically replayed after an uncertain timeout.
The additional native upload fix passes the **encrypted payload length** to
Signal's attachment upload form endpoint. The Python overlay alone is not the
complete attachment-capable bridge release.

`bridge/Dockerfile.combined` is the portable public build. It needs no private
parent image, local toolchain or live account. `Dockerfile.native-upload` is retained
as historical evidence and is not the public installer. See BUILD.md and LICENSES.md.

## Installation sequence

1. Reserve your own XMPP domain (`chat.example.org`) and upload hostname
   (`upload.example.org`). The component JID is `signal.chat.example.org`.
   Allocate storage, backups and an operator contact. This guide provides no hosted account.
2. Install Prosody 13 and Docker Engine with Compose v2 on a dedicated Linux amd64
   host. Set up the firewall and time synchronization. Keep component TCP 5347
   private; publish only the intended client TCP 5222 and HTTPS TCP 443 endpoints.
   Server-to-server federation is a separate deliberate choice, not required here.
3. Obtain trusted TLS certificates. Import them using Prosody's documented
   certificate procedure and configure renewal hooks. Confirm renewals reload the
   correct service. Expose HTTP upload through an HTTPS reverse proxy; internal
   HTTP 5280 must remain loopback-only. Never send the component secret over the Internet.
4. Adapt the [Prosody template](examples/prosody.cfg.lua.example) in a dedicated
   config. It is not a replacement for an existing server's global configuration.
   Install required modules, check available privileges against the pinned Slidge
   documentation, and run `prosodyctl check config` and `prosodyctl check certs`.
   Missing component privileges are a setup failure, not a reason to grant all access.
5. Create an XMPP account interactively with `prosodyctl adduser alice@chat.example.org`.
   Keep public registration disabled. Generate a random component secret into a
   private file, then enter the same value into both component configurations.
   Do not put the secret into shell history, a Git repository or a support report.
6. Copy the Compose and Slidgnal templates to a private installation directory.
   Set `BRIDGE_IMAGE` to the reviewed **combined** image digest, not `latest`.
   Create its state directory owned by UID/GID 10000 with mode 0700. Mount only
   the reviewed config (read-only) and state. The example uses host networking on
   Linux so the component endpoint is loopback; it exposes no new container listener.
7. Validate `docker compose config --quiet` before activation. After the isolated
   acceptance test, start the intended stack with `docker compose up -d` and inspect
   its status. Do not paste raw logs publicly. Verify the component authenticates.
8. Log in with an XMPP client supporting gateway registration/ad-hoc commands.
   Follow Slidgnal's registration flow and scan the linked-device QR from your
   existing Signal phone account. A separate administration client may be needed;
   the Haiku fork is not yet a verified complete onboarding UI. QR codes are private.
   For an existing bridge, preserve linked-device state and **do not re-register**.
9. Apply the matching Haiku client profile. Test only agreed synthetic/test contacts
   and groups: both directions, Unicode, attachment upload/download, reactions,
   reconnect, history, missing avatar and one unavailable group.
10. Reboot the isolated server/client, confirm persistence, exercise backup restore
    in an offline clone, then run a documented soak. See OPERATIONS.md.

## Services and retention

XEP-0363 uploads need an HTTPS endpoint and appropriate authorization for both
client and bridge. Privileged entity access is sensitive: pin the exact Prosody
module and Slidge requirements, restrict it to the bridge JID, and verify stanza
permissions rather than using blanket component privileges. The generated privilege snippet uses the pinned Slidge/Prosody requirements; keep
it restricted to the selected component and preserve other server privileges.

Recommended starting limits are 50 MiB/file, 200 MiB/user/day, 512 MiB total and
7-day upload/archive retention. Confirm actual module options and available disk
space. Retention is not automatic deletion on every receiving device or backup.
Do not advertise unlimited storage or message recovery.

Primary upstream references:
- https://slidge.im/
- https://codeberg.org/slidge/slidgnal
- https://codeberg.org/slidge/slidge
- https://prosody.im/doc/

Use documentation for the pinned versions, not an unreviewed current branch.
