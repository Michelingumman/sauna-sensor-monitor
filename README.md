# 🔥 Sauna Monitoring System with ESP32

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-green.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino-blue.svg)](https://www.arduino.cc/)

A smart monitoring system that tracks temperature and humidity in saunas, detects sessions automatically, and provides real-time visualization through an OLED display and optional web interface.

![Sauna Monitor Display](https://github.com/user-attachments/assets/5eeba7a8-1e52-4ab0-8149-8ff183ecbd70)
![Sauna Monitor Graph](https://github.com/user-attachments/assets/7d22766a-37de-4d0f-9adf-860cdadb0d28)

## 🔍 Overview

This ESP32-based system provides comprehensive monitoring for saunas, automatically detecting when sessions begin and end based on temperature changes. The device displays real-time data on an OLED screen and can optionally connect to Blynk for remote monitoring.

## ✨ Features

### Time Synchronization
- Automatically syncs with NTP servers for accurate time display
- Shows current time in format: `Monday 01-13:00`

### Intelligent Session Detection
- Automatically detects sauna sessions when temperature rises from 20°C to 30°C within 15 minutes
- Tracks session duration and highest temperature
- Detects session end when temperature drops to 30% of peak temperature

### Real-time Monitoring
- Updates temperature and humidity readings every 2 seconds
- Shows current readings with icons
- Sauna Session active indicator

### Connectivity
- Wi-Fi connectivity for time synchronization
- Built-in web server with real-time dashboard
- OTA (Over-the-Air) update capability

## 🛠️ Hardware Requirements

### Components
- **ESP32 Development Board**: Any standard ESP32 board
- **SSD1306 OLED Display**: I2C interface
- **SHT2x Temperature/Humidity Sensor**: I2C interface


### Connections
| Component | ESP32 Pin |
|-----------|-----------|
| OLED SDA  | GPIO 6    |
| OLED SCL  | GPIO 7    |
| SHT2x     | Same I2C bus |
| Power     | 3.3V      |

## 💻 Software Requirements

### Development Environment
- PlatformIO (recommended) or Arduino IDE
- Git (optional, for version control)

### Required Libraries
- Adafruit SSD1306
- Adafruit GFX
- SHT2x library
- AsyncTCP
- ESPAsyncWebServer
- ElegantOTA
- WiFi (built into ESP32 core)

## 📥 Installation & Setup

### 1. Clone or Download the Repository
```bash
git clone https://github.com/yourusername/sauna-sensor-monitor.git
cd sauna-sensor-monitor
```

### 2. Create secrets.h File
Create a file named `secrets.h` in the src directory with the following content:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "your_ssid"
#define WIFI_PASS "your_password"

#endif //SECRETS_H
```

> **⚠️ IMPORTANT**: Never commit the secrets.h file to version control. It contains sensitive information. Add it to .gitignore

### 3. Upload to ESP32
Using PlatformIO or Arduino IDE, compile and upload the code to your ESP32 board.

## ⚙️ How It Works

### Startup Sequence
1. ESP32 initializes and connects to WiFi using credentials from secrets.h
2. Synchronizes time with NTP servers and configures for correct timezone
3. Initializes the web server and OTA update capability
4. Begins monitoring temperature and humidity

### Main Operation Loop
- **Time Display**: Updates time at the top of the OLED once per minute
- **Sauna Detection**:
  - Monitors for temperature rise (20°C → 30°C within 15 minutes)
  - When detected, starts session timer and tracks peak temperature
  - Ends session when temperature drops below 30% of peak value
- **Data Visualization**:
  - Updates temperature and humidity readings every 10 seconds
  - Plots temperature as solid line and humidity as dotted line
  - Shows current values with icons

### Web Interface
- Provides a modern, responsive dashboard
- Displays real-time temperature and humidity
- Shows session status and historical data
- Enables OTA firmware updates

## 🔧 Configuration

The project can be configured by modifying the following parameters in main.cpp:

```cpp
// OLED display settings
#define SCREEN_WIDTH 128        // OLED display width
#define SCREEN_HEIGHT 64        // OLED display height
#define OLED_RESET    -1        // Reset pin
#define SCREEN_ADDRESS 0x3C     // I2C address
#define SDA_PIN 6               // SDA pin
#define SCL_PIN 7               // SCL pin

// Timing settings
const unsigned long WIFI_RETRY_INTERVAL = 60000;  // WiFi retry (1 min)
const unsigned long WIFI_CONNECT_TIMEOUT = 10000; // WiFi timeout (10 sec)
```

---

Built with ❤️ for enhanced sauna experiences.
