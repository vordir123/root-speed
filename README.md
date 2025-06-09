# ESP32 Speed Manipulator & CAN Logger

## Overview
This project provides firmware for an ESP32‑WROOM‑32 module and a Teensy 4.1. The ESP32 reads a reed sensor to calculate wheel speed, outputs a manipulated speed via PWM, and hosts a web dashboard. The Teensy logs CAN messages to an SD card and forwards important messages to the ESP32.

## Hardware Connections
### ESP32‑WROOM‑32
```
Reed sensor      -> GPIO4 (INPUT_PULLUP)
PWM out to dash  -> GPIO21 (LEDC CH0)
Status LED       -> GPIO2
UART RX/TX       -> GPIO16/GPIO17 (to Teensy)
```
### Teensy 4.1
```
CAN transceiver   -> CAN1 pins
SD card           -> CS on pin 10
UART to ESP32     -> Serial1 (pins 0/1)
Status LED        -> LED_BUILTIN
```
Power both boards with a stable 5 V supply. The reed sensor requires a pull‑up to 3.3 V.

## Setup
1. Install the libraries listed in `libraries.txt` using the Arduino IDE Library Manager or from GitHub.
2. Open `ESP32-wroom-32_SpeedManipulator.ino` in Arduino IDE and configure your Wi‑Fi credentials in `setup()`.
3. Open `Teensy41_CANLogger.ino` in Arduino IDE. Select the correct board and upload.
4. After both boards boot, connect to the ESP32 IP address in a browser to access the dashboard.

## Usage
* The dashboard shows real and manipulated speed values and lets you adjust the scale and offset.
* The Teensy continuously logs all CAN traffic to the SD card with rotating log files.
* Serial communication between boards allows forwarding of filtered CAN data.


