"""Synthetic Java lifecycle and JNI source-boundary regression fixtures."""
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from tools.deadspace_java_bootstrap import examine, survey


class JavaCodeRecoveryTests(unittest.TestCase):
    def test_lifecycle_calls_and_native_declarations_without_source(self):
        source = """
package example.launcher;
public class GameActivity extends android.app.Activity {
  static { System.loadLibrary("sampleNative"); }
  private native void beginGame(int value);
  @Override public void onCreate(android.os.Bundle state) {
    super.onCreate(state);
    setRequestedOrientation(1);
    beginGame(1);
    setContentView(new android.view.View(this));
  }
  @Override public void onPause() {
    super.onPause();
    pauseGame();
  }
}
"""
        found = examine(source)
        self.assertEqual(found["class"], "example.launcher.GameActivity")
        self.assertEqual(found["native_declarations"], ["beginGame"])
        self.assertEqual(found["library_names"], ["sampleNative"])
        self.assertEqual(found["callbacks"][0]["callback"], "onCreate")
        self.assertIn("setContentView", found["callbacks"][0]["public_runtime_call_sequence"])
        self.assertNotIn("beginGame(1)", json.dumps(found))
        self.assertNotIn("super.onPause()", json.dumps(found))

    def test_prioritizes_bootstrap_classes(self):
        with TemporaryDirectory() as tmp:
            base = Path(tmp)
            (base / "Idle.java").write_text(
                "public class Idle {\n  public void run() { consume(); }\n}")
            (base / "Entry.java").write_text(
                'public class Entry { static { System.loadLibrary("core"); } '
                'private native int bootstrap(); void onCreate() { finish(); } }')
            result = survey(base)
            self.assertEqual(result["total_relevant_classes"], 2)
            self.assertEqual(result["relevant_classes"][0]["library_names"], ["core"])
            self.assertNotIn("void onCreate", json.dumps(result))


if __name__ == "__main__":
    unittest.main()
