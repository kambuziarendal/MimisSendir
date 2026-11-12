# Beta 8 verification, 12 November 2026

Package: `0.1.0~beta8-2`. Native Haiku R1 beta6 x86_64 acceptance in an isolated QEMU overlay with synthetic accounts. No production bridge changes.

- Upgraded beta 7 to beta 8 and installed upstream Renga 1.29 without uninstalling either. Both installed applications run with independent signatures, icons, profiles and keyrings.
- Copied the legacy profile, verified its original bytes remained unchanged, loaded history/reactions/status, and copied the selected password with KeyStore permission. The original Renga still loads its password. Copy is refused while Renga runs; Start fresh and subsequent restart were exercised.
- Portable and native migration checks cover copying, isolation, 0700/0600 permissions, existing destinations, links and failed-copy cleanup. Native transactional settings checks, 44 sticker protocol checks, 15 GIPHY protocol checks and 3 sticker-installer checks passed.
- Native service-key checks cover validation, private store/read/delete and secret-free result messages. Actual Settings UI verified masked input, empty saved-key field, save, replacement, denied replacement retaining the previous selection, deletion and restart persistence. Profile XML contains only a non-secret identifier, never the test key. No real GIPHY provider key was used.
- Explicitly stop process-owned key and avatar loopers at shutdown. Repeated connected and idle app closure, including Settings/key operations, completed without the earlier Bad port ID crash. Installed 16/32/64 px icon and Deskbar MIME cache checks passed.
- Installed executable SHA-256: `9ce7ea1828e5e2e0594ae6562bff7ae118ac02d84c8ef3f5e9284cded5773c72`.
- HPKG SHA-256: `4fe43db169932ee6b997d11ec25110a860e575c757fcf5bd35d67f5e226e9111`.

Live Signal/GIPHY interoperability and native HaikuDepot retrieval were not exercised. Existing long-history UI latency remains. This is an early beta.

Recovery: keep both profiles and the original KeyStore entries. Beta 7 uses the old `jabber-for-beos` profile, not beta 8's `MimisSendir` profile. Do not overwrite either with a stale backup or assume a downgrade merges newer history. The new profile remains available when reverting the executable. GIPHY keys are in Haiku KeyStore and are not included in a profile-only backup.
