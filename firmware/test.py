import smbus
#import RPi.GPIO as GPIO
from time import sleep
from math import pi, acos

RESET = 0x7EF

I2C_ADDR = 100
led_mode = [0] * 8

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

def get_panel_info(pnlId):
    if pnlId < 1 or pnlId > 63:
        return
    bus.write_i2c_block_data(I2C_ADDR, 0, [0, 0, 0, 0])
    data = bus.read_i2c_block_data(I2C_ADDR, pnlId, 17)
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
    if data[0] == 2:
        return {"panel": data[1], "type": "ADC", "offset":data[2], "value": data[3] + (data[4] << 8)}
    elif data[0] == 3:
        return {"panel": data[1], "type": "SWITCH", "offset":data[2], "value": data[3]}
    return None

def get_ema_cutoff(fs, a):
     return fs / (2 * pi) * acos(1 - (a / (2 * (1 - a))))


def write_raw_can(pnlId, opcode, msg):
    x = (pnlId << 4) | opcode
    bus.write_i2c_block_data(I2C_ADDR, 0xFF, [x >> 8, x & 0xff] + msg)

def set_led_mode(pnlId, led, mode):
    if mode > 7:
         return
    global led_mode
    write_raw_can(pnlId, 1, [led, mode])
    led_mode[led] = mode

def set_led_colour(pnlId, led, r1, g1, b1, r2=None, g2=0, b2=0):
    if r2 is None:
        write_raw_can(pnlId, 1, [led, led_mode[led], r1, g1, b1])
    else:
        write_raw_can(pnlId, 1, [led, led_mode[led], r1, g1, b1, r2, g2, b2])


def scan():
    while True:
        try:
            a = get_changed_values()
            if a:
                print(a)
        except Exception as e:
            print(e)
            sleep(1)

def test_leds(num):
    for mode in range(8):
        for i in range(num):
            set_led_mode(1, i, mode)
        sleep(2)
