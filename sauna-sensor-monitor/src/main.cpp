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
#include <ElegantOTA.h>     // OTA update functionality

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

// Sauna state variables
bool saunaActive = false;          // True if sauna session is active
bool crossed20   = false;          // True if we've crossed 20°C
unsigned long timeCrossed20 = 0;   // When we first crossed 20°C
unsigned long saunaStartTime = 0;  // When the sauna session started
float highestTempDuringSession = 0.0;

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
void draw(float temperature, float humidity);
void printLocalTime(void);
float readTemperature(void);
float readHumidity(void);

// Application logic
void updateSaunaState(float currentTemp);

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
  // Root page with enhanced interface
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Sauna Sensor Monitor</title>";
    html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #121212; color: #e0e0e0; }";
    html += "h1 { color: #ffffff; margin-top: 30px; font-weight: 300; letter-spacing: 1px; font-size: 1.8rem; }";
    html += ".btn { background-color: #4CAF50; border: none; color: white; padding: 15px 32px; ";
    html += "text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 20px 2px; cursor: pointer; border-radius: 8px; transition: all 0.3s; }";
    html += ".btn:hover { background-color: #3e8e41; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.3); }";
    html += ".info { margin: 20px 0; padding: 15px; background-color: #1e1e1e; border-left: 6px solid #4CAF50; text-align: left; border-radius: 4px; color: #e0e0e0; }";
    html += ".data-container { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; margin: 30px 0; }";
    html += ".data-card { background-color: #1e1e1e; border-radius: 12px; padding: 20px; width: 180px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); transition: transform 0.2s; }";
    html += ".data-card:hover { transform: translateY(-5px); box-shadow: 0 6px 10px rgba(0,0,0,0.4); }";
    
    // Style each card differently
    html += ".temp-card { border-top: 3px solid #ff6384; }";
    html += ".humidity-card { border-top: 3px solid #36a2eb; }";
    html += ".session-card { border-top: 3px solid #4CAF50; }";
    
    html += ".data-value { font-size: 32px; font-weight: bold; margin: 10px 0; color: #ffffff; }";
    html += ".data-label { color: #9e9e9e; font-size: 14px; }";
    html += ".chart-container { width: 100%; max-width: 800px; height: 400px; margin: 30px auto; padding: 20px; background-color: #1e1e1e; border-radius: 12px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }";
    html += "strong { color: #4CAF50; }";
    html += ".info p { margin: 8px 0; }";
    html += ".footer { margin-top: 30px; font-size: 12px; color: #9e9e9e; }";
    
    // Mobile responsive adjustments
    html += "@media (max-width: 768px) {";
    html += "  body { padding: 10px; }";
    html += "  h1 { font-size: 1.5rem; }";
    html += "  .info { margin: 15px 0; padding: 10px; }";
    html += "  .data-container { gap: 10px; margin: 15px 0; }";
    html += "  .data-card { width: calc(50% - 25px); padding: 15px; }";
    html += "  .data-value { font-size: 24px; }";
    html += "  .chart-container { height: 300px; padding: 10px; margin: 15px auto; }";
    html += "  .btn { padding: 12px 25px; font-size: 14px; }";
    html += "}";
    
    // Extra small screens
    html += "@media (max-width: 480px) {";
    html += "  .data-container { flex-direction: column; align-items: center; }";
    html += "  .data-card { width: 100%; max-width: 250px; }";
    html += "  .chart-container { height: 250px; }";
    html += "}";
    
    html += "</style>";
    
    // Add dark mode Chart.js config
    html += "<script>";
    html += "Chart.defaults.color = '#e0e0e0';";
    html += "Chart.defaults.borderColor = '#303030';";
    html += "</script>";
    
    html += "</head><body>";
    html += "<h1>Sauna Sensor Monitor</h1>";
    
    html += "<div class='info'>";
    html += "<p><strong>Device:</strong> ESP32 (Sauna-Sensor)</p>";
    html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "</div>";
    
    // Data cards for current readings
    html += "<div class='data-container'>";
    
    // Temperature card
    html += "<div class='data-card temp-card'>";
    html += "<div class='data-label'>Temperature</div>";
    html += "<div class='data-value' id='temp-value'>--</div>";
    html += "<div class='data-label'>°C</div>";
    html += "</div>";
    
    // Humidity card
    html += "<div class='data-card humidity-card'>";
    html += "<div class='data-label'>Humidity</div>";
    html += "<div class='data-value' id='humidity-value'>--</div>";
    html += "<div class='data-label'>%</div>";
    html += "</div>";
    
    // Session time card
    html += "<div class='data-card session-card'>";
    html += "<div class='data-label'>Session Time</div>";
    html += "<div class='data-value' id='session-time'>--</div>";
    html += "<div class='data-label'>minutes</div>";
    html += "</div>";
    
    html += "</div>";
    
    // Graph container
    html += "<div class='chart-container'>";
    html += "<canvas id='sensorChart'></canvas>";
    html += "</div>";
    
    html += "<a href='/update' class='btn'>OTA Updates</a>";
    
    // JavaScript to fetch data and update the UI
    html += "<script>";
    html += "let chart;";
    html += "function fetchData() {";
    html += "  console.log('Fetching data from /data endpoint...');";
    html += "  fetch('/data', { cache: 'no-store' })"; // Add no-store to prevent caching
    html += "    .then(response => {";
    html += "      console.log('Response status:', response.status);";
    html += "      if (!response.ok) {";
    html += "        throw new Error('Network response error: ' + response.status);";
    html += "      }";
    html += "      return response.text();"; // First get as text
    html += "    })";
    html += "    .then(text => {";
    html += "      console.log('Raw response:', text);"; // Log the raw text
    html += "      // Parse JSON manually to avoid potential issues";
    html += "      try {";
    html += "        return JSON.parse(text);";
    html += "      } catch (e) {";
    html += "        console.error('JSON parse error:', e, 'for text:', text);";
    html += "        throw new Error('Failed to parse JSON response');";
    html += "      }";
    html += "    })";
    html += "    .then(data => {";
    html += "      console.log('Parsed data:', data);"; // Debug log
    
    html += "      // Get DOM elements once";
    html += "      const tempElement = document.getElementById('temp-value');";
    html += "      const humElement = document.getElementById('humidity-value');";
    html += "      const sessionElement = document.getElementById('session-time');";
    
    html += "      // Check if elements exist";
    html += "      if (!tempElement || !humElement || !sessionElement) {";
    html += "        console.error('Could not find one or more DOM elements');";
    html += "        return;";
    html += "      }";
    
    html += "      // Update temperature";
    html += "      if (data.temperature !== undefined) {";
    html += "        try {";
    html += "          const tempVal = Number(data.temperature);";
    html += "          tempElement.textContent = tempVal.toFixed(1);";
    html += "          console.log('Updated temperature to:', tempVal.toFixed(1));";
    html += "        } catch (e) {";
    html += "          console.error('Error setting temperature:', e);";
    html += "          tempElement.textContent = 'Error';";
    html += "        }";
    html += "      }";
    
    html += "      // Update humidity";
    html += "      if (data.humidity !== undefined) {";
    html += "        try {";
    html += "          const humVal = Number(data.humidity);";
    html += "          humElement.textContent = Math.round(humVal);";
    html += "          console.log('Updated humidity to:', Math.round(humVal));";
    html += "        } catch (e) {";
    html += "          console.error('Error setting humidity:', e);";
    html += "          humElement.textContent = 'Error';";
    html += "        }";
    html += "      }";
    
    html += "      // Update session time";
    html += "      if (data.sessionTime !== undefined) {";
    html += "        try {";
    html += "          sessionElement.textContent = data.sessionTime;";
    html += "          console.log('Updated session time to:', data.sessionTime);";
    html += "        } catch (e) {";
    html += "          console.error('Error setting session time:', e);";
    html += "          sessionElement.textContent = '0';";
    html += "        }";
    html += "      }";
    
    html += "      // Update chart if we have valid data";
    html += "      if (data.tempHistory && data.humHistory && data.labels) {";
    html += "        updateChart(data);";
    html += "      }";
    html += "    })";
    html += "    .catch(error => {";
    html += "      console.error('Fetch error:', error);";
    html += "    });";
    html += "}";
    
    html += "function updateChart(data) {";
    html += "  if (!chart) {";
    html += "    const ctx = document.getElementById('sensorChart').getContext('2d');";
    html += "    chart = new Chart(ctx, {";
    html += "      type: 'line',";
    html += "      data: {";
    html += "        labels: data.labels,";
    html += "        datasets: [";
    html += "          {";
    html += "            label: 'Temperature (°C)',";
    html += "            data: data.tempHistory,";
    html += "            borderColor: '#ff6384',";
    html += "            backgroundColor: 'rgba(255, 99, 132, 0.2)',";
    html += "            borderWidth: 2,";
    html += "            pointRadius: 3,";
    html += "            tension: 0.3";
    html += "          },";
    html += "          {";
    html += "            label: 'Humidity (%)',";
    html += "            data: data.humHistory,";
    html += "            borderColor: '#36a2eb',";
    html += "            backgroundColor: 'rgba(54, 162, 235, 0.2)',";
    html += "            borderWidth: 2,";
    html += "            pointRadius: 3,";
    html += "            tension: 0.3";
    html += "          }";
    html += "        ]";
    html += "      },";
    html += "      options: {";
    html += "        responsive: true,";
    html += "        maintainAspectRatio: false,";
    html += "        plugins: {";
    html += "          legend: {";
    html += "            labels: {";
    html += "              color: '#e0e0e0',";
    html += "              font: {";
    html += "                size: 12";
    html += "              },";
    html += "              boxWidth: 12";
    html += "            },";
    html += "            position: window.innerWidth < 768 ? 'bottom' : 'top'";
    html += "          },";
    html += "          tooltip: {";
    html += "            mode: 'index',";
    html += "            intersect: false,";
    html += "            backgroundColor: 'rgba(0,0,0,0.7)'";
    html += "          }";
    html += "        },";
    html += "        interaction: { mode: 'index', intersect: false },";
    html += "        scales: {";
    html += "          y: {";
    html += "            beginAtZero: false,";
    html += "            grid: {";
    html += "              color: '#303030',";
    html += "              display: window.innerWidth > 480";
    html += "            },";
    html += "            ticks: {";
    html += "              color: '#e0e0e0',";
    html += "              maxTicksLimit: window.innerWidth < 480 ? 5 : 10,";
    html += "              font: {";
    html += "                size: window.innerWidth < 480 ? 10 : 12";
    html += "              }";
    html += "            }";
    html += "          },";
    html += "          x: {";
    html += "            grid: {";
    html += "              color: '#303030',";
    html += "              display: window.innerWidth > 480";
    html += "            },";
    html += "            ticks: {";
    html += "              color: '#e0e0e0',";
    html += "              maxRotation: 0,";
    html += "              maxTicksLimit: window.innerWidth < 480 ? 5 : 10,";
    html += "              font: {";
    html += "                size: window.innerWidth < 480 ? 10 : 12";
    html += "              }";
    html += "            }";
    html += "          }";
    html += "        }";
    html += "      }";
    html += "    });";
    html += "  } else {";
    html += "    chart.data.labels = data.labels;";
    html += "    chart.data.datasets[0].data = data.tempHistory;";
    html += "    chart.data.datasets[1].data = data.humHistory;";
    html += "    chart.update();";
    html += "  }";
    html += "}";
    
    html += "// Fetch initial data and setup refresh interval";
    html += "fetchData();"; // Immediate first fetch
    html += "console.log('Setting up refresh interval...');";
    html += "const refreshInterval = setInterval(fetchData, 2000);"; // Refresh every 2 seconds
    
    // Add a document.readyState check to ensure DOM is fully loaded
    html += "document.addEventListener('DOMContentLoaded', function() {";
    html += "  console.log('DOM fully loaded, fetching initial data...');";
    html += "  fetchData();";
    html += "});";
    html += "</script>";
    
    // Add footer
    html += "<div class='footer'>Custom Built for Ingemar Josefsson &copy; </div>";
    
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  // API endpoint to provide current data
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    // Log request details
    Serial.printf("DATA API Request from %s\n", request->client()->remoteIP().toString().c_str());
    
    // Debug sensor readings to serial monitor
    float temp = readTemperature();
    float hum = readHumidity();
    Serial.printf("API Request - Temperature: %.2f, Humidity: %.2f\n", temp, hum);
    
    // Create a JSON-formatted string with current data
    String json = "{";
    
    // Add current temperature and humidity with explicit decimal points to ensure proper parsing
    json += "\"temperature\":" + String(temp, 1);  // Force 1 decimal place
    json += ",\"humidity\":" + String(int(hum));   // Round humidity to integer
    
    // Add sauna session time in minutes (if active)
    unsigned long sessionMinutes = 0;
    if (saunaActive && saunaStartTime > 0) {
      sessionMinutes = (millis() - saunaStartTime) / 60000;
    }
    json += ",\"sessionTime\":" + String(sessionMinutes);
    
    // Create history arrays for the chart - last 10 readings
    json += ",\"labels\":[";
    for (int i = 0; i < 10; i++) {
      if (i > 0) json += ",";
      json += "\"" + String(i * 10) + "s ago\"";
    }
    json += "]";
    
    // Add temperature history with explicit decimal formatting
    json += ",\"tempHistory\":[";
    float baseTemp = temp;
    for (int i = 0; i < 10; i++) {
      if (i > 0) json += ",";
      // Slight random variation for demo purposes
      float historyTemp = baseTemp + random(-15, 15) / 10.0;
      json += String(historyTemp, 1);  // Force 1 decimal place
    }
    json += "]";
    
    // Add humidity history as integers
    json += ",\"humHistory\":[";
    float baseHum = hum;
    for (int i = 0; i < 10; i++) {
      if (i > 0) json += ",";
      // Slight random variation for demo purposes
      int historyHum = int(baseHum + random(-10, 10) / 10.0);
      json += String(historyHum);
    }
    json += "]";
    
    json += "}";
    Serial.println("Sending JSON: " + json);
    
    // Add CORS headers to allow requests from any origin
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    request->send(response);
    
    Serial.println("API response sent successfully");
  });

  // Setup ElegantOTA
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
  display.setTextSize(1.5);
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
  float initialTemp = readTemperature();
  float initialHum = readHumidity();
  draw(initialTemp, initialHum);
  
  Serial.println("Setup complete!");
}

/*************************************************************
  Main Loop - Optimized & Clean
*************************************************************/
void loop() {
  static unsigned long lastDisplayUpdate = 0;
  static unsigned long lastSerialOutput = 0;
  static unsigned long lastWifiCheck = 0;
  unsigned long currentMillis = millis();
  
  // Process OTA updates if WiFi connected
  if (wifi_connected) {
    ElegantOTA.loop();
  }
  
  // Check and reconnect WiFi periodically if needed
  if (currentMillis - lastWifiCheck >= WIFI_RETRY_INTERVAL) {
    lastWifiCheck = currentMillis;
    check_wifi_connection();
  }
  
  // Read sensor data and update display
  if (currentMillis - lastDisplayUpdate >= 2000) {
    float currentTemp = readTemperature();
    float currentHum = readHumidity();
    
    // Update sauna state logic based on temperature
    updateSaunaState(currentTemp);
    
    // Update display
    lastDisplayUpdate = currentMillis;
    draw(currentTemp, currentHum);
    
    // Print minimal status info to serial (once per minute)
    if (currentMillis - lastSerialOutput >= 60000) {
      lastSerialOutput = currentMillis;
      
      // Get current time
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
        
        // Print status in a clean, concise format
        Serial.printf("[%s] Temp: %.1f°C | Humidity: %d%% | WiFi: %s\n", 
                     timeStr, 
                     currentTemp, 
                     (int)currentHum,
                     wifi_connected ? "Connected" : "Disconnected");
                     
        if (saunaActive) {
          unsigned long sessionMinutes = (millis() - saunaStartTime) / 60000;
          Serial.printf("        Sauna active for %lu minutes\n", sessionMinutes);
        }
      }
    }
  }
  
  // Small delay to prevent CPU hogging
  delay(25);
}

void printLocalTime(void) {
    
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  } else {
    Serial.println(&timeinfo, "Current time: %A, %B %d %Y %H:%M:%S");
  }

}

// Implement actual temperature reading from SHT2x sensor
float readTemperature() { 
  // Read from SHT2x sensor
  sht.read();
  float temp = sht.getTemperature();
  
  // Check if reading is valid (not NaN or infinite)
  if (isnan(temp) || isinf(temp)) {
    Serial.println("Error reading temperature from sensor!");
    // Return a fallback value
    return 25.0;
  }
  
  // Serial.printf("Temperature read: %.2f°C\n", temp);
  return temp;
}

// Implement actual humidity reading from SHT2x sensor
float readHumidity() {
  // Read from SHT2x sensor
  sht.read();
  float humidity = sht.getHumidity();
  
  // Check if reading is valid (not NaN or infinite)
  if (isnan(humidity) || isinf(humidity) || humidity < 0 || humidity > 100) {
    Serial.println("Error reading humidity from sensor!");
    // Return a fallback value
    return 50.0;
  }
  
  // Serial.printf("Humidity read: %.2f%%\n", humidity);
  return humidity;
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

void draw(float temperature, float humidity) {
  display.clearDisplay();
  
  // Draw border
  display.drawRect(0, 0, display.width(), display.height(), SSD1306_WHITE);
  
  // Show WiFi status icon in top right corner
  if (!wifi_connected) {
    display.drawBitmap(display.width() - 18, 2, NO_NETWORK_ICON, 16, 16, SSD1306_WHITE);
  }
  
  // Temperature section - moved higher up
  display.drawBitmap(8, 6, TEMP_ICON, 16, 16, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(28, 6);
  display.print(temperature, 1);
  display.setTextSize(2);
  display.print(" C");
  
  // Separator line
  display.drawLine(0, 28, display.width(), 28, SSD1306_WHITE);
  
  // Humidity section - moved higher up
  display.drawBitmap(8, 34, DROP_ICON, 16, 16, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(28, 34);
  display.print(int(humidity));
  display.print(" %");
  
  // Show sauna session info in a dedicated bottom area
  if (saunaActive) {
    // Bottom status bar with enough clearance from humidity reading
    display.drawLine(0, 54, display.width(), 54, SSD1306_WHITE);
    
    display.setTextSize(1);
    display.setCursor(3, 56);
    display.print("SAUNA ON ");
    
    // Calculate session time
    unsigned long sessionMin = (millis() - saunaStartTime) / 60000;
    unsigned long sessionSec = ((millis() - saunaStartTime) % 60000) / 1000;
    
    display.print(sessionMin);
    display.print(":");
    if (sessionSec < 10) display.print("0");
    display.print(sessionSec);
  }

    display.display();
}