# Operator handbook

Applies to the separate Linux bridge. Adapt paths to your reviewed deployment.
Do not copy a live private deployment's config or data into a public example.

For guided installations, use the scoped `build`, `start`, `stop`, `doctor`,
`backup` and `restore` commands in [BRIDGE.md](BRIDGE.md). They never discover
or alter an unrelated bridge.

## Routine checks

Check component connectivity, linked-device state, worker/queue health, disk/quota,
TLS expiry, backup freshness and upload expiration. XMPP presence alone is not a
Signal delivery check. Use an agreed test peer for an end-to-end health test.
Do not send automatic health messages to real users without their consent.

Use `docker compose ps` and bounded local logs. Prefer counters, failure categories
and version/digest to payload logs. Rotate logs and restrict access. Never collect
Signal keys, QR codes, passwords or complete conversation databases for routine support.

## Backup

Back up the entire persistent bridge state (Signal device files, SQLite mappings,
avatar references), Prosody state/config, upload storage as required by retention,
and the exact image/source manifest. Configs and backups contain secrets.
Encrypt backups, restrict access and choose a documented retention/offsite policy.

For a simple consistent backup, arrange a maintenance window, stop the bridge
and relevant writers, verify they are stopped, then snapshot/copy their complete
state. Restart the same images after the copy. For an online SQLite backup use
SQLite's backup API; do not copy only the live `.sqlite` file and ignore WAL.
An online database backup alone does not make concurrent Signal-state files consistent.

Record timestamp, application/schema versions, image digest and encrypted backup
checksum. Run SQLite `PRAGMA integrity_check` on the backup in an offline copy.
Test restoration to a network-isolated clone. **Never start two active bridges
with the same Signal device identity** as a restore test.

## Upgrade

1. Read upstream and fork release notes, schema changes and rollback limitations.
2. Pin the new image digest and its complete source manifest. Run offline adapter
   tests and the native upload regression against the exact candidate.
3. Verify a consistent backup and offline restoration. Record the previous image
   and current volume/config identities. Never use `docker compose down -v`.
4. Stop only the intended bridge, change only its image pin, restart, and confirm
   the same current state volume and linked-device identity are used.
5. Check component login, read-only synchronization and agreed test conversations,
   attachments, reconnect and worker health. Capture sanitized evidence.
6. If unhealthy, stop the candidate. A code-only rollback may reuse current data
   only if schema compatibility is confirmed. Otherwise prepare a compatible
   forward fix; do not overwrite new messages/state with an old database.

Database/Signal-state restoration is disaster recovery, not a routine downgrade.
It can lose current messages and invalidate linked-device state. Assess it separately.

## Failure handling

- An unavailable/inaccessible group should not prevent other groups syncing.
- A native action timeout has an **unknown delivery outcome**. Check the receiver;
  do not replay sends automatically or create another worker until the old call ends.
- A timed-out metadata call may retain its worker slot; explicit busy/failure is
  preferable to unbounded threads. Inspect health before restarting.
- Authentication/403 errors are not transient retries. Rate limiting requires cooldown.
- Queue overload is visible failure, not a durable replay guarantee. Document any gap.
- Missing group avatars may originate in bridge storage. Inspect counts/associations
  read-only; never publish contact identifiers or images while diagnosing it.
- Do not delete bridge state or scan a new QR as the first troubleshooting step.

## Acceptance before public support

Verify fresh setup by a second operator, a restart/reconnect sequence, a test
attachment above and below configured limits, revoked access to one group,
expired uploads, certificate renewal, disk-full behavior and offline restore.
Record a real stability observation period (target at least 24 hours); elapsed time
alone does not prove that all these checks ran.
