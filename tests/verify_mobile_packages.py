"""Validate installed mobile binaries without running them on the build host."""
import argparse
import pathlib
import re
import subprocess


def output(*command):
    return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=pathlib.Path, required=True)
    parser.add_argument("--ndk", type=pathlib.Path, required=True)
    args = parser.parse_args()
    ios = args.prefix / "platforms/ios/lib/libiiPaintEngine.dylib"
    android = args.prefix / "platforms/android/lib/libiiPaintEngine.so"
    assert ios.is_file(), f"Missing iOS library: {ios}"
    assert android.is_file(), f"Missing Android library: {android}"
    assert output("xcrun", "lipo", "-archs", str(ios)).strip() == "arm64", "iOS library must contain ARM64 only"
    assert re.search(r"platform\s+IOS\b", output("xcrun", "vtool", "-show-build", str(ios))), "iOS library contains a host or simulator build"
    readelf = next((args.ndk / "toolchains/llvm/prebuilt").glob("*/bin/llvm-readelf"))
    header = output(str(readelf), "-h", str(android))
    assert re.search(r"Machine:\s+AArch64\b", header), "Android library must contain AArch64 code"
    for platform in ("ios", "android"):
        config = args.prefix / "platforms" / platform / "lib/cmake/iiPaintEngine/iiPaintEngineConfig.cmake"
        assert config.is_file(), f"Missing {platform} CMake package"
    print("Installed iOS ARM64 and Android AArch64 packages verified")


if __name__ == "__main__":
    main()
