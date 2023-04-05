import smbus
import RPi.GPIO as GPIO
from time import sleep
import json

RESET_PIN = 17
module_map = {}
i2c = smbus.SMBus(1)

def read(addr, command, register=None):
    if register is not None:
        i2c.write_i2c_block_data(addr, command, list(int.to_bytes(register, 2, "little")))
    a = i2c.read_i2c_block_data(addr, command, 6)
    control = int.from_bytes(a[:2], 'little')
    value = int.from_bytes(a[2:], 'little')
    return [control, value]


def get_changed(addr):
    return read(addr, 1)


def get_value(addr, param):
    return read(addr, 2, param)


def set_value(addr, param, value):
    data = list(int.to_bytes(param, 2, "little")) + list(int.to_bytes(value, 4, "little"))
    i2c.write_i2c_block_data(addr, 3, data)


def reset():
    GPIO.output(RESET_PIN, 0)
    sleep(0.1)
    GPIO.output(RESET_PIN, 1)


def create_cardinal_json():
    pass


def init():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(RESET_PIN, GPIO.OUT)
    reset()
    sleep(1)
    pos = 0
    while True:
        try:
            x, type = read(0x77, 0)
        except:
            break
        module_map[pos] = type
        i2c.write_byte_data(0x77, 0xfe, 11 + pos)
        print(f"Configured module type {type} with I2C address {10+pos:02x}")
        sleep(0.1)
        pos += 1
    print("Finished init")


def run():
    x=0
    while True:
        a = read(11, 1)
        if a[0] == 4 and a[1] != x:
            x = a[1]
            print(f"Cap: {x})
        if a[0] == 2:
            print(f"Ana: {a[1]}")

