""" riban modular
  
  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Python test program
"""

import smbus # python3-smbus
import RPi.GPIO as GPIO # python3-rpi.gpio
from time import sleep
import json
import liblo # python3-liblo
import logging

RESET_PIN = 4 # RPi GPIO pin used to reset first modules
module_map = {} # Map of module config indexed by I2C address
sw2input = {}
sw2output = {}
sw2toggle = {}
sw2led = {}
i2c_addr = 0x77
selected_src = None
selected_dst = None
routing = {} # Routing graph as list of sources, indexed by destination

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

def get_module_type(addr):
    result = bus.read_i2c_block_data(addr, 0xF0, 3)
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
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(RESET_PIN, GPIO.OUT)
    GPIO.output(RESET_PIN, 0)
    sleep(0.1)
    GPIO.output(RESET_PIN, 1)
    GPIO.setup(RESET_PIN, GPIO.IN, pull_up_down=GPIO.PUD_OFF)

def init_modules():
    global module_map, i2c_addr, sw2input, sw2output, sw2toggle, sw2led
    patch = {"version": "2.1", "modules": [], "cables": []}
    with open("/home/dietpi/modular/config/panel.json", "r") as f:
        cfg = json.load(f)
    hw_reset()
    sleep(0.1)
    addr = 10
    module_map = {}
    print("Detecting attached modules:")
    while True:
        try:
            type = get_module_type(0x77)
        except:
            break
        set_target_address(addr)
        if str(type) in cfg:
            mod_cfg = cfg[str(type)]
            patch["modules"].append({"id": addr, "plugin": mod_cfg["plugin"], "model": mod_cfg["model"]})
            i2c_addr = addr
            module_map[addr] = cfg[str(type)]
            switch_n = 0
            pot_n = 0
            if "inputs" in module_map[addr]:
                for x,y in module_map[addr]["inputs"]:
                    set_wsled(x, 0, 0x00, 0x00, 0xff)
                    set_wsled_secondary(x, 2, 0x00, 0x00, 0x30)
                    sw2input[switch_n] = x
                    sw2led[switch_n] = y
                    switch_n += 1
            if "outputs" in module_map[addr]:
                for x,y in module_map[addr]["outputs"]:
                    set_wsled(x, 0, 0x00, 0xff, 0x00)
                    set_wsled_secondary(x, 2, 0x00, 0x30, 0x00)
                    sw2output[switch_n] = x
                    sw2led[switch_n] = y
                    switch_n += 1
            if "toggles" in module_map[addr]:
                for x,y,z in module_map[addr]["toggles"]:
                    set_wsled(x, 0, 0x80, 0x80, 0x00)
                    set_wsled_secondary(x, 0, 0x00, 0x00, 0x00)
                    sw2toggle[switch_n] = z
                    sw2led[switch_n] = y
                    switch_n += 1
            if "pots" in module_map[addr]:
                pot_n += len(module_map[addr]['pots'])
            module_map[addr]["knob_values"] = [0]  * pot_n
            module_map[addr]["switch_states"] = [0] * switch_n
        sleep(1)
        addr += 1
    with open("/home/dietpi/patch.vcv", "w") as f:
        json.dump(patch, f)
    liblo.send(uri, "/load", ("s", "patch.vcv"))
    print("Finished init")

def scan_modules():
    global i2c_addr
    for i2c_addr, cfg in module_map.items():
        while(True):
            result = get_changed()
            if result[0] == 0:
                # No more changed parameters so move to next panel
                break
            elif result[0] < 0x10:
                # No commands defined for 1-15
                continue
            elif result[0] < 0x20:
                # GPI (bank) changed
                gpis = result[1]
                for offset, value in enumerate(cfg['switch_states']):
                    switch = (result[0] - 0x10) * 16 + offset
                    if value != gpis & 1:
                        # GPI has changed state
                        if switch in sw2toggle:
                            if module_map[i2c_addr]["switch_states"][switch] == 0:
                                module_map[i2c_addr]["switch_states"][switch] = 1
                            else:
                                module_map[i2c_addr]["switch_states"][switch] = 1
                            set_param(i2c_addr, sw2toggle[switch], module_map[i2c_addr]["switch_states"][switch])
                        elif value and switch in sw2input:
                            if selected_src == (i2c_addr, sw2input[switch]):
                                selected_src = None
                            else:
                                selected_src = (i2c_addr, sw2input[switch])
                            #TODO: Show routing
                        elif switch in sw2output:
                            if selected_dst == (i2c_addr, sw2input[switch]):
                                selected_dst = None
                            else:
                                selected_dst = (i2c_addr, sw2input[switch])
                            #TODO: Show routing
                    gpis >>= 1
            elif result[0] < 0x50:
                if cfg["knob_values"] != result[1]:
                    cfg["knob_values"] = result[1]
                    pot = result[0] - 0x20
                    param = cfg["pots"][pot]
                    set_param(i2c_addr, param, result[1] / 1024)
                    cfg['knob_values'][pot] = result[1]
                    #print(f"ADC {result[0] - 0x20} changed to {result[1]}")


def toggle_route(src, dst):
    """ Toggles routing between selected source and destination

    src - List of (source module I2C address, source output index)
    dst - List of (destination module I2C address, destination output index)
    """

    global routing
    connect = True
    if dst in routing:
        # Already has a source routed
        connect = routing[dst] != src # Make new connection if different from current routing
        # Remove current routing
        remove_cable_by_port(routing[dst][0], routing[dst][1], dst[0], dst[1])
        del routing[dst]
    # Request to make new connection
    if connect:
        add_cable(src[0], src[1], dst[0], dst[1])
        routing[dst] = (src[0], src[1])

###############################################################################
# OSC interface

uri = "osc.udp://localhost:2228"
module2addr = {} # Map of I2C address indexed by module id (optimisation)
cables = {} # Map of cable connections indexed by cable id
models = [] # List of available module models
patches = [] # List of available patach (preset) vcv files

def on_osc(path, args, types, src):
    """Handle received OSC message
    
    path - OSC path
    args - parameter values
    types - parameter types
    src - OSC client sender address
    """

    logging.warning(f"OSC Rx: {path} {args}")
    global models
    global modules
    global addr2modules
    global cables
    if path == "/resp/cable":
        cable_id = args[0]
        src_module = args[1]
        src_port = args[2]
        dst_module = args[3]
        dst_port = args[4]

def add_cable(src_module, output, dst_module, input):
    """Add and connect a cable
    
    src_module - UID of installed source module
    output - Index of source module's output 
    dst_module - UID of installed destination module
    input - INdex of destination module's input
    """
    
    liblo.send(uri, "/add_cable", ("h", src_module), ("i", output), ("h", dst_module), ("i", input))

def remove_cable(cable_id):
    """Disconnect and remove a cable
    
    cable_id - UID of installed cable
    """
    
    liblo.send(uri, "/remove_cable", ("h", cable_id))

def remove_cable_by_port(src_module_id, output, dst_module_id, input):
    """Disconnect and remove a cable
    
    src_module - UID of installed source module
    output - Index of source module's output 
    dst_module - UID of installed destination module
    input - INdex of destination module's input
    """

    liblo.send(uri, "/remove_cable", ("h", src_module_id), ("i", output), ("h", dst_module_id), ("i", input))

def set_param(module_id, index, value):
    """Set a parameter value
    
    module_id - UID of installed module
    index - index of parameter
    value - value to set parameter (float)
    """
    
    liblo.send(uri, "/param", ("h", module_id), ("i", index), ("f", value))


