/*************************************************************
  Includes
*************************************************************/
#include <Arduino.h>
#include "images.h"         // For OLED display images
#include "secrets.h"         // Contains WIFI_SSID, WIFI_PASS, BLYNK_AUTH_TOKEN
#include <gpio_viewer.h>     // For GPIO monitoring

#include <Wire.h>
#include "SHT2x.h"

#include <Adafruit_I2CDevice.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>    // For OLED display

#include <WiFi.h>
#include <WiFiClient.h>
#include <time.h>            // For NTP time synchronization

// #include <FS.h>
// #include <SPIFFS.h>
// #include <ArduinoJson.h>

#include <BlynkSimpleEsp32.h> // Blynk library for ESP32

/*************************************************************
  Definitions
*************************************************************/
#define DEMO_PIN  18            // Example pin to toggle (currently unused)

// OLED display settings
#define SCREEN_WIDTH 128        // OLED display width, in pixels
#define SCREEN_HEIGHT 64        // OLED display height, in pixels
#define OLED_RESET    -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C     // I2C address for OLED display
#define OLED_SDA 6              // OLED SDA pin
#define OLED_SCL 7              // OLED SCL pin


/*************************************************************
  Global Objects
*************************************************************/
GPIOViewer gpioViewer;          // For GPIO state visualization
SHT2x sht;                  // For SHT temperature/humidity sensors (not used yet)
bool pinState = false;          // Tracks toggling state



// Create an OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);




/*************************************************************
  FUNCTION Declarations
*************************************************************/
float readTemperature(void);
float readHumidity(void);

void updateSaunaState(float currentTemp);
void checkAndPrintTime(void);
void logAndDisplayData(float temp, float hum);




/*************************************************************
  VARIABLE Declarations
*************************************************************/
// -- For once-per-minute time display --
unsigned long lastTimePrint = 0;

// -- For sauna logic --
bool saunaActive = false;          // True if sauna session is active
bool crossed20   = false;          // True if we’ve crossed 20°C
unsigned long timeCrossed20 = 0;   // When we first crossed 20°C
unsigned long saunaStartTime = 0;  // When the sauna session started
float highestTempDuringSession = 0.0;

// -- For 10-second logging & plotting --
#define LOG_INTERVAL 10000UL
unsigned long lastLogTime = 0;

// Example arrays for storing sensor data (128 points max)
#define MAX_POINTS 128
float tempLog[MAX_POINTS];
float humLog[MAX_POINTS];
int   logIndex = 0;



/*************************************************************
  setup()
*************************************************************/
void setup()
{
  /***************** Serial Debug Setup ********************/
  Serial.begin(9600);
  Serial.setDebugOutput(true);    // Enable ESP32 internal debug output

  /***************** GPIO Viewer Setup *********************/
  gpioViewer.setSamplingInterval(125);

  /***************** I2C Setup *****************************/
  // Use custom SDA and SCL pins; adjust if necessary for your board
  Wire.begin(OLED_SDA, OLED_SCL);  // SDA, SCL
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
  WiFi.setHostname("SaunaSensor");  // Set a custom hostname for the device
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  /***************** NTP Time Synchronization **************/
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

  /***************** Blynk Initialization ******************/
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);

  /***************** GPIO Viewer Initialization ************/
  gpioViewer.begin();
}

/*************************************************************
  loop()
*************************************************************/
void loop()
{
  // Run Blynk tasks
  Blynk.run();

  //1. get current temperature and humidity
  float currentTemp = readTemperature();
  float currentHum = readHumidity();

  // 2) Check if a minute has passed => display time
  checkAndPrintTime();  

  // 3) Update sauna on/off logic
  updateSaunaState(currentTemp);

  // 4) Every 10s, log and draw the data
  logAndDisplayData(currentTemp, currentHum);

  // Optional small delay to keep loop stable
  delay(100);



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

