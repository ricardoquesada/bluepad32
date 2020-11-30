#!/usr/bin/python3

# Original taken from Nina-fw: https://github.com/arduino/nina-fw
# Modified for Bluepad32

import os
import sys


def combine(path, platform):
    booloaderData = open(os.path.join(path, "bootloader/bootloader.bin"), "rb").read()
    partitionData = open(os.path.join(path, "partitions_singleapp.bin"), "rb").read()
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

    outputFilename = f"bluepad32-{platform}-full.bin"

    # write out
    with open(outputFilename, "w+b") as f:
        f.seek(0)
        f.write(outputData)
        print(f"Firmware generated: {outputFilename}")


def help():
    print(f"{sys.argv[0]} path-to-buid-folder")
    print("e.g:")
    print(f"{sys.argv[0]} ./build")
    sys.exit(0)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        help()
    plat = os.getenv("PLATFORM")
    if plat is None:
        print("PLATFORM env variable empty. Set it accordingly")
        sys.exit(0)
    combine(sys.argv[1], plat)
