#!/usr/bin/env python3
"""Apply the Iris protocol extension to a build-owned copy, leaving the upstream submodule clean."""
import pathlib
import shutil
import subprocess
import sys
import tempfile


def prepare(source, destination):
    """@brief Copy and patch native protocol sources independently of the caller's Git working directory.

    @param source Unmodified moonlight-common-c source directory.
    @param destination Generated source directory consumed by ndk-build.
    @throws RuntimeError If Git skips a required protocol change.
    """
    source = source.resolve()
    destination = destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    for path in source.iterdir():
        if path.is_file():
            shutil.copy2(path, destination / path.name)
    # git apply silently skips absolute paths when invoked from a repository's
    # subdirectory (Gradle's working directory is iris/app). Use a directory
    # outside any repository so every patch entry is actually applied.
    with tempfile.TemporaryDirectory(prefix="iris-protocol-patch-") as working:
        subprocess.run(["git", "apply", "--unsafe-paths", "--directory=" + str(destination),
                        str(pathlib.Path(__file__).with_name("moonlight-common-c.patch"))],
                       cwd=working, check=True)
    required = {
        "Limelight.h": "#define VIDEO_FORMAT_MASK_PYROWAVE  0xF0000",
        "RtspConnection.c": '"x-prism-pyrowave.version"',
        "SdpGenerator.c": '"x-prism-pyrowave.version", "1"',
        "VideoDepacketizer.c": "VIDEO_FORMAT_MASK_AV1 | VIDEO_FORMAT_MASK_PYROWAVE",
    }
    for name, marker in required.items():
        if marker not in (destination / name).read_text():
            raise RuntimeError("PyroWave protocol patch was not applied to " + name)


if __name__ == "__main__":
    prepare(*map(pathlib.Path, sys.argv[1:]))
