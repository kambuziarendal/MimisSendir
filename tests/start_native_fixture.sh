#!/bin/sh
# Isolated synthetic XMPP/HTTPS fixture. Never reads production configuration.
set -eu
fixture=${1:?Usage: sh tests/start_native_fixture.sh /absolute/empty/fixture-directory}
case "$fixture" in /*) ;; *) echo 'Use an absolute path.' >&2; exit 1 ;; esac
if [ -e "$fixture" ]; then
    echo 'Fixture directory must not already exist.' >&2
    exit 1
fi
umask 077
mkdir -p "$fixture/data" "$fixture/certs"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$fixture/key.pem" \
    -out "$fixture/ca.pem" -days 2 -subj '/CN=fixture.test' \
    -addext 'subjectAltName=DNS:fixture.test,DNS:upload.fixture.test,DNS:signal.fixture.test,IP:10.0.2.2' \
    > "$fixture/certificate.log" 2>&1
python3 - "$fixture" <<'PY'
from pathlib import Path
import json
import sys
root = Path(sys.argv[1])
quote = lambda path: json.dumps(str(path))
config = f'''-- Synthetic, local-only test fixture. No production includes.
daemonize = false
pidfile = {quote(root / 'prosody.pid')}
data_path = {quote(root / 'data')}
interfaces = {{ "127.0.0.1" }}
c2s_ports = {{ 15422 }}
s2s_ports = {{ }}
http_ports = {{ }}
https_ports = {{ 15443 }}
https_interfaces = {{ "127.0.0.1" }}
http_interfaces = {{ "127.0.0.1" }}
unix_admin_socket_enabled = false
modules_enabled = {{ "tls"; "saslauth"; "roster"; "disco"; "ping"; "carbons"; "pep"; "bookmarks"; }}
modules_disabled = {{ "s2s"; "admin_shell"; "posix"; }}
authentication = "internal_hashed"
ssl = {{ key = {quote(root / 'key.pem')}; certificate = {quote(root / 'ca.pem')}; }}
log = {{ {{ levels = {{ min = "info" }}; to = "file"; filename = {quote(root / 'prosody.log')}; }} }}
VirtualHost "fixture.test"
Component "signal.fixture.test" "muc"
    restrict_room_creation = false
Component "upload.fixture.test" "http_file_share"
    http_external_url = "https://upload.fixture.test:15443/"
    http_file_share_size_limit = 52428800
    http_file_share_global_quota = 104857600
    http_file_share_access = {{ "fixture.test" }}
'''
(root / 'prosody.cfg.lua').write_text(config)
PY
# Both commands explicitly receive the new config and data path.
prosodyctl --config "$fixture/prosody.cfg.lua" register alice fixture.test fixture-only-alice
prosodyctl --config "$fixture/prosody.cfg.lua" register bob fixture.test fixture-only-bob
printf 'Fixture config: %s\n' "$fixture/prosody.cfg.lua"
exec prosody -F --config "$fixture/prosody.cfg.lua"
