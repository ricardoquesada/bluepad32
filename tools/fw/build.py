#!/usr/bin/python3

# Simple script that prepares the builds ready to be uploaded.

import argparse
import os
import shutil
import subprocess


class Distro:
    def __init__(self, platform, version=""):
        # List of platforms to build
        if platform == "all":
            self._platforms = ("unijoysticle", "airlift", "mightymiggy")
        else:
            self._platforms = (platform,)

        self._version = version

    def build(self) -> None:

        for p in self._platforms:
            self._clean(p)
            self._build(p)
            self._combine(p)
            self._dist(p)

    def _clean(self, platform) -> None:
        print(f"Clean: {platform}")
        cmd = f"PLATFORM={platform} make clean"
        self._exe(cmd)

    def _build(self, platform) -> None:
        cmd = f"PLATFORM={platform} make -j"
        self._exe(cmd)

    def _exe(self, cmd) -> None:
        subprocess.run(cmd, cwd="../../src", shell=True, check=True)

    def _dist(self, platform) -> None:
        # Create folder
        dest_dir = f"dist/bluepad32-{platform}-{self._version}"
        os.makedirs(dest_dir)

        # Copy README + firmware
        orig = f"template-README-{platform}.md"
        dest = os.path.join(dest_dir, "README.md")
        shutil.copyfile(orig, dest)

        orig = f"bluepad32-{platform}-full-{self._version}.bin"
        shutil.copyfile(orig, os.path.join(dest_dir, orig))

        # Create compressed folder
        zip_name = f"bluepad32-{platform}-{self._version}"
        shutil.make_archive(
            zip_name,
            "gztar",
            root_dir="dist",
            base_dir=f"bluepad32-{platform}-{self._version}",
        )

    def _combine(self, platform) -> None:
        path = "../../src/build/"
        booloaderData = open(
            os.path.join(path, "bootloader/bootloader.bin"), "rb"
        ).read()
        partitionData = open(
            os.path.join(path, "partitions_singleapp.bin"), "rb"
        ).read()
        appData = open(os.path.join(path, f"bluepad32-{platform}.bin"), "rb").read()

        # calculate the output binary size, app offset
        outputSize = 0x10000 + len(appData)
        if outputSize % 1024:
            outputSize += 1024 - (outputSize % 1024)

        # allocate and init to 0xff
        outputData = bytearray(b"\xff") * outputSize

        # copy data: bootloader, partitions, app
        for i in range(0, len(booloaderData)):
            outputData[0x1000 + i] = booloaderData[i]

        for i in range(0, len(partitionData)):
            outputData[0x8000 + i] = partitionData[i]

        for i in range(0, len(appData)):
            outputData[0x10000 + i] = appData[i]

        outputFilename = f"bluepad32-{platform}-full-{self._version}.bin"

        # write out
        with open(outputFilename, "w+b") as f:
            f.seek(0)
            f.write(outputData)
            print(f"Firmware generated: {outputFilename}")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Build the distro files of Bluepad32 for the different platforms",
        epilog="""Example:

$ %(prog)s --set-version v2.0.0 unijoysticle
""",
    )

    parser.add_argument(
        "--set-version",
        metavar="<version>",
        default="dirty",
        help="Version of Bluepad32. Default: dirty",
    )

    parser.add_argument(
        "platform",
        choices=["all", "unijoysticle", "airlift", "mightymiggy"],
        help="Platform to build for",
    )

    args = parser.parse_args()
    return args


def main():
    args = parse_args()
    Distro(args.platform, version=args.set_version).build()


if __name__ == "__main__":
    main()
