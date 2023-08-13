""" riban modular
  
  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with Foobar. If not, see <https://www.gnu.org/licenses/>.

  Python test program
"""

import smbus # python3-smbus:arm64
import RPi.GPIO as GPIO # python3-rpi.gpio
from time import sleep

RESET_PIN = 17
module_map = {}
i2c_addr = 0x77

bus = smbus.SMBus(1)

def set_target_address(addr):
    global i2c_addr
    bus.write_i2c_block_data(0x77, 0xfe, [addr])
    i2c_addr = addr

def set_wsled_mode(led, mode):
    bus.write_i2c_block_data(i2c_addr, 0xF1, [led, mode])

def set_wsled(led, mode, red, green, blue):
    bus.write_i2c_block_data(i2c_addr, 0xF2, mode, red, green, blue)

def led_off(led):
    set_wsled_mode(led, 0)

def led_on(led):
    set_wsled_mode(led, 1)

def led_flash(led):
    set_wsled_mode(led, 2)

def led_flash_fast(led):
    set_wsled_mode(led, 3)

def led_pulse(led):
    set_wsled_mode(led, 4)

def led_pulse_fast(led):
    set_wsled_mode(led, 5)

def get_module_type():
    result = bus.read_i2c_block_data(i2c_addr, 0xF0, 4)
    return result[0] + (result[1] << 8) + (result[2] << 16) + (result[3] << 24)

def get_adc(adc):
    result = bus.read_i2c_block_data(i2c_addr, adc, 4)
    return result[0] + (result[1] << 8) + (result[2] << 16) + (result[3] << 24)

def get_gpi(gpi):
    result = bus.read_i2c_block_data(i2c_addr, 32, 4)
    value = (result[0] + (result[1] << 8) + (result[2] << 16) + (result[3] << 24))
    return ((1 << gpi) & value == 1 << gpi)

def reset():
    bus.write_i2c_block_data(i2c_addr, 0xff, [])

def read_adc(adc):
    b = 0
    while True:
        a = get_adc(adc)
        if abs(a-b) > 1:
            print(a)
            b = a
        sleep(0.01)

def read_gpi(gpi):
    b = 0
    while True:
        a = get_gpi(gpi)
        if a != b:
            print(a)
            b = a
        sleep(0.01)

def reset():
    GPIO.output(RESET_PIN, 0)
    sleep(0.1)
    GPIO.output(RESET_PIN, 1)

def init():
    global module_map, i2c_addr
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(RESET_PIN, GPIO.OUT)
    reset()
    sleep(1)
    pos = 0
    module_map = {}
    while True:
        i2c_addr = 0x77
        try:
            type = get_module_type()
        except:
            break
        module_map[pos] = type
        set_target_address(10 + pos)
        print(f"Configured module type {type} with I2C address {10+pos:02x}")
        sleep(1)
        pos += 1
    print("Finished init")

init()

