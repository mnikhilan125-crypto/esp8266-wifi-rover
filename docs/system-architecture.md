# System Architecture

## 1. Overview

The Mars Rover uses an ESP8266 NodeMCU as its central controller. The ESP8266 hosts a web server and creates its own Wi-Fi network, allowing a smartphone or laptop to control the rover and view sensor readings through a browser.

The system consists of four main subsystems:

- **Communication:** ESP8266 Wi-Fi Access Point and HTTP web server.
- **Movement:** L293D motor driver and two DC geared motors.
- **Motion sensing:** MPU6050 accelerometer and gyroscope.
- **Environmental sensing:** DHT11 and BMP280 sensors.

## 2. System Workflow

1. The ESP8266 initializes the motor-control pins and stops the motors.
2. It loads previously saved sensor addresses from EEPROM.
3. It initializes the I²C bus and attempts to initialize the MPU6050 and BMP280.
4. It initializes the DHT11 sensor.
5. It creates the `MARS_ROVER` Wi-Fi network and starts the web server.
6. The user connects to the rover's network and opens `http://192.168.4.1`.
7. Directional controls send HTTP requests to the ESP8266.
8. The ESP8266 changes the motor-driver outputs according to the requested direction.
9. Sensor readings are updated periodically and returned to the dashboard as JSON data.
10. The I²C scanner can identify responding devices, and selected sensor addresses can be saved to EEPROM.

## 3. Communication Architecture

The system uses the following communication methods:

- **Wi-Fi:** Connects the phone or laptop to the ESP8266 access point.
- **HTTP:** Transfers movement commands and sensor data between the browser and the ESP8266 web server.
- **I²C:** Connects the MPU6050 and BMP280 to the ESP8266.
- **Digital GPIO:** Controls the L293D motor-driver inputs and receives the DHT11 data signal.
- **EEPROM:** Stores the configured MPU6050 and BMP280 I²C addresses across reboots.

## 4. Web Server Endpoints

| Endpoint | Purpose |
|---|---|
| `/` | Serves the rover dashboard |
| `/move?dir=forward` | Commands forward movement |
| `/move?dir=backward` | Commands backward movement |
| `/move?dir=left` | Commands a left turn |
| `/move?dir=right` | Commands a right turn |
| `/move?dir=stop` | Stops the motors |
| `/data` | Returns sensor readings and status as JSON |
| `/scan` | Scans the I²C bus for responding devices |
| `/setaddr?sensor=mpu&addr=0x68` | Configures and saves the MPU6050 address |
| `/setaddr?sensor=bmp&addr=0x76` | Configures and saves the BMP280 address |

The addresses shown in the examples are defaults; the actual addresses depend on the connected hardware.

## 5. Sensor Data Processing

The MPU6050 supplies acceleration and angular velocity along three axes. The firmware uses accelerometer readings to calculate approximate X and Y tilt angles.

The DHT11 provides temperature and humidity readings. The BMP280 provides temperature and pressure readings, from which the firmware estimates altitude using an assumed sea-level pressure.

Sensor readings are refreshed approximately every 500 milliseconds. The browser requests dashboard data approximately every 800 milliseconds.

## 6. Current System Limitations

- Movement is controlled manually through the web interface.
- Autonomous navigation and obstacle avoidance are not implemented in the supplied firmware.
- Tilt estimates may be affected by rover movement and vibration.
- Altitude accuracy depends on the sea-level pressure reference.
- A movement-command timeout should be added to improve safety if communication is interrupted.

