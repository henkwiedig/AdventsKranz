#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <time.h>

AsyncWebServer server(80);
DNSServer dnsServer;
Preferences preferences;

int deviceID = 0; // Initialize the device ID
int on;
int off;
int tick = 0;
int bootcount;
String currentTime;
String wlanPrefix;
String apPassword;
const char* NULLpassword = NULL;

// Define your GPIO pins
const int gpioPins[] = {GPIO_NUM_23, GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_17, GPIO_NUM_16};
const int numPins = sizeof(gpioPins) / sizeof(gpioPins[0]);

void setup() {
  Serial.begin(115200);

  preferences.begin("device_prefs", false);

  //Factory reset counter
  bootcount = preferences.getInt("bootcount", 0);
  bootcount++;
  preferences.putInt("bootcount", bootcount);
  Serial.print("Current bootcount: ");
  Serial.println(bootcount);
  if (bootcount > 3) {
    Serial.print("Factory Reset !!!!!");
    preferences.putInt("bootcount", 0);
    preferences.putInt("deviceID", 0);
    preferences.putInt("on", 8);
    preferences.putInt("off", 20);
    preferences.putString("wlanPrefix", "Adventskranz");
    preferences.putString("apPassword", "");
    preferences.putString("currentTime", "2023-11-15T09:00:00");    
  }

  // Retrieve the stored Device ID
  deviceID = preferences.getInt("deviceID", 0);
  on = preferences.getInt("on", 8);
  off = preferences.getInt("off", 20);
  wlanPrefix = preferences.getString("wlanPrefix", "Adventskranz");
  apPassword = preferences.getString("apPassword", "");

  currentTime = preferences.getString("currentTime", "2023-11-15T09:00:00");
  struct tm timeinfo;
  strptime(currentTime.c_str(), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  time_t t = mktime(&timeinfo);
  struct timeval tv = {t, 0};
  settimeofday(&tv, NULL);   

  // Generate a unique SSID based on ESP32's MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String apSSID = wlanPrefix + "-" + deviceID;

  // Configure ESP as an access point
  Serial.println("apPassword: " + apPassword);
  WiFi.softAP(apSSID.c_str(), apPassword.length() == 0 ? NULLpassword : apPassword);

  // IP address of the ESP's AP
  IPAddress apIP(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  // Set the IP address for the AP
  WiFi.softAPConfig(apIP, apIP, subnet);

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/");
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><body>";
    html += "<h1>ESP32 GPIO Control</h1>";

    // Display the current time
    html += "<h2>Current Time: " + currentTime + "</h2>";

    // Add a form for setting the day and time
    html += "<h2>Set Day and Time:</h2>";
    html += "<form method='GET' action='/setdaytime'>";
    html += "  <label for='daytime'>Enter day and time:</label>";
    html += "  <input type='datetime-local' id='daytime' name='daytime' value='" + currentTime + "'>";
    html += "  <input type='submit' value='Set Day and Time'>";
    html += "</form>";

    // Add a form for setting the device ID
    html += "<h2>Set Device ID:</h2>";
    html += "<form method='GET' action='/setid'>";
    html += "  <label for 'id'>Enter Device ID (integer):</label>";
    html += "  <input type='number' id='id' name='id' value='" + String(deviceID) + "'>";
    html += "  <input type='submit' value='Set ID'>";
    html += "</form>";

    // Add a form for setting the device ID
    html += "<h2>Set WLAN Prefix:</h2>";
    html += "<form method='GET' action='/setwlanprefix'>";
    html += "  <label for 'id'>Prefix:</label>";
    html += "  <input type='string' id='prefix' name='prefix' value='" + String(wlanPrefix) + "'>";
    html += "  <input type='submit' value='Set Prefix'>";
    html += "</form>";

    // Add a form for setting the apPassword
    html += "<h2>Set apPassword:</h2>";
    html += "<form method='GET' action='/setapPassword'>";
    html += "  <label for 'id'>apPassword:</label>";
    html += "  <input type='string' id='appassword' name='appassword' value='" + String(apPassword) + "'>";
    html += "  <input type='submit' value='Set apPassword'>";
    html += "</form>";

    // Add a form for setting the on hour
    html += "<h2>Set on hour:</h2>";
    html += "<form method='GET' action='/seton'>";
    html += "  <label for 'id'>on:</label>";
    html += "  <input type='string' id='on' name='on' value='" + String(on) + "'>";
    html += "  <input type='submit' value='Set on'>";
    html += "</form>";

    // Add a form for setting the off hour
    html += "<h2>Set off hour:</h2>";
    html += "<form method='GET' action='/setoff'>";
    html += "  <label for 'id'>off:</label>";
    html += "  <input type='string' id='off' name='off' value='" + String(off) + "'>";
    html += "  <input type='submit' value='Set off'>";
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
      request->redirect("/");
    });

    server.on(("/gpio/off/" + String(gpioPins[i])).c_str(), HTTP_GET, [i](AsyncWebServerRequest *request) {
      digitalWrite(gpioPins[i], LOW);
      request->redirect("/");
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
    String newDayTime = request->arg("daytime") + ":00";
    struct tm timeinfo;
    Serial.print("Setting new time: ");
    Serial.println(newDayTime.c_str());
    if (strptime(newDayTime.c_str(), "%Y-%m-%dT%H:%M:%S", &timeinfo) != NULL) {
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
    preferences.putString("currentTime", request->arg("daytime") + ":00");
    request->redirect("/");
  });

  server.on("/setwlanprefix", HTTP_GET, [](AsyncWebServerRequest *request) {
    wlanPrefix = request->arg("prefix");
    preferences.putString("wlanPrefix", request->arg("prefix"));
    preferences.putString("currentTime", currentTime);
    ESP.restart();
  });

  server.on("/setapPassword", HTTP_GET, [](AsyncWebServerRequest *request) {
    apPassword = request->arg("appassword");
    preferences.putString("apPassword", request->arg("appassword"));
    preferences.putString("currentTime", currentTime);
    ESP.restart();
  });

  server.on("/seton", HTTP_GET, [](AsyncWebServerRequest *request) {
    on = request->arg("on").toInt();
    preferences.putInt("on", request->arg("on").toInt());
    request->redirect("/");
  });

  server.on("/setoff", HTTP_GET, [](AsyncWebServerRequest *request) {
    off = request->arg("off").toInt();
    preferences.putInt("off", request->arg("off").toInt());
    request->redirect("/");
  });  

  server.on("/setid", HTTP_GET, [](AsyncWebServerRequest *request) {
    String newID = request->arg("id");
    if (newID.length() > 0) {
      deviceID = newID.toInt();
      // Store the new Device ID in preferences
      preferences.putInt("deviceID", deviceID);
    }
    preferences.putString("currentTime", currentTime);
    ESP.restart();
  });

  server.begin();
}

void loop() {

  tick++;
  if (tick == 5) {
    Serial.println("Resettting bootcount");
    preferences.putInt("bootcount", 0);
  }

  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  currentTime = timeStr;
  Serial.println(currentTime);

  //Hande GPIOS
  if (timeinfo.tm_hour >= on && timeinfo.tm_hour < off && timeinfo.tm_mon == 11) {
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

  //Store time from time to time
  if (timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
    Serial.println("Sotring Time");
    preferences.putString("currentTime", currentTime);
  }
  dnsServer.processNextRequest();
  delay(1000);
}
