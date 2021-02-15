# AmadorUAVs Antenna Tracker/APS System

This repository forms the low-level control portion of AmadorUAVs' APS (Antenna
Positioning System). For the high level portion, see [azimuth](https://gitlab.com/amadoruavs/azimuth).

## Getting Started

The low-level control portion of the APS runs on the Zephyr RT/OS,
on top of an STM32F401CE (Black Pill) microcontroller. It uses the west
build system for dependency management.

To get started:
1. [Setup Zephyr and west](https://docs.zephyrproject.org/latest/getting_started/index.html).
2. Clone this repository.
3. `cd` into it and run `west init`.
4. Run `west update` when prompted.
5. Run `west build -b blackpill_f401ce` to build for the blackpill board. (This can technically be substituted later)
6. Run `west flash` to flash the program to the board.
  6a. If you are using an ST-Link and want to/have to flash it manually, `cd build/zephyr` and `st-flash write zephyr.bin 0x08000000`.

## MAVLink Compatibility
The APS implements
a minimal portion of the MAVLink Gimbal Protocol v2, allowing basic control
of the antenna tracker via MAVSDK and other MAVLink-enabled components.

Currently, the subset of commands implemented are:
- Heartbeat (obviously!)
- Gimbal connection protocol (req message, parameters, etc.) - note parameters are hardcoded
- Gimbal telemetry (device attitude status)
- Basic gimbal control (set device attitude - quaternion)
    - Note that RoI commands and more advanced gimbal control is not implemented
    - Also note that gimbal mode-setting (e.g. yaw-lock) is not implemented (TODO?)
- Other protocols (such as arm) are not implemented, attempting to call them will
  fail (and in MAVSDK's case stop the program)

## Layout
Most file names should be self explanatory.

## TODO
Some nice quality of life features:
- Implement more of the MAVLink protocl (maybe arm authorization? mode set? yaw lock?)
- Clean up the code base some more
- Clean up sensor calibration and improve it (especially mag)
