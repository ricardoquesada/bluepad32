# Copyright 2020 Ricardo Quesada
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Bluepad32 support for CircuitPython
# Requires the Bluepad32 firmware (instead of Nina-fw)

import struct

from adafruit_esp32spi import adafruit_esp32spi
from micropython import const


class ESP_SPIcontrol(adafruit_esp32spi.ESP_SPIcontrol):
    """Implements the SPI commands for Bluepad32"""

    # Nina-fw commands stopped at 0x50. Bluepad32 extensions start at 0x60
    # See: https://github.com/adafruit/Adafruit_CircuitPython_ESP32SPI/blob/master/adafruit_esp32spi/adafruit_esp32spi.py
    _GET_GAMEPADS_DATA = const(0x60)
    _GET_GAMEPADS_PROPERTIES = const(0x61)
    _SET_GAMEPAD_PLAYER_LEDS = const(0x62)
    _SET_GAMEPAD_COLOR_LED = const(0x63)
    _SET_GAMEPAD_RUMBLE = const(0x64)
    _BLUETOOTH_DEL_KEYS = const(0x65)

    def get_gamepads_data(self) -> list:
        """Returns a list of gamepads. Empty if no gamepad are connected.
        Each gamepad entry is a dictionary that represents that gamepad state
        like: buttons pressed, axis values, dpad and more.
        """
        resp = self._send_command_get_response(_GET_GAMEPADS_DATA)
        # Response has exactly one argument
        assert len(resp) == 1, "Invalid number of responses."
        arg1 = resp[0]
        gamepads = []
        total_gamepads = arg1[0]
        # Sanity check
        assert total_gamepads < 8, "Invalid number of gamepads"

        # TODO: Expose these gamepad constants to Python:
        # https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/src/main/uni_gamepad.h
        offset = 1
        for _ in range(total_gamepads):
            unp = struct.unpack_from("<BBiiiiiiHB", arg1, offset)
            gamepad = {
                "idx": unp[0],
                "dpad": unp[1],
                "axis_x": unp[2],
                "axis_y": unp[3],
                "axis_rx": unp[4],
                "axis_ry": unp[5],
                "brake": unp[6],
                "accelerator": unp[7],
                "buttons": unp[8],
                "misc_buttons": unp[9],
            }
            gamepads.append(gamepad)
            offset += 29
        return gamepads

    def set_gamepad_player_leds(self, gamepad_idx: int, leds: int) -> int:
        """TODO"""
        resp = self._send_command_get_response(
            _SET_GAMEPAD_PLAYER_LEDS, ((gamepad_idx,), (leds,))
        )
        return resp[0][0]

    def set_gamepad_color_led(self, gamepad_idx: int, rgb: tuple[int, int, int]) -> int:
        """TODO"""
        resp = self._send_command_get_response(
            _SET_GAMEPAD_COLOR_LED, ((gamepad_idx,), rgb)
        )
        return resp[0][0]

    def set_gamepad_rumble(self, gamepad_idx: int, force: int, duration: int) -> int:
        """TODO"""
        resp = self._send_command_get_response(
            _SET_GAMEPAD_RUMBLE, ((gamepad_idx,), (force, duration))
        )
        return resp[0][0]

    def bluetooth_del_keys(self) -> int:
        """TODO"""
        resp = self._send_command_get_response(_BLUETOOTH_DEL_KEYS)
        return resp[0][0]
