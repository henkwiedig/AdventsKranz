#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <time.h>

AsyncWebServer server(80);
Preferences preferences;

int deviceID = 0; // Initialize the device ID
String currentTime = "N/A";


// Define your GPIO pins
const int gpioPins[] = {GPIO_NUM_23, GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_17, GPIO_NUM_16};
const int numPins = sizeof(gpioPins) / sizeof(gpioPins[0]);

void setup() {
  Serial.begin(115200);

  preferences.begin("device_prefs", false);

  // Retrieve the stored Device ID
  deviceID = preferences.getInt("deviceID", 0);

  // Generate a unique SSID based on ESP32's MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String macStr = String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
  String apSSID = "Adventskranz-" + macStr;

  // Set a password for the AP (change to your desired password)
  const char* apPassword = "caritas1337";

  // Configure ESP as an access point
  WiFi.softAP(apSSID.c_str(), apPassword);

  // IP address of the ESP's AP
  IPAddress apIP(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  // Set the IP address for the AP
  WiFi.softAPConfig(apIP, apIP, subnet);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><body>";
    html += "<h1>ESP32 GPIO Control</h1>";

    // Display the current time
    html += "<h2>Current Time: " + currentTime + "</h2>";

    // Display the Device ID
    html += "<h2>Device ID: " + String(deviceID) + "</h2>";

    // Add a form for setting the day and time
    html += "<h2>Set Day and Time:</h2>";
    html += "<form method='GET' action='/setdaytime'>";
    html += "  <label for='daytime'>Enter day and time (YYYY-MM-DD HH:MM:SS):</label>";
    html += "  <input type='text' id='daytime' name='daytime'>";
    html += "  <input type='submit' value='Set Day and Time'>";
    html += "</form>";

    // Add a form for setting the device ID
    html += "<h2>Set Device ID:</h2>";
    html += "<form method='GET' action='/setid'>";
    html += "  <label for 'id'>Enter Device ID (integer):</label>";
    html += "  <input type='number' id='id' name='id' value='" + String(deviceID) + "'>";
    html += "  <input type='submit' value='Set ID'>";
    html += "</form>";

    html += "<h2>Settable GPIO Pins:</h2>";
    for (int i = 0; i < numPins; i++) {
      html += "<p>GPIO " + String(gpioPins[i]) + ": ";
      html += "<a href='/gpio/on/" + String(gpioPins[i]) + "'>Turn On</a>&nbsp;";
      html += "<a href='/gpio/off/" + String(gpioPins[i]) + "'>Turn Off</a></p>";
    }

    html += "<a href='/gpio/on/all'>Turn All On</a>&nbsp;";
    html += "<a href='/gpio/off/all'>Turn All Off</a>&nbsp;";

    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  for (int i = 0; i < numPins; i++) {
    pinMode(gpioPins[i], OUTPUT);
    server.on(("/gpio/on/" + String(gpioPins[i])).c_str(), HTTP_GET, [i](AsyncWebServerRequest *request) {
      digitalWrite(gpioPins[i], HIGH);
      request->send(200, "text/plain", "GPIO " + String(gpioPins[i]) + " is On");
    });

    server.on(("/gpio/off/" + String(gpioPins[i])).c_str(), HTTP_GET, [i](AsyncWebServerRequest *request) {
      digitalWrite(gpioPins[i], LOW);
      request->send(200, "text/plain", "GPIO " + String(gpioPins[i]) + " is Off");
    });
  }

  server.on("/gpio/on/all", HTTP_GET, [](AsyncWebServerRequest *request) {
    for (int i = 0; i < numPins; i++) {
      digitalWrite(gpioPins[i], HIGH);
    }
    request->redirect("/");
  });

  server.on("/gpio/off/all", HTTP_GET, [](AsyncWebServerRequest *request) {
    for (int i = 0; i < numPins; i++) {
      digitalWrite(gpioPins[i], LOW);
    }
    request->redirect("/");
  });    

  server.on("/setdaytime", HTTP_GET, [](AsyncWebServerRequest *request) {
    String newDayTime = request->arg("daytime");
    struct tm timeinfo;
    if (strptime(newDayTime.c_str(), "%Y-%m-%d %H:%M:%S", &timeinfo) != NULL) {
      // Print the current time
      Serial.print("Current Time: ");
      Serial.print(timeinfo.tm_year + 1900);
      Serial.print("-");
      Serial.print(timeinfo.tm_mon + 1);
      Serial.print("-");
      Serial.print(timeinfo.tm_mday);
      Serial.print(" ");
      Serial.print(timeinfo.tm_hour);
      Serial.print(":");
      Serial.print(timeinfo.tm_min);
      Serial.print(":");
      Serial.println(timeinfo.tm_sec);  
      time_t t = mktime(&timeinfo);
      struct timeval tv = {t, 0};
      settimeofday(&tv, NULL);   
    }
    request->redirect("/");
  });

  server.on("/setid", HTTP_GET, [](AsyncWebServerRequest *request) {
    String newID = request->arg("id");
    if (newID.length() > 0) {
      deviceID = newID.toInt();
      // Store the new Device ID in preferences
      preferences.putInt("deviceID", deviceID);
    }
    request->redirect("/");
  });

  server.begin();
}

void loop() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
  currentTime = timeStr;
  Serial.println(currentTime);
  if (timeinfo.tm_hour >= 8 && timeinfo.tm_hour < 20 && timeinfo.tm_mon == 11) {
    Serial.print("Current Day of Month: ");
    Serial.print(timeinfo.tm_mday);
    Serial.print(" deviceID ");
    Serial.println(deviceID);
    Serial.print("Enable RPIO: ");
    for (int i = 1; i <= timeinfo.tm_mday - (deviceID * 8) && i <= 8; i++) {
      Serial.print(i);
      Serial.print(",");
      digitalWrite(gpioPins[i-1], HIGH);
    }
    Serial.println("");

  } else
  {
    for (int i = 0; i < numPins; i++) {
      digitalWrite(gpioPins[i], LOW);
    }
  };
  
  delay(1000);
}
