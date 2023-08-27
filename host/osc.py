import logging
import jack
import liblo
from time import sleep
import threading
from  subprocess import Popen,DEVNULL
import os

running = True


###############################################################################
# Start Cardinal

ENV = os.environ.copy()
ENV["JACK_NO_AUDIO_RESERVATION"] = "1"
jackd_proc = Popen(['jackd', '-dalsa', '-dhw:CODEC', '-Xraw', '-p128'], stdout=DEVNULL, stderr=DEVNULL, env=ENV)
sleep(1) #TODO: Check if jackd has started before progressing
cardinal_proc = Popen(['Cardinal'], stdout=DEVNULL, stderr=DEVNULL)

###############################################################################
# Jack routing and monitoring

xcount = 0

def on_xrun(arg):
    global xcount
    xcount += 1
    logging.warning(f"xrun ({xcount}): {arg}")


def reset_xrun():
    global xcount
    xcount = 0


client = jack.Client("riban-modular")
client.set_xrun_callback(on_xrun)
client.activate()

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

autoconnect_thread = threading.Thread(name="Autoconnect", target=autoconnect_worker, daemon=True)
autoconnect_thread.start()


###############################################################################
# OSC interface

uri = "osc.udp://localhost:2228"
modules = {} # Map of installed modules indexed by module id
addr2modules = {} # Map of module id indexed by I2C address (optimisation)
cables = {} # Map of cable connections indexed by cable id
models = [] # List of available module models
patches = [] # List of available patach (preset) vcv files
tmp_panel_addr = None # Holds panel I2C address during async add panel operation

def on_osc(path, args, types, src):
    """Handle received OSC message
    
    path - OSC path
    args - parameter values
    types - parameter types
    src - OSC client sender address
    """

    global models
    global modules
    global addr2modules
    global cables
    global tmp_panel_addr
    if path == "/resp/module":
        if args[0] not in modules:
            if args[0] < 3:
                tmp_panel_addr = args[0]
            modules[args[0]] = {"address": tmp_panel_addr}
            if tmp_panel_addr is not None:
                addr2modules[tmp_panel_addr] = args[0]
            tmp_panel_addr = None
            get_module_info(args[0])
    elif path == "/resp/remove_module":
        if args[0] in modules:
            del modules[args[0]]
    elif path == "/resp/module_info":
        if args[0] not in modules:
            modules[args[0]] = {}
        modules[args[0]]["plugin"] = args[1]
        modules[args[0]]["model"] = args[2]
        if "params" not in modules[args[0]]:
            modules[args[0]]["params"] = [{} for i in range(args[3])]
        if "inputs" not in modules[args[0]]:
            modules[args[0]]["inputs"] = [{} for i in range(args[4])]
        if "outputs" not in modules[args[0]]:
            modules[args[0]]["outputs"] = [{} for i in range(args[5])]
        if "lights" not in modules[args[0]]:
            modules[args[0]]["lights"] = [{} for i in range(args[6])]
        for i in range(args[3]):
            get_param_info(args[0], i)
        for i in range(args[4]):
            get_input_info(args[0], i)
        for i in range(args[5]):
            get_output_info(args[0], i)
        for i in range(args[6]):
            get_light_info(args[0], i)
    elif path == "/resp/param_info":
        try:
            modules[args[0]]["params"][args[1]]["name"] = args[2]
            modules[args[0]]["params"][args[1]]["unit"] = args[3]
            modules[args[0]]["params"][args[1]]["value"] = args[4]
            modules[args[0]]["params"][args[1]]["min_value"] = args[5]
            modules[args[0]]["params"][args[1]]["max_value"] = args[6]
            modules[args[0]]["params"][args[1]]["default_value"] = args[7]
            modules[args[0]]["params"][args[1]]["display_value"] = args[8]
            modules[args[0]]["params"][args[1]]["display_value_string"] = args[9]
            modules[args[0]]["params"][args[1]]["display_precision"] = args[10]
            modules[args[0]]["params"][args[1]]["description"] = args[11]
        except Exception as e:
            print(e)
    elif path == "/resp/input_info":
        try:
            modules[args[0]]["inputs"][args[1]]["name"] = args[2]
        except Exception as e:
            print(e)
    elif path == "/resp/output_info":
        try:
            modules[args[0]]["outputs"][args[1]]["name"] = args[2]
        except Exception as e:
            print(e)
    elif path == "/resp/light_info":
        try:
            modules[args[0]]["lights"][args[1]]["name"] = args[2]
            modules[args[0]]["lights"][args[1]]["description"] = args[3]
        except Exception as e:
            print(e)
    elif path == "/resp/cable":
        if args[0] not in cables:
            cables[args[0]] = {}
        cables[args[0]]["src_module"] = args[1]
        cables[args[0]]["output"] = args[2]
        cables[args[0]]["dst_module"] = args[3]
        cables[args[0]]["input"] = args[4]
    elif path == "/resp/remove_cable":
        if args[0] in cables:
            del cables[args[0]]
    elif path == "/resp":
        if args[0] == "clear" and args[1] == "ok":
            modules= {}
            cables = {}
        elif args[0] == "load" and args[1] == "ok":
            get_modules()
            get_cables()
    elif path == "/resp/model":
        models.append({"brand": args[0], "plugin": args[1], "model": args[2], "description": args[3], "manual": args[4], "tags": args[5]})
    elif path == "/resp/patch":
        patches.append(args[0])
    else:
        msg = f"Unhandled OSC Rx: path={path}"
        for i,val in enumerate(args):
            msg += (f" ({types[i]}, {val})")
        print(msg)


def clear():
    """Clear patch removing cables and modules"""

    liblo.send(uri, "/clear")


def get_patches():
    """Request list of patch (vcv) files"""

    global patches
    patches = []
    liblo.send(uri, "/get_patches")


def get_models():
    """Request list of module models"""

    global models
    models = []
    liblo.send(uri, "/get_models")


def filter_models(tags):
    """Get a list of models containing specified tags
    
    tags - space separated tags
    Returns - List of model structures
    TODO: This should probably be a list of tags to allow searching for tags with spaces
    """

    ret = []
    for model in models:
        for tag in tags.split():
            if tag in model["tags"]:
                ret.append(model)
    return ret

def get_modules():
    """Request list of installed modules
    This will also populate modules dictionary
    """
    
    global modules
    modules = {}
    liblo.send(uri, "/get_modules")

def get_module_info(module_id):
    """Request info for a module
    
    module_id - UID of installed module
    """
    liblo.send(uri, "/get_module_info", ("h", module_id))


def get_input_info(module_id, input):
    """Request info about an input
    
    module_id - UID of installed module
    input - index of input
    """
    
    liblo.send(uri, "/get_input_info", ("h", module_id), ("i", input))


def get_output_info(module_id, output):
    """Request info about an output
    
    module_id - UID of installed module
    output - index of output
    """
    
    liblo.send(uri, "/get_output_info", ("h", module_id), ("i", output))


def get_light_info(module_id, light):
    """Request info about a light
    
    module_id - UID of installed module
    light - index of light
    Note - Many modules do not populate this info
    """
    
    liblo.send(uri, "/get_light_info", ("h", module_id), ("i", light))


def add_module(plugin, model):
    """Add a module
    
    plugin - name of plugin
    model - name of module model
    """

    liblo.send(uri, "/add_module", ("s", plugin), ("s", model))


def remove_module(module_id):
    """Remove a module
    
    module_id - UID of installed module
    """
    
    liblo.send(uri, "/remove_module", ("h", module_id))


def get_cables():
    """Request list of all connected cables
    Note - Also populates cables structure
    """
    
    global cables
    cables = {}
    liblo.send(uri, "/get_cables")


def get_input_cables(module_id, input):
    """Request list of cables connected to an input
    
    module_id - UID of installed module
    input - Index of input
    """
    
    liblo.send(uri, "/get_input_cables", ("h", module_id), ("i", input))


def get_output_cables(module_id, output):
    """Request list of cables connected to an output
    
    module_id - UID of installed module
    input - Index of output
    """
    
    liblo.send(uri, "/get_output_cables", ("h", module_id), ("i", output))


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


def get_param(module_id, index):
    """Request a value of a parameter
    module_id - UID of installed module
    index - index of parameter
    """
    
    liblo.send(uri, "/param", ("h", module_id), ("i", index))


def get_param_info(module_id, index):
    """Request info about a parameter
    
    module_id - UID of installed module
    index - index of parameter
    """
    
    liblo.send(uri, "/get_param_info", ("h", module_id), ("i", index))


def load(filename):
    """Load a patch from file
    filename - patch filename including extension but excluding path
    Note - file must be in home directory
    """
    
    liblo.send(uri, "/load", ("s", filename))




def help():
    print("Cardinal OSC interface")
    print("======================")
    print("clear() - Remove all modules\n\tResponse: /resp: 'clear','ok'|'fail'")
    print("get_patches() - Populate the patches list from on-disk files")
    print("get_models() - Populate the list of available modular models")
    print("get_modules() - Request list of modules\n\tResponse: /resp/module: module_id (one message per installed module)")
    print("add_module(plugin, model) - Add a module to the rack\n\tResponse: /resp/module: module_id")
    print("remove_module(module_id) - Remove a module from the rack\n\tResponse: None")
    print("get_module_info(module_id) - Request info for a module\n\tResponse: /resp/module_info: module_id, plugin_name, model_name, num_params, num_inputs, num_outputs, num_lights")
    print("get_cables() - Request all cables\n\tResponse: /resp/cable: src_module_id, output, dst_module_id, input (one message for each cable)") 
    print("get_input_cables(module_id, input) - Request cables connected to module input\n\tResponse: /resp/cable: src_module_id, output, dst_module_id, input (one message per connected cable)")
    print("get_output_cables(module_id, output) - Request cables connected to module input\n\tResponse: /resp/cable: src_module_id, output, dst_module_id, input (one message per connected cable)")
    print("add_cable(src_module_id, output, dst_module_id, input) - Add and connect a cable\n\tResponse: /resp/cable: src_module_id, output, dst_module_id, input")
    print("remove_cable(cable_id) - Disconnect and remove a cable\n\tResponse: None")
    print("remove_cable_by_port(src_module_id, output, dst_module_id, input) - Disconnect and remove a cable\n\tResponse: /resp/remove_cable: cable_id")
    print("set_param(module_id, index, value) - Set the value of a parameter\n\tResponse: None")
    print("get_param(module_id, index) - Request the value of a parameter\n\tResponse: /resp/param module_id, index, value")
    print("get_param_info(module_id, index) - Request info about a parameter\n\tResponse: /resp/param_info module_id, param_id, name, unit, value, minValue, maxValue, defaultValue, displayValue, DisplayValueString, displayPrecision, description")
    print("load(filename) - Load a patch from file\n\tResponse: /resp 'load', 'ok'|'fail'")
    

osc_server = liblo.ServerThread(2001)
osc_server.add_method(None, None, on_osc, None)
osc_server.start()

def get_working_models():
    get_models()
    broken_models = [["Fundamental", "Quantizer"]]

    for i in models:
        clear()
        if i['plugin'] == 'Cardinal' or " " in i['plugin'] or " " in i['model'] or [i['plugin'], i['model']] in broken_models:
            continue
        add_module(i['plugin'], i['model'])
        sleep(2)
        if not modules:
            (i['plugin'], i['model'])

###############################################################################
# Start of core app

import json

module_db = {} # Maps panel UID to Cardinal plugin,model
selected_source = None # [module, source] of currently selected source
selected_destination = None # [module, source] of currently selected destination

def load_module_db(filename="config/panel.json"):
    """Load panel / plugin,model mapping databse
    
    filename - Filename of json file including path
    """
    
    global module_db
    with open(filename) as f:
        data = json.load(f)
    module_db = {int(k): v for k, v in data.items()}


def on_add_panel(addr, id):
    """Handle a hardware panel added to device
    
    addr - panel I2C address
    id - panel UID / type
    """

    global tmp_panel_addr
    tmp_panel_addr = addr
    try:
        plugin = module_db[id]["plugin"]
        model = module_db[id]["model"]
        add_module(plugin, model)
    except:
        pass

def on_remove_panel(addr):
    """Handle a hardware panel removed from device
    
    addr - panel I2C address
    """

    try:
        module_id = addr2modules.pop(addr)
        remove_module(module_id)
    except:
        pass

def toggle_cable(src_module, output, dst_module, input):
    """Add or remove a cable between module ports
    
    src_module - UID of installed source module
    output - Index of source module's output
    dst_module - UID of installed destination module
    input - Index of destination module's input
    """

    for id, cable in cables.items():
        if cable["src_module"] == src_module and cable["output"] == output and cable["dst_module"] == dst_module and cable["input"] == input:
            # Found existing cable so remove
            remove_cable(id)
            return
    add_cable(src_module, output, dst_module, input)

def on_route_touch(addr, button):
    """Handle press of panel routing button

    addr - panel I2C address
    button - index of routing button
    """

    global selected_source, selected_destination
    try:
        module_id = addr2modules[addr]
        module_info = modules[module_id]
        num_inputs = len(module_info['inputs']) 
        num_outputs = len(module_info['outputs']) 
        if button < num_inputs:
            # Input selected
            if selected_source is None:
                # No destination selected so select source
                if selected_destination == (module_id, button):
                    selected_destination = None
                else:
                    selected_destination = (module_id, button)
                #TODO: Update indications
            else:
                toggle_cable(selected_source[0], selected_source[1], module_id, button)
                selected_source = None
                selected_destination = None
        else:
            button -= num_inputs
            if num_outputs < button:
                # Invalid button
                return
            # Output selected
            if selected_destination is None:
                # No source selected so select destination
                if selected_source == (module_id, button):
                    selected_source = None
                else:
                    selected_source = (module_id, button)
                #TODO: Update indications
            else:
                toggle_cable(module_id, button, selected_destination[0], selected_destination[1])
                selected_source = None
                selected_destination = None
    except Exception as e:
        logging.warning(e)

def on_control_change(addr, control, value):
    """Handle change of control value
    
    addr - panel I2C addresss
    control - control index
    value - control value
    """

    try:
        module_id = addr2modules[addr]
        module_info = modules[module_id]
        num_params = len(module_info['params']) 
        if control < num_params:
            set_param(module_id, control, value)
    
    except Exception as e:
        logging.warning(e)



logging.warning("Hello riban modular")
load("riban-template.vcv")
load_module_db("config/panel.json")
sleep(2)

# Add VCO
on_add_panel(10, 1)
sleep(1) #TODO: Use a transaction to check module is added
# Add VCF
on_add_panel(11, 3)
sleep(1) #TODO: Use a transaction to check module is added
# Add VCA
on_add_panel(12, 4)
sleep(1) #TODO: Use a transaction to check module is added
# Add ADSR
on_add_panel(14, 9)
sleep(1) #TODO: Use a transaction to check module is added

# Route VCO to VCF
on_route_touch(10, 6)
on_route_touch(11, 3)
sleep(1)

# Route VCF to output
on_route_touch(11, 1)
on_route_touch(1, 0)
sleep(1)

while True:
    if cardinal_proc.poll() is not None:
        # Cardninal has terminated so let's restart it
        cardinal_proc = Popen(['Cardinal'], stdout=DEVNULL, stderr=DEVNULL)
    if jackd_proc.poll() is not None:
        # jackd has terminated so let's restart it
        #TODO: This does not work - need more robust handling of jackd failure
        jackd_proc = Popen(['jackd', '-dalsa', '-dCODEC', '-Xraw', '-p128'], stdout=DEVNULL, stderr=DEVNULL)

    print(">")
    cmd = input()
    try:
        if cmd.lower().startswith('q'):
            break
        elif cmd.lower().startswith('h') or cmd == "?":
            print("Commands:")
            print("quit: Quit")
            print("help: Help")
            print("?: Help")
            print("modules: List installed modules")
            print("models,brand: Lists models by brand")
            print("+x,y: Add a panel with I2C address x of type y")
            print("-x: Remove a panel with I2C address x")
            print("x,y: Push button y on panel with I2C address x")
            print("x,y,z: Set parameter y on panel with I2C address x to value z")
        elif cmd.startswith("+"):
            x, y = cmd[1:].split(",")
            addr = int(x)
            id = int(y)
            on_add_panel(addr, id)
        elif cmd.startswith("-"):
            addr = int(cmd[1:])
            on_remove_panel(addr)
        elif cmd.startswith("modules"):
            for addr,module_id in addr2modules.items():
                module = modules[module_id]
                print(f"{addr}: {module['plugin']} : {module['model']}")
        elif cmd.startswith("models,"):
            load_module_db()
            get_models()
            brand = cmd.split(',')[1]
            for model in models:
                if brand.lower() in model['brand'].lower():
                    print(f"{model['plugin']}: {model['model']} - {model['description']}")
        else:
            try:
                x,y,z = cmd.split(",")
                addr = int(x)
                param = int(y)
                value = float(z)
                on_control_change(addr, param, value)
            except:
                x,y = cmd.split(",")
                addr = int(x)
                button = int(y)
                on_route_touch(addr, button)
    except Exception as e:
        logging.warning(e)

running = False
#autoconnect_thread.join()

cardinal_proc.terminate()
client.deactivate()
sleep(1)
jackd_proc.terminate()
sleep(1)
