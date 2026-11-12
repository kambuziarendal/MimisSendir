"""Pinned Slidgnal launcher with a secret-free component heartbeat.

Authenticated XMPP transport only, not proof of a paired Signal account.
"""
import json
import os
import time
from pathlib import Path
from slidgnal import Gateway, main

HEARTBEAT = Path("/tmp/mimissendir-component-health.json")
original_init = Gateway.__init__


def init(self):
    original_init(self)
    authenticated = False

    def connected(_):
        nonlocal authenticated
        authenticated = True

    def disconnected(_):
        nonlocal authenticated
        authenticated = False
        HEARTBEAT.unlink(missing_ok=True)

    def heartbeat():
        try:
            temporary = HEARTBEAT.with_suffix(".new")
            temporary.write_text(json.dumps({"pid": os.getpid(), "monotonic": time.monotonic(),
                                             "component_authenticated": authenticated}))
            temporary.chmod(0o600)
            temporary.replace(HEARTBEAT)
        finally:
            self.loop.call_later(10, heartbeat)

    self.add_event_handler("session_start", connected)
    self.add_event_handler("disconnected", disconnected)
    self.loop.call_soon(heartbeat)


Gateway.__init__ = init
if __name__ == "__main__":
    try:
        main()
    finally:
        HEARTBEAT.unlink(missing_ok=True)
