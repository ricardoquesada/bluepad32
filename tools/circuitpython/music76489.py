#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# 76489 music player
# Supports VGM format 1.50:
#  - https://www.smspower.org/uploads/Music/vgmspec150.txt
#  - There are newer VGM formats, but supporting the one used in Deflemask.
# -----------------------------------------------------------------------------
"""
"""
import adafruit_74hc595
import board
import busio
import digitalio
import struct
import time

"""copy paste to test on console
import adafruit_74hc595
import board
import busio
import digitalio

spi = busio.SPI(board.A1, MOSI=board.A2)
latch_pin = digitalio.DigitalInOut(board.A3)
sr = adafruit_74hc595.ShiftRegister74HC595(spi, latch_pin)
we = digitalio.DigitalInOut(board.A4)
we.direction = digitalio.Direction.OUTPUT

sr.gpio = 0xff

"""


__docformat__ = "restructuredtext"


class Music76489:
    """The class that does all the conversions"""

    def __init__(self):
        self._offset = 0
        self._data = bytearray()
        self._should_loop = False
        self._loop_offset = 0
        self._prev_time = time.monotonic()
        self._time_to_wait = 0
        self._end_of_song = False

        self._spi = busio.SPI(board.A1, MOSI=board.A2)
        self._latch_pin = digitalio.DigitalInOut(board.A3)
        self._sr = adafruit_74hc595.ShiftRegister74HC595(self._spi, self._latch_pin)
        self._sn76489_we = digitalio.DigitalInOut(board.A4)
        self._sn76489_we.direction = digitalio.Direction.OUTPUT

        self._sn76489_we.value = True

        self.reset()

    def load_song(self, filename):
        self.reset()
        with open(filename, "rb") as fd:
            # FIXME: Assuming VGM version is 1.50 (64 bytes of header)
            header = bytearray(fd.read(0x40))

            # 0x00: "Vgm " (0x56 0x67 0x6d 0x20) file identification (32 bits)
            if header[:4].decode("utf-8") != "Vgm ":
                raise Exception("Invalid header")

            # 0x08: Version number (32 bits)
            #  Version 1.50 is stored as 0x00000150, stored as 0x50 0x01 0x00 0x00.
            #  This is used for backwards compatibility in players, and defines which
            #  header values are valid.
            vgm_version = struct.unpack_from("<I", header, 8)[0]
            if vgm_version != 0x150:
                raise Exception(
                    "Invalid VGM version format; got %x, want 0x150" % vgm_version
                )

            # 0x0c: SN76489 clock (32 bits)
            #  Input clock rate in Hz for the SN76489 PSG chip. A typical value is
            #  3579545. It should be 0 if there is no PSG chip used.
            sn76489_clock = struct.unpack_from("<I", header, 12)[0]
            if sn76489_clock != 3579545:
                raise Exception(
                    "Invalid VGM clock freq; got %d, want 357945" % sn76489_clock
                )

            # 0x04: Eof offset (32 bits)
            #  Relative offset to end of file (i.e. file length - 4).
            #  This is mainly used to find the next track when concatanating
            #  player stubs and multiple files.
            file_len = struct.unpack_from("<I", header, 4)[0]
            self._data = bytearray(fd.read(file_len + 4 - 0x40))

            # 0x1c: Loop offset (32 bits)
            #  Relative offset to loop point, or 0 if no loop.
            #  For example, if the data for the one-off intro to a song was in bytes
            #  0x0040-0x3fff of the file, but the main looping section started at
            #  0x4000, this would contain the value 0x4000-0x1c = 0x00003fe4.
            loop = struct.unpack_from("<I", header, 0x1C)[0]
            self._should_loop = True if loop != 0 else False
            self._loop_offset = loop + 0x1C - 0x40

    def play(self):
        if self._end_of_song:
            raise Exception("End of song reached")

        if self._time_to_wait > 0:
            now = time.monotonic()
            dt = now - self._prev_time
            self._prev_time = now
            self._time_to_wait -= dt
            # Allow some error margin
            if self._time_to_wait > 0.0001:
                return
            self._time_to_wait = 0

        # Convert to local variables (easier to ready... and tiny bit faster?)
        data = self._data
        i = self._offset
        while True:
            if i >= len(data):
                raise Exception(f"unexpected offset: {i} >= {len(data)}")

            # Valid commands
            #  0x4f dd    : Game Gear PSG stereo, write dd to port 0x06
            #  0x50 dd    : PSG (SN76489/SN76496) write value dd
            #  0x51 aa dd : YM2413, write value dd to register aa
            #  0x52 aa dd : YM2612 port 0, write value dd to register aa
            #  0x53 aa dd : YM2612 port 1, write value dd to register aa
            #  0x54 aa dd : YM2151, write value dd to register aa
            #  0x61 nn nn : Wait n samples, n can range from 0 to 65535 (approx 1.49
            #               seconds). Longer pauses than this are represented by multiple
            #               wait commands.
            #  0x62       : wait 735 samples (60th of a second), a shortcut for
            #               0x61 0xdf 0x02
            #  0x63       : wait 882 samples (50th of a second), a shortcut for
            #               0x61 0x72 0x03
            #  0x66       : end of sound data
            #  0x67 ...   : data block: see below
            #  0x7n       : wait n+1 samples, n can range from 0 to 15.
            #  0x8n       : YM2612 port 0 address 2A write from the data bank, then wait
            #               n samples; n can range from 0 to 15. Note that the wait is n,
            #               NOT n+1.
            #  0xe0 dddddddd : seek to offset dddddddd (Intel byte order) in PCM data bank

            # print(f'data: 0x{data[i]:02x}')

            #  0x50 dd    : PSG (SN76489/SN76496) write value dd
            if data[i] == 0x50:
                self.write_port_data(data[i + 1])
                i = i + 2

            #  0x61 nn nn : Wait n samples, n can range from 0 to 65535 (approx 1.49
            #               seconds). Longer pauses than this are represented by multiple
            #               wait commands.
            elif data[i] == 0x61:
                # unpack little endian unsigned short
                delay = struct.unpack_from("<H", data, i + 1)[0]
                self.delay_n(delay)
                i = i + 3
                break

            #  0x62       : wait 735 samples (60th of a second), a shortcut for
            #               0x61 0xdf 0x02
            elif data[i] == 0x62:
                self.delay_one()
                i = i + 1
                break

            #  0x66       : end of sound data
            elif data[i] == 0x66:
                if self._should_loop:
                    i = self._loop_offset
                else:
                    self.end_song()
                    break

            else:
                raise Exception("Unknown value: data[0x%x] = 0x%x" % (i, data[i]))
        # update offset
        self._offset = i

    def end_song(self):
        self._end_of_song = True

    def delay_one(self):
        self._time_to_wait = 1 / 60

    def delay_n(self, samples):
        # 735 samples == 1/60s
        self._time_to_wait = samples / (735 * 60)

    def write_port_data(self, byte_data):
        # If you wire everything upside-down, might easier to reverse it
        # in software than to re-wire everything again... at least in the
        # prototype phase.
        # reversed: MSB -> LSB
        def reverse_byte(a):
            r = (
                ((a & 0x1) << 7)
                | ((a & 0x2) << 5)
                | ((a & 0x4) << 3)
                | ((a & 0x8) << 1)
                | ((a & 0x10) >> 1)
                | ((a & 0x20) >> 3)
                | ((a & 0x40) >> 5)
                | ((a & 0x80) >> 7)
            )
            return r

        # Send data
        self._sr.gpio = reverse_byte(byte_data)

        # Enable SN76489
        self._sn76489_we.value = False
        # Allow it to read, and wait a very small time (needed?)
        # time.sleep(0.001)
        # Disable SN76489
        self._sn76489_we.value = True

    def reset(self):
        reset_seq = [0x9F, 0xBF, 0xDF, 0xFF]
        for b in reset_seq:
            self.write_port_data(b)
        self._offset = 0
        self._data = bytearray()


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="VGM music playaer",
        epilog="""Example:

$ %(prog)s my_music.vgm
""",
    )
    parser.add_argument("filename", metavar="<filename>", help="file to play")
    args = parser.parse_args()
    if args.filename:
        m = Music76489(args.filename)
        while True:
            m.parse()
            time.sleep(1 / 60)


if __name__ == "__main__":
    main()
