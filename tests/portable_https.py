#!/usr/bin/env python3
"""Exercise the actual libcurl transport against a local certificate-checked server."""
import http.server
import os
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import threading

binary = str(Path(sys.argv[1]).resolve())
seen = []

class Server(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def handle(self):
        try:
            super().handle()
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass  # Expected when a client cancels or rejects a TLS connection.

    def do_PUT(self):
        size = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(size)
        seen.append((self.path, body, self.headers.get("Authorization")))
        status = {"/ok": 201, "/reject": 413, "/redirect": 307, "/not-created": 200,
                  "/large-response": 201}.get(self.path, 500)
        self.send_response(status)
        if self.path == "/redirect":
            self.send_header("Location", "/should-not-follow")
        payload = b"x" * 20000 if self.path == "/large-response" else b""
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        try:
            self.wfile.write(payload)
        except (BrokenPipeError, ssl.SSLError):
            pass

    def do_GET(self):
        seen.append((self.path, b"GET", self.headers.get("Authorization")))
        content = bytes(range(256)) * 4096
        if self.path == "/get-limit":
            content = b"z" * (50 * 1024 * 1024)
        if self.path == "/get-over-stream":
            content = b"z" * (50 * 1024 * 1024 + 1)
        self.send_response(404 if self.path == "/get-missing" else
                           307 if self.path == "/get-redirect" else 200)
        if self.path == "/get-redirect":
            self.send_header("Location", "/should-not-follow")
        self.send_header("Content-Type", "application/octet-stream")
        if self.path != "/get-over-stream":
            size = len(content) + 1 if self.path == "/get-short" else len(content)
            if self.path == "/get-over-header":
                size = 50 * 1024 * 1024 + 1
            self.send_header("Content-Length", str(size))
        self.end_headers()
        self.wfile.write(content)

with tempfile.TemporaryDirectory(prefix="renga-https-test-") as temp:
    root = Path(temp)
    cert, key = root / "cert.pem", root / "key.pem"
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                    "-keyout", str(key), "-out", str(cert), "-days", "1",
                    "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost"],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Server)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert, key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    port = server.server_port
    payload = root / "synthetic.bin"
    payload.write_bytes(bytes(range(256)) * 4096)
    checks = 0

    def run(path="/ok", mode="normal", expected=True, ca=cert, size=None, host="localhost"):
        global checks
        result = subprocess.run([binary, f"{host}:{port}", path, str(payload),
                                 str(payload.stat().st_size if size is None else size), str(ca), mode],
                                text=True, capture_output=True, timeout=20)
        assert result.returncode == (0 if expected else 1), result.stdout + result.stderr
        assert "synthetic" not in result.stdout, "credentials leaked into output"
        checks += 1
        return result.stdout

    try:
        run()
        assert seen[-1] == ("/ok", payload.read_bytes(), "Bearer synthetic")
        run("/reject", expected=False)
        count = len(seen)
        run("/redirect", expected=False)
        assert len(seen) == count + 1 and seen[-1][0] == "/redirect"
        run("/not-created", expected=False)
        run("/large-response", expected=False)
        count = len(seen)
        run(ca="system", expected=False)  # untrusted CA must fail
        run(host="127.0.0.1", expected=False)  # certificate hostname must match
        run(mode="cancel", expected=False)
        run(mode="cancel-progress", expected=False)
        run(size=50 * 1024 * 1024 + 1, expected=False)
        run(size=payload.stat().st_size - 1, expected=False)
        assert len(seen) == count, "rejected operations sent a PUT"
        payload.write_bytes(b"")
        run()
        assert seen[-1][1] == b""
        print(f"{checks} HTTPS integration cases passed")
        upload_checks = checks
        for path, expected in [("/get", True), ("/get-missing", False),
                               ("/get-redirect", False), ("/get-short", False),
                               ("/get-over-header", False), ("/get-over-stream", False),
                               ("/get-limit", True)]:
            payload.write_bytes(b"")
            run(path, mode="get", expected=expected)
            if not expected:
                assert payload.stat().st_size == 0, "failed download left partial data"
            elif path == "/get-limit":
                assert payload.stat().st_size == 50 * 1024 * 1024
            else:
                assert payload.read_bytes() == bytes(range(256)) * 4096
            assert seen[-1][2] is None, "download leaked upload credentials"
        for mode in ["get-cancel", "get-cancel-progress"]:
            payload.write_bytes(b"")
            run("/get", mode=mode, expected=False)
            assert payload.stat().st_size == 0
        payload.write_bytes(b"keep existing contents")
        run("/get", mode="get", expected=False)
        assert payload.read_bytes() == b"keep existing contents", "existing file overwritten"
        payload.write_bytes(b"")
        run("/get", mode="get", ca="system", expected=False)
        run("/get", mode="get", host="127.0.0.1", expected=False)
        assert not any(p == "/should-not-follow" for p, _, _ in seen)
        print(f"{checks - upload_checks} HTTPS download cases passed")
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
