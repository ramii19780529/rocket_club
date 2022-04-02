# 28BYJ-48_ULN2003

# Raspberry Pi Schematics
# https://datasheets.raspberrypi.com/rpi4/raspberry-pi-4-reduced-schematics.pdf

# 28BYJ-48 Stepper Motor
# https://www.makerguides.com/wp-content/uploads/2019/04/28byj48-Stepper-Motor-Datasheet.pdf

# ULN2003 Stepper Motor Driver PCB
# https://www.makerguides.com/wp-content/uploads/2019/04/ULN2003-Stepper-Motor-Driver-PCB.pdf

# RPi.GPIO
# https://sourceforge.net/projects/raspberry-gpio-python/


import RPi.GPIO as GPIO
from time import sleep, time
import sys


def init_motor_phases(pins: tuple) -> tuple:
    """Initialize the motor and return the phases.

    Keyword arguments:
    pins -- The pins connected to the motor.
    """

    # Set GPIO to use the pin number on the RPi board.
    GPIO.setmode(GPIO.BOARD)

    # Set all of our pins to output mode.
    GPIO.setup(pins, GPIO.OUT)

    # The 28BYJ-48 motor uses 4 full step phases.
    # This variable is tuple of four tuples that we used to track what pin
    # should be set to high or low during each phase. The phases are listed
    # in the correct order to make the motor spin clockwise.
    phases = (
        (GPIO.HIGH, GPIO.HIGH, GPIO.LOW, GPIO.LOW),
        (GPIO.LOW, GPIO.HIGH, GPIO.HIGH, GPIO.LOW),
        (GPIO.LOW, GPIO.LOW, GPIO.HIGH, GPIO.HIGH),
        (GPIO.HIGH, GPIO.LOW, GPIO.LOW, GPIO.HIGH),
    )

    return phases


def run_rotations(rotations, motor_steps, gear_ratio, pins, step_time):
    """Run the motor for the specified number of rotations.

    Keyword arguments:
    rotations -- Set the number of times to rotate the motor.
    motor_steps -- Set the number of full steps per motor rotation.
    gear_ratio -- Set the number of motor rotations per output shaft rotation.
    pins -- Set the pins connected to the motor.
    step_time -- Set how long to wait between motor steps.
    """

    # Initialize the motor and phases.
    phases = init_motor_phases(pins)

    # Initialize a variable to keep track of how many steps we take.
    steps = 0

    # Calculate the number of steps required.
    stop_step = rotations * motor_steps * gear_ratio

    # Start timer.
    start_time = time()

    # Keep stepping through the phases until it is time to stop.
    while steps < stop_step:
        GPIO.output(pins, phases[steps % 4])
        steps += 1
        sleep(step_time)

    # Stop timer.
    stop_time = time()

    # Calculate and print how many seconds the motor was running.
    run_time = stop_time - start_time
    print(f"The motor took {run_time:2.2f} seconds to rotate {rotations} times.")

    # This returns the GPIO to its initial state.
    GPIO.cleanup()


def run_seconds(seconds, motor_steps, gear_ratio, pins, step_time):
    """Run the motor for the specified number of seconds.

    Keyword arguments:
    seconds -- Set the number of seconds to run the motor.
    motor_steps -- Set the number of full steps per motor rotation.
    gear_ratio -- Set the number of motor rotations per output shaft rotation.
    pins -- Set the pins connected to the motor.
    step_time -- Set how long to wait between motor steps.
    """

    # Initialize the motor and phases.
    phases = init_motor_phases(pins)

    # Initialize a variable to keep track of how many steps we take.
    steps = 0

    # Calculate the time to stop spinning the motor.
    stop_time = time() + seconds

    # Keep stepping through the phases until it is time to stop.
    while time() < stop_time:
        GPIO.output(pins, phases[steps % 4])
        steps += 1
        sleep(step_time)

    # Calculate the number of steps to complete one entire rotation.
    steps_per_output_shaft_rotation = motor_steps * gear_ratio

    # Calculate and print how many times the output shaft rotated.
    output_shaft_rotations = steps / steps_per_output_shaft_rotation
    print(
        f"The output shaft rotated {output_shaft_rotations:2.2f} times in {seconds} seconds."
    )

    # This returns the GPIO to its initial state.
    GPIO.cleanup()


def main():

    run_rotations(
        rotations=3,
        motor_steps=32,
        gear_ratio=64,
        pins=(31, 33, 35, 37),
        step_time=0.00186,
    )

    run_seconds(
        seconds=12,
        motor_steps=32,
        gear_ratio=64,
        pins=(31, 33, 35, 37),
        step_time=0.00186,
    )


if __name__ == "__main__":
    sys.exit(main())
