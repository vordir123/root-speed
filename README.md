# ESP32 Speed Manipulator & CAN Logger

## Overview
This repository contains two Arduino sketches and a small web dashboard. The
ESP32‑WROOM‑32 monitors a reed sensor on a bicycle wheel, calculates the real
speed and outputs a manipulated speed signal via PWM. A Teensy 4.1 captures CAN
traffic and stores it on an SD card while forwarding selected messages to the
ESP32.

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
CAN transceiver  -> CAN1 (e.g. MCP2562)
SD card CS       -> pin 10
UART to ESP32    -> Serial1 (pins 0/1)
Status LED       -> LED_BUILTIN
```
Power both boards from a stable 5 V supply. The reed sensor must be pulled up to
3.3 V.

## Setup
1. Install the libraries listed in `libraries.txt` using the Arduino IDE Library
   Manager.
2. Open `ESP32-wroom-32_SpeedManipulator.ino` and flash it to the board. Wi‑Fi
   credentials and other parameters can be set from the dashboard and are stored
   in NVS flash.
3. Open `Teensy41_CANLogger.ino`, select the Teensy 4.1 board and compile.
4. After flashing both boards, open a browser and navigate to the ESP32 IP
   address to access the dashboard.

## Dashboard
The dashboard shows the current real and manipulated speed, Wi‑Fi signal,
available heap and system uptime. A graph displays the last minute of speed
data and incoming CAN messages are listed in real time. All configuration
values including scaling, wheel size, PWM parameters and Wi‑Fi credentials can
be edited in the browser and saved to NVS flash on the ESP32.

Calibration mode outputs 15 km/h for 30 seconds. Enter the value shown on the
vehicle speedometer to adjust the offset automatically.

## CAN Logging
The Teensy stores every CAN frame as CSV files on the SD card. Logs rotate every
minute or when the file size exceeds 5 MB. Frames that differ strongly from the
running average are forwarded to the ESP32 over the UART connection.

