# Beta 13 verification

Native Haiku R1 beta6 x86_64, isolated overlay, synthetic XMPP accounts only.

- 344 native attachment/timeline checks passed, including viewport anchoring,
  hit targeting, resize and the beta 12 read-only caret redraw regressions.
- Actual conversation: scroll up, receive a message without snapping down,
  preserve an unsent draft, click the floating button and hide it at the bottom.
- Normal dark and compact light windows inspected. No composer slot remains.
- Installed final HPKG tested, not merely the build-tree application.
- 100 settled conversation frames identical with the floating control visible.
  Initial samples during history loading were not used as idle evidence.
- Final installed UI probe: 10 replies, zero failures, maximum 1219 microseconds.
- Beta 12 to beta 13 package upgrade preserved all 81 synthetic profile files.
  A later same-version candidate required replacement in the isolated VM;
  final installed binary was hash-checked against the final build.

Final binary SHA-256:
`59710639529dd952057111eabda100dbb19288f16ce608a414be5ed87aac376d`

Final HPKG SHA-256:
`e47822ac04486c73ce7869c90f681d47d0f5255afa97b1a0eea5158846c72707`

The rejected sibling-overlay prototype failed visual/hit-target checks and is
not shipped. Its failing test exited while native views still held references;
that test-process debugger report was retained in local evidence. The final
native test executable completed cleanly. No production bridge was changed.

Not tested: physical Haiku hardware or installation through the Spritz client.
Recovery: quit MímisSendir and reinstall the beta 12 HPKG. No profile format or
migration changed. Keep current profile data; do not restore stale history.
