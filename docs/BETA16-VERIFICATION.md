# Beta 16: forwarding recipients and local deletion

Compiled source: `c1abe63` (the later acceptance commit changes release metadata
and this note only). Native Haiku R1 beta6 x86_64; synthetic accounts only.

- 89 native history checks and 9 forwarding-model checks pass. Deletion covers
  failed saves, aliases, correction/reaction/receipt callbacks, restart, account
  isolation and suppression of archived/live duplicates beyond the visible limit.
- Existing delivery correlation/reordered receipts and 171 conversation checks pass.
- Installed forwarding: favorites first, recent conversations next, searchable
  Other contacts including groups, readable labels, empty-search disabled action,
  Escape cancellation, keyboard group choice and single-click filtered selection.
  Switching picker modes preserves its frame. Original drafts remain unchanged.
- Fixed a search-field blur notification that cleared the clicked recipient and
  stale sidebar selection after opening an unlisted recipient.
- After the package roundtrip, the deleted message stays absent after login.
  Changing the source conversation while the picker is open preserves its body
  and recipient. Choosing the recipient sends nothing; an explicit Send delivers
  the exact forwarded body to the synthetic peer.
- Actual logout closes the account-bound picker. Native history account isolation
  passes; a separate UI account-switch attempt was inconclusive and is not counted.
- Installed Delete confirmation explains local-only scope. Cancel preserves the
  message byte for byte. Delete removes body/media from the local record and keeps
  an identifier tombstone. A repeated live message does not resurrect it.
- Dark/light native picker and confirmation inspected. 561 tracked source inputs
  match the guest source used to build the uninstrumented executable.
- Actual installed beta16 -> beta15 -> beta16 roundtrip: both executable identities
  match; all 100 profile files remain byte-identical, including deletion markers.
  The retained beta15 app launches. It does not understand the new tombstone flag,
  so rollback is data-preserving, not full backward feature compatibility.

Executable SHA-256:
`93c54f60edc49c7f73c1dfe3abc1df97139b02095b967f53c0cf90cc87ebf75d`.
HPKG SHA-256:
`84b44d96e27e98762e809b0b14f073bd3397d3423ee970d36a0ce5daf1cd77db`.

Recovery: quit, reinstall retained beta15 package, keep current history/settings.
Do not restore stale profiles. Delete is not server retraction or secure erasure;
other participants, existing quotations and downloaded files are unchanged.
Physical hardware and live Signal forwarding/deletion are not verified. The prior
send/scroll flicker investigation remains paused at the user's request.
