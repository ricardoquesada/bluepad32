# Paint example for CircuitPython
# Requires the "Bluepad32 for AirLift" firmware
# See: https://gitlab.com/ricardoquesada/bluepad32

# Copy this file the CIRCUITPY folder, and rename it as "code.py"
# Then install the following CircuitPython libs to CIRCUITPY/lib/
#  - adafruit_bus_device
#  - adafruit_esp32spi
#  - adafruit_matrixportal

import math
import time

import bluepad32

import board
import busio
import displayio
from adafruit_matrixportal.matrix import Matrix
from digitalio import DigitalInOut
from micropython import const


class Paint:
    _SCREEN_WIDTH = const(64)
    _SCREEN_HEIGHT = const(32)
    _PALETTE_SIZE = const(16)

    def __init__(self):
        self._esp = self._init_spi()
        self._bitmap, self._palette = self._init_bitmap()
        self._display = self._init_display(self._bitmap, self._palette)

    def _init_spi(self):
        # If you are using a board with pre-defined ESP32 Pins:
        esp32_cs = DigitalInOut(board.ESP_CS)
        esp32_ready = DigitalInOut(board.ESP_BUSY)
        esp32_reset = DigitalInOut(board.ESP_RESET)

        # If you have an AirLift Shield:
        # esp32_cs = DigitalInOut(board.D10)
        # esp32_ready = DigitalInOut(board.D7)
        # esp32_reset = DigitalInOut(board.D5)

        # If you have an AirLift Featherwing or ItsyBitsy Airlift:
        # esp32_cs = DigitalInOut(board.D13)
        # esp32_ready = DigitalInOut(board.D11)
        # esp32_reset = DigitalInOut(board.D12)

        # If you have an externally connected ESP32:
        # NOTE: You may need to change the pins to reflect your wiring
        # esp32_cs = DigitalInOut(board.D10)
        # esp32_ready = DigitalInOut(board.D11)
        # esp32_reset = DigitalInOut(board.D12)

        spi = busio.SPI(board.SCK, board.MOSI, board.MISO)
        esp = bluepad32.ESP_SPIcontrol(
            spi, esp32_cs, esp32_ready, esp32_reset, debug=0)
        return esp

    def _init_bitmap(self):
        bitmap = displayio.Bitmap(_SCREEN_WIDTH, _SCREEN_HEIGHT, _PALETTE_SIZE)

        # For fun, just use the CGA color palette
        palette = displayio.Palette(_PALETTE_SIZE)
        palette[0] = (0, 0, 0)  # Black
        palette[1] = (0, 0, 170)  # Blue
        palette[2] = (0, 170, 0)  # Green
        palette[3] = (0, 170, 170)  # Cyan
        palette[4] = (170, 0, 0)  # Red
        palette[5] = (170, 0, 170)  # Magenta
        palette[6] = (170, 85, 0)  # Brown
        palette[7] = (170, 170, 170)  # Light Gray

        palette[8] = (85, 85, 85)  # Dark Gray
        palette[9] = (85, 85, 255)  # Light Blue
        palette[10] = (85, 255, 85)  # Light Green
        palette[11] = (85, 255, 255)  # Light Cyan
        palette[12] = (255, 85, 85)  # Light Red
        palette[13] = (255, 85, 255)  # Light Magenta
        palette[14] = (255, 255, 85)  # Yellow
        palette[15] = (255, 255, 255)  # White

        return bitmap, palette

    def _init_display(self, bitmap, palette):
        tile_grid = displayio.TileGrid(bitmap, pixel_shader=palette)
        group = displayio.Group()
        group.append(tile_grid)
        # 16 colors == 4 bit of depth
        matrix = Matrix(bit_depth=math.ceil(math.log(_PALETTE_SIZE, 2)))
        display = matrix.display
        display.show(group)

        return display

    def run(self):
        # esp.set_esp_debug(1)
        x = 0
        y = 0
        color = 1
        max_color = len(self._palette)

        while True:
            # TODO: Is there a "displayio.wait_for_refresh()". Couldn't find it.
            # In the meantime fix speed at 30 FPS
            time.sleep(0.032)
            gamepad = self._esp.get_gamepads_data()
            if len(gamepad) > 0:
                gp = gamepad[0]

                # d-pad constants are defined here:
                # https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/src/main/uni_gamepad.h
                dpad = gp["dpad"]
                if dpad & 0x01:  # Up
                    y -= 1
                if dpad & 0x02:  # Down
                    y += 1
                if dpad & 0x04:  # Right
                    x += 1
                if dpad & 0x08:  # Left
                    x -= 1

                # axis + accel + brake have a 10-bit resolution
                # axis range: -512-511
                # accel, brake range: 0-1023
                axis_x = gp["axis_x"]
                axis_y = gp["axis_y"]
                if axis_x < -100:
                    x -= 1
                if axis_x > 100:
                    x += 1
                if axis_y < -100:
                    y -= 1
                if axis_y > 100:
                    y += 1

                x = max(x, 0)
                x = min(x, _SCREEN_WIDTH - 1)
                y = max(y, 0)
                y = min(y, _SCREEN_HEIGHT - 1)

                # Button constants are defined here:
                # https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/src/main/uni_gamepad.h
                buttons = gp["buttons"]

                if buttons & (0x01 | 0x02):
                    if buttons & 0x01:  # Button A
                        color += 1
                    if buttons & 0x02:  # Button B
                        color -= 1
                    color = color % max_color

                    # Only a few gamepads support changing the LED color, like the DUALSHOCK 4
                    # and the DualSense.
                    # If the connected gamepad doesn't support it, nothing will happen.
                    r = (self._palette[color] & 0xff000) >> 16
                    g = (self._palette[color] & 0xff00) >> 8
                    b = (self._palette[color] & 0xff)
                    self._esp.set_gamepad_color_led(gp['idx'], (r, g, b))

                if buttons & 0x04:  # Button X
                    # fill screen with current color
                    for i in range(0, _SCREEN_WIDTH):
                        for j in range(0, _SCREEN_HEIGHT):
                            self._bitmap[i, j] = color

            self._bitmap[x, y] = color


Paint().run()
