#!/usr/bin/env python3
"""Drive the native typing probe through the local synthetic XMPP fixture.

Run with the directory from tests/start_native_fixture.sh and keep stdin open.
Open the Bob chat, optionally send a few synthetic outgoing messages, and use
"seed" to load history. Once the chat drains, start RengaUiProbe --typing in
Haiku, and issue "burst" just before or when the probe prints READY. Use a
fresh synthetic profile and the same script arguments for each build.
"""

import argparse
import queue
import socket
import threading
import time
import uuid
from pathlib import Path
from xml.sax.saxutils import escape, quoteattr

from stability_fixture import login


ALICE = "alice@fixture.test"
CLIENT = "{jabber:client}"


def send_batch(stream, phase, count, spacing, pending):
    nonce = uuid.uuid4().hex[:12]
    receipts = list(pending)
    pending.clear()
    started = time.monotonic()
    print(f"START {phase} rows={count} receipts={len(receipts)}", flush=True)
    for index in range(count):
        delay = started + index * spacing - time.monotonic()
        if delay > 0:
            time.sleep(delay)
        identifier = quoteattr(f"typing-{phase}-{nonce}-{index}")
        body = escape(
            f"Synthetic {phase} row {index:04d}. "
            "A deliberately wrapped message with several words for native layout. "
            "The draft remains independent of this conversation update."
        )
        stream.send(
            f'<message type="chat" to="{ALICE}" id={identifier}>'
            f"<body>{body}</body></message>"
        )
        # Spread delayed receipts throughout the burst rather than front-loading.
        previous = index * len(receipts) // count
        sent = (index + 1) * len(receipts) // count
        for target, outgoing_id in receipts[previous:sent]:
            stream.send(
                '<message type="chat" to=' + quoteattr(target) + '>'
                '<received xmlns="urn:xmpp:receipts" id=' + quoteattr(outgoing_id)
                + '/></message>'
            )
    print(f"DONE {phase} elapsed_s={time.monotonic() - started:.3f}", flush=True)


def enqueue_commands(commands):
    try:
        while True:
            commands.put(input().strip())
    except EOFError:
        commands.put("quit")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--history", type=int, default=400)
    parser.add_argument("--burst", type=int, default=240)
    parser.add_argument("--burst-interval-ms", type=int, default=16)
    args = parser.parse_args()
    if min(args.history, args.burst, args.burst_interval_ms) < 1:
        parser.error("counts and interval must be positive")

    commands = queue.Queue()
    threading.Thread(target=enqueue_commands, args=(commands,), daemon=True).start()
    stream = login("bob", args.fixture / "ca.pem", resource="typing-fixture")
    pending = {}
    try:
        stream.send("<presence/>")
        stream.sock.settimeout(0.05)
        print("READY Bob; commands: seed, burst, pending, quit", flush=True)
        while True:
            try:
                command = commands.get_nowait()
            except queue.Empty:
                command = None
            if command == "quit":
                return
            if command == "seed":
                send_batch(stream, command, args.history, 0.005, {})
            elif command == "burst":
                send_batch(stream, command, args.burst, args.burst_interval_ms / 1000, pending)
            elif command == "pending":
                print(f"PENDING receipts={len(pending)}", flush=True)
            elif command:
                print(f"Unknown command: {command}", flush=True)
            try:
                stanza = stream.next()
            except socket.timeout:
                continue
            target = stanza.get("from", "")
            identifier = stanza.get("id", "")
            if stanza.tag == CLIENT + "iq" and stanza.get("type") == "get":
                stream.send(
                    "<iq type='result' to=" + quoteattr(target)
                    + " id=" + quoteattr(identifier) + "/>"
                )
            if (
                stanza.tag == CLIENT + "message"
                and target.startswith(ALICE + "/")
                and identifier
                and stanza.findtext(CLIENT + "body")
            ):
                pending[(target, identifier)] = None
                print(f"PENDING receipts={len(pending)}", flush=True)
    finally:
        stream.sock.close()


if __name__ == "__main__":
    main()
