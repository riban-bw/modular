""" riban modular
  
  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Python test program
"""

import smbus # python3-smbus:arm64
import RPi.GPIO as GPIO # python3-rpi.gpio
from time import sleep, monotonic
import json

RESET_PIN = 17
module_map = {} # Map of module config indexed by I2C address
i2c_addr = 0x77

bus = smbus.SMBus(1)

def set_target_address(addr):
    global i2c_addr
    bus.write_i2c_block_data(0x77, 0xfe, [addr])

def set_wsled_mode(led, mode):
    bus.write_i2c_block_data(i2c_addr, 0xF1, [led, mode])

def led_colour(led, red, green, blue):
    """ Sets LED colour after next mode set"""
    bus.write_i2c_block_data(i2c_addr, 0xF1, [led, red, green, blue])

def led_secondary_colour(led, red, green, blue):
    """ Sets LED secondary colour after next mode set"""
    bus.write_i2c_block_data(i2c_addr, 0xF2, [led, red, green, blue])

def set_wsled(led, mode, red, green, blue):
    bus.write_i2c_block_data(i2c_addr, 0xF1, [led, mode, red, green, blue])

def set_wsled_secondary(led, mode, red, green, blue):
    bus.write_i2c_block_data(i2c_addr, 0xF2, [led, mode, red, green, blue])

def led_off(led):
    set_wsled_mode(led, 0)

def led_on(led, secondary=False):
    if secondary:
        set_wsled_mode(led, 2)
    else:
        set_wsled_mode(led, 1)

def led_flash(led):
    set_wsled_mode(led, 3)

def led_flash_fast(led):
    set_wsled_mode(led, 4)

def led_pulse(led):
    set_wsled_mode(led, 5)

def led_pulse_fast(led):
    set_wsled_mode(led, 6)

def led_mute(mute):
    bus.write_i2c_block_data(i2c_addr, 0xF3, [mute])

def get_module_type():
    result = bus.read_i2c_block_data(i2c_addr, 0xF0, 3)
    return result[0] | (result[1] << 8)

def get_adc(adc):
    result = bus.read_i2c_block_data(i2c_addr, 0x20 + adc, 3)
    return result[0] | (result[1] << 8)

def get_gpis(bank):
    if bank < 16:
        result = bus.read_i2c_block_data(i2c_addr, 0x10 + bank, 3)
        return result[0] | (result[1] << 8)
    return 0

def get_gpi(gpi):
    bank = gpi // 16
    gpi = gpi % 16
    if bank > 15:
        return 0
    value = get_gpis(bank)
    return ((1 << gpi) & value == 1 << gpi)

def get_changed():
    result = bus.read_i2c_block_data(i2c_addr, 0x00, 3)
    return (result[2], result[0] | (result[1] << 8))

def reset():
    bus.write_i2c_block_data(i2c_addr, 0xff, [])

def read_changed():
    while(True):
        result = get_changed()
        if result[0]:
            if result[0] < 0x20:
                print(f"GPI {result[0] - 0x10} changed to {result[1]}")
            else:
                print(f"ADC {result[0] - 0x20} changed to {result[1]}")

def read_changed_gpi():
    while(True):
        result = get_changed()
        if result[0] == 0x10:
            print(result)

def read_adc(adc):
    b = get_adc(adc)
    print(b)
    while True:
        sleep(0.01)
        a = get_adc(adc)
        if abs(a-b) > 1:
            print(a)
            b = a

def read_gpi(gpi):
    b = get_gpi(gpi)
    print(b)
    while True:
        sleep(0.01)
        a = get_gpi(gpi)
        if a != b:
            print(a)
            b = a

def hw_reset():
    GPIO.output(RESET_PIN, 0)
    sleep(0.1)
    GPIO.output(RESET_PIN, 1)

def init_modules():
    global module_map, i2c_addr
    with open("/home/dietpi/.config/riban/panel.json", "r") as f:
        cfg = json.load(f)
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(RESET_PIN, GPIO.OUT)
    hw_reset()
    sleep(1)
    addr = 10
    module_map = {}
    print("Detecting attached modules:")
    while True:
        i2c_addr = 0x77
        try:
            type = get_module_type()
        except:
            break
        set_target_address(addr)
        if str(type) in cfg:
            module_map[addr] = cfg[str(type)]
            module_map[addr]["switch_states"] = []
            module_map[addr]["knob_values"] = []
            switch_n = 0
            if 'inputs' in cfg:
                switch_n += len(cfg['inputs'])
            if 'outputs' in cfg:
                switch_n += len(cfg['outputs'])
            if 'toggles' in cfg:
                switch_n += len(cfg['toggles'])
            module_map[addr]["switch_states"] = [0] * switch_n
            print(f"  0x{addr:02x}: {module_map[addr]}")
        sleep(1)
        addr += 1
    print("Finished init")

def scan_modules():
    global i2c_addr
    for i2c_addr, cfg in module_map.items():
        while(True):
            result = get_changed()
            if result[0] < 0x10:
                break
            if result[0] < 0x20:
                gpis = result[1]
                for switch, value in enumerate(cfg['switch_states']):
                    if value != gpis & 1:
                        cfg['switch_states'][switch] = gpis & 1
                        print(f"GPI {result[0] - 0x10} changed to {result[1]}")
                        #TODO: Handle change of switch state
                    gpis >>= 1
            elif result[0] < 0x50:
                if cfg["knob_values"] != result[1]:
                    cfg["knob_values"] = result[1]
                    #TODO: Handle change of knob value
                    print(f"ADC {result[0] - 0x20} changed to {result[1]}")

#init_modules()

