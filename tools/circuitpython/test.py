import time
import struct

# Get this file from BLUEPAD32_SRC/tools/circuitpython/
import bluepad32

import board
import busio
from digitalio import DigitalInOut
from micropython import const

# If you are using a board with pre-defined ESP32 Pins:
#esp32_cs = DigitalInOut(board.ESP_CS)
#esp32_ready = DigitalInOut(board.ESP_BUSY)
#esp32_reset = DigitalInOut(board.ESP_RESET)

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
esp32_cs = DigitalInOut(board.D10)
esp32_ready = DigitalInOut(board.D9)
esp32_reset = DigitalInOut(board.D6)

spi = busio.SPI(board.SCK, board.MOSI, board.MISO)
esp = bluepad32.ESP_SPIcontrol(spi, esp32_cs, esp32_ready, esp32_reset, debug=0)

# Optionally, to enable UART logging in the ESP32
esp.set_esp_debug(1)

# Should display "Bluepad32 for Airlift"
print('Firmware vers:', esp.firmware_version)

first_time = False
color = [0xff, 0x80, 0x20]
players_led = 0x01

while True:
    gamepads = esp.get_gamepads_data()

    for gp in gamepads:
        if first_time == False:
            first_time = True
            print(gp)
            resp = esp.set_gamepad_color_led(gp['idx'], color)
            print(resp)

        if gp['buttons'] & 0x01:
            # Shuffle colors. "random.shuffle" not preset in CircuitPython
            color = (color[2], color[0], color[1])
            rest = esp.set_gamepad_color_led(gp['idx'], color)
            print(rest)

        if gp['buttons'] & 0x02:
            rest = esp.set_gamepad_player_leds(gp['idx'], players_led)
            print(rest)
            players_led += 1
            players_led &= 0x0f

        if gp['buttons'] & 0x04:
            rest = esp.set_gamepad_rumble(gp['idx'], 255, 255)
            print(rest)

    time.sleep(0.032)