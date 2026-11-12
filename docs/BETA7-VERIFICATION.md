# Beta 7 verification, 12 November 2026

Source: `a17cf18`, release `v0.1.0-beta.7`, package `0.1.0~beta7-1`.

- Native Haiku build and 523 tracked source inputs verified.
- 72 history, 66 protocol/reaction and 329 attachment/layout checks passed; delivery correlation checks also passed.
- Reproduced an old delayed mention notification on beta 6. The beta 7 candidate suppressed replayed mentions and duplicates while emitting each fresh nickname/reference mention once. Captured actual native notification sends in an isolated synthetic fixture.
- Exercised one muted sent check, two muted delivered checks, and two white read checks from actual synthetic XMPP receipts. Light/dark and narrow/wide layouts inspected.
- Installed beta 7 over beta 6-3. Status and reactions survived application restart. A late delivery receipt updated persisted history after restart.
- Installed executable SHA-256: `72488501ee74f8981977b8672711dbbab158b3b2701f7c585ebda5242114d164`.
- Anonymous GitHub downloads matched local HPKG and source hashes. HPKG: `1b61b28003839b32ff204b213a17590c72967cbed0a1c0fe04ce9255efe9347d`.

Read status requires a real compatible displayed receipt, not a transport acknowledgement. Group counts do not imply everyone has read a message. Live Signal interoperability and native Spritz/HaikuDepot installation were not exercised in this acceptance pass. The production bridge was not modified.

Rollback: retain a profile backup before downgrading and use the prior beta 6-3 release. Beta 7 did not add a history schema migration. Do not restore stale history over newer messages merely to roll back the executable.
