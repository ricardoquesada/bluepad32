# Paint example for CircuitPython
# Requires the "Bluepad32 for AirLift" firmware
# See: https://gitlab.com/ricardoquesada/bluepad32

# Copy this file the CIRCUITPY folder, and rename it as "code.py"
# Install these CircuitPython libs:
#  - adafruit_bus_device
#  - adafruit_esp32spi
#  - adafruit_matrixportal

import time

import board
import busio
import displayio
from digitalio import DigitalInOut
from adafruit_esp32spi import adafruit_esp32spi
from adafruit_matrixportal.matrix import Matrix


class Paint:
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
        esp = adafruit_esp32spi.ESP_SPIcontrol(
            spi, esp32_cs, esp32_ready, esp32_reset, debug=0)
        return esp

    def _init_bitmap(self):
        bitmap = displayio.Bitmap(64, 32, 16)

        # For fun, just use the CGA color palette
        palette = displayio.Palette(16)
        palette[0] = (0, 0, 0)  # Black
        palette[1] = (0, 0, 170)  # Blue
        palette[2] = (0, 170, 0)  # Green
        palette[3] = (0, 170, 170)  # Cyan
        palette[4] = (170, 0, 0)  # Red
        palette[5] = (170, 0, 170)  # Magenta
        palette[6] = (170, 85, 0)  # Brown
        palette[7] = (170, 170, 170)  # Light gray

        palette[8] = (85, 85, 85)  # Drak gray
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
        matrix = Matrix(bit_depth=4)
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
                dpad = gp['dpad']
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
                axis_x = gp['axis_x']
                axis_y = gp['axis_y']
                if axis_x < -100:
                    x -= 1
                if axis_x > 100:
                    x += 1
                if axis_y < -100:
                    y -= 1
                if axis_y > 100:
                    y += 1

                x = max(x, 0)
                x = min(x, 63)
                y = max(y, 0)
                y = min(y, 31)

                # Button constants are defined here:
                # https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/src/main/uni_gamepad.h
                buttons = gp['buttons']

                if buttons & 0x01:  # Button A
                    color += 1
                if buttons & 0x02:  # Button B
                    color -= 1
                color = color % max_color

                if buttons & 0x04:  # Button X
                    # fill with current color
                    for i in range(0, 64):
                        for j in range(0, 32):
                            self._bitmap[i, j] = color

            self._bitmap[x, y] = color


Paint().run()