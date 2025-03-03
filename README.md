# Sauna Monitoring with ESP32

This project monitors temperature and humidity in a sauna, logs data, and displays 
information on an SSD1306 OLED. It uses NTP for time synchronization, 
includes logic to detect when the sauna turns on/off, and optionally integrates with Blynk.


<img src="https://github.com/user-attachments/assets/bb26ba26-0b00-46c6-a9fa-7dae553f842b" style="width:50%;"/>

--------

## Features
#### 1. Time Display (Once per Minute)
- Fetches the current local time via NTP and displays it in the format Monday 01-13:00 at the top of the OLED screen.

#### 2. Sauna Session Detection
- If the temperature rises from 20°C to 30°C within 15 minutes, the sauna is considered on, and a session start time is recorded.
During the session, the code tracks the highest temperature.
- If the temperature drops to 30% of that highest recorded value, the sauna is considered off, and the session ends.
Data Logging & Graphing (Every 10 Seconds)

#### 3. Reads temperature and humidity, storing them in arrays.
- Plots the temperature as a solid line and humidity as dotted pixels on the OLED.
- Displays the latest temp/hum values at the bottom of the screen.

#### 4. Blynk Integration (Optional)
- Connects to Blynk using your credentials in secrets.h, allowing remote monitoring (not shown on the OLED but available if needed).

--------

## Hardware Setup
#### 1. ESP32 Board
- Any standard ESP32 development board.

#### 2. SSD1306 OLED Display
- I2C address 0x3C (adjust if your display differs).
- SDA → GPIO 6, SCL → GPIO 7 in the code (change to match your specific board).

#### 3. Temperature/Humidity Sensor
- Code placeholders reference something like an SHT sensor.
- Update readTemperature() and readHumidity() with actual library calls to your sensor.

#### 4. Wiring
- OLED → 3.3V, GND, SDA, SCL (as above).
- Sensor → 3.3V, GND, and appropriate data pins.

----------

## Software Requirements
#### 1. PlatformIO (or Arduino IDE)

#### 2. Libraries
- Adafruit SSD1306
- Adafruit GFX
- Blynk Library (optional for remote monitoring)
- Any library for your temperature/humidity sensor (if needed)


> [!IMPORTANT]
> Make sure to never expose your WIFI's SSID or password and make sure to keep all these in the secrets.h file
- secrets.h File:
  ```cpp
  
    #ifndef SECRETS_H
    #define SECRETS_H
    
    #define BLYNK_TEMPLATE_ID "your_templade_id"
    #define BLYNK_TEMPLATE_NAME "your_template_name"
    #define BLYNK_AUTH_TOKEN "your_blynk_token"
    
    #define WIFI_SSID "your_ssid"
    #define WIFI_PASS "your_password"
    
    #endif //SECRETS_H
  ```

  -------

## How It Works

#### 1. Startup
- The ESP32 connects to Wi-Fi using the credentials in secrets.h.
- It fetches the current time from NTP servers and configures the correct time zone (e.g., Sweden with CET/CEST).

#### 2. Main Loop
- Once per minute: Prints the time (e.g., Monday 01-13:00) at the top of the OLED.

- Sauna logic:
   - Monitors temperature for a rapid rise from 20°C → 30°C within 15 minutes to detect “sauna on.”
   - Tracks highest temperature during the session.
   - If the temperature drops below 30% of that highest value, it detects “sauna off.”
     
- Every 10 seconds: Logs temperature/humidity, updates the graph (solid line for temp, dotted line for humidity), and shows the latest values on the OLED.

#### 3. Serial Output
- Logs messages like “Sauna session started!” and “Sauna session ended. Duration (ms): ...” for debugging.

- 
  
