# M5OSC
Sending sensor data from an M5 Stick C using Open Sound Control
![M5OSC Running on an M5 Stick C](img/2019-10-01-Two_Sticks_Running_M5OSC_different_IMUs.jpg?raw=true "M5OSC Running on an M5 Stick C")

Be sure to set your WiFi SSID, network password, IP Address, and port number in the arduino file!

The M5 Stick C has three different buttons built-in. This project detects short (< 0.5 seconds) and long (>= 0.5 seconds) presses of each button, and offers the following features:
  * Center button:
    * Short:  Cycle screen brightness (off, low, high)
    * Long: No function
  * Left PWR button:
    * Short: No function
    * Long:  No function
    * > 5s: Toggle power
  * Right secondary button:
    * Short: Toggle Turbo mode
    * Long:  Reset the IMU

## Installation
- [OSC](https://github.com/CNMAT/OSC) by Adrian Freed, Yotam Mann
	- Available in the Arduino Library Manager
- [ESP32 Board Libraries](https://docs.m5stack.com/#/en/arduino/arduino_development)
	- Requires adding a URL to the Arduino Board Manager
	- Follow the above referenced installation guide for the ESP32-based M5 Stick C
- [M5StickC Libraries](https://github.com/m5stack/M5StickC)
	- Adds support for features built into the M5StickC that are not standard to the ESP32
	- Install by cloning the M5StickC repository to your Arduino `libraries` folder
