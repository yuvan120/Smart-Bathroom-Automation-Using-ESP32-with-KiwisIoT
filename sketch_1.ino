#include <WiFi.h>
#include <KiwisIoT.h>
#include <DHT.h>

// ------------ WiFi Details ------------
const char* ssid = "YOUR_WIFI_NAME";
const char* pass = "YOUR_WIFI_PASSWORD";

// Replace with your actual KiwisIoT panel topic
const char* topic = "dash_YOUR_PANEL_ID";

// ------------ ESP32 GPIO Pins ------------
#define PIR_PIN       27
#define DHT_PIN       4
#define LEAK_PIN      34

#define LIGHT_RELAY  26
#define FAN_RELAY    25
#define BUZZER_PIN   18

#define DHTTYPE DHT22

// Most relay modules are active LOW.
// Change these if your module is active HIGH.
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// Change to HIGH if your leakage module
// outputs HIGH when water is detected.
#define LEAK_ACTIVE LOW

DHT dht(DHT_PIN, DHTTYPE);
KiwisIoT kiwisiot(ssid, pass, topic);

// ------------ System Variables ------------
bool autoMode = true;
bool lightState = false;
bool fanState = false;
bool leakState = false;

unsigned long lastMotionTime = 0;
unsigned long lastSendTime = 0;

const unsigned long LIGHT_TIMEOUT = 30000;

// ------------ Output Functions ------------
void setLight(bool state) {
  lightState = state;
  digitalWrite(LIGHT_RELAY, state ? RELAY_ON : RELAY_OFF);
}

void setFan(bool state) {
  fanState = state;
  digitalWrite(FAN_RELAY, state ? RELAY_ON : RELAY_OFF);
}

// ------------ KiwisIoT Dashboard Commands ------------
void onControl(String ch, String val) {

  bool command = (val == "1");

  // Channel 0: Bathroom Light
  if (ch == "0") {
    setLight(command);
  }

  // Channel 1: Exhaust Fan
  if (ch == "1") {
    setFan(command);
  }

  // Channel 8: Automatic / Manual Mode
  if (ch == "8") {
    autoMode = command;
  }
}

// ------------ Setup ------------
void setup() {

  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(LEAK_PIN, INPUT);

  pinMode(LIGHT_RELAY, OUTPUT);
  pinMode(FAN_RELAY, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Start with all outputs OFF
  setLight(false);
  setFan(false);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();

  kiwisiot.begin();
  kiwisiot.onReceive(onControl);

  Serial.println("Smart Bathroom Automation Started");
}

// ------------ Main Loop ------------
void loop() {

  // Read motion sensor
  bool motion = digitalRead(PIR_PIN);

  // Read water leakage sensor
  leakState = (digitalRead(LEAK_PIN) == LEAK_ACTIVE);

  // Read temperature and humidity
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // ------------ Water Leakage Alert ------------
  digitalWrite(BUZZER_PIN, leakState ? HIGH : LOW);

  // ------------ Automatic Mode ------------
  if (autoMode) {

    // Automatic light
    if (motion == HIGH) {
      lastMotionTime = millis();
      setLight(true);
    }

    // Turn light OFF after no motion for 30 seconds
    if (lightState &&
        millis() - lastMotionTime >= LIGHT_TIMEOUT) {
      setLight(false);
    }

    // Automatic exhaust fan with hysteresis
    if (!isnan(humidity)) {

      if (humidity >= 70.0) {
        setFan(true);
      }

      if (humidity <= 60.0) {
        setFan(false);
      }
    }
  }

  // ------------ Send Data to KiwisIoT ------------
  if (millis() - lastSendTime >= 2000) {

    lastSendTime = millis();

    // Channel 2: Water Leakage
    kiwisiot.send("2", leakState ? 1 : 0);

    // Channel 3: Temperature
    if (!isnan(temperature)) {
      kiwisiot.send("3", temperature);
    }

    // Channel 4: Humidity
    if (!isnan(humidity)) {
      kiwisiot.send("4", humidity);
    }

    // Channel 5: Motion
    kiwisiot.send("5", motion ? 1 : 0);

    // Channel 6: Leakage Alert
    kiwisiot.send("6", leakState ? 1 : 0);
  }

  // Process cloud communication
  kiwisiot.run();

  delay(100);
}