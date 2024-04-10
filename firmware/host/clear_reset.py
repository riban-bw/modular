import RPi.GPIO as GPIO # python3-rpi.gpio

RESET_PIN = 17
GPIO.setmode(GPIO.BCM)
GPIO.setup(RESET_PIN, GPIO.IN)
