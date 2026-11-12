#!/usr/bin/env python3
"""Export public signalstickers.org packs to local image folders. Never send messages.
Requires Python cryptography. Pack keys are read in memory from the public catalog.
"""
import argparse
import hashlib
import hmac
import json
import re
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from cryptography.hazmat.primitives import hashes, padding
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.kdf.hkdf import HKDF


def fetch(url, limit):
    request = urllib.request.Request(url, headers={'User-Agent': 'Renga-local-sticker-export/1'})
    with urllib.request.urlopen(request, timeout=30) as response:
        if response.url.split('/')[2] not in ('api.signalstickers.org', 'cdn-ca.signal.org'):
            raise ValueError('Unexpected download host')
        data = response.read(limit + 1)
        if len(data) > limit:
            raise ValueError('Download exceeds limit')
        return data


def decrypt(blob, key):
    if len(blob) < 64 or (len(blob) - 48) % 16:
        raise ValueError('Invalid encrypted payload')
    keys = HKDF(algorithm=hashes.SHA256(), length=64, salt=bytes(32), info=b'Sticker Pack').derive(bytes.fromhex(key))
    mac = hmac.new(keys[32:], blob[:-32], hashlib.sha256).digest()
    if not hmac.compare_digest(mac, blob[-32:]):
        raise ValueError('Sticker authentication failed')
    decoder = Cipher(algorithms.AES(keys[:32]), modes.CBC(blob[:16])).decryptor()
    plaintext = decoder.update(blob[16:-32]) + decoder.finalize()
    unpad = padding.PKCS7(128).unpadder()
    return unpad.update(plaintext) + unpad.finalize()


def fields(data):
    at = 0
    def varint():
        nonlocal at
        value = 0
        for shift in range(0, 70, 7):
            if at >= len(data):
                raise ValueError('Truncated protobuf')
            byte = data[at]; at += 1
            value |= (byte & 127) << shift
            if not byte & 128:
                return value
        raise ValueError('Invalid protobuf integer')
    while at < len(data):
        tag = varint(); number, wire = tag >> 3, tag & 7
        if not number:
            raise ValueError('Invalid protobuf tag')
        if wire == 0:
            value = varint()
        elif wire in (1, 2, 5):
            length = varint() if wire == 2 else (8 if wire == 1 else 4)
            if length > len(data) - at:
                raise ValueError('Truncated protobuf field')
            value = data[at:at + length]; at += length
        else:
            raise ValueError('Unsupported protobuf wire type')
        yield number, value


def export(entry, output):
    pack_id, key = entry['meta']['id'], entry['meta']['key']
    manifest = decrypt(fetch(f'https://cdn-ca.signal.org/stickers/{pack_id}/manifest.proto', 1024 * 1024), key)
    title, author, stickers = '', '', []
    for number, value in fields(manifest):
        if number in (1, 2):
            if not isinstance(value, bytes): raise ValueError('Invalid manifest string')
            if number == 1: title = value.decode('utf-8')
            else: author = value.decode('utf-8')
        elif number == 4:
            sticker = dict(fields(value))
            sticker_id = sticker.get(1, 0)
            if not isinstance(sticker_id, int) or not 0 <= sticker_id <= 65535: raise ValueError('Invalid sticker ID')
            stickers.append((sticker_id, sticker.get(2, b'').decode('utf-8')))
    if not stickers or len(stickers) > 200 or len(set(i for i, _ in stickers)) != len(stickers):
        raise ValueError('Invalid sticker count or duplicate IDs')
    directory = output / pack_id
    directory.mkdir(parents=True, exist_ok=False)
    def download(sticker):
        sticker_id, emoji = sticker
        data = decrypt(fetch(f'https://cdn-ca.signal.org/stickers/{pack_id}/full/{sticker_id}', 12 * 1024 * 1024), key)
        ext = '.webp' if data[:4] == b'RIFF' and data[8:12] == b'WEBP' else '.png' if data[:8] == b'\x89PNG\r\n\x1a\n' else '.gif' if data[:6] in (b'GIF87a', b'GIF89a') else None
        if not ext: raise ValueError('Unsupported sticker image')
        name = f'{pack_id[:8]}-{sticker_id:03d}{ext}'
        with (directory / name).open('xb') as file: file.write(data)
        return {'id': sticker_id, 'emoji': emoji, 'file': name, 'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}
    with ThreadPoolExecutor(max_workers=3) as pool:
        result = list(pool.map(download, stickers))
    record = {'title': title, 'author': author, 'source': f'https://signalstickers.org/pack/{pack_id}', 'stickers': result}
    (directory / 'PACK.json').write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'pack': pack_id, 'title': title, 'count': len(result), 'bytes': sum(i['bytes'] for i in result)}, ensure_ascii=False), flush=True)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('pack_ids', nargs='+')
    args = parser.parse_args()
    if any(not re.fullmatch('[a-f0-9]{32}', value) for value in args.pack_ids): parser.error('Use 32-character pack IDs')
    catalog = json.loads(fetch('https://api.signalstickers.org/v2/packs', 20 * 1024 * 1024))
    entries = {entry['meta']['id']: entry for entry in catalog}
    for pack_id in args.pack_ids:
        if pack_id not in entries: raise ValueError('Pack is not in the public catalog')
        export(entries[pack_id], args.output)


if __name__ == '__main__':
    main()
