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
import random

import bluepad32

import board
import busio
import displayio
from adafruit_matrixportal.matrix import Matrix
from digitalio import DigitalInOut
from micropython import const

# Global constants
SCREEN_WIDTH = const(64)
SCREEN_HEIGHT = const(32)
PALETTE_SIZE = const(16)


class Fruit:
    def __init__(self, pos):
        self._pos = pos
        self._color = 4
        x, y = pos[0], pos[1]
        self._pixels = [(pos), (x + 1, y), (x, y + 1), (x + 1, y + 1)]

    def animate(self):
        pass

    def draw(self, bitmap):
        for p in self._pixels:
            bitmap[p[0], p[1]] = self._color

    def pixels(self):
        return self._pixels

    def set_position(self, x, y):
        self._pixels = [(x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1)]


class Snake:
    def __init__(self, pixels, direction, color):
        self._direction = direction
        self._pixels = pixels
        self._len = len(pixels)
        self._color = color

    def set_direction(self, direction) -> None:
        self._direction = direction

    def increase_tail(self, units: int) -> None:
        self._len += units

    def eat_fruit(self, fruit: Fruit) -> bool:
        head = self._pixels[0]
        return head in fruit.pixels()

    def animate(self) -> int:
        head = self._pixels[0]
        # x
        x = self._direction[0] + head[0]
        # y
        y = self._direction[1] + head[1]
        if x < 0 or x >= SCREEN_WIDTH:
            return -1
        if y < 0 or y >= SCREEN_HEIGHT:
            return -1
        self._pixels.insert(0, (x, y))
        if len(self._pixels) > self._len:
            # Remove tail
            del self._pixels[-1]
        return 0

    def draw(self, bitmap) -> None:
        for p in self._pixels:
            bitmap[p[0], p[1]] = self._color

    def pixels(self):
        return self._pixels

    def next_head(self):
        head = self._pixels[0]
        return (head[0] + self._direction[0], head[1] + self._direction[1])

    def remove_tail(self):
        if len(self._pixels) > 1:
            del self._pixels[-1]


class Display:
    def __init__(self):
        self._bitmap, self._palette = self._init_bitmap()
        self._display = self._init_display(self._bitmap, self._palette)

    def get_bitmap(self):
        return self._bitmap

    def _init_bitmap(self):
        bitmap = displayio.Bitmap(SCREEN_WIDTH, SCREEN_HEIGHT, PALETTE_SIZE)

        # For fun, just use the CGA color palette
        palette = displayio.Palette(PALETTE_SIZE)
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
        matrix = Matrix(bit_depth=math.ceil(math.log(PALETTE_SIZE, 2)))
        display = matrix.display
        # display.auto_refresh = False
        # display.rotation = 0
        display.show(group)

        return display

    def refresh(self):
        self._display.refresh(target_frames_per_second=30)


class Game:
    def __init__(self):
        self._esp = self._init_spi()
        self._display = Display()

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
        esp = bluepad32.ESP_SPIcontrol(spi, esp32_cs, esp32_ready, esp32_reset, debug=0)
        return esp

    def wait_for_gamepads(self):
        gamepads = []
        while len(gamepads) != 2:
            # TODO: Is there a "displayio.wait_for_refresh()". Couldn't find it.
            # In the meantime fix speed at 30 FPS
            time.sleep(0.032)
            gamepads = self._esp.get_gamepads_data()
            self._display.refresh()

        # Blue for #0
        self._esp.set_gamepad_color_led(0, (0, 0, 255))
        # Green for #1
        self._esp.set_gamepad_color_led(1, (0, 255, 0))

    def animate_if_no_collision(self, s0, s1):
        all_pixels = s0.pixels() + s1.pixels()
        if s0.next_head() not in all_pixels:
            s0.animate()
        else:
            s0.remove_tail()
        if s1.next_head() not in all_pixels:
            s1.animate()
        else:
            s1.remove_tail()

    def run(self):
        # esp.set_esp_debug(1)
        x = SCREEN_WIDTH // 2
        y = SCREEN_HEIGHT // 2

        snake0 = Snake(
            [(x // 2, y), (x // 2 + 1, y), (x // 2 + 2, y)], direction=(-1, 0), color=1
        )
        snake1 = Snake(
            [(x + x // 2 + 2, y), (x + x // 2 + 1, y), (x + x // 2 + 0, y)],
            direction=(1, 0),
            color=2,
        )

        fruit = Fruit(
            pos=(
                random.randint(0, SCREEN_WIDTH - 2),
                random.randint(0, SCREEN_HEIGHT - 2),
            )
        )

        sprites = (snake0, snake1, fruit)
        bitmap = self._display.get_bitmap()

        # Draw sprites / fruit. Initial position
        bitmap.fill(0)
        for sprite in sprites:
            sprite.draw(bitmap)

        self.wait_for_gamepads()

        while True:
            start_time = time.monotonic()

            gamepads = self._esp.get_gamepads_data()
            for gp in gamepads:

                # D-pad constants are defined here:
                # https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/src/main/uni_gamepad.h
                dpad = gp["dpad"]
                needs_update = True
                dir_x, dir_y = 0, 0
                if dpad & 0x01:  # Up
                    dir_x, dir_y = 0, -1
                elif dpad & 0x02:  # Down
                    dir_x, dir_y = 0, 1
                elif dpad & 0x04:  # Right
                    dir_x, dir_y = 1, 0
                elif dpad & 0x08:  # Left
                    dir_x, dir_y = -1, 0
                else:
                    needs_update = False

                if needs_update:
                    idx = gp["idx"]
                    s = snake0 if idx == 0 else snake1
                    s.set_direction((dir_x, dir_y))

            self.animate_if_no_collision(snake0, snake1)

            # check collision
            if snake0.eat_fruit(fruit):
                fruit.set_position(
                    random.randint(0, SCREEN_WIDTH - 2),
                    random.randint(0, SCREEN_HEIGHT - 2),
                )
                snake0.increase_tail(1)
            elif snake1.eat_fruit(fruit):
                fruit.set_position(
                    random.randint(0, SCREEN_WIDTH - 2),
                    random.randint(0, SCREEN_HEIGHT - 2),
                )
                snake1.increase_tail(1)

            # Clear screen + draw different sprites
            bitmap.fill(0)
            for sprite in sprites:
                sprite.draw(bitmap)

            self._display.refresh()

            dt = time.monotonic() - start_time
            # Target at 30 FPS
            sleep_time = 0.0333333 - dt
            if sleep_time > 0:
                time.sleep(sleep_time)


Game().run()