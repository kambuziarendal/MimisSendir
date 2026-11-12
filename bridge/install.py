#!/usr/bin/env python3
"""Private, opt-in Signal component installation for an existing local Prosody.

No shell evaluation, production discovery, account registration or automatic
Prosody edits. All lifecycle actions are scoped to the recorded installation.
"""
import argparse
import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import secrets
import shutil
import socket
import subprocess
import tarfile
import tempfile
import time
import uuid

SCHEMA = 1
LABEL = 'no.kambuzi.mimissendir.installation'
SOURCE = Path(__file__).resolve().parent


def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def output(args):
    return subprocess.check_output(args, text=True).strip()


def write(path, text, mode=0o600, uid=None):
    # Exclusive creation protects against accidental replacement or symlink use.
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, mode)
    with os.fdopen(fd, 'w') as stream:
        stream.write(text)
        stream.flush()
        os.fsync(stream.fileno())
    if uid is not None:
        os.chown(path, uid, uid)


def domain(value):
    value = value.strip().lower()
    if len(value) > 253 or '.' not in value or not all(
        re.fullmatch(r'[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?', label)
        for label in value.split('.')
    ):
        raise ValueError('Use a plain ASCII/Punycode DNS name, without protocol or path.')
    return value


def account(value, host):
    if '@' not in value:
        value += '@' + host
    local, at, server = value.partition('@')
    if not re.fullmatch(r'[A-Za-z0-9_.+-]{1,64}', local) or server != host or not at:
        raise ValueError('Use one existing XMPP account on the selected domain.')
    return value


def source_digest(root):
    digest = hashlib.sha256()
    for path in sorted(root.rglob('*')):
        if path.is_symlink():
            raise ValueError('Installer source must not contain links.')
        if path.is_file():
            digest.update(path.relative_to(root).as_posix().encode() + b'\0')
            digest.update(hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()


def create(directory, host, user, component, upload, port):
    host, component, upload = map(domain, (host, component, upload))
    user = account(user, host)
    if len({host, component, upload}) != 3 or not 1024 <= port <= 65535:
        raise ValueError('Use distinct hostnames and a component port from 1024 to 65535.')
    directory = Path(directory).absolute()
    if directory.resolve() == SOURCE.resolve() or SOURCE.resolve() in directory.resolve().parents:
        raise ValueError('Installation must be outside the source checkout.')
    if directory.exists() or directory.is_symlink():
        raise ValueError('Destination exists. Nothing was changed; use a new directory.')
    directory.mkdir(mode=0o700)  # parent must already exist; no implicit system edits
    try:
        shutil.copytree(SOURCE, directory / 'source', ignore=shutil.ignore_patterns(
            '__pycache__', '*.pyc', '.env', 'state', 'backups'))
        token = secrets.token_urlsafe(48)
        identifier = uuid.uuid4().hex
        metadata = dict(schema=SCHEMA, installation=identifier, project='mimis-' + identifier[:12],
                        domain=host, user=user, component=component, upload=upload, port=port,
                        source_sha256=source_digest(directory / 'source'), image=None)
        write(directory / 'installation.json', json.dumps(metadata, indent=2) + '\n')
        (directory / 'state').mkdir(mode=0o700)
        os.chown(directory / 'state', 10000, 10000)
        config = (f'jid = {component}\nserver = 127.0.0.1\nport = {port}\nsecret = {token}\n'
                  f'home-dir = /var/lib/slidge\nuser-jid-validator = ^{re.escape(user)}$\n'
                  f'upload-service = {upload}\nupload-url-prefix = https://{upload}/\nquiet = true\n')
        write(directory / 'slidgnal.conf', config, uid=10000)
        # Separate files make the scope explicit. Do not merge blindly into a server.
        privileges = f'''-- Apply inside the existing VirtualHost "{host}".
-- Add "privilege" to that host's modules_enabled (do not replace its other modules).
-- Install source/install/prosody/mod_privilege.lua in Prosody's module path.
privileged_entities = {{
  ["{component}"] = {{
    roster = "both";
    message = "outgoing";
    iq = {{
      ["http://jabber.org/protocol/pubsub"] = {{ get = true; set = true }};
      ["http://jabber.org/protocol/pubsub#owner"] = {{ set = true }};
      ["urn:xmpp:http:upload:0"] = {{ get = true }};
    }};
  }};
}};
'''
        write(directory / 'prosody-virtualhost.lua', privileges)
        write(directory / 'prosody-component.lua', f'''-- Add as a separate component, not inside a VirtualHost.
-- Global component_interfaces must be {{ "127.0.0.1" }} and component_ports {{ {port} }}.
Component "{component}"
  component_secret = "{token}"
-- Allow this component on the existing HTTP upload service "{upload}".
-- Keep its existing HTTPS certificate, reverse proxy, quotas and retention.
''')
        write(directory / 'NEXT-STEPS.txt', f'''1. Back up the existing Prosody configuration.
2. Review the two prosody-*.lua fragments and install the included privilege module.
   Do not replace the server's existing hosts, TLS, modules, privileges or upload configuration.
3. Keep component TCP {port} on loopback. Check Prosody config/certificates, then reload it.
4. Ensure {user} exists and the HTTPS upload service {upload} works.
5. Run: sudo python3 source/install.py build {directory}
6. Run: sudo python3 source/install.py start {directory}
7. Run: sudo python3 source/install.py doctor {directory}
8. Register with {component} using Gajim or another supported XMPP client and scan
   the Signal linked-device QR on the phone. QR/device keys are private.
9. Configure MímisSendir with our existing XMPP account/server. Test both directions,
   media and restart before relying on it. The doctor checks XMPP, not Signal delivery.

Never run two copies of the linked-device state. Backups contain private device keys.
''')
    except BaseException:
        # Preserve incomplete output for inspection; never delete a caller's path.
        raise
    return metadata


def load(directory):
    directory = Path(directory).absolute()
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError('Installation must be a real directory.')
    path = directory / 'installation.json'
    if path.is_symlink():
        raise ValueError('Linked installation metadata is not accepted.')
    meta = json.loads(path.read_text())
    if meta.get('schema') != SCHEMA or not re.fullmatch(r'[0-9a-f]{32}', meta.get('installation', '')):
        raise ValueError('Unknown installation metadata.')
    if not re.fullmatch(r'mimis-[a-z0-9]{12}', meta.get('project', '')):
        raise ValueError('Invalid Compose project identity.')
    return directory, meta


@contextlib.contextmanager
def locked(directory):
    fd = os.open(directory / '.operator.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield
    except BlockingIOError:
        raise ValueError('Another operation owns this installation. Wait for it to finish.')
    finally:
        os.close(fd)


def save_meta(directory, meta):
    temporary = directory / ('.metadata-' + uuid.uuid4().hex)
    write(temporary, json.dumps(meta, indent=2) + '\n')
    temporary.replace(directory / 'installation.json')


def compose_file(directory, meta):
    image = meta.get('image')
    if not image or not re.fullmatch(r'sha256:[0-9a-f]{64}', image):
        raise ValueError('Build this installation first.')
    service = dict(image=image, network_mode='host', user='10000:10000', read_only=True,
                   cap_drop=['ALL'], security_opt=['no-new-privileges:true'],
                   tmpfs=['/tmp:rw,nosuid,size=256m'], restart='unless-stopped', stop_grace_period='30s',
                   command=['-c', '/etc/slidge/slidgnal.conf'],
                   volumes=['./slidgnal.conf:/etc/slidge/slidgnal.conf:ro', './state:/var/lib/slidge:rw'],
                   labels={LABEL: meta['installation']},
                   healthcheck={'test':['CMD','/venv/bin/python','/opt/mimissendir/health.py'],
                                'interval':'15s','timeout':'5s','retries':3,'start_period':'45s'},
                   logging={'driver':'local','options':{'max-size':'5m','max-file':'2'}})
    temporary = directory / ('.compose-' + uuid.uuid4().hex)
    write(temporary, json.dumps({'services': {'signal-bridge': service}}, indent=2) + '\n')
    temporary.replace(directory / 'compose.json')


def compose(directory, meta, *args, capture=False):
    cmd = ['docker','compose','--project-directory',str(directory),'-p',meta['project'],
           '-f',str(directory / 'compose.json'),*args]
    return output(cmd) if capture else run(cmd)


def containers(meta, running=False):
    args = ['docker','ps'] + ([] if running else ['-a'])
    return output(args + ['-q','--filter','label='+LABEL+'='+meta['installation']]).split()


def owned_running(meta):
    found = containers(meta, running=True)
    for cid in found:
        project = output(['docker','inspect','-f','{{index .Config.Labels "com.docker.compose.project"}}',cid])
        if project != meta['project']:
            raise ValueError('Another copy of this installation is running. Stop it before using restored state.')
    return found


def build(directory, meta):
    if containers(meta, running=True):
        raise ValueError('Stop this installation before rebuilding; keep a private backup first.')
    if source_digest(directory / 'source') != meta['source_sha256']:
        raise ValueError('Installed source changed. Prepare a separate reviewed installation instead.')
    tag = 'mimissendir-bridge:' + meta['source_sha256'][:16]
    run(['docker','build','--tag',tag,'-f',str(directory/'source/Dockerfile.combined'),str(directory/'source')])
    meta['image'] = output(['docker','image','inspect','-f','{{.Id}}',tag])
    save_meta(directory, meta)
    compose_file(directory, meta)
    compose(directory, meta, 'config', '--quiet')


def start(directory, meta):
    owned_running(meta)
    # Immutable local image ID, no tag refresh or unexpected image pull at startup.
    if output(['docker','image','inspect','-f','{{.Id}}',meta.get('image') or 'missing']) != meta['image']:
        raise ValueError('The verified local image is unavailable. Rebuild from retained source.')
    with socket.create_connection(('127.0.0.1',meta['port']),timeout=3):
        pass
    compose_file(directory, meta)
    compose(directory, meta, 'config', '--quiet')
    compose(directory, meta, 'up','-d','--no-build','--pull','never')


def doctor(directory, meta):
    ids = owned_running(meta)
    if len(ids) != 1:
        raise ValueError('No single running component. Run start and inspect private local logs if needed.')
    state = json.loads(output(['docker','inspect','-f','{{json .State}}',ids[0]]))
    image = output(['docker','inspect','-f','{{.Image}}',ids[0]])
    healthy = state.get('Health',{}).get('Status') == 'healthy'
    print('Recorded image: ' + ('matches' if image == meta['image'] else 'MISMATCH'))
    print('XMPP component: ' + ('authenticated, event loop alive' if healthy else 'starting or unhealthy'))
    print('Signal pairing/message delivery: verify separately with the phone and a test contact.')
    if image != meta['image'] or not healthy:
        raise ValueError('Health check not complete. Allow startup time, then check Prosody/authentication.')


def backup(directory, meta, target):
    target = Path(target).absolute()
    if target.exists() or target.is_symlink() or directory == target or directory in target.parents:
        raise ValueError('Backup must be a new file outside the installation directory.')
    was_running = bool(owned_running(meta))
    stopped = False
    temporary = None
    try:
        if was_running:
            compose(directory,meta,'stop','--timeout','30')
            if containers(meta,running=True):
                raise ValueError('Component did not stop; no backup was taken.')
            stopped = True
        for path in directory.rglob('*'):
            if path.is_symlink() or not (path.is_file() or path.is_dir()):
                raise ValueError('Backup contains a link or special file; nothing was archived.')
        fd, name = tempfile.mkstemp(prefix='.mimis-backup-', dir=target.parent)
        os.close(fd); temporary = Path(name)
        with tarfile.open(temporary,'w:gz') as archive:
            for item in sorted(directory.iterdir()):
                if item.name == '.operator.lock': continue
                archive.add(item, arcname=item.name, recursive=True)
        with temporary.open('rb') as stream: os.fsync(stream.fileno())
        os.link(temporary,target)  # No overwrite if another process created the destination.
        temporary.unlink(); temporary = None
        print('Private offline-consistent backup saved. It contains linked-device keys.')
    finally:
        if temporary is not None: temporary.unlink(missing_ok=True)
        if stopped:
            compose(directory,meta,'start')


def restore(archive_path, directory):
    directory = Path(directory).absolute()
    if directory.exists() or directory.is_symlink():
        raise ValueError('Restore requires a new destination. Existing profiles are never overwritten.')
    staging = Path(tempfile.mkdtemp(prefix='.mimis-restore-',dir=directory.parent))
    try:
        with tarfile.open(archive_path,'r:gz') as archive:
            total=0; count=0
            for member in archive:
                path=Path(member.name); count+=1; total+=member.size
                if path.is_absolute() or '..' in path.parts or not (member.isfile() or member.isdir()) or count>100000 or total>16*1024**3:
                    raise ValueError('Unsafe or oversized backup. No restored installation activated.')
                dest=staging/path
                if dest.exists() or dest.is_symlink():
                    if member.isdir() and dest.is_dir(): continue
                    raise ValueError('Duplicate backup path.')
                if member.isdir(): dest.mkdir(mode=0o700,parents=True,exist_ok=True)
                else:
                    dest.parent.mkdir(mode=0o700,parents=True,exist_ok=True)
                    with archive.extractfile(member) as source, dest.open('xb') as out:
                        shutil.copyfileobj(source,out)
                    # Source file readability/executable bits are needed by Docker COPY
                    # after rebuilding a restored snapshot. Never restore setuid
                    # or group/world write bits; private data stays owner-only.
                    dest.chmod((0o755 if member.mode & 0o111 else 0o644)
                               if path.parts[0] == 'source' else 0o600)
        _,meta=load(staging)
        if source_digest(staging/'source') != meta['source_sha256']:
            raise ValueError('Backup source digest mismatch.')
        # Retain linked-device identity but use a separate Compose project. start()
        # refuses to run while the original copy still runs on this Docker daemon.
        meta['project']='mimis-'+uuid.uuid4().hex[:12]
        save_meta(staging,meta)
        for path in [staging/'slidgnal.conf',staging/'state',*(staging/'state').rglob('*')]:
            os.chown(path,10000,10000)
        if meta['image']: compose_file(staging,meta)
        if directory.exists() or directory.is_symlink(): raise ValueError('Restore destination appeared.')
        staging.rename(directory)
        print('Restored offline. Never run the original and restored linked-device copies simultaneously.')
    except BaseException:
        shutil.rmtree(staging)
        raise


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    init=sub.add_parser('init',help='Guided new installation; does not change Prosody or start services')
    init.add_argument('directory')
    for name in ['build','start','stop','doctor']:
        sub.add_parser(name).add_argument('directory')
    p=sub.add_parser('backup');p.add_argument('directory');p.add_argument('archive')
    p=sub.add_parser('restore');p.add_argument('archive');p.add_argument('directory')
    args=parser.parse_args()
    if os.geteuid()!=0 or platform.system()!='Linux' or platform.machine() not in ('x86_64','amd64'):
        parser.error('Run with sudo on Linux amd64. The script owns only its new installation directory.')
    os.umask(0o077)
    try:
        if args.command=='init':
            host=domain(input('Existing XMPP domain: '))
            user=input('Existing XMPP account (name or name@domain): ').strip()
            component=input(f'Signal component [signal.{host}]: ').strip() or 'signal.'+host
            upload=input(f'Existing HTTPS upload host [upload.{host}]: ').strip() or 'upload.'+host
            port=int(input('Local Prosody component port [5347]: ').strip() or '5347')
            create(args.directory,host,user,component,upload,port)
            print('Private setup created. Read '+str(Path(args.directory)/'NEXT-STEPS.txt')+'. Nothing was started.')
        elif args.command=='restore': restore(args.archive,args.directory)
        else:
            directory,meta=load(args.directory)
            with locked(directory):
                if args.command=='build': build(directory,meta)
                elif args.command=='start': start(directory,meta)
                elif args.command=='doctor': doctor(directory,meta)
                elif args.command=='backup': backup(directory,meta,args.archive)
                else:
                    owned_running(meta)
                    compose(directory,meta,'stop','--timeout','30')
    except (ValueError,OSError,subprocess.CalledProcessError,tarfile.TarError) as error:
        # Do not display config files, subprocess output containing config, or secrets.
        parser.exit(1,'Operation stopped: '+str(error)+'\n')


if __name__=='__main__': main()
