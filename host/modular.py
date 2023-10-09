#!/usr/bin/python3
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
from time import sleep, monotonic
import json
import liblo # python3-liblo
import logging
import jack
from threading import Thread, Lock
from subprocess import run, Popen, DEVNULL, PIPE
from glob import glob
from shutil import rmtree

logging.basicConfig(level=logging.DEBUG)
RESET_PIN = 17 # RPi GPIO pin used to reset first modules
module_map = {} # Map of module config indexed by I2C address
sw2dest = {} # Map of destination, mapped by (mod_id, switch index)
sw2src = {} # Map of source, mapped by (module_id, switch index)
sw2toggle = {} # Map of [module index, toggle param], mapped by (mod_id, switch index)
selected_src = None # Currently selected source in format (panel i2c addr, output index)
selected_dst = None # Currently selected destination in format (panel i2c addr, input index)
gpi_values = {} # Map of gpi_values indexed by panel id (I2C address) in form [x,x,x...] where x is 16-bitwise values for each bank of GPI
routing = {} # Routing graph as list of (source module I2C address, source output index), indexed by destination
# For panels controlling multiple modules the source and destinations indicies are concatenated
uri = "osc.udp://localhost:2228"
module_count = 0 # Quantity of modules installed
universal_panels = [] # List of universal panel I2C addresses

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
    """ Sets LED primary colourupdatePending
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
        result = bus.read_i2c_block_data(addr, 0xF0, 3)
    except:
        return None
    return result[0] | (result[1] << 8)

def get_adc(addr, adc):
    """ Get ADC value
    
    addr - Panel I2C address
    adc - ADC index
    Returns - ADC value [0..1023] or 0 if ADCload_patch not found
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
    Returns - Tuple containing (parameter index, parameter value) or (0, 0) if no more parameters changed parameter not found or None on error 
    Note - (0,0) is returned after all changed parameters have been read even if one of those parameters has subsequently changed value
    """

    try:
        result = bus.read_i2c_block_data(addr, 0x00, 3)
        return (result[2], result[0] | (result[1] << 8))
    except:
        return None

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

    logging.debug(f"OSC Rx: {path} {args}")
    if path == "/resp/cable":
        cable_id = args[0]
        src_module = args[1]
        src_port = args[2]
        dst_module = args[3]
        dst_port = args[4]

def add_cable(src_module, src, dst_module, port):
    """Add and connect a cable
    
    src_module - UID of installed source module
    src - Index of source module's output 
    dst_module - UID of installed destination module
    port - INdex of destination module's input
    """
    
    liblo.send(uri, "/add_cable", ("h", src_module), ("i", src), ("h", dst_module), ("i", port))

def remove_cable_by_port(src_module_id, src, dst_module_id, port):
    """Disconnect and remove a cable
    
    src_module - UID of installed source module
    src - Index of source module's output 
    dst_module - UID of installed destination module
    port - Index of destination module's input
    """

    liblo.send(uri, "/remove_cable", ("h", src_module_id), ("i", src), ("h", dst_module_id), ("i", port))

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
    set_wsled_secondary(addr, led, LED_ON2, 0x00, 0x40, 0x00)

def configure_destination_led(addr, led):
    """ Configure a LED as a destination button
    
    addr - Panel I2C address
    led - LED index
    """

    set_wsled(addr, led, LED_OFF, 0x80, 0x00, 0xff)
    set_wsled_secondary(addr, led, LED_ON2, 0x10, 0x00, 0x40)

def config_toggle_led(addr, led):
    """ Configure a LED as discrete (toggle) value
    
    addr - Panel I2C address
    led - LED index
    """

    set_wsled(addr, led, LED_OFF, 0x80, 0x00, 0x00)
    set_wsled_secondary(addr, led, LED_OFF, 0x00, 0x00, 0x00)

def hw_reset():
    """ Reset all panels"""

    GPIO.setmode(GPIO.BCM)
    GPIO.setup(RESET_PIN, GPIO.OUT)
    GPIO.output(RESET_PIN, 0)
    sleep(0.1)
    GPIO.output(RESET_PIN, 1)
    #GPIO.setup(RESET_PIN, GPIO.IN, pull_up_down=GPIO.PUD_OFF)

def add_module(i2c_addr, mod_index, mod_cfg):
    global module_count
    
    module_count += 1
    module_map[i2c_addr][mod_index]["mod_id"] = module_count
    dst_n = 0
    src_n = 0
    toggle_n = 0
    pot_n = 0
    mod_id = mod_cfg["mod_id"]
    if "dests" in mod_cfg:
        for x,y in mod_cfg["dests"]:
            configure_destination_led(i2c_addr, x)
            sw2dest[(mod_id, y)] = dst_n
            dst_n += 1
    if "srcs" in mod_cfg:
        for x,y in mod_cfg["srcs"]:
            config_source_led(i2c_addr, x)
            sw2src[(mod_id, y)] = src_n
            src_n += 1
    if "toggles" in mod_cfg:
        for x,y,z in mod_cfg["toggles"]:
            for l in z:
                config_toggle_led(i2c_addr, l[0])
            sw2toggle[(mod_id, y)] = [mod_index, toggle_n]
            toggle_n += 1
    if "pots" in mod_cfg:
        pot_n += len(mod_cfg['pots'])
    module_map[i2c_addr][mod_index]["knob_values"] = [0]  * pot_n
    module_map[i2c_addr][mod_index]["switch_states"] = [0] * (dst_n + src_n + toggle_n)
    logging.debug(f"Found module: {mod_cfg['model']} at {i2c_addr} - assigning id {module_count}")

def init_module(i2c_addr, uid):
    """ Initialise rack and detect installed panels"""

    global module_map, universal_panels
    update = False
    with open("/home/dietpi/modular/config/panel.json", "r") as f:
        cfg = json.load(f)
    type = get_panel_type(i2c_addr)
    if str(type) in cfg:
        update = True
        module_map[i2c_addr] = cfg[str(type)]
        for mod_index, mod_cfg in enumerate(cfg[str(type)]):
            add_module(i2c_addr, mod_index, mod_cfg)
    elif type == 7:
        # Universal Panel
        universal_panels.append(i2c_addr)
        module_map[i2c_addr] = []
        logging.debug(f"Found universal panel at {i2c_addr}")
    logging.debug("Finished init")
    return update

def load_patch():
    patch = {"version": "2.1", "modules": [], "cables": []}
    for cfgs in module_map.values():
        for cfg in cfgs:
            patch["modules"].append({"id": cfg["mod_id"], "plugin": cfg["plugin"], "model": cfg["model"]})

    with open("/home/dietpi/patch.vcv", "w") as f:
        json.dump(patch, f)
    liblo.send(uri, "/load", ("s", "patch.vcv"))

def scan_modules():
    """Scan panels sensors and update model"""

    global module_count

    missing_modules = []

    for addr, cfgs in module_map.items():
        while(True):
            sleep(0.01)
            result = get_changed(addr)
            if result is None:
                missing_modules.append(addr)
                continue
            if result[0] == 0:
                # No more changed parameters so move to next panel
                break
            elif result[0] == 1:
                # Change of Universal Panel config
                logging.debug("Universal Panel changed")
                update = False
                result = bus.read_i2c_block_data(addr, 0x01, 8)
                result = bus.read_i2c_block_data(addr, 0x01, 8)
                module_map[addr] = []
                with open("/home/dietpi/modular/config/panel.json", "r") as f:
                    cfg = json.load(f)
                    for mod_index, type in enumerate(result):
                        if str(type) in cfg:
                            module_map[addr] += (cfg[str(type)])
                            for mod_cfg in cfg[str(type)]:
                                add_module(addr, mod_index, mod_cfg)
                                
                load_patch()

            elif result[0] < 0x10:
                # No commands defined for 1-15
                continue
            elif result[0] < 0x20:
                # GPI (in bank) changed
                bank = result[0] - 0x10
                gpi_flags = result[1] # Flags indicates which GPI have changed
                if gpi_flags:
                    gpis = get_gpis(addr, bank) # Actual GPI values
                    for cfg in cfgs:
                        for offset, last_val in enumerate(cfg['switch_states']):
                            switch = (result[0] - 0x10) * 16 + offset
                            value = gpis & 1
                            flag = gpi_flags & 1
                            if flag and value:
                                # Switch has been pressed#
                                mod_id = cfg["mod_id"]
                                if (mod_id, switch) in sw2toggle:
                                    toggle = sw2toggle[(mod_id, switch)][0]
                                    leds = cfg["toggles"][toggle][2]
                                    param = cfg["toggles"][toggle][0]
                                    state = int(cfg["switch_states"][switch]) + 1
                                    if state >= len(leds):
                                        state = 0
                                    cfg["switch_states"][switch] = state
                                    set_param(cfg["mod_id"], param, state)
                                    for led,val in leds:
                                        if val == state:
                                            set_wsled_mode(addr, led, LED_ON)
                                        else:
                                            set_wsled_mode(addr, led, LED_OFF)                                
                                elif (mod_id, switch) in sw2dest:
                                    select_destination(addr, sw2dest[(mod_id, switch)])
                                elif (mod_id, switch) in sw2src:
                                    select_source(mod_id, sw2src[(mod_id, switch)])
                            gpis >>= 1
                            gpi_flags >>= 1
            elif result[0] < 0x50:
                pot = result[0] - 0x20
                for cfg in cfgs:
                    if pot >= len(cfg["knob_values"]):
                        pot -= len(cfg["knob_values"])
                        continue
                    if cfg["knob_values"][pot] != result[1]:
                        cfg["knob_values"][pot] = result[1]
                        param = cfg["pots"][pot][0]
                        r = cfg["pots"][pot][2] - cfg["pots"][pot][1]
                        set_param(cfg["mod_id"], param, cfg["pots"][pot][1] + r * result[1] / 1023)
                        cfg['knob_values'][pot] = result[1]
    
    if missing_modules:
        load_patch()


def toggle_route(src_id, src_port, dst_id, dst_port):
    """ Toggle routing between specified source and destination

    src_id - Source module ID
    src_port - Source output index
    dst_id - Destination module ID
    dst_port - Destination output index
    """

    global routing
    connect = True

    mod_src = src_port
    mod_dst = dst_port
    src_id = None
    dst_id = None

    if src_id is None or dst_id is None:
        logging.WARNING("Failed to find source / destination to route")
        return

    if (dst_id, dst_port) in routing:
        # Already has a source routed
        connect = routing[(dst_id, dst_port)] != (src_id, src_port) # Make new connection if different from current routing
        # Remove current routing
        remove_cable_by_port(src_id, mod_src, dst_id, mod_dst)
        del routing[(dst_id, dst_port)]
    # Request to make new connection
    if connect:
        add_cable(src_id, mod_src, dst_id, mod_dst)
        routing[(dst_id, dst_port)] = (src_id, src_port,)

def refresh_routes():
    """ Update routing LEDs"""

    if selected_dst is None and selected_src is None:
        srcs = []
        # No selection so highlight currently routed ports
        port = 0
        for addr, cfgs in module_map.items():
            for cfg in cfgs:
                mod_id = cfg["mod_id"]
                for dest in cfg['dests']:
                    if (mod_id, port) in routing:
                        set_wsled_mode(addr, dest[0], LED_ON)
                        srcs.append(routing[(addr, port)])
                    else:
                        set_wsled_mode(addr, dest[0], LED_ON2)
                    port += 1
        port = 0
        for addr, cfgs in module_map.items():
            for cfg in cfgs:
                mod_id = cfg["mod_id"]
                for src in cfg['srcs']:
                    if (mod_id, port) in srcs:
                        set_wsled_mode(addr, src[0], LED_ON)
                    else:
                        set_wsled_mode(addr, src[0], LED_ON2)
                    port += 1
    elif selected_src is None:
        # Only destination selected so flash destination, highlight connected source and show available sources
        if (selected_dst[0], selected_dst[1]) in routing:
            routed_src = routing[(selected_dst[0], selected_dst[1])]
        else:
            routed_src = None
        for addr, cfgs in module_map.items():
            s_port = 0
            d_port = 0
            for cfg in cfgs:
                mod_id = cfg["mod_id"]
                for dest in cfg['dests']:
                    if selected_dst[0] == mod_id and d_port == selected_dst[1]:
                        set_wsled_mode(addr, dest[0], LED_FLASH)
                    else:
                        set_wsled_mode(addr, dest[0], LED_OFF)
                    d_port += 1
                for src in cfg['srcs']:
                    if (mod_id, s_port) == routed_src:
                        set_wsled_mode(addr, src[0], LED_ON)
                    else:
                        set_wsled_mode(addr, src[0], LED_ON2)
                    s_port += 1
    elif selected_dst is None:
        # Only source selected so flash source, highlight connected destinaton and show available destinations
        src_port = selected_src[1]
        for addr, cfgs in module_map.items():
            s_port = 0
            d_port = 0
            for cfg in cfgs:
                mod_id = cfg["mod_id"]
                for src in cfg['srcs']:
                    if mod_id == selected_src[0] and s_port == src_port:
                      set_wsled_mode(addr, src[0], LED_FLASH)
                    else:
                      set_wsled_mode(addr, src[0], LED_OFF)
                    s_port += 1
                for dest in cfg['dests']:
                    if (mod_id, d_port) in routing:
                        if selected_src == routing[(addr, d_port)]:
                            set_wsled_mode(addr, dest[0], LED_ON)
                        else:
                            set_wsled_mode(addr, dest[0], LED_OFF)
                    else:
                        set_wsled_mode(addr, dest[0], LED_ON2)
                    d_port += 1


def select_source(mod_id, src):
    """ Select a source for routing

    addr - Panel I2C address
    src -  Panel output index
    """

    global selected_src, selected_dst
    if selected_dst:
        # Destination already selected so toggle route
        toggle_route(mod_id, src, selected_dst[0], selected_dst[1])
        selected_src = None
        selected_dst = None
    elif selected_src == (mod_id, src):
        # Source already selected so deselect
        selected_src = None
    else:
        # elect source
        selected_src = (mod_id, src)
    refresh_routes()

def select_destination(mod_id, dst):
    """ Select a destination for routing

    addr - Panel I2C address
    dst -  Panel input index
    """

    global selected_src, selected_dst
    if selected_src:
        # Source already selected so toggle route
        toggle_route(selected_src[0], selected_src[1], mod_id, dst)
        selected_src = None
        selected_dst = None
    elif selected_dst == (mod_id, dst):
        # Destination already selected so deselect
        selected_dst = None
    else:
        # Select destination
        selected_dst = (mod_id, dst)
    refresh_routes()



def on_xrun(arg):
    global xcount
    xcount += 1
    logging.warning(f"xrun ({xcount}): {arg}")


def reset_xrun():
    global xcount
    xcount = 0

def connect_audio():
    """Connect Cardinal audio output to soundcard outputs"""

    try:
        client.connect("Cardinal:audio_out_1", "system:playback_1")
    except:
        pass
    try:
        client.connect("Cardinal:audio_out_2", "system:playback_2")
    except:
        pass

def connect_midi():
    """Connect MIDI input to Cardinal MIDI input"""

    src_ports = client.get_ports(is_midi=True, is_output=True)
    for port in src_ports:
        if port.name.startswith("Cardinal"):
            continue
        try:
            client.connect(port, "Cardinal:events-in")
        except:
            pass

def autoconnect_worker():
    while running:
        connect_audio()
        connect_midi()
        sleep(2)


def detect_modules():
    global mutex
    i2c_addr = 10
    uid = ""
    new_addrs = []
    while running:
        update = False
        uid = run(['/home/dietpi/modular/host/scan', str(i2c_addr)], stdout=PIPE, stderr=PIPE).stdout.decode('utf-8').split('\n')[0]
        sleep(1)
        if uid:
            new_addrs.append([i2c_addr, uid])
            i2c_addr += 1
        elif new_addrs:
            mutex.acquire()
            for i in new_addrs:
                update |= init_module(i[0], i[1])
            if update:
                load_patch()
            mutex.release()
            new_addrs = []
        else:
            sleep(5)

# Initiate OSC communication with rack
osc_server = liblo.ServerThread(2001)
osc_server.add_method(None, None, on_osc, None)
osc_server.start()

# Find installed panels and initiate rack
bus = smbus.SMBus(1)
hw_reset()
sleep(3)
module_map = {}
mutex = Lock()

#refresh_routes()
xcount = 0

# Jack client
client = jack.Client("riban")
client.set_xrun_callback(on_xrun)
client.activate()

running = True
detect_thread = Thread(target=detect_modules, daemon=True, name="Detect Modules")
detect_thread .start()
autoconnect_thread = Thread(name="Autoconnect", target=autoconnect_worker, daemon=True)
autoconnect_thread.start()

###############################################################################
# Start Cardinal
try:
    for dir in glob("/tmp/Cardinal*"):
        rmtree(dir)
except:
    pass
cardinal_proc = Popen(['/home/dietpi/Cardinal/bin/Cardinal'])#, stdout=DEVNULL, stderr=DEVNULL)

# Main program loop
while True:
    if cardinal_proc.poll() is not None:
        # Cardninal has terminated so let's restart it
        logging.warning("Restarting Cardinal")
        try:
            rmtree("/tmp/Cardinal*")
        except:
            pass
        cardinal_proc = Popen(['/home/dietpi/Cardinal/bin/Cardinal'])#, stdout=DEVNULL, stderr=DEVNULL)
        load_patch()
    mutex.acquire()
    scan_modules()
    mutex.release()
    #sleep(0.1)
