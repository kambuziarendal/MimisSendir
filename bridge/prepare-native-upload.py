#!/usr/bin/env python3
"""Stage the pinned native upload fix. Never opens production config or state."""
import argparse, hashlib, shutil
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('destination',type=Path);a=p.parse_args()
relative='vendor/go.mau.fi/mautrix-signal/pkg/signalmeow/attachments.go'
original=a.source/relative
if hashlib.sha256(original.read_bytes()).hexdigest()!='1d59ea4cb88018dcaaae33890770e2f1c3a1b8202c4f753146ae9f76471f8419':raise SystemExit('Pinned native source hash mismatch')
if a.destination.exists():raise SystemExit('Refusing to overwrite a staged build')
shutil.copytree(a.source,a.destination)
path=a.destination/relative
text=path.read_text()
old='attributesPath := "/v4/attachments/form/upload"'
assert text.count(old)==1
text=text.replace(old,'attributesPath := attachmentUploadPath(encryptedWithMAC)')
text+='\n// Signal requires the encrypted, padded payload length, including the MAC.\nfunc attachmentUploadPath(payload []byte) string {\n return fmt.Sprintf("/v4/attachments/form/upload?uploadLength=%d", len(payload))\n}\n'
path.write_text(text)
shutil.copy2(Path(__file__).parent/'tests/attachment_upload_test.go',path.parent/'attachment_upload_test.go')
print('Pinned native upload source staged. No production action performed.')
