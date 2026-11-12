# Beta 6 verification

Native Haiku x86_64, isolated QEMU overlay; synthetic XMPP identities only.

- 72 native history checks, 66 protocol checks and 149 attachment/layout checks passed, plus delivery-state checks.
- Installed client requests displayed markers on sent text. A delivery receipt alone leaves the message without Read; a matching displayed marker sets Read on that message only.
- Unknown and incoming-message targets are rejected. Group receipts count reporting occupants without claiming everyone read. Native history tests cover room-assigned aliases, account separation, duplicate receipts and restart.
- Installed direct and group read state survived an upgrade/restart. Existing reaction badges remain beside the timestamp. Hidden conversations did not acknowledge incoming content; visible foreground content did.
- Compact 680x600 and normal 1000x700 windows were inspected. The Mimir icon is generated from the same geometry as SVG and native HVIF; no runtime image converter.
- Live Signal transport, real-phone privacy settings and server MAM replay were not exercised in this fixture. Archived marker handling is implemented; local history persistence was tested. The current Signal bridge is unchanged.

Upgrade uses the same application identity and additive history metadata. Previous packages remain available. No history migration, deletion or automatic message resend is performed. Reinstall a retained beta5 package to roll back the client; preserve settings/history and bridge state.
