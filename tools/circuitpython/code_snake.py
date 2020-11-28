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
        x = _SCREEN_WIDTH // 2
        y = _SCREEN_HEIGHT // 2
        color = 1

        snake = [(x, y)]
        dir_x = -1
        dir_y = 0

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
                if dpad & (0x01 | 0x02):  # Up or Down
                    dir_y = -1 if dpad & 0x01 else 1
                    dir_x = 0
                elif dpad & (0x04 | 0x08): # Right or Left
                    dir_x = 1 if dpad & 0x04 else -1
                    dir_y = 0

            head = snake[0]
            new_x = head[0] + dir_x
            new_x = max(0, new_x)
            new_x = min(_SCREEN_WIDTH-1, new_x)

            new_y = head[1] + dir_y
            new_y = max(0, new_y)
            new_y = min(_SCREEN_HEIGHT-1, new_y)

            snake.insert(0, (new_x, new_y))
            if len(snake) > 10:
                del snake[-1]

            self._bitmap.fill(0)
            for s in snake:
                self._bitmap[s[0], s[1]] = color


Paint().run()