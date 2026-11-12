# Spritz Depot

Spritz Depot indexes author-hosted Haiku packages. Our primary download/source host is [GitHub Releases](https://github.com/kambuziarendal/MimisSendir/releases).

A pinned cichéto manifest is provided in `distribution/no.kambuzi.mimissendir.yaml` for the latest public beta, using an immutable HPKG URL and SHA-256. This file alone does not make the application appear in the public Spritz catalog. It can be included in a Git tap (bàcaro) or submitted to the authenticated house tap by an owner.

Beta 6 is published in the [Spritz Depot catalog](https://spritzdepot.desktoponfire.com/app/no.kambuzi.mimissendir). The public app page, catalog search (`Sendir`) and x86_64 resolver have been verified. The resolved HPKG URL and SHA-256 match the verified GitHub download. Installation through the native Spritz client has not yet been tested. GitHub downloads work independently of Spritz.

The house publisher exposes pinned packages through its `stable` install channel. This is a registry channel name, not a claim that MímisSendir is stable: the published version is explicitly beta 6. The repository manifest retains its `beta` channel for independent taps. The house publishing API expects flat form fields, not that manifest directly.

To regenerate, use `tools/release/make_spritz_manifest.py` with the final local HPKG, pinned release URL, public homepage and accurate license field. `POST /publish` validates a form; `POST /publications` publishes immediately and is not a preview endpoint.

Use the manifest on the current main branch. The initial beta tags preserve a pre-upload manifest with a `~` in the HPKG URL; GitHub normalized that character to `.` when creating the asset. Main uses the actual verified asset URL. The HPKG contents and package version are unchanged.
