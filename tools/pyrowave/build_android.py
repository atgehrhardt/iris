#!/usr/bin/env python3
"""Build pinned PyroWave and Iris's optional native renderer for supported 64-bit ABIs."""
import argparse
import pathlib
import shutil
import subprocess
import sys


def run(*args):
    """Run a build command, preserving its failure status."""
    subprocess.run([str(arg) for arg in args], check=True)


def main():
    """Build outside tracked source trees and package all required shared libraries."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", required=True, type=pathlib.Path)
    parser.add_argument("--abis", default="arm64-v8a,x86_64")
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[2]
    build = root / "cmake-build-pyrowave"
    source = build / "source"
    run(sys.executable, pathlib.Path(__file__).with_name("bootstrap.py"), source)
    for abi in args.abis.split(","):
        if abi not in ("arm64-v8a", "x86_64"):
            raise ValueError("Unsupported PyroWave ABI: " + abi)
        common = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                  "-DCMAKE_TOOLCHAIN_FILE=" + str(args.ndk / "build/cmake/android.toolchain.cmake"),
                  "-DANDROID_ABI=" + abi, "-DANDROID_PLATFORM=android-26",
                  "-DANDROID_STL=c++_shared", "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"]
        codec_build = build / abi / "codec"
        run("cmake", "-S", source, "-B", codec_build, *common)
        run("cmake", "--build", codec_build, "--target", "pyrowave-shared", "--parallel", "4")
        library = codec_build / "libpyrowave-shared.so"
        renderer_build = build / abi / "renderer"
        run("cmake", "-S", root / "app/src/main/jni/pyrowave", "-B", renderer_build, *common,
            "-DPYROWAVE_SOURCE=" + str(source), "-DPYROWAVE_LIBRARY=" + str(library))
        run("cmake", "--build", renderer_build, "--parallel", "4")
        output = root / "app/build/generated/pyrowave/jniLibs" / abi
        output.mkdir(parents=True, exist_ok=True)
        shutil.copy2(library.resolve(), output / "libpyrowave-shared.so")
        shutil.copy2(renderer_build / "libiris-pyrowave.so", output / "libiris-pyrowave.so")
        target = "aarch64-linux-android" if abi == "arm64-v8a" else "x86_64-linux-android"
        runtimes = list((args.ndk / "toolchains/llvm/prebuilt").glob("*/sysroot/usr/lib/" + target + "/libc++_shared.so"))
        if len(runtimes) != 1:
            raise RuntimeError("Cannot locate the Android C++ runtime for " + abi)
        shutil.copy2(runtimes[0], output / "libc++_shared.so")
    licenses = root / "app/build/generated/pyrowave/assets/licenses"
    licenses.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / "LICENSE", licenses / "PyroWave.txt")
    shutil.copy2(source / "Granite/LICENSE", licenses / "Granite.txt")
    shutil.copy2(source / "Granite/third_party/volk/LICENSE.md", licenses / "volk.txt")
    shutil.copytree(source / "Granite/third_party/khronos/vulkan-headers/LICENSES",
                    licenses / "Vulkan-Headers", dirs_exist_ok=True)


if __name__ == "__main__":
    main()
