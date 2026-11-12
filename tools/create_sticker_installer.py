#!/usr/bin/env python3
"""Create a self-contained browser handoff page and native Renga pack-link files.
Input JSON: [{"title": "Pack title", "url": "sgnl://addstickers/?..."}].
Only public sticker pack links belong in distributable installers.
"""
import argparse
import base64
import html
import json
import re
from pathlib import Path
from urllib.parse import parse_qsl


def pack_file(url):
    prefixes = ('sgnl://addstickers/?', 'sgnl://addstickers?',
                'https://signal.art/addstickers/#', 'renga-stickers://addstickers/?')
    prefix = next((p for p in prefixes if url.startswith(p)), None)
    if not prefix or len(url) > 1024:
        raise ValueError('Unsupported Signal sticker link')
    query = url[len(prefix):]
    # No escaping or normalization of malformed fields.
    if '%' in query or '+' in query:
        raise ValueError('Use literal hexadecimal pack ID and key')
    pairs = parse_qsl(query, keep_blank_values=True, strict_parsing=True)
    fields = dict(pairs)
    if len(pairs) != 2 or set(fields) != {'pack_id', 'pack_key'}:
        raise ValueError('Exactly one pack ID and key required')
    if not re.fullmatch('[0-9a-fA-F]{32}', fields['pack_id']) or not re.fullmatch('[0-9a-fA-F]{64}', fields['pack_key']):
        raise ValueError('Invalid pack ID or key')
    pack_id = fields['pack_id'].lower()
    url = 'sgnl://addstickers/?pack_id=' + pack_id + '&pack_key=' + fields['pack_key']
    return pack_id, ('RENGA-STICKER-LINK 1\n' + url + '\n').encode('ascii')


def create(entries, output):
    output.mkdir(parents=True, exist_ok=True)
    sections = []
    for entry in entries:
        pack_id, data = pack_file(entry['url'])
        name = pack_id + '.renga-sticker'
        (output / name).write_bytes(data)
        uri = 'data:application/x-vnd.Kambuzi-RengaSticker;base64,' + base64.b64encode(data).decode('ascii')
        sections.append('<section><h2>' + html.escape(entry['title']) + '</h2><a class="download" href="' + uri
                        + '" download="' + name + '">Open in MímisSendir: download pack link</a></section>')
    page = '''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Signal stickers for MímisSendir</title>
<style>body{font:18px/1.5 sans-serif;margin:32px auto;padding:0 24px;max-width:850px;background:#202431;color:#fff}
a{display:inline-block;padding:14px 20px;background:#365a9f;color:#fff;border-radius:10px}section{margin:28px 0}h2{font-size:22px}code{overflow-wrap:anywhere}#next{border-left:4px solid #80bfff;padding:12px}small{color:#ddd}</style>
<h1>Signal stickers for MímisSendir</h1>
<p>Install MímisSendir 0.1.0-beta.1 or newer and start it once. Choose a pack below,
then open the downloaded <code>.renga-sticker</code> file from Firefox's Downloads list.</p>
<p>If Firefox asks for an application, select <strong>MímisSendir</strong>
(<code>/boot/system/apps/mimissendir</code>). You can also double-click the downloaded file in Tracker.</p>
<p>MímisSendir shows a preview. Choose Import to save a local copy. <strong>No message is sent.</strong>
Close the preview before opening the next pack.</p>
''' + '\n'.join(sections) + '''
<p id="next" role="status">If nothing opens: look in Firefox's Downloads list and open the saved pack link.
This page cannot detect whether MímisSendir has opened. It does not use custom browser protocols.</p>
<small>Download blocked? Right-click the button and choose Save Link As, keeping the .renga-sticker extension.
The same pack-link files are also included beside this page in the ZIP.</small>
</html>'''
    (output / 'INSTALL-IN-RENGA.html').write_text(page, encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    create(json.loads(args.input.read_text(encoding='utf-8')), args.output)
