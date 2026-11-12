"""Offline operator safeguards. Run as root in a disposable test directory."""
import importlib.util
import io
import json
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('installer',Path(__file__).parents[1]/'install.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class Installer(unittest.TestCase):
    def setUp(self):
        self.root=Path(tempfile.mkdtemp(prefix='mimis-installer-test-'))
        self.directory=self.root/'install'
    def tearDown(self):
        m.shutil.rmtree(self.root)
    def create(self):
        return m.create(self.directory,'chat.example.org','alice','signal.chat.example.org','upload.example.org',15347)
    def test_private_configuration_is_scoped_and_validated(self):
        meta=self.create()
        self.assertEqual(self.directory.stat().st_mode&0o777,0o700)
        config=self.directory/'slidgnal.conf'
        self.assertEqual(config.stat().st_mode&0o777,0o600)
        self.assertEqual(config.stat().st_uid,10000)
        secret=[x for x in config.read_text().splitlines() if x.startswith('secret = ')][0][9:]
        self.assertGreaterEqual(len(secret),48)
        self.assertNotIn(secret,(self.directory/'installation.json').read_text())
        self.assertNotIn(secret,(self.directory/'NEXT-STEPS.txt').read_text())
        self.assertIn('^alice@chat\\.example\\.org$',config.read_text())
        self.assertEqual(m.source_digest(self.directory/'source'),meta['source_sha256'])
        before=config.read_bytes()
        with self.assertRaises(ValueError): self.create()
        self.assertEqual(config.read_bytes(),before)
    def test_injection_and_recursive_destination_are_refused(self):
        for value in ['x.example\nsecret=x','https://chat.example.org','bad".example.org','*.example.org','-x.example.org']:
            with self.assertRaises(ValueError):m.domain(value)
        for value in ['alice@other.example','alice\nsecret=x','alice/evil','alice@chat.example.org:5222']:
            with self.assertRaises(ValueError):m.account(value,'chat.example.org')
        with self.assertRaises(ValueError):m.create(m.SOURCE/'should-not-exist','chat.example.org','alice','signal.example.org','upload.example.org',15347)
        self.assertFalse((m.SOURCE/'should-not-exist').exists())
    def test_offline_backup_restore_preserves_bytes_and_is_independent(self):
        meta=self.create();state=self.directory/'state/device.sqlite';state.write_bytes(b'synthetic device state')
        archive=self.root/'private-backup.tar.gz'
        with patch.object(m,'owned_running',return_value=[]):m.backup(self.directory,meta,archive)
        self.assertEqual(archive.stat().st_mode&0o777,0o600)
        target=self.root/'restored';m.restore(archive,target)
        restored=json.loads((target/'installation.json').read_text())
        self.assertEqual(restored['installation'],meta['installation'])
        self.assertNotEqual(restored['project'],meta['project'])
        self.assertEqual((target/'state/device.sqlite').read_bytes(),state.read_bytes())
        self.assertEqual((target/'source/install/launch.py').stat().st_mode&0o777,0o644)
        (target/'state/device.sqlite').write_bytes(b'changed clone')
        self.assertEqual(state.read_bytes(),b'synthetic device state')
        with self.assertRaises(ValueError):m.restore(archive,self.directory)
    def test_archive_escape_and_links_are_refused(self):
        for name,kind in [('../escape',tarfile.REGTYPE),('/escape',tarfile.REGTYPE),('link',tarfile.SYMTYPE),('device',tarfile.FIFOTYPE)]:
            archive=self.root/'bad.tar.gz'
            with tarfile.open(archive,'w:gz') as out:
                item=tarfile.TarInfo(name);item.type=kind;item.linkname='/etc';out.addfile(item,io.BytesIO())
            with self.assertRaises(ValueError):m.restore(archive,self.root/'bad')
            self.assertFalse((self.root/'bad').exists())
    def test_backup_failure_resumes_previously_running_component(self):
        meta=self.create();calls=[]
        with patch.object(m,'owned_running',return_value=['owned']),patch.object(m,'containers',return_value=[]),patch.object(m,'compose',side_effect=lambda *a,**kw:calls.append(a[2:])),patch.object(m.tarfile,'open',side_effect=OSError('test write failure')):
            with self.assertRaises(OSError):m.backup(self.directory,meta,self.root/'fail.tar.gz')
        self.assertEqual(calls,[('stop','--timeout','30'),('start',)])
        self.assertFalse((self.root/'fail.tar.gz').exists())
    def test_restored_copy_cannot_start_alongside_original(self):
        meta=self.create()
        with patch.object(m,'containers',return_value=['original']),patch.object(m,'output',return_value='other-project'):
            with self.assertRaises(ValueError):m.owned_running(meta)
    def test_lock_prevents_concurrent_state_operations(self):
        self.create()
        with m.locked(self.directory):
            with self.assertRaises(ValueError):
                with m.locked(self.directory):pass

if __name__=='__main__':unittest.main()
