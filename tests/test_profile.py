import subprocess,time,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class ProfileTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary=ROOT/'build/profile-test'
        subprocess.run(['cc','-std=gnu11','-DHOST_BUILD','-DPROFILE_TEST','-Wall','-Wextra','-Werror','-I'+str(ROOT/'payload'),str(ROOT/'tests/profile_harness.c'),str(ROOT/'payload/profile.c'),'-lpthread','-o',str(cls.binary)],check=True)
    def run_mode(self,mode):return subprocess.check_output([str(self.binary),mode],text=True,timeout=4).splitlines()
    def test_nickname_and_hex_id_and_cache(self):self.assertEqual(self.run_mode('success'),['测试用户 Player','测试用户 Player','calls=1'])
    def test_loader_already_initialized(self):self.assertEqual(self.run_mode('already')[0],'测试用户 Player')
    def test_failure_keeps_storage_id(self):self.assertEqual(self.run_mode('failure'),['本地用户 10000001','本地用户 10000001','calls=1'])
    def test_invalid_buffer_falls_back(self):self.assertEqual(self.run_mode('unterminated')[0],'本地用户 10000001')
    def test_slow_service_returns_before_worker_finishes(self):
        start=time.monotonic()
        p=subprocess.Popen([str(self.binary),'slow'],stdout=subprocess.PIPE,text=True,bufsize=1)
        # stdout is block buffered, so verify the first label and eventual cached
        # result rather than assuming when the pipe flushes.
        out,_=p.communicate(timeout=4)
        self.assertEqual(out.splitlines(),['本地用户 10000001','测试用户 Player','calls=1'])
        self.assertLess(time.monotonic()-start,2)
