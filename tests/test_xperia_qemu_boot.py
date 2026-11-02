import tempfile
import unittest
from pathlib import Path
from tools.xperia_qemu_boot import milestones, commands, BootError, verify_game
class RealGameBootTests(unittest.TestCase):
    def test_trace_acceptance(self):
        trace=("TRACE: module loaded\nTRACE: JNI_OnLoad returned\n"
               "TRACE: NativeOnCreate returned\nTRACE: surface created\n"
               "TRACE: frames=570\nTRACE: framebuffer non-black\n"
               "TRACE: summary assets=84 textures=151 draws=35511\n"
               "TRACE: autopilot keys=11 scenes=4\n")
        a=milestones(trace,0)
        self.assertEqual(a["milestone"],7)
        self.assertTrue(a["real_game_started"])
        b=milestones("TRACE: module loaded\nTRACE: frames=100",1)
        self.assertEqual(b["milestone"],2)
        self.assertFalse(b["real_game_started"])
        self.assertEqual(milestones(trace.replace("assets=84","assets=0"),0)["milestone"],5)
    def test_game_only_runtime_mount_not_build_image(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);g=root/"private";s=root/"gpl"
            a,b,c=commands(s,g,570,150)
            self.assertNotIn(str(g), " ".join(a+b))
            self.assertIn(str(g.resolve())+":/game:ro",c)
            self.assertIn("qemu-arm",c)
            with self.assertRaises(BootError):verify_game(root)
if __name__=="__main__":unittest.main()
