"""Fetch the pinned upstream Go toolchain, verify before extraction."""
import hashlib
from pathlib import Path
import urllib.request
import zipfile

URL = "https://proxy.golang.org/golang.org/toolchain/@v/v0.0.1-go1.26.10.linux-amd64.zip"
SHA256 = "f64ecb0807852be2c98899561c45288a9ad57f4e88f3cd002ffabeccbd03bfe1"
archive = Path("/tmp/go.zip")
with urllib.request.urlopen(URL, timeout=120) as response, archive.open("wb") as out:
    while chunk := response.read(1024 * 1024):
        out.write(chunk)
if hashlib.file_digest(archive.open("rb"), "sha256").hexdigest() != SHA256:
    raise SystemExit("Go toolchain checksum mismatch")
prefix = "golang.org/toolchain@v0.0.1-go1.26.10.linux-amd64/"
with zipfile.ZipFile(archive) as bundle:
    for entry in bundle.infolist():
        if not entry.filename.startswith(prefix):
            raise SystemExit("Unexpected toolchain archive path")
        relative = Path(entry.filename[len(prefix):])
        if relative.is_absolute() or ".." in relative.parts:
            raise SystemExit("Unsafe archive path")
        target = Path("/opt/go") / relative
        if entry.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(bundle.read(entry))
            target.chmod(0o755 if entry.external_attr >> 16 & 0o111 else 0o644)
archive.unlink()
