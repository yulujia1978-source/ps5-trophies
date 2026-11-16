import fcntl,json,os,shutil,socket,subprocess,sys,tempfile,threading,time,unittest
from pathlib import Path
from urllib.request import urlopen
ROOT=Path(__file__).resolve().parents[1]
VERSION='0.2.3'
class LifecycleTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.processes=[];self.fds=[]
        self.directory=self.root/'data/ps5-trophies';self.directory.mkdir(parents=True)
        with socket.socket() as s:s.bind(('127.0.0.1',0));self.port=s.getsockname()[1]
    def tearDown(self):
        for p in self.processes:
            if p.poll() is None:p.terminate()
        for p in self.processes:p.wait(timeout=5)
        for f in self.fds:
            if not f.closed:f.close()
        self.tmp.cleanup()
    def launch(self,wait=True,timeout=15000):
        env={**os.environ,'TROPHY_TEST_WAIT_MS':str(timeout)}
        p=subprocess.Popen([str(ROOT/'build/trophy-server'),str(self.root),str(self.port)],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);self.processes.append(p)
        if wait:
            for _ in range(200):
                if p.poll() is not None:self.fail('server exited before ready')
                try:
                    self.status();break
                except OSError:time.sleep(.02)
            else:self.fail('server did not become ready')
        return p
    def status(self):
        with urlopen(f'http://127.0.0.1:{self.port}/api/status',timeout=.3) as r:return json.load(r)
    def hold(self,name='service.lock',text=''):
        f=open(self.directory/name,'w+');f.write(text);f.flush();fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);self.fds.append(f);return f
    def install(self,version=VERSION,slot='ps5-trophies',valid=True):
        target=self.root/f'data/pldmgr/payloads/local/{slot}';target.mkdir(parents=True)
        name=f'ps5-trophies-v{version}.elf';(target/'meta.json').write_text(json.dumps({'filename':name,'kind':'local'}))
        elf=bytearray(64);elf[:6]=b'\x7fELF\x02\x01';elf[18]=62
        if valid:elf+=f'PS5Trophies ELF version={version}'.encode()+b'\0'
        (target/name).write_bytes(elf);return target/name
    def test_status_and_stop(self):
        p=self.launch();j=self.status();self.assertEqual(j['version'],VERSION);self.assertTrue(j['session']);(self.directory/'STOP').touch();self.assertEqual(p.wait(timeout=4),0)
    def test_old_empty_lock_handoff(self):
        f=self.hold();marker=self.directory/'STOP';observed=threading.Event()
        def old():
            while not marker.exists():time.sleep(.01)
            observed.set();fcntl.flock(f,fcntl.LOCK_UN)
        t=threading.Thread(target=old,daemon=True);t.start();p=self.launch();t.join(timeout=2);self.assertTrue(observed.is_set());self.assertFalse(marker.exists());self.assertIsNone(p.poll())
    def test_real_old_process_closes_port_before_handoff(self):
        code="""import fcntl,socket,sys,time
from pathlib import Path
root=Path(sys.argv[1]);f=open(root/'service.lock','w+');fcntl.flock(f,fcntl.LOCK_EX)
s=socket.socket();s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);s.bind(('127.0.0.1',int(sys.argv[2])));s.listen()
print('ready',flush=True)
while not (root/'STOP').exists():time.sleep(.01)
s.close();f.close()
"""
        old=subprocess.Popen([sys.executable,'-u','-c',code,str(self.directory),str(self.port)],stdout=subprocess.PIPE,text=True);self.processes.append(old)
        self.assertEqual(old.stdout.readline().strip(),'ready')
        new=self.launch();self.assertEqual(old.wait(timeout=3),0);old.stdout.close();self.assertIsNone(new.poll());self.assertEqual(self.status()['version'],VERSION)
    def test_recorded_older_version_handoff(self):
        f=self.hold(text='PS5Trophies version=0.2.2 pid=99999\n')
        def old():
            while not (self.directory/'STOP').exists():time.sleep(.01)
            fcntl.flock(f,fcntl.LOCK_UN)
        t=threading.Thread(target=old,daemon=True);t.start();self.launch();t.join(timeout=2);self.assertFalse(t.is_alive())
    def test_same_and_newer_are_preserved(self):
        for v in [VERSION,'0.9.0']:
            with self.subTest(v=v):
                f=self.hold(text=f'PS5Trophies version={v} pid=99\n');p=self.launch(False);self.assertEqual(p.wait(timeout=3),1);self.assertFalse((self.directory/'STOP').exists());f.close()
    def test_unknown_lock_is_preserved(self):
        self.hold(text='unrecognized');p=self.launch(False);self.assertEqual(p.wait(timeout=3),1);self.assertFalse((self.directory/'STOP').exists())
    def test_handoff_timeout_cleans_only_own_marker(self):
        f=self.hold();p=self.launch(False,timeout=200);self.assertEqual(p.wait(timeout=3),1);self.assertFalse((self.directory/'STOP').exists());self.assertEqual((self.directory/'service.lock').read_text(),'');fcntl.flock(f,fcntl.LOCK_UN)
    def test_concurrent_launcher_does_not_send_stop(self):
        self.hold('upgrade.lock');p=self.launch(False);self.assertEqual(p.wait(timeout=3),1);self.assertFalse((self.directory/'STOP').exists())
    def test_duplicate_launch_keeps_live_service(self):
        p=self.launch();before=self.status()['session'];new=self.launch(False);self.assertEqual(new.wait(timeout=3),1);self.assertIsNone(p.poll());self.assertEqual(self.status()['session'],before)
    def test_deleted_verified_install_stops_service(self):
        installed=self.install();p=self.launch();installed.unlink();self.assertEqual(p.wait(timeout=6),0)
    def test_short_replacement_gap_does_not_stop(self):
        installed=self.install();p=self.launch();backup=installed.read_bytes();installed.unlink();time.sleep(.7);installed.write_bytes(backup);time.sleep(3);self.assertIsNone(p.poll());self.status()
    def test_unverified_install_does_not_arm_watch(self):
        installed=self.install(valid=False);p=self.launch();installed.unlink();time.sleep(3.1);self.assertIsNone(p.poll())
    def test_ambiguous_install_does_not_arm_watch(self):
        a=self.install(slot='one');self.install(slot='two');p=self.launch();a.unlink();time.sleep(3.1);self.assertIsNone(p.poll())
    def test_stop_does_not_touch_turbo(self):
        other=self.root/'data/ps5-turbo';other.mkdir();(other/'service.lock').write_text('turbo record');p=self.launch();(self.directory/'STOP').touch();p.wait(timeout=4);self.assertEqual((other/'service.lock').read_text(),'turbo record');self.assertFalse((other/'STOP').exists())
