"""Offline checks for a release metadata boundary, never a publication test."""
import argparse
import copy
import hashlib
from pathlib import Path
import tempfile
import unittest
from make_spritz_manifest import create_manifest, https_url


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.file = Path(self.tmp.name) / 'synthetic.hpkg'
        # Metadata helper checks magic only, not full package integrity.
        self.file.write_bytes(b'hpkg synthetic metadata test, not installable')
        self.args = argparse.Namespace(id='no.kambuzi.synthetic', name='Synthetic',
            version='0.0.1', license='MIT', hpkg=self.file,
            url='https://downloads.publisher.site/synthetic.hpkg',
            homepage='https://publisher.site/', bacaro='kambuzi-edb')

    def test_hash_and_credit(self):
        result = create_manifest(self.args)
        self.assertEqual(result['channels']['beta']['artifacts']['x86_64']['sha256'],
                         hashlib.sha256(self.file.read_bytes()).hexdigest())
        self.assertEqual(result['author']['name'], 'Kambuzi EDB')
        self.assertIn('OpenAI Codex through OpenClaw', result['description'])
        self.assertNotIn('bridge', result)

    def test_reject_path_and_placeholders(self):
        for field, value in [('id', '../escape'), ('id', 'no.kambuzi.CHOSEN_ID'),
                             ('name', 'CHOSEN_NAME'), ('version', 'FINAL_VERSION')]:
            with self.subTest(field=field, value=value):
                args = copy.copy(self.args)
                setattr(args, field, value)
                with self.assertRaises(ValueError):
                    create_manifest(args)

    def test_reject_non_package(self):
        self.file.write_bytes(b'PK not an hpkg')
        with self.assertRaises(ValueError):
            create_manifest(self.args)

    def test_reject_unsafe_or_example_urls(self):
        for url in ['http://publisher.site/a', 'https://user:pass@publisher.site/a',
                    'https://example.org/a', 'https://files.example.com/a',
                    'https://local.test/a', 'https://publisher.site/a#fragment']:
            with self.subTest(url=url), self.assertRaises(argparse.ArgumentTypeError):
                https_url(url)
        self.assertEqual(https_url(self.args.url), self.args.url)


if __name__ == '__main__':
    unittest.main()
