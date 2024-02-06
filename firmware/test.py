import smbus
#import RPi.GPIO as GPIO
from time import sleep

RESET = 0x7EF

I2C_ADDR = 100
led_mode = [False] * 8

bus = smbus.SMBus(1)

def can_write(id, msg=[]):
    buffer = []
    buffer.append((id >> 8) & 0xff)
    buffer.append(id & 0xff)
    buffer += msg
    bus.write_i2c_block_data(I2C_ADDR, 0x01, buffer)

def get_num_panels():
     data = bus.read_i2c_block_data(I2C_ADDR, 0xf0, 1)
     return data[0]

def get_panel_info(id):
    if id < 1 or id > 63:
        return
    bus.write_i2c_block_data(I2C_ADDR, 1, [0, 0, 0, 0])
    data = bus.read_i2c_block_data(I2C_ADDR, id, 17)
    result = {}
    result["type"] = data[0]
    result["uuid"] = 0
    for i in range(12):
        result["uuid"] |= (data[i + 1] << (i * 8))
    result["version"] = 0
    for i in range(4):
        result["version"] |= (data[i + 13] << (i * 8))
    return result

def get_changed_values():
    data = bus.read_i2c_block_data(I2C_ADDR, 0x00, 9)
    if data[0] == 3:
        return {"panel": data[3], "type": "ADC", "offset":data[4], "value": data[1] + (data[2] << 8)}
    elif data[0] == 4:
        return {"panel": data[5], "type": "SWITCH", "value": data[1] + (data[2] << 8) + (data[3] << 16) + (data[4] << 24)}
    return None

def read_i2c():
    while bus.read_i2c_block_data(100, 0, 1)[0] == 0:
        pass
    a = bus.read_i2c_block_data(100, 1, 8)
    t = 0
    for i in range(4):
        t += (a[i] << (8 * i))
    return t


def set_led_mode(led, mode, rgb1=None, rgb2=None):
    if mode > 7:
         return
    global led_mode
    bus.write_i2c_block_data(100, 0xf0, [led, mode])
    led_mode[led] = mode

def set_led_colour(led, r1, g1, b1, r2=None, g2=None, b2=None):
    if r2 is None:
        bus.write_i2c_block_data(100, 0xf0, [led, led_mode[led], r1, g1, b1])
    else:
        bus.write_i2c_block_data(100, 0xf0, [led, led_mode[led], r1, g1, b1, r2, g2, b2])


while True:
    try:
            read_i2c() / 1000
    except Exception as e:
            print(e)
            sleep(1)

