import serial
from time import sleep, strftime
from math import pi, acos

led_mode = [0] * 16

port = serial.Serial("/dev/ttyS0", 1000000)

def tx(buffer):
    """ Convert buffer into COBS encoding and send to serial USART port"""
    data = [0] + buffer + [0]
    ptr = 0
    for i in range(1, len(buffer) + 2):
        if data[i] == 0:
            data[ptr] = i - ptr
            ptr = i
    port.write(bytearray(data))

def write_raw_can(pnlId, opcode, msg):
    """ Send CAN message to serial USART port"""
    x = (pnlId << 4) | opcode
    data = [x >> 8, x & 0xff] + msg
    checksum = -sum(data) & 0xff
    tx(data + [checksum])

def scan():
    """ Display messages received on serial USART port"""
    while True:
        buffer = []
        while True:
            buffer.append(ord(port.read()))
            if buffer[-1] == 0:
                break
        nextZero = buffer[0]
        for i in range(len(buffer)):
            if i == nextZero:
                nextZero = i + buffer[i]
                buffer[i] = 0
        data = buffer[1:-1]
        if -sum(data) & 0xff:
            print(f"Checksum error: {data}")
            continue
        if data[1] == 2:
            print(f"{strftime('%Y-%m-%d %H:%M:%S')}: Panel {data[2]} ADC {data[3] + 1}: {data[4] + (data[5] << 8)}")
        elif data[1] == 3:
            print(f"{strftime('%Y-%m-%d %H:%M:%S')}: Panel {data[2]} Switch {data[3] + 1}: {data[4]}")

def get_ema_cutoff(fs, a):
    """ Get the cutoff frequency of EMA filter
        fs - Sample frequency
        a - EMA factor (0..1)
    """
    return fs / (2 * pi) * acos(1 - (a / (2 * (1 - a))))

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

def test_leds(num):
    for mode in range(8):
        for i in range(num):
            set_led_mode(1, i, mode)
        sleep(2)
