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
import music76489

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

BLACK = const(0)
WHITE = const(1)
RED = const(2)
CYAN = const(3)
VIOLET = const(4)
GREEN = const(5)
BLUE = const(6)
YELLOW = const(7)
ORANGE = const(8)
BROWN = const(9)
LIGHT_RED = const(10)
DARK_GREY = const(11)
GREY_2 = const(12)
LIGHT_GREEN = const(13)
LIGHT_BLUE = const(14)
LIGHT_GRAY = const(15)


class C64Label:
    """Renders bitmap labels based in on a 8 * 256 charset.
    E.g: Character 0 is defined by bytes 0-7, Character 1 from bytes 8-15,
    and so on.
    This format is used in many 8-bit computers like the Commodore 64.
    Use VChar64 editor to edit your own charset:
    https://github.com/ricardoquesada/vchar64
    """

    def __init__(
        self, label: str, pos=(0, 0), color=WHITE, file="data/c64-font-charset.bin"
    ):
        self._charset = None
        self._label = label
        self._pos = pos
        self._color = color
        self._music = None
        with open(file, "rb") as f:
            self._charset = f.read()

    def draw(self, bitmap) -> None:
        bitmap_x = self._pos[0]
        for c in self._label:
            # Each char in chardef takes 8 bytes
            idx = ord(c) * 8
            bitmap_y = self._pos[1]
            for y in range(8):
                b = self._charset[idx + y]
                for x in range(8):
                    bit = b & (1 << (7 - x))
                    if bit:
                        bitmap[bitmap_x + x, bitmap_y + y] = self._color
            bitmap_x += 8


class Fruit:
    """A Sprite that represents what the snakes eat"""

    def __init__(self, pos):
        self._pos = pos
        self._color = RED
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
    """A sprite that represents what the players control"""

    def __init__(self, pixels, direction, color):
        self._direction = direction
        self._pixels = pixels
        self._len = len(pixels)
        self._color = color

    def set_direction(self, direction) -> None:
        # Forbid opposite direction
        if direction[0] != 0 and direction[0] == -self._direction[0]:
            return
        if direction[1] != 0 and direction[1] == -self._direction[1]:
            return
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
            # self._len -= 1


class Display:
    def __init__(self):
        self._bitmap, self._palette = self._init_bitmap()
        self._display = self._init_display(self._bitmap, self._palette)

    def get_bitmap(self):
        return self._bitmap

    def _init_bitmap(self):
        bitmap = displayio.Bitmap(SCREEN_WIDTH, SCREEN_HEIGHT, PALETTE_SIZE)

        # For fun, just use the C64 color palette
        palette = displayio.Palette(PALETTE_SIZE)
        palette[0] = (0, 0, 0)  # Black
        palette[1] = (255, 255, 255)  # White
        palette[2] = (170, 0, 0)  # Red
        palette[3] = (170, 255, 238)  # Cyan
        palette[4] = (204, 68, 204)  # Violet
        palette[5] = (0, 170, 0)  # Green
        palette[6] = (0, 0, 170)  # Blue
        palette[7] = (238, 238, 119)  # Yellow

        palette[8] = (221, 136, 85)  # Orange
        palette[9] = (102, 68, 0)  # Brown
        palette[10] = (255, 119, 199)  # Light red
        palette[11] = (51, 51, 51)  # Dark Grey
        palette[12] = (119, 119, 199)  # Grey 2
        palette[13] = (170, 255, 102)  # Light green
        palette[14] = (0, 136, 255)  # Light Blue
        palette[15] = (187, 187, 187)  # Light Grey

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

    def show_title(self):
        bitmap = self._display.get_bitmap()
        bitmap.fill(BLACK)
        label1 = C64Label("CIRCUIT", pos=(4, 5), color=VIOLET)
        label2 = C64Label("Snake", pos=(12, 12), color=RED)
        label3 = C64Label("v0.1", pos=(16, 24), color=DARK_GREY)
        label1.draw(bitmap)
        label2.draw(bitmap)
        label3.draw(bitmap)
        self._display.refresh()
        self._music.load_vgm("data/anime.vgm")
        elapsed = 0
        start_time = time.monotonic()
        while elapsed < 5:
            self._music.tick()
            time.sleep(0.016)
            elapsed = time.monotonic() - start_time

    def show_gamepads(self):
        bitmap = self._display.get_bitmap()
        label1 = C64Label("connect", pos=(4, 4), color=LIGHT_BLUE)
        label2 = C64Label("two", pos=(20, 12), color=LIGHT_BLUE)
        label3 = C64Label("gamepads", pos=(0, 20), color=LIGHT_BLUE)

        def show():
            bitmap.fill(BLACK)
            label1.draw(bitmap)
            label2.draw(bitmap)
            label3.draw(bitmap)

        def hide():
            bitmap.fill(BLACK)

        gamepads = []
        do_show = True
        while len(gamepads) != 2:
            # TODO: Is there a "displayio.wait_for_refresh()". Couldn't find it.
            # In the meantime fix speed at 30 FPS
            if do_show:
                show()
            else:
                hide()
            do_show = not do_show
            start_time = time.monotonic()
            elapsed = 0
            while elapsed < 0.7:
                self._music.tick()
                time.sleep(0.016)
                elapsed = time.monotonic() - start_time

            gamepads = self._esp.get_gamepads_data()
            self._display.refresh()

        self._music.reset()

        # Blue for #0
        self._esp.set_gamepad_color_led(0, (0, 0, 255))
        # Green for #1
        self._esp.set_gamepad_color_led(1, (0, 255, 0))

    def show_winner(self, winner):
        if winner == 0:
            label = "BLUE"
            color = BLUE
            x = 16
        else:
            label = "GREEN"
            color = GREEN
            x = 12

        bitmap = self._display.get_bitmap()
        bitmap.fill(BLACK)
        label1 = C64Label(label, pos=(x, 8), color=color)
        label2 = C64Label("WINS", pos=(16, 16), color=color)
        label1.draw(bitmap)
        label2.draw(bitmap)
        self._display.refresh()
        elapsed = 0
        start_time = time.monotonic()
        while elapsed < 1:
            self._music.tick()
            time.sleep(1 / 60)
            elapsed = time.monotonic() - start_time

    def is_game_over(self, snake0, snake1) -> bool:
        l0 = len(snake0.pixels())
        l1 = len(snake1.pixels())
        # At least one player reaches len 10, but not both at the same time
        return (l0 > 15 or l1 > 15) and l0 is not l1

    def play(self) -> int:
        """Returns which player won the game"""

        # Start Game
        bitmap = self._display.get_bitmap()

        x = SCREEN_WIDTH // 2
        y = SCREEN_HEIGHT // 2
        snake0 = Snake(
            [(x // 2, y), (x // 2 + 1, y), (x // 2 + 2, y)],
            direction=(-1, 0),
            color=BLUE,
        )
        snake1 = Snake(
            [(x + x // 2 + 2, y), (x + x // 2 + 1, y), (x + x // 2 + 0, y)],
            direction=(1, 0),
            color=GREEN,
        )

        fruit = Fruit(
            pos=(
                random.randint(0, SCREEN_WIDTH - 2),
                random.randint(0, SCREEN_HEIGHT - 2),
            )
        )

        sprites = (snake0, snake1, fruit)

        # Draw sprites / fruit. Initial position
        bitmap.fill(BLACK)
        for sprite in sprites:
            sprite.draw(bitmap)

        while not self.is_game_over(snake0, snake1):
            start_time = time.monotonic()

            self._music.tick()

            gamepads = self._esp.get_gamepads_data()
            for gp in gamepads:

                # d-pad constants are defined here:
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
            bitmap.fill(BLACK)
            for sprite in sprites:
                sprite.draw(bitmap)

            self._display.refresh()

            dt = time.monotonic() - start_time

            # Game at 30Hz, Music at 60Hz.
            sleep_time = 0.016666 - dt
            if sleep_time > 0:
                time.sleep(sleep_time)

            self._music.tick()
            time.sleep(0.016666)

        # Who won the game? player 0 or 1
        if len(snake0.pixels()) > len(snake1.pixels()):
            return 0
        return 1

    def init_music(self):
        self._music = music76489.Music76489()

    def run(self):
        # esp.set_esp_debug(1)

        self.init_music()

        # Show Main title
        self.show_title()

        # Show & Wait for gamepads
        self.show_gamepads()

        self._music.load_vgm("data/boss_battle.vgm")
        while True:
            # play
            winner = self.play()
            self.show_winner(winner)


Game().run()
