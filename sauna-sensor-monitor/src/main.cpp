/*************************************************************
  Includes
*************************************************************/
#include <Arduino.h>
#include "images.h"         // For OLED display images
#include "secrets.h"         // Contains WIFI_SSID, WIFI_PASS, BLYNK_AUTH_TOKEN

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "SHT2x.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>     // Change from ElegantOTA to AsyncElegantOTA

#include <time.h>            // For NTP time synchronization

/*************************************************************
  Definitions
*************************************************************/
#define DEMO_PIN  18            // Example pin to toggle (currently unused)

// OLED display settings
#define SCREEN_WIDTH 128        // OLED display width, in pixels
#define SCREEN_HEIGHT 64        // OLED display height, in pixels
#define OLED_RESET    -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C     // I2C address for OLED display
#define SDA_PIN 6              // OLED SDA pin
#define SCL_PIN 7              // OLED SCL pin

/*************************************************************
  Global Objects
*************************************************************/
SHT2x sht;                      // For SHT temperature/humidity sensors
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AsyncWebServer server(80);      // Web server for OTA updates and file access

// State variables
bool pinState = false;          // Tracks toggling state
bool wifi_connected = false;
unsigned long last_wifi_attempt = 0;

// OTA update variables
unsigned long ota_progress_millis = 0;

// Timing settings
const unsigned long WIFI_RETRY_INTERVAL = 60000;  // Try to reconnect WiFi every minute
const unsigned long WIFI_CONNECT_TIMEOUT = 10000; // 10 seconds timeout for WiFi connection
unsigned long lastTimePrint = 0;
unsigned long lastLogTime = 0;

// Sauna state variables
bool saunaActive = false;          // True if sauna session is active
bool crossed20   = false;          // True if we've crossed 20°C
unsigned long timeCrossed20 = 0;   // When we first crossed 20°C
unsigned long saunaStartTime = 0;  // When the sauna session started
float highestTempDuringSession = 0.0;

// Logging and plotting variables
#define LOG_INTERVAL 10000UL
#define MAX_POINTS 128
float tempLog[MAX_POINTS];
float humLog[MAX_POINTS];
int logIndex = 0;

/*************************************************************
  FUNCTION Declarations
*************************************************************/
// OTA callback functions
void onOTAStart();
void onOTAProgress(size_t current, size_t final);
void onOTAEnd(bool success);

// WiFi and server functions
void check_wifi_connection();            // Check and attempt to reconnect WiFi
void setup_web_server();                 // Setup web server routes

// Display and sensor functions
void draw();
void printLocalTime(void);
float readTemperature(void);
float readHumidity(void);

// Application logic
void updateSaunaState(float currentTemp);
void checkAndPrintTime(void);
void logAndDisplayData(float temp, float hum);

/*************************************************************
  OTA CALLBACK IMPLEMENTATIONS
*************************************************************/
/**
 * Called when OTA update begins
 */
void onOTAStart() {
  Serial.println("OTA update started!");
  // Show update status on display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("OTA Update Started");
  display.display();
}

/**
 * Called periodically during OTA update process
 */
void onOTAProgress(size_t current, size_t final) {
  // Log progress every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress: %u of %u bytes (%.1f%%)\n", 
                 current, final, (current * 100.0) / final);
    
    // Update progress on display
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("OTA Update Progress:");
    display.printf("%.1f%%\n", (current * 100.0) / final);
    
    // Draw progress bar
    int barWidth = (current * 100) / final;
    display.drawRect(14, 30, 100, 10, SSD1306_WHITE);
    display.fillRect(14, 30, barWidth, 10, SSD1306_WHITE);
    display.display();
  }
}

/**
 * Called when OTA update completes
 */
void onOTAEnd(bool success) {
  if (success) {
    Serial.println("OTA update completed successfully!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("OTA Update Complete!");
    display.println("Rebooting...");
    display.display();
  } else {
    Serial.println("Error during OTA update!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("OTA Update Failed!");
    display.display();
  }
}

/*************************************************************
  Web Server Setup
*************************************************************/
void setup_web_server() {
  // Default route for root web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><head><title>Sauna Sensor Monitor</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; text-align: center; }";
    html += "h1 { color: #333366; }";
    html += ".btn { background-color: #4CAF50; border: none; color: white; padding: 15px 32px; ";
    html += "text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; border-radius: 8px; }";
    html += ".info { margin: 20px; padding: 10px; background-color: #e7f3fe; border-left: 6px solid #2196F3; }";
    html += "</style></head><body>";
    html += "<h1>Sauna Sensor Monitor</h1>";
    html += "<div class='info'>";
    html += "<p><strong>Device:</strong> ESP32</p>";
    html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "</div>";
    html += "<a href='/update' class='btn'>OTA Updates</a>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });
  
  ElegantOTA.begin(&server);
  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

/*************************************************************
  WiFi Connection Functions
*************************************************************/
void check_wifi_connection() {
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to WiFi... ");
    
    // Make sure we're in station mode
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    // Wait up to 10 seconds for connection
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      Serial.println("Connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      
    } else {
      wifi_connected = false;
      Serial.println("Failed. Will retry later.");
      WiFi.disconnect(true);
    }
  } else {
    wifi_connected = true;
  }
}

/*************************************************************
  Setup
*************************************************************/
void setup()
{
  /***************** Serial Debug Setup ********************/
  Serial.begin(9600);

  Serial.println("\n=== Battery Management System ===");
  Serial.println("Initializing...");

  /***************** I2C Setup *****************************/
  Wire.begin(SDA_PIN, SCL_PIN);  // SDA, SCL


  /***************** OLED Display Initialization ***********/
  sht.begin();
  Serial.print(uint8_t(sht.getStatus()), HEX);
  Serial.println();
  
  /***************** OLED Display Initialization ***********/
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("Starting...");
  display.display();  // Update the display

  /***************** WiFi Connection *************************/
  WiFi.setHostname("Sauna-Sensor");  // Set a custom hostname for the device
  check_wifi_connection();


  /***************** NTP Time Synchronization **************/
  if (wifi_connected) {
    // Configure time using NTP servers without manual offsets
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    // Set timezone for Sweden (CET/CEST): CET is GMT+1 and CEST is GMT+2.
    // DST starts on the last Sunday in March at 2:00 and ends on the last Sunday in October at 3:00.
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();
    
    Serial.println("Waiting for time synchronization...");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
    } else {
      Serial.println(&timeinfo, "Current time: %A, %B %d %Y %H:%M:%S");
    }
  }

  /***************** Web Server & OTA Setup ***************/
  if (wifi_connected) {
    setup_web_server();
    Serial.println("OTA updates initialized");
  }

  /***************** Display Initial UI *******************/
  draw();
  
  Serial.println("Setup complete!");
}

/*************************************************************
  Main Loop
*************************************************************/
void loop() {
  // Check and reconnect WiFi periodically if disconnected
  static unsigned long lastWifiCheck = 0;
  if (millis() - last_wifi_attempt > WIFI_RETRY_INTERVAL) {
    check_wifi_connection();
  }
  
  // Get the current time in milliseconds
  unsigned long currentMillis = millis();

  // 1. Read current sensor values (temperature and humidity)
  float currentTemp = readTemperature();
  float currentHum = readHumidity();
  
  // 2. Check if a minute has passed to update the time display
  static unsigned long lastTimePrint = 0;
  if (currentMillis - lastTimePrint >= 60000UL) {
    lastTimePrint = currentMillis;
    printLocalTime();
    Serial.print("Temperature: " + String(currentTemp) + " °C");
    Serial.println("Humidity: " + String(currentHum) + " %");
    Serial.println();
    checkAndPrintTime();  
  }
  
  // 3. Update sauna on/off logic based on current temperature
  updateSaunaState(currentTemp);
  
  // 4. Every 10 seconds, log sensor data and update the graph on the display
  static unsigned long lastLogTime = 0;
  if (currentMillis - lastLogTime >= 10000UL) {
    lastLogTime = currentMillis;
    logAndDisplayData(currentTemp, currentHum);
  }
  
  // A short delay to keep the loop stable
  delay(200);
}

void printLocalTime(void) {
    
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  } else {
    Serial.println(&timeinfo, "Current time: %A, %B %d %Y %H:%M:%S");
  }

}

// For demonstration; replace with your actual temperature/humidity read calls
float readTemperature() { 
  // TODO: Replace with sensor code, e.g., sht.getTemperature()
  // sht.read();
  // sht.getTemperature();
  
  return 25.0 + (rand() % 10) * 0.1; 
}


// For demonstration; replace with your actual temperature/humidity read calls
float readHumidity() {
  // TODO: Replace with sensor code, e.g., sht.getHumidity()
  // sht.read();
  // sht.getHumidity();

  return 50.0 + (rand() % 10) * 0.1; 
}


/*************************************************************
  checkAndPrintTime()
  - Once per minute, prints local time in "Monday 01-13:00" style
*************************************************************/
void checkAndPrintTime(void) {
  unsigned long now = millis();
  if (now - lastTimePrint >= 60000) {
    lastTimePrint = now;

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      // Format: DayOfWeek DayOfMonth-Hour:Minute
      // E.g., "Monday 01-13:00"
      // %A = Full weekday name, %d = day of month, %H = hour (24h), %M = minute
      char timeStr[32];
      strftime(timeStr, sizeof(timeStr), "%A %d-%H:%M", &timeinfo);

      // Print to OLED
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);       // Adjust text size as needed
      display.setCursor(0, 0);      // Position at top-left
      display.fillRect(0, 0, 128, 10, SSD1306_BLACK); // Clear just the top line
      display.setTextWrap(false);
      display.print(timeStr);
      display.display();
      Serial.println(timeStr);
    }
  }
}




/*************************************************************
  updateSaunaState(currentTemp)
  - 1) If temperature rises from 20°C to 30°C within 15 mins => sauna on
  - 2) Track session time
  - 3) If temp drops below 30% of highest recorded => sauna off
*************************************************************/
void updateSaunaState(float currentTemp) {
  unsigned long now = millis();

  // If sauna is NOT active, watch for 20→30°C jump within 15 min
  if (!saunaActive) {
    // Check crossing 20°C
    if (!crossed20 && currentTemp >= 20.0) {
      crossed20 = true;
      timeCrossed20 = now;
    }
    // If we crossed 20°C, check if we've reached 30°C
    if (crossed20 && currentTemp >= 30.0) {
      // Did it happen within 15 minutes (900,000 ms)?
      if ((now - timeCrossed20) <= 900000UL) {
        saunaActive = true;
        saunaStartTime = now;
        highestTempDuringSession = currentTemp;
        Serial.println("Sauna session started!");
      }
      // Reset the 20°C flag whether or not it met the 15-min condition
      crossed20 = false;
    }
  }
  // If sauna is active, track highest temp and check for cooldown
  else {
    if (currentTemp > highestTempDuringSession) {
      highestTempDuringSession = currentTemp;
    }
    // If temp falls to 30% of highest recorded, consider sauna off
    float offThreshold = 0.30 * highestTempDuringSession;
    if (currentTemp <= offThreshold) {
      saunaActive = false;
      unsigned long sessionDuration = now - saunaStartTime;
      Serial.print("Sauna session ended. Duration (ms): ");
      Serial.println(sessionDuration);

      // Reset tracking
      highestTempDuringSession = 0.0;
      crossed20 = false;
    }
  }
}




/*************************************************************
  logAndDisplayData(temp, hum)
  - Every 10s: store temp & humidity
  - Then draw them on the OLED (temp = solid line, hum = dotted line)
*************************************************************/
void logAndDisplayData(float temp, float hum) {
  unsigned long now = millis();
  if (now - lastLogTime >= LOG_INTERVAL) {
    lastLogTime = now;

    // 1) Store the sensor data in arrays
    tempLog[logIndex] = temp;
    humLog[logIndex]  = hum;
    logIndex = (logIndex + 1) % MAX_POINTS;

    // 2) Simple example: draw graph
    // Clear the display area for the graph
    display.fillRect(0, 16, SCREEN_WIDTH, SCREEN_HEIGHT - 16, SSD1306_BLACK);
    
    // We'll scale the data to fit the display. Adjust as needed.
    // For example, assume 0–100°C and 0–100% humidity for demonstration.
    // Each data point maps to an x-pixel; y is reversed on many displays.
    for (int i = 0; i < MAX_POINTS - 1; i++) {
      int idx1 = (logIndex + i) % MAX_POINTS;
      int idx2 = (logIndex + i + 1) % MAX_POINTS;
      
      // Temperature (solid line)
      int x1t = i;
      int y1t = map((int)tempLog[idx1], 0, 100, SCREEN_HEIGHT - 1, 16);
      int x2t = i + 1;
      int y2t = map((int)tempLog[idx2], 0, 100, SCREEN_HEIGHT - 1, 16);
      display.drawLine(x1t, y1t, x2t, y2t, SSD1306_WHITE);

      // Humidity (dotted line): we draw a dot at each data point
      int y1h = map((int)humLog[idx1], 0, 100, SCREEN_HEIGHT - 1, 16);
      display.drawPixel(x1t, y1h, SSD1306_WHITE);
    }

    // Optionally print the latest values (temp & hum) as text
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 54); // near bottom
    display.print("T:");
    display.print(temp);
    display.print("C  H:");
    display.print(hum);
    display.print("%");

    // Push changes to the display
    display.display();

    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.print("°C, Humidity: ");
    Serial.print(hum);
    Serial.println("%");
    
  }
}





void draw(void) {
    display.clearDisplay();

    // Layer 1
    display.drawBitmap(76, 70, image_Layer_1_bits, 153, 239, 1);

    // weather_humidity_white
    display.drawBitmap(113, 19, image_hum, 11, 16, 1);

    // weather_temperature
    display.drawBitmap(112, 45, image_temp, 16, 16, 1);

    // Layer 5
    display.drawRect(0, 9, 111, 55, 1);

    // Layer 6 (copy)
    display.setTextColor(1);
    display.setTextWrap(false);
    display.setCursor(1, 0);
    display.print("Session: 00:10");

    // Layer 8
    display.drawBitmap(2, 23, image_Layer_8_bits, 107, 34, 1);

    display.display();
}