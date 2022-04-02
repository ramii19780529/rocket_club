import time
import board
from adafruit_motorkit import MotorKit

throttle = 1
seconds = 10

kit = MotorKit(i2c=board.I2C())

kit.motor1.throttle = throttle
kit.motor2.throttle = throttle
kit.motor3.throttle = throttle
kit.motor4.throttle = throttle

time.sleep(seconds)

kit.motor1.throttle = None
kit.motor2.throttle = None
kit.motor3.throttle = None
kit.motor4.throttle = None
