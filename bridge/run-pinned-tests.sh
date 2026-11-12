#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
image=${BRIDGE_TEST_IMAGE:-sha256:eb83c2c1d2010d827120ea4527e0bfec256925ae3bb9800efae914d45d21ef6b}
# This is an isolated disposable container, NOT docker exec on production.
set -- --rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp:rw,size=64m -v "$repo/bridge:/tests:ro"
if [ -n "${BRIDGE_CANDIDATE:-}" ]; then
    for file in slidgnal/session.py slidgnal/group.py slidgnal/event_dispatch.py slidgnal/native_calls.py slidgnal/bounded_map.py slidgnal/contact.py slidgnal/mixins.py slidge/group/room.py; do
        test -f "$BRIDGE_CANDIDATE/$file"
        set -- "$@" -v "$BRIDGE_CANDIDATE/$file:/venv/lib/python3.13/site-packages/$file:ro"
    done
fi
exec sudo -n docker run "$@" --entrypoint /venv/bin/python "$image" /tests/tests/pinned_integration.py
