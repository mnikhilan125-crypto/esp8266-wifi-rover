# 🚀 Mars Rover — ESP8266-Based Rover with Live Sensor Dashboard

A Wi-Fi-controlled mobile rover built using the **ESP8266 NodeMCU**, featuring browser-based movement controls and real-time sensor monitoring. The rover hosts its own web dashboard in Access Point (AP) mode, allowing users to control movement and monitor motion and environmental data directly from a smartphone or laptop without requiring an external Wi-Fi router.

## Overview

The system integrates an L293D motor driver with two DC geared motors for differential-drive movement, an MPU6050 accelerometer and gyroscope for motion sensing, a DHT11 sensor for temperature and humidity, and a BMP280 sensor for atmospheric pressure, temperature, and estimated altitude.

The ESP8266 hosts an interactive web interface that displays sensor readings and provides directional controls. An integrated I²C scanner helps identify connected devices, while configurable sensor addresses are saved to EEPROM so they persist across reboots.

**Key features**
- Wi-Fi-based rover control through a web browser
- Standalone Access Point mode
- Live motion and environmental sensor dashboard
- Two-motor differential-drive control
- I²C device scanning and sensor-address configuration
- EEPROM-based persistence of sensor addresses

**Access:** Connect to the rover's Wi-Fi network and open `http://192.168.4.1` in a browser.

