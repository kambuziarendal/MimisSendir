"""Container health: event loop alive and XMPP component authenticated."""
import json
import os
from pathlib import Path
import sys
import time
try:
    value = json.loads(Path("/tmp/mimissendir-component-health.json").read_text())
    age = time.monotonic() - value["monotonic"]
    os.kill(value["pid"], 0)
    ok = value.get("component_authenticated") is True and 0 <= age < 35
except (OSError, ValueError, KeyError, TypeError):
    ok = False
print("XMPP component authenticated" if ok else "XMPP component not ready")
sys.exit(0 if ok else 1)
