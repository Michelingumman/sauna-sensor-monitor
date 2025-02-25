#include <Arduino.h>
#include <Wire.h>
#include <SHTSensor.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_SSD1306.h>

#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "secrets.h"

#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include <BlynkSimpleEsp32.h>


SHTSensor sht;
char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;



// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // Some boards use reset pin, -1 if not used
#define SCREEN_ADDRESS 0x70  // Most SSD1306 I2C displays use 0x3C

// Initialize OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup()
{
  // Debug console
  Serial.begin(9600);

  Wire.begin(21, 22);  // SDA, SCL pins

    // Initialize SSD1306
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      Serial.println(F("SSD1306 allocation failed"));
      for (;;); // Don't proceed if display initialization fails
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("Hello!");
  display.display();  // Update the screen


  // WiFi connection Stuff
  WiFi.setHostname("SaunaSensor");  // Set custom device name

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");

  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP()); // Print IP address to Serial Monitor


  // Starting Blynk instance
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}
  
void loop()
{
  Blynk.run();
}
