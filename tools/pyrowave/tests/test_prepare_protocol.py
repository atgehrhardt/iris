"""@brief Regression tests for patching native sources from Gradle's nested working directory."""
import importlib.util
import pathlib
import subprocess
import tempfile
import unittest
from unittest import mock

TOOLS = pathlib.Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
SOURCE = ROOT / "app/src/main/jni/moonlight-core/moonlight-common-c/src"
spec = importlib.util.spec_from_file_location("prepare_protocol", TOOLS / "prepare_protocol.py")
protocol = importlib.util.module_from_spec(spec)
spec.loader.exec_module(protocol)


class PrepareProtocolTest(unittest.TestCase):
    """@brief Ensure native codec negotiation cannot silently retain the upstream fallback."""

    def test_nested_git_working_directory_and_repeated_builds(self):
        """@brief Apply all four changes from a nested Git directory, preserving upstream sources."""
        names = ("Limelight.h", "RtspConnection.c", "SdpGenerator.c", "VideoDepacketizer.c")
        originals = {name: (SOURCE / name).read_bytes() for name in names}
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            subprocess.run(["git", "init", "-q", root], check=True)
            app = root / "app"
            app.mkdir()
            output = app / "build/generated/pyrowave/common"
            for _ in range(2):
                subprocess.run(["python3", TOOLS / "prepare_protocol.py", SOURCE, output],
                               cwd=app, check=True)
                for name in names:
                    self.assertNotEqual(originals[name], (output / name).read_bytes())
                    self.assertEqual(originals[name], (SOURCE / name).read_bytes())
                self.assertIn("NegotiatedVideoFormat = requested;", (output / "RtspConnection.c").read_text())
                self.assertIn('"x-nv-vqos[0].bitStreamFormat", "3"', (output / "SdpGenerator.c").read_text())

    def test_silently_skipped_patch_fails_build(self):
        """@brief Reject a successful Git exit code when the expected changes are missing."""
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(protocol.subprocess, "run"):
            with self.assertRaisesRegex(RuntimeError, "not applied"):
                protocol.prepare(SOURCE, pathlib.Path(temporary))

    def test_incompatible_upstream_fails_build(self):
        """@brief Propagate patch failures rather than packaging an unmodified protocol."""
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = root / "source"
            source.mkdir()
            with self.assertRaises(subprocess.CalledProcessError):
                protocol.prepare(source, root / "output")


if __name__ == "__main__":
    unittest.main()
