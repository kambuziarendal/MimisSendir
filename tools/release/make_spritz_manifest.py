#!/usr/bin/env python3
"""Prepare a pinned Spritz manifest locally. Does not upload or publish anything."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import urlparse


def https_url(value):
    u = urlparse(value)
    if u.scheme != 'https' or not u.hostname or u.username or u.password or u.fragment:
        raise argparse.ArgumentTypeError('Use a credential-free HTTPS URL without a fragment')
    host = u.hostname.lower()
    if host == 'localhost' or host.endswith(('.invalid', '.test', '.example', '.localhost')) or host in ('example.org', 'example.com', 'example.net') or any(host.endswith('.' + d) for d in ('example.org', 'example.com', 'example.net')):
        raise argparse.ArgumentTypeError('Replace the example/development host with a reviewed release host')
    return value


def create_manifest(args):
    if not re.fullmatch(r'[a-z0-9]+(?:[._-][a-z0-9]+)*\.[a-z0-9]+(?:[._-][a-z0-9]+)*', args.id):
        raise ValueError('id must be a lowercase reverse-domain identifier')
    for name in ('name', 'version', 'license'):
        value = getattr(args, name)
        if not value.strip() or re.search(r'CHOSEN|FINAL_|REVIEWED_|PENDING|TODO|REPLACE', value):
            raise ValueError('Resolve placeholder metadata: ' + name)
    if args.hpkg.suffix != '.hpkg' or not args.hpkg.is_file():
        raise ValueError('A local HPKG is required, not an installer ZIP')
    with args.hpkg.open('rb') as f:
        if f.read(4) != b'hpkg':
            raise ValueError('File does not have the HPKG magic')
        f.seek(0)
        digest = hashlib.file_digest(f, 'sha256').hexdigest()
    return {
        'cicheto': 1, 'id': args.id, 'name': args.name,
        'summary': 'Native Haiku XMPP messaging with optional self-hosted Signal bridging.',
        'description': 'Developed by Kambuzi EDB with AI-assisted programming using OpenAI Codex through OpenClaw. '
                       'Based on Renga and its original contributors. '
                       'A separate bridge and existing Signal account are required for Signal access. '
                       'No hosted service is included. Signal encryption terminates at the bridge.',
        'homepage': args.homepage, 'license': args.license,
        'author': {'name': 'Kambuzi EDB'},
        'packager': {'name': 'Kambuzi EDB', 'bacaro': args.bacaro},
        'channels': {getattr(args, 'channel', 'beta'): {'version': args.version, 'kind': 'hpkg',
                     'artifacts': {'x86_64': {'url': args.url, 'sha256': digest}}}},
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('id', 'name', 'version', 'license'):
        p.add_argument('--' + name, required=True)
    p.add_argument('--channel', default='beta', choices=('beta', 'stable'), help='Use beta for prerelease packages')
    p.add_argument('--hpkg', type=Path, required=True)
    p.add_argument('--url', type=https_url, required=True)
    p.add_argument('--homepage', type=https_url, required=True)
    p.add_argument('--bacaro', default='kambuzi-edb', help='Proposed own tap slug; use spritz for house tap')
    args = p.parse_args()
    try:
        result = create_manifest(args)
    except (ValueError, OSError) as error:
        p.error(str(error))
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
