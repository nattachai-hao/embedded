#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>
#include <math.h>

// ---------- WiFi ----------
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// IP ของ Board 2
const char* serverURL = "http://192.168.1.102/sensor-data";

// ---------- Sensor Pins ----------
#define DHT_PIN 4
#define DHT_TYPE DHT22

#define MQ135_PIN 34
#define SOUND_PIN 35
#define PIR_PIN 27

DHT dht(DHT_PIN, DHT_TYPE);
BH1750 lightMeter;

// ---------- Timing ----------
unsigned long lastSend = 0;
const unsigned long interval = 2000;

// ---------- WiFi ----------
void connectWiFi() {
  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  Serial.print("Board 1 IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);

  dht.begin();

  Wire.begin(21, 22);

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 Ready");
  } else {
    Serial.println("BH1750 Failed");
  }

  connectWiFi();
}

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (millis() - lastSend >= interval) {

    lastSend = millis();

    // Read DHT22
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    // Read MQ135
    int airQuality = analogRead(MQ135_PIN);

    // Read Sound
    int soundLevel = analogRead(SOUND_PIN);

    // Read PIR
    int motion = digitalRead(PIR_PIN);

    // Read BH1750
    float light = lightMeter.readLightLevel();

    // Validate readings
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("DHT22 Read Error");
      temperature = -999;
      humidity = -999;
    }

    if (isnan(light) || light < 0) {
      light = -1;
    }

    // Create JSON
    String json = "{";

    json += "\"temperature\":" + String(temperature, 2) + ",";
    json += "\"humidity\":" + String(humidity, 2) + ",";
    json += "\"air_quality\":" + String(airQuality) + ",";
    json += "\"sound\":" + String(soundLevel) + ",";
    json += "\"motion\":" + String(motion) + ",";
    json += "\"light\":" + String(light, 2);

    json += "}";

    Serial.println("\nSending Data:");
    Serial.println(json);

    // Send HTTP POST
    if (WiFi.status() == WL_CONNECTED) {

      HTTPClient http;

      http.begin(serverURL);
      http.addHeader("Content-Type", "application/json");

      int httpCode = http.POST(json);

      if (httpCode > 0) {
        Serial.print("HTTP Response: ");
        Serial.println(httpCode);
        Serial.println(http.getString());
      } else {
        Serial.print("Send Failed: ");
        Serial.println(http.errorToString(httpCode));
      }

      http.end();
    }
  }
}