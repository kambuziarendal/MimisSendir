"""Browser download payload and strict generator input regression tests."""
import base64
import re
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from create_sticker_installer import create, pack_file


class InstallerTest(unittest.TestCase):
    url = 'sgnl://addstickers/?pack_id=' + 'a' * 32 + '&pack_key=' + '0' * 64

    def test_download_and_adjacent_file_are_identical(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            create([{'title': '<script>alert(1)</script>', 'url': self.url}], root)
            page = (root / 'INSTALL-IN-RENGA.html').read_text()
            data = base64.b64decode(re.search(r';base64,([^"]+)', page).group(1))
            self.assertEqual(data, (root / ('a' * 32 + '.renga-sticker')).read_bytes())
            self.assertEqual(data, ('RENGA-STICKER-LINK 1\n' + self.url + '\n').encode())
            self.assertNotIn('<script>', page)
            self.assertIn('&lt;script&gt;', page)
            self.assertNotIn('href="renga-stickers:', page)

    def test_untrusted_fields_and_hosts_rejected(self):
        for url in [self.url + '&command=run', self.url + '&pack_id=' + 'a' * 32,
                    self.url[:-1], self.url.replace('sgnl:', 'file:'),
                    self.url.replace('sgnl://addstickers/?', 'https://evil.test/?'),
                    self.url.replace('pack_id=a', 'pack_id=%61'), self.url + '\n']:
            with self.subTest(url=url), self.assertRaises(ValueError):
                pack_file(url)

    def test_alternate_url_and_uppercase_normalized(self):
        url = self.url.replace('sgnl://addstickers/?', 'https://signal.art/addstickers/#').replace('a' * 32, 'A' * 32)
        pack_id, data = pack_file(url)
        self.assertEqual(pack_id, 'a' * 32)
        self.assertEqual(data, ('RENGA-STICKER-LINK 1\n' + self.url + '\n').encode())


if __name__ == '__main__':
    unittest.main()
