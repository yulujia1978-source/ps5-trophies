import json, socket, struct, subprocess, tempfile, time, unittest
from pathlib import Path
from urllib.request import urlopen, Request
from urllib.error import HTTPError
from fixtures import create, trp, ucp

ROOT=Path(__file__).resolve().parents[1]
class ServerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory();cls.root=Path(cls.tmp.name);create(cls.root)
        with socket.socket() as s:s.bind(('127.0.0.1',0));cls.port=s.getsockname()[1]
        cls.proc=subprocess.Popen([str(ROOT/'build/trophy-server'),str(cls.root),str(cls.port)],stdout=subprocess.DEVNULL)
        for _ in range(100):
            try:
                with urlopen(cls.url('/'),timeout=.2) as r:
                    if r.status==200:break
            except OSError:time.sleep(.02)
        else:raise RuntimeError('server failed to start')
    @classmethod
    def tearDownClass(cls):cls.proc.terminate();cls.proc.wait(timeout=5);cls.tmp.cleanup()
    @classmethod
    def url(cls,path):return f'http://127.0.0.1:{cls.port}{path}'
    def get(self,path):
        with urlopen(self.url(path),timeout=5) as r:return r.read(),r.headers
    def js(self,path):return json.loads(self.get(path)[0])
    def path(self,name='NPWR00001_00'):return self.root/f'user/home/10000000/trophy2/nobackup/data/{name}/TRPTITLE.DAT'
    def test_static_and_security_headers(self):
        for p in ['/','/?demo=1','/app.js','/model.js','/style.css']:
            data,h=self.get(p);self.assertGreater(len(data),100);self.assertIn("default-src 'self'",h['Content-Security-Policy']);self.assertEqual(h['X-Content-Type-Options'],'nosniff')
    def test_catalog_separates_users_and_unknown(self):
        j=self.js('/api/catalog');users={u['id']:u for u in j['users']};self.assertEqual(set(users),{'10000000','20000000'})
        a={s['id']:s for s in users['10000000']['sets']};self.assertEqual(a['NPWR00001_00']['earned'],1);self.assertEqual(a['NPWR00002_00']['earned'],0);self.assertFalse(a['NPWR00003_00']['readable'])
        self.assertEqual(users['20000000']['sets'][0]['earned'],0)
    def test_metadata_and_definition(self):
        base='/api/set/10000000/NPWR00001_00/'
        self.assertEqual(self.js(base+'meta')['metadata']['titleMetadata']['name'],'本地测试游戏');self.assertEqual(len(self.js(base+'conf')['trophies']),2)
    def test_state_flags_and_missing_rows(self):
        p=self.path();original=p.read_bytes()
        try:
            p.write_bytes(trp(((1,0x11,63900000000000000),)))
            j=self.js('/api/set/10000000/NPWR00001_00/state');self.assertEqual(j['earned'],1);self.assertEqual(j['unknown'],1);self.assertIsNone(j['rows'][0]['unlocked']);self.assertTrue(j['rows'][1]['unlocked'])
        finally:p.write_bytes(original)
    def test_conflicting_rows_are_unknown(self):
        p=self.path();original=p.read_bytes()
        try:
            p.write_bytes(trp(((1,1,63900000000000000),(1,0,0))))
            j=self.js('/api/set/10000000/NPWR00001_00/state');self.assertEqual(j['earned'],0);self.assertEqual(j['unknown'],2)
        finally:p.write_bytes(original)
    def test_truncated_record_is_not_read(self):
        p=self.path();original=p.read_bytes()
        try:
            p.write_bytes(trp()[:0x800+2*0x60-1]);j=self.js('/api/set/10000000/NPWR00001_00/state');self.assertEqual(j['unknown'],1)
        finally:p.write_bytes(original)
    def test_malformed_archive_bounds(self):
        p=self.root/'user/trophy2/nobackup/conf/NPWR00001_00/TROPHY.UCP';original=p.read_bytes()
        try:
            bad=bytearray(ucp({'tropconf.json':'{}'}));struct.pack_into('>Q',bad,0x60+32,2**63);p.write_bytes(bad)
            with self.assertRaises(HTTPError) as ctx:self.get('/api/set/10000000/NPWR00001_00/conf')
            self.assertEqual(ctx.exception.code,404)
        finally:p.write_bytes(original)
    def test_no_writes_or_arbitrary_file_access(self):
        for path in ['/api/set/../../etc/passwd','/api/set/10000000/NPWR00001_00/%2e%2e','/api/set/10000000/NPWR00001_00/../../secret','/api/set/invalid/NPWR00001_00/state','/api/set/10000000/NPWR00001_00/other','/demo.json']:
            with self.assertRaises(HTTPError):self.get(path)
        with self.assertRaises(HTTPError) as ctx:urlopen(Request(self.url('/api/catalog'),data=b'{}',method='POST'))
        self.assertEqual(ctx.exception.code,405)
    def test_slow_header_deadline(self):
        with socket.create_connection(('127.0.0.1',self.port),timeout=5) as s:
            s.sendall(b'GET / HTTP/1.1\r\n');s.settimeout(4);data=s.recv(4096);self.assertIn(b'400',data)
        self.assertEqual(self.js('/api/catalog')['source'],'fixture')
    def test_missing_storage_is_error(self):
        path=self.root/'user/home';renamed=self.root/'user/home-hidden';path.rename(renamed)
        try:
            with self.assertRaises(HTTPError) as ctx:self.get('/api/catalog')
            self.assertEqual(ctx.exception.code,503)
        finally:renamed.rename(path)
