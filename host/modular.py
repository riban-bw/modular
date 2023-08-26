""" riban modular
  
  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Core host application
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
selected_src = None
selected_dst = None
gpi_values = {} # Map of gpi_values indexed by panel id (I2C address) in form [x,x,x...] where x is 16-bitwise values for each bank of GPI
routing = {} # Routing graph as list of sources, indexed by destination
uri = "osc.udp://localhost:2228"

###############################################################################
# I2C interface
###############################################################################

LED_OFF         = 0
LED_ON          = 1
LED_ON2         = 2
LED_FLASH       = 3
LED_FLASH_FAST  = 4
LED_PULSE       = 5
LED_PULSE_FAST  = 6

def change_panel_address(old_addr, new_addr):
    """ Set Panel I2C address at 0x77
    
    old_addr - Current I2C address
    new_addr - New I2C address
    Returns - True on success
    """

    try:
        bus.write_i2c_block_data(old_addr, 0xfe, [new_addr])
    except:
        return False
    return True

def set_wsled_mode(addr, led, mode):
    """ Set LED mode
    
    addr - Panel I2C address
    led - LED index
    mode - Mode [LED_OFF | LED_ON | LED_ON2 | LED_FLASH | LED_FLASH_FAST | LED_PULSE | LED_PULSE_FAST]
    """

    try:
        bus.write_i2c_block_data(addr, 0xF1, [led, mode])
    except:
        pass

def led_colour(addr, led, red, green, blue):
    """ Sets LED primary colour

    addr - Panel I2C address
    led - LED index
    red - Red intensity [0..255]
    green - Green intensity [0..255]
    blue - Blue intensity [0..255]
    Note - Colour changes after next mode set
    """

    try:
        bus.write_i2c_block_data(addr, 0xF1, [led, red, green, blue])
    except:
        pass

def led_secondary_colour(addr, led, red, green, blue):
    """ Sets LED secondary colour

    addr - Panel I2C address
    led - LED index
    red - Red intensity [0..255]
    green - Green intensity [0..255]
    blue - Blue intensity [0..255]
    Note - Colour changes after next mode set
    """

    try:
        bus.write_i2c_block_data(addr, 0xF2, [led, red, green, blue])
    except:
        pass

def set_wsled(addr, led, mode, red, green, blue):
    """ Sets LED mode and primary colour

    addr - Panel I2C address
    led - LED index
    mode - Mode [LED_OFF | LED_ON | LED_ON2 | LED_FLASH | LED_FLASH_FAST | LED_PULSE | LED_PULSE_FAST]
    red - Red intensity [0..255]
    green - Green intensity [0..255]
    blue - Blue intensity [0..255]
    Note - Colour changes after next mode set
    """

    try:
        bus.write_i2c_block_data(addr, 0xF1, [led, mode, red, green, blue])
    except:
        pass

def set_wsled_secondary(addr, led, mode, red, green, blue):
    """ Sets LED mode and secondary colour

    addr - Panel I2C address
    led - LED index
    mode - Mode [LED_OFF | LED_ON | LED_ON2 | LED_FLASH | LED_FLASH_FAST | LED_PULSE | LED_PULSE_FAST]
    red - Red intensity [0..255]
    green - Green intensity [0..255]
    blue - Blue intensity [0..255]
    Note - Colour changes after next mode set
    """

    try:
        bus.write_i2c_block_data(addr, 0xF2, [led, mode, red, green, blue])
    except:
        pass

def led_mute(addr, mute):
    """ Temporarily turn off all LEDs

    addr - Panel I2C address
    mute - True to extinguish LEDs or false to restore to previous mode and colour
    """

    try:
        bus.write_i2c_block_data(addr, 0xF3, [mute])
    except:
        pass

def get_panel_type(addr):
    """ Get panel type
    
    addr - Panel I2C address
    Returns - Panel type id or None if no panel found
    """

    try:
        result = bus.read_i2c_block_data(addr, 0xF0, 3)
    except:
        return None
    return result[0] | (result[1] << 8)

def get_adc(addr, adc):
    """ Get ADC value
    
    addr - Panel I2C address
    adc - ADC index
    Returns - ADC value [0..1023] or 0 if ADC not found
    """

    try:
        result = bus.read_i2c_block_data(addr, 0x20 + adc, 3)
    except:
        return 0
    return result[0] | (result[1] << 8)

def get_gpis(addr, bank, force=True):
    """ Get bank of GPI values

    addr - Panel I2C address
    bank - GPI bank [0..15]
    force - True to foce an I2C read or false to read from cache (Default: True)
    Returns - 16-bitwise GPI values or 0 if bank not found
    """

    global gpi_values
    if bank < 16:
        try:
            if force:
                result = bus.read_i2c_block_data(addr, 0x10 + bank, 3)
                gpi_values[(addr, bank)] = result[0] | (result[1] << 8)
            return gpi_values[(addr, bank)]
        except:
            pass
    return 0

def get_gpi(addr, gpi, force=False):
    """ Get GPI value
    
    addr - Panel I2C address
    gpi - GPI index
    force - True to force an I2C read (Default: Read from cache)
    Returns - GPI value [0|1] or 0 if GPI not found
    """

    bank = gpi // 16
    gpi = gpi % 16
    if bank > 15:
        return 0
    value = gpi_values(addr, bank, force)
    return ((1 << gpi) & value == 1 << gpi)

def get_changed(addr):
    """ Get the next changed parameter from panel

    addr - Panel I2C address
    Returns - Tuple containing (parameter index, parameter value) or (0, 0) if no more parameters changed or parameter not found
    Note - (0,0) is returned after all changed parameters have been read even if one of those parameters has subsequently changed value
    """

    try:
        result = bus.read_i2c_block_data(addr, 0x00, 3)
        return (result[2], result[0] | (result[1] << 8))
    except:
        return (0, 0)

def reset(addr):
    """ Reset panel
    
    addr - Panel I2C address
    """

    bus.write_i2c_block_data(addr, 0xff, [])

###############################################################################
# OSC interface
###############################################################################

def on_osc(path, args, types, src):
    """Handle received OSC message
    
    path - OSC path
    args - parameter values
    types - parameter types
    src - OSC client sender address
    """

    logging.warning(f"OSC Rx: {path} {args}")
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
    input - Index of destination module's input
    """

    liblo.send(uri, "/remove_cable", ("h", src_module_id), ("i", output), ("h", dst_module_id), ("i", input))

def set_param(module_id, index, value):
    """Set a parameter value
    
    module_id - UID of installed module
    index - index of parameter
    value - value to set parameter (float)
    """
    
    liblo.send(uri, "/param", ("h", module_id), ("i", index), ("f", value))

###############################################################################
# General Functions
###############################################################################

def config_source_led(addr, led):
    """ Configure a LED as a source button
    
    addr - Panel I2C address
    led - LED index
    """

    set_wsled(addr, led, LED_OFF, 0x00, 0xff, 0x00)
    set_wsled_secondary(addr, led, LED_ON2, 0x00, 0x30, 0x00)

def configure_destination_led(addr, led):
    """ Configure a LED as a destination button
    
    addr - Panel I2C address
    led - LED index
    """

    set_wsled(addr, led, LED_OFF, 0x00, 0x00, 0xff)
    set_wsled_secondary(addr, led, LED_ON2, 0x00, 0x00, 0x30)

def config_toggle_led(addr, led):
    """ Configure a LED as a destination button
    
    addr - Panel I2C address
    led - LED index
    """

    set_wsled(addr, led, LED_OFF, 0x80, 0x80, 0xff)
    set_wsled_secondary(addr, led, LED_OFF, 0x00, 0x00, 0x00)

def hw_reset():
    """ Reset first panel"""

    GPIO.setmode(GPIO.BCM)
    GPIO.setup(RESET_PIN, GPIO.OUT)
    GPIO.output(RESET_PIN, 0)
    sleep(0.1)
    GPIO.output(RESET_PIN, 1)
    GPIO.setup(RESET_PIN, GPIO.IN, pull_up_down=GPIO.PUD_OFF)

def init_modules():
    """ Initialise rack and detect installed panels"""

    global module_map, sw2input, sw2output, sw2toggle, sw2led
    patch = {"version": "2.1", "modules": [], "cables": []}
    with open("/home/dietpi/modular/config/panel.json", "r") as f:
        cfg = json.load(f)
    hw_reset()
    addr = 10
    module_map = {}
    logging.debug("Detecting attached modules:")
    while True:
        sleep(0.1) # Wait for panel to initialise
        if addr > 10:
            type = get_panel_type(0x77)
            if type is None:
                break
            change_panel_address(0x77, addr)
        else:
            type = 1
        if str(type) in cfg:
            mod_cfg = cfg[str(type)]
            patch["modules"].append({"id": addr, "plugin": mod_cfg["plugin"], "model": mod_cfg["model"]})
            module_map[addr] = cfg[str(type)]
            switch_n = 0
            pot_n = 0
            if "inputs" in module_map[addr]:
                for x,y in module_map[addr]["inputs"]:
                    configure_destination_led(addr, x)
                    sw2input[switch_n] = x
                    sw2led[switch_n] = y
                    switch_n += 1
            if "outputs" in module_map[addr]:
                for x,y in module_map[addr]["outputs"]:
                    config_source_led(addr, x)
                    sw2output[switch_n] = x
                    sw2led[switch_n] = y
                    switch_n += 1
            if "toggles" in module_map[addr]:
                for x,y,z in module_map[addr]["toggles"]:
                    config_toggle_led(addr, x)
                    sw2toggle[switch_n] = z
                    sw2led[switch_n] = y
                    switch_n += 1
            if "pots" in module_map[addr]:
                pot_n += len(module_map[addr]['pots'])
            module_map[addr]["knob_values"] = [0]  * pot_n
            module_map[addr]["switch_states"] = [0] * switch_n
        addr += 1
    with open("/home/dietpi/patch.vcv", "w") as f:
        json.dump(patch, f)
    liblo.send(uri, "/load", ("s", "patch.vcv"))
    logging.debug("Finished init")

def scan_modules():
    """Scan panels and update model"""

    for addr, cfg in module_map.items():
        while(True):
            result = get_changed(addr)
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
                            if module_map[addr]["switch_states"][switch] == 0:
                                module_map[addr]["switch_states"][switch] = 1
                            else:
                                module_map[addr]["switch_states"][switch] = 1
                            set_param(addr, sw2toggle[switch], module_map[addr]["switch_states"][switch])
                        elif value and switch in sw2input:
                            if selected_src == (addr, sw2input[switch]):
                                selected_src = None
                            else:
                                selected_src = (addr, sw2input[switch])
                            #TODO: Show routing
                        elif switch in sw2output:
                            if selected_dst == (addr, sw2input[switch]):
                                selected_dst = None
                            else:
                                selected_dst = (addr, sw2input[switch])
                            #TODO: Show routing
                    gpis >>= 1
            elif result[0] < 0x50:
                pot = result[0] - 0x20
                if cfg["knob_values"][pot] != result[1]:
                    cfg["knob_values"][pot] = result[1]
                    param = cfg["pots"][pot][0]
                    r = cfg["pots"][pot][2] - cfg["pots"][pot][1]
                    set_param(addr, param, cfg["pots"][pot][1] + r * result[1] / 1024)
                    cfg['knob_values'][pot] = result[1]
                    #print(f"ADC {result[0] - 0x20} changed to {result[1]}")


def toggle_route(src, dst):
    """ Toggle routing between specified source and destination

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

# Initiate OSC communication with rack
osc_server = liblo.ServerThread(2001)
osc_server.add_method(None, None, on_osc, None)
osc_server.start()

# Find installed panels and initiate rack
bus = smbus.SMBus(1)
init_modules()

add_cable(11,0,10,0)
# Main program loop
while True:
    scan_modules()