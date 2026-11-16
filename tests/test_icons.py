import base64,socket,subprocess,tempfile,time,unittest
from pathlib import Path
from urllib.request import urlopen
from urllib.error import HTTPError
from fixtures import create,ucp
ROOT=Path(__file__).resolve().parents[1]
PNG=base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+jRZkAAAAASUVORK5CYII=')
class IconTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);create(self.root)
        with socket.socket() as s:s.bind(('127.0.0.1',0));self.port=s.getsockname()[1]
        self.proc=subprocess.Popen([str(ROOT/'build/trophy-server'),str(self.root),str(self.port)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        for _ in range(100):
            try:
                with urlopen(self.url('/'),timeout=.2):break
            except OSError:time.sleep(.02)
        else:self.fail('server did not start')
    def tearDown(self):self.proc.terminate();self.proc.wait(timeout=5);self.tmp.cleanup()
    def url(self,path):return f'http://127.0.0.1:{self.port}{path}'
    def write(self,path,data):
        p=self.root/path.lstrip('/');p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    def archive(self,entries):self.write('/user/trophy2/nobackup/conf/NPWR00001_00/TROPHY.UCP',ucp(entries))
    def bind(self,root='system_data/priv/appmeta',title='PPSA00001',setid='NPWR00001_00'):
        self.write(f'{root}/{title}/'+('sce_sys/' if root=='user/app' else '')+'trophy2/npbind.dat',b'header\0'+setid.encode()+b'\0')
    def image(self):
        with urlopen(self.url('/api/set/10000000/NPWR00001_00/icon'),timeout=5) as r:
            self.assertEqual(r.headers['Content-Type'],'image/png');self.assertEqual(r.read(),PNG)
    def test_exact_archive_icon(self):self.archive({'icon0.png':PNG});self.image()
    def test_preferred_localized_icon(self):self.archive({'icon0_zh-Hans.png':PNG,'icon0_en-US.png':b'invalid'});self.image()
    def test_other_language_icon(self):self.archive({'icon0_fr-FR.png':PNG});self.image()
    def test_invalid_icon_does_not_hide_valid_variant(self):self.archive({'icon0.png':b'invalid','icon0_ja-JP.png':PNG});self.image()
    def test_appmeta_icon_via_exact_npwr_binding(self):
        self.bind();self.write('/system_data/priv/appmeta/PPSA00001/icon0.png',PNG);self.image()
    def test_installed_game_icon(self):
        self.bind('user/app');self.write('/user/app/PPSA00001/sce_sys/icon0.png',PNG);self.image()
    def test_unrelated_title_is_never_used(self):
        self.bind(setid='NPWR00002_00');self.write('/system_data/priv/appmeta/PPSA00001/icon0.png',PNG)
        with self.assertRaises(HTTPError) as c:self.image()
        self.assertEqual(c.exception.code,404)
    def test_missing_icons_remain_missing(self):
        with self.assertRaises(HTTPError) as c:self.image()
        self.assertEqual(c.exception.code,404)
    def test_multiple_editions_share_set(self):
        self.bind(title='PPSA00001');self.bind(title='PPSA00002');self.write('/system_data/priv/appmeta/PPSA00002/icon0.png',PNG);self.image()
    def test_false_prefix_is_not_an_icon(self):
        self.archive({'icon01.png':PNG,'icon0_not-image.txt':PNG})
        with self.assertRaises(HTTPError) as c:self.image()
        self.assertEqual(c.exception.code,404)
