# Beta releases

## Beta 6

- New Mímir speaking-head icon in bronze and teal, with a speech-bubble beard.

- Display peer read receipts beside message timestamps, distinct from delivery.
- Persist exact-message read state across restart and archive reconciliation.
- Request read markers for text and attachments; retain foreground-visible dwell before acknowledging incoming content.
- Show distinct group readers without claiming all participants have read.
- Ignore unmatched, cross-conversation and own-device markers; keep reactions and drafts intact.
- Beta 5 is now available in Spritz Depot. No bridge deployment is required by this client change.

## Beta 5

Visible incoming-message read acknowledgements, jump-to-latest button and bridge-ready avatar refresh. Read acknowledgements require server/bridge support and an active window. Server-side action-queue and group-avatar persistence fixes are separate from the client and included as source patches in this repository.

## Beta 4

Edit sent text messages, preserve composer drafts when cancelling, and show reactions beside the timestamp. Correlated failures restore the prior reaction/message state.

## Beta 3

Native conversations sidebar, media/sticker support, replies, forwards, reactions and expanded emoji support, with preserved profile/upgrade identity.

## Public packaging

The public builds use neutral service defaults and contain no GIPHY API key. Their HPKG revisions distinguish them from private builds: beta3-6, beta4-5 and beta5-3. Configure your own server and optional bridge. These packages have not been tested against every XMPP server or on every Haiku revision.
