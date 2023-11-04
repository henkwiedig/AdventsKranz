#include <Arduino.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#ifdef ESP8266
#include <Updater.h>
#include <ESP8266WiFi.h>
#define U_PART U_FS
#else
#include <WiFi.h>
#include <Update.h>
#define U_PART U_SPIFFS
#endif
#include <Preferences.h>
#include <time.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>

AsyncWebServer server(80);
DNSServer dnsServer;
Preferences preferences;

#ifdef ESP8266
TwoWire I2C = TwoWire();
#else
TwoWire I2C = TwoWire(0);
#endif

RTC_DS3231 rtc;

int deviceID = 0; // Initialize the device ID
int on;
int off;
int tick = 0;
int bootcount;
int wlanTimeout;
String currentTime;
String wlanPrefix;
String apPassword;
const char* NULLpassword = NULL;
size_t content_len;

// Define your GPIO pins
#ifdef ESP8266
const int gpioPins[] = {16, 14, 12, 13, 15, 0, 4, 5};
#define SDA_PIN 2
#define SCL_PIN 3
#else
const int gpioPins[] = {GPIO_NUM_23, GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_17, GPIO_NUM_16};
#define SDA_PIN GPIO_NUM_14
#define SCL_PIN GPIO_NUM_27
#endif
const int numPins = sizeof(gpioPins) / sizeof(gpioPins[0]);

DateTime parseDateTime(String datetimeStr) {
  // Split the received string into date and time parts
  String dateStr = datetimeStr.substring(0, 10);
  String timeStr = datetimeStr.substring(11, 19);

  // Parse the date
  int year = dateStr.substring(0, 4).toInt();
  int month = dateStr.substring(5, 7).toInt();
  int day = dateStr.substring(8, 10).toInt();

  // Parse the time
  int hour = timeStr.substring(0, 2).toInt();
  int minute = timeStr.substring(3, 5).toInt();
  int second = timeStr.substring(6, 9).toInt();

  // Create a DateTime object
  DateTime parsedDatetime(year, month, day, hour, minute, second);

  return parsedDatetime;
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index){
    Serial.println("Update");
    content_len = request->contentLength();
    // if filename includes spiffs, update the spiffs partition
    int cmd = (filename.indexOf("spiffs") > -1) ? U_PART : U_FLASH;
#ifdef ESP8266
    Update.runAsync(true);
    if (!Update.begin(content_len, cmd)) {
#else
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
#endif
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  } else {
    Serial.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
  }

  if (final) {
    Serial.println("Update final");
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "20");  
    response->addHeader("Location", "/");
    request->send(response);
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      Serial.println("Update complete");
      Serial.flush();
      ESP.restart();
    }
  }
}

void setup() {

#ifdef ESP8266
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  I2C.pins(SDA_PIN, SCL_PIN);
#else
  Serial.begin(115200);
  I2C.setPins(SDA_PIN, SCL_PIN);
#endif

  if (! rtc.begin(&I2C)) {
    Serial.println("RTC module is NOT found");
    pinMode(LED_BUILTIN, OUTPUT);
    while(1) {
        Serial.println("RTC module is NOT found");
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
    }
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

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
    preferences.putInt("timeSlice", 3600);
    preferences.putInt("wlanTimeout", 0);
    preferences.putString("wlanPrefix", "Adventskranz");
    preferences.putString("apPassword", "");
    for (int i = 0; i < numPins; i++) {
      pinMode(gpioPins[i], OUTPUT);
      digitalWrite(gpioPins[i], HIGH);
      delay(1000);
    }
  }

  // Retrieve the stored settings
  deviceID = preferences.getInt("deviceID", 0);
  on = preferences.getInt("on", 8);
  off = preferences.getInt("off", 20);
  wlanTimeout = preferences.getInt("wlanTimeout", 0);
  wlanPrefix = preferences.getString("wlanPrefix", "Adventskranz");
  apPassword = preferences.getString("apPassword", "");

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
    html += "<h1>Adventskranz-" + String(deviceID) + " nach Wichern</h1>";

    // Display the current time
    html += "<h2>Current Time: " + currentTime + "</h2>";
    html += "<h2>Current Tick: " + String(tick) + "</h2>";

    // Add a form for setting the day and time
    html += "<h2>Set Day and Time:</h2>";
    html += "<form method='GET' action='/setdaytime'>";
    html += "  <label for='daytime'>Enter day and time:</label>";
    html += "  <input type='datetime-local' step='1' id='daytime' name='daytime' value='" + currentTime + "'>";
    html += "  <input type='submit' value='Set Day and Time'>";
    html += "</form>";

    // Add a form for setting the device ID
    html += "<h2>Set Device ID:</h2>";
    html += "<form method='GET' action='/setid'>";
    html += "  <label for 'id'>Enter Device ID (integer):</label>";
    html += "  <input type='number' id='id' name='id' value='" + String(deviceID) + "' min='0' max='2'>";
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

    // Add a form for setting the wlanTimeout
    html += "<h2>Set wlanTimeout:</h2>";
    html += "<form method='GET' action='/setwlanTimeout'>";
    html += "  <label for 'id'>wlanTimeout:</label>";
    html += "  <input type='string' id='wlanTimeout' name='wlanTimeout' value='" + String(wlanTimeout) + "'>";
    html += "  <input type='submit' value='Set wlanTimeout'>";
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

    html += "<a href='/gpio/on/all'>Turn All On</a><br>";
    html += "<a href='/gpio/off/all'>Turn All Off</a><br>";
    html += "<form method='POST' action='/doUpdate' enctype='multipart/form-data'><label for 'id'>Update:</label><input type='file' id='update' name='update'><input type='submit' value='Update'></form><br>";
    html += "<a href='/reboot'>Reboot</a><br><br>";
    html += "Version: " + String(VERSION)+ "<br>";
    html += "Build Time: " + String(__DATE__) + " " + String(__TIME__) + "<br>" ;

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
    DateTime new_time = parseDateTime(request->arg("daytime"));
    char myDateFormat[21] = "YYYY-MM-DDThh:mm:ss";
    Serial.println("new_time: " + String(new_time.toString(myDateFormat)));
    rtc.adjust(new_time);
    request->redirect("/");
  });

  server.on("/setwlanprefix", HTTP_GET, [](AsyncWebServerRequest *request) {
    wlanPrefix = request->arg("prefix");
    preferences.putString("wlanPrefix", request->arg("prefix"));
    ESP.restart();
  });

  server.on("/setapPassword", HTTP_GET, [](AsyncWebServerRequest *request) {
    apPassword = request->arg("appassword");
    preferences.putString("apPassword", request->arg("appassword"));
    request->redirect("/");
    ESP.restart();
  });

  server.on("/setwlanTimeout", HTTP_GET, [](AsyncWebServerRequest *request) {
    wlanTimeout = request->arg("wlanTimeout").toInt();
    preferences.putInt("wlanTimeout", wlanTimeout);
    request->redirect("/");
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
    request->redirect("/");
    ESP.restart();
  });

  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/");
    ESP.restart();
  });

   server.on("/doUpdate", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
      handleDoUpdate(request, filename, index, data, len, final);
    }
  );

  //all off by default
  for (int i = 0; i < numPins; i++) {
    digitalWrite(gpioPins[i], LOW);
  }
  server.begin();
}

void loop() {

  tick++;

  //bootcount reset
  if (tick == 5) {
    Serial.println("Resettting bootcount");
    preferences.putInt("bootcount", 0);
  }

  //wifi turn off after wlanTimeout seconds
  if (tick == wlanTimeout) {
    Serial.println("WiFi off");
    WiFi.mode(WIFI_OFF);
  }

  DateTime now = rtc.now();
  char myDateFormat[21] = "YYYY-MM-DDThh:mm:ss";
  currentTime = now.toString(myDateFormat);
  Serial.println(currentTime);

  //Hande GPIOS
  if (now.hour() >= on && now.hour() < off && now.month() == 12 && !Update.isRunning()) {
    Serial.print("Current Day of Month: ");
    Serial.print(now.day());
    Serial.print(" deviceID ");
    Serial.println(deviceID);
    Serial.print("Enable RPIO: ");
    for (int i = 1; i <= now.day() - (deviceID * 8) && i <= 8; i++) {
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

  dnsServer.processNextRequest();
  delay(1000);
}
