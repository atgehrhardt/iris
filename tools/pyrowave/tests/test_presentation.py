"""@brief Compile and run the renderer's native orientation and display-coverage regression tests."""
import os
import pathlib
import shlex
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]


class PresentationTest(unittest.TestCase):
    """@brief Test the same geometry helper compiled into the Android renderer."""

    def test_native_presentation(self):
        """@brief Cover rotations, mirrors, full-screen scaling, letterboxing, and rejected dimensions."""
        with tempfile.TemporaryDirectory() as temporary:
            executable = pathlib.Path(temporary) / "presentation_test"
            subprocess.run([
                *shlex.split(os.environ.get("CXX", "c++")), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I" + str(ROOT / "app/src/main/jni/pyrowave"),
                "-I" + str(ROOT / "cmake-build-pyrowave/source/Granite/third_party/khronos/vulkan-headers/include"),
                str(pathlib.Path(__file__).with_name("presentation_test.cpp")), "-o", executable,
            ], check=True)
            subprocess.run([executable], check=True)


if __name__ == "__main__":
    unittest.main()
