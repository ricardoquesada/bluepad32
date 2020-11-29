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
        self._pixels = [(pos), (x+1, y), (x, y+1), (x+1, y+1)]

    def animation(self):
        pass

    def draw(self, bitmap):
        for p in self._pixels:
            bitmap[p[0], p[1]] = self._color

    def pixels(self):
        return self._pixels

class Snake:
    def __init__(self, snake, direction, color):
        self._direction = direction
        self._snake = snake
        self._len = len(snake)
        self._color = color

    def set_direction(self, direction) -> None:
        self._direction = direction

    def increase_tail(self, units: int) -> None:
        self._len += units

    def eat_fruit(self, fruit: Fruit) -> bool:
        head = self._snake[0]
        return head in fruit.pixels()

    def animate(self) -> int:
        head = self._snake[0]
        # x
        x = self._direction[0] + head[0]
        # y
        y = self._direction[1] + head[1]
        if x < 0 or x >= SCREEN_WIDTH:
            return -1
        if y < 0 or y >= SCREEN_HEIGHT:
            return -1
        self._snake.insert(0, (x,y))
        if len(self._snake) > self._len:
            # Remove tail
            del self._snake[-1]
        return 0

    def draw(self, bitmap) -> None:
        for s in self._snake:
            bitmap[s[0], s[1]] = self._color

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
        display.auto_refresh = True
        display.rotation = 0
        display.show(group)

        return display

    def refresh(self):
        self._display.refresh()
        #print(dir(self._display))
        #print(dir(self._display.framebuffer))
        #xxx

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
        esp = bluepad32.ESP_SPIcontrol(
            spi, esp32_cs, esp32_ready, esp32_reset, debug=0)
        return esp

    def run(self):
        # esp.set_esp_debug(1)
        x = SCREEN_WIDTH // 2
        y = SCREEN_HEIGHT // 2

        snake0 = Snake([(x//2, y), (x//2+1,y), (x//2+2,y)], direction=(-1,0), color=1)
        fruit = Fruit(pos=(random.randint(0,SCREEN_WIDTH-2), random.randint(0,SCREEN_HEIGHT-2)))

        bitmap = self._display.get_bitmap()

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
                needs_update = True
                if dpad & 0x01: # Up
                    dir_x, dir_y = 0, -1
                elif dpad & 0x02: # Down
                    dir_x, dir_y = 0, 1
                elif dpad & 0x04: # Right
                    dir_x, dir_y = 1, 0
                elif dpad & 0x08: # Left
                    dir_x, dir_y = -1, 0
                else:
                    needs_update = False

                if needs_update:
                    snake0.set_direction((dir_x, dir_y))

            snake0.animate()

            # Clear screen + draw different sprites
            bitmap.fill(0)
            snake0.draw(bitmap)
            fruit.draw(bitmap)

            # check collision
            if snake0.eat_fruit(fruit):
                fruit = Fruit(pos=(random.randint(0,SCREEN_WIDTH-2), random.randint(0,SCREEN_HEIGHT-2)))
                snake0.increase_tail(1)

            self._display.refresh()
Game().run()