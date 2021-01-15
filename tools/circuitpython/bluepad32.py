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
# Bluepad32 support for CircuitPython.
# Requires the Bluepad32 firmware (instead of Nina-fw).

"""
`quico_bluepad32`
================================================================================

CircuitPython library for receiving gamepad commands from the Bluepad32 firmware
that runs in the ESP32 co-processor.


* Author(s): Ricardo Quesada

Implementation Notes
--------------------

**Hardware:**

**Software and Dependencies:**

* Adafruit ESP32SPI: https://github.com/adafruit/Adafruit_CircuitPython_ESP32SPI
"""

import struct

from adafruit_esp32spi import adafruit_esp32spi
from micropython import const


# Nina-fw commands stopped at 0x50. Bluepad32 extensions start at 0x60. See:
# https://github.com/adafruit/Adafruit_CircuitPython_ESP32SPI/blob/master/adafruit_esp32spi/adafruit_esp32spi.py
_GET_GAMEPADS_DATA = const(0x60)
_GET_GAMEPADS_PROPERTIES = const(0x61)
_SET_GAMEPAD_PLAYER_LEDS = const(0x62)
_SET_GAMEPAD_LIGHTBAR_COLOR = const(0x63)
_SET_GAMEPAD_RUMBLE = const(0x64)
_BLUETOOTH_DEL_KEYS = const(0x65)


class ESP_SPIcontrol(adafruit_esp32spi.ESP_SPIcontrol):
    """Implement the SPI commands for Bluepad32"""

    def get_gamepads_data(self) -> list:
        """
        Return a list of connected gamepads.

        Each gamepad entry is a dictionary that represents the gamepad state:
        gamepad index, buttons pressed, axis values, dpad and more.

        :returns: List of connected gamepads.
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

    def set_gamepad_player_leds(self, gamepad_idx: int, leds: int) -> bool:
        """
        Set the gamepad's player LEDs.

        Some gamepads have 4 LEDs that are used to indicate, among other things,
        the "player number".

        Applicable only to gamepads that have a player's LEDs like Nintendo Wii,
        Nintendo Switch, etc.

        :param int gamepad_idx: Gamepad index, returned by get_gamepads_data().
        :param int leds: Only the 4 LSB bits are used. Each bit indicates a LED.
        :return: True if the request was successful, False otherwise.
        """
        resp = self._send_command_get_response(
            _SET_GAMEPAD_PLAYER_LEDS, ((gamepad_idx,), (leds,))
        )
        return resp[0][0] == 1

    def set_gamepad_lightbar_color(self, gamepad_idx: int, rgb) -> bool:
        """
        Set the gamepad's lightbar color.

        Applicable only to gamepads that have a color LED like the Sony
        DualShok 4 or DualSense.

        :param int gamepad_idx: Gamepad index, returned by get_gamepads_data().
        :param tuple[int, int, int] rgb: Red,Green,Blue values to set.
        :return: True if the request was successful, False otherwise.
        """
        # Typing is not supported in CircuitPython. Parameter "rgb" should be:
        #  Typing.tuple[int, int, int]
        resp = self._send_command_get_response(
            _SET_GAMEPAD_LIGHTBAR_COLOR, ((gamepad_idx,), rgb)
        )
        return resp[0][0] == 1

    def set_gamepad_rumble(self, gamepad_idx: int, force: int, duration: int) -> bool:
        """
        Set the gamepad's rumble (AKA force-feedback).

        Applicable only to gamepads that have rumble support, like Xbox One,
        DualShok 4, Nintendo Switch, etc.

        :param int gamepad_idx: Gamepad index, returned by get_gamepads_data().
        :param int force: 8-bit value where 255 is max force, 0 nothing.
        :param int duration: 8-bit value, where 255 is about 1 second.
        :return: True if the request was successful, False otherwise.
        """
        resp = self._send_command_get_response(
            _SET_GAMEPAD_RUMBLE, ((gamepad_idx,), (force, duration))
        )
        return resp[0][0] == 1

    def bluetooth_del_keys(self) -> bool:
        """
        Delete stored Bluetooth keys.

        After establishing a Bluetooth connection, a key is saved in the ESP32.
        This is useful for a quick reconnect. Removing the keys requires to
        establish a new connection, something that might be needed in some
        circumstances.

        :return: True if the request was successful, False otherwise.
        """
        resp = self._send_command_get_response(_BLUETOOTH_DEL_KEYS)
        return resp[0][0] == 1
