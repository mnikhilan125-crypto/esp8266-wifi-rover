# Wiring and Hardware Connections

This document describes the hardware pin assignments used by the ESP8266 Mars Rover firmware.

## 1. ESP8266 NodeMCU Pin Mapping

| Component | Signal | ESP8266 Pin | GPIO |
|---|---|---|---|
| MPU6050 | SDA | D2 | GPIO4 |
| BMP280 | SDA | D2 | GPIO4 |
| MPU6050 | SCL | D1 | GPIO5 |
| BMP280 | SCL | D1 | GPIO5 |
| L293D | IN1 — Left motor | D5 | GPIO14 |
| L293D | IN2 — Left motor | D6 | GPIO12 |
| L293D | IN3 — Right motor | D7 | GPIO13 |
| L293D | IN4 — Right motor | D8 | GPIO15 |
| DHT11 | DATA | D0 | GPIO16 |

## 2. I²C Sensors

The MPU6050 and BMP280 share the same I²C bus.

- Connect both sensors' SDA signals to D2.
- Connect both sensors' SCL signals to D1.
- Connect sensor power and ground according to the breakout boards' specifications.
- Use the web dashboard's I²C scanner to identify responding device addresses.

The firmware defaults to MPU6050 address `0x68` and BMP280 address `0x76`. The actual address depends on the sensor module and its configuration.

## 3. Motor Driver

The L293D receives four direction-control signals from the ESP8266.

- IN1 and IN2 control the left motor direction.
- IN3 and IN4 control the right motor direction.
- The firmware sets these pins to control forward, backward, turning, and stopping.

The L293D motor supply and logic supply must be wired according to the driver datasheet and motor requirements. Connect the ESP8266 and motor-driver grounds together.

## 4. DHT11 Sensor

Connect the DHT11 data signal to D0 (GPIO16). Connect power and ground according to the sensor module specifications. A bare DHT11 sensor may require a pull-up resistor on its data line.

## 5. Power and Safety

- Do not power the motors directly from ESP8266 GPIO pins.
- Use a suitable motor supply and check the L293D's current and voltage limits.
- Verify the pin connections and supply voltages before powering the rover.
- Avoid connecting 5 V signals directly to ESP8266 GPIO pins, which are designed for 3.3 V logic.
- Check the specific breakout-board documentation before connecting sensor power.

## 6. Verification

This pin map is derived from the firmware's pin definitions. Confirm the physical wiring against your actual rover before using it as a final circuit reference.
