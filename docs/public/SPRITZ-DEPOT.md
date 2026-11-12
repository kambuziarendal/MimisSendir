# Spritz Depot

Spritz Depot indexes author-hosted Haiku packages. Our primary download/source host is [GitHub Releases](https://github.com/kambuziarendal/MimisSendir/releases).

A pinned cichéto manifest is provided in `distribution/no.kambuzi.mimissendir.yaml` for the latest public beta, using an immutable HPKG URL and SHA-256. This file alone does not make the application appear in the public Spritz catalog. It can be included in a Git tap (bàcaro) or submitted to the authenticated house tap by an owner.

The manifest is validated against the [SpritzDepot source schema](https://github.com/atomozero/SpritzDepot/blob/main/app/schemas.py). No account has been created and no public catalog submission has been made. The production catalog URL/account and an actual Spritz install still need confirmation. GitHub downloads work independently of Spritz.

To regenerate, use `tools/release/make_spritz_manifest.py` with the final local HPKG, pinned release URL, public homepage and accurate license field. `POST /publish` validates a form; `POST /publications` publishes immediately and is not a preview endpoint.
