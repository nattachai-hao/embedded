#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ===== Wi-Fi =====
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// IP ของบอร์ดเพื่อนที่รับข้อมูล
const char* serverURL = "http://172.30.129.241/api/sensors";

// ===== ขาเซนเซอร์ =====
#define DHT_PIN 32
#define DHT_TYPE DHT22
#define PIR_PIN 26

// KY-018: S -> GPIO34, + -> 3.3V, - -> GND
const int LIGHT_PIN = 34;

// ระดับแสงอ้างอิง 0-100 ไม่ใช่หน่วย lux จริง
const int DARK_VALUE = 4095;
const int BRIGHT_VALUE = 2000;

int lightLevel = 0;

DHT dht(DHT_PIN, DHT_TYPE);

// อ่านและส่งข้อมูลทุก 2 วินาที
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 2000;

void connectWiFi() {
  Serial.print("Connecting WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Sender IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed");
  }
}

void sendData(float temperature, float humidity,
              int pir, int lightPercent) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected - data not sent");
    return;
  }

  JsonDocument doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["pir"] = pir;

  doc["lux"] = lightPercent;

  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);

  if (!http.begin(serverURL)) {
    Serial.println("Cannot open server URL");
    return;
  }

  http.addHeader("Content-Type", "application/json");

  Serial.print("Sending: ");
  Serial.println(payload);

  int code = http.POST(payload);

  if (code > 0) {
    Serial.print("HTTP status: ");
    Serial.println(code);
    Serial.println(http.getString());

    if (code >= 200 && code < 300) {
      Serial.println("Data sent successfully");
    } else {
      Serial.println("Server rejected the request");
    }
  } else {
    Serial.print("POST failed: ");
    Serial.println(HTTPClient::errorToString(code));
  }

  http.end();
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  dht.begin();

  analogReadResolution(12);
  analogSetPinAttenuation(LIGHT_PIN, ADC_11db);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  connectWiFi();

  Serial.println("Ready");
  Serial.println("PIR needs about 30-60 seconds to stabilize");
  Serial.println("KY-018: GPIO34 | Light level: 0-100");

  lastSendTime = millis();
}

void loop() {
  if (millis() - lastSendTime < SEND_INTERVAL) {
    delay(10);
    return;
  }

  lastSendTime = millis();

  // ===== อ่าน PIR =====
  int pir = digitalRead(PIR_PIN);

  // ===== อ่าน KY-018 =====
  int rawLight = analogRead(LIGHT_PIN);

  lightLevel = map(
    rawLight,
    DARK_VALUE,
    BRIGHT_VALUE,
    0,
    100
  );
  lightLevel = constrain(lightLevel, 0, 100);

  // ===== อ่าน DHT22 =====
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  Serial.print("PIR: ");
  Serial.println(pir ? "MOTION DETECTED" : "NO MOTION");

  Serial.print("Light level: ");
  Serial.print(lightLevel);
  Serial.print("/100 | Raw: ");
  Serial.println(rawLight);

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT22 reading failed - data not sent");
    return;
  }

  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.print(" C | Humidity: ");
  Serial.print(humidity, 1);
  Serial.println(" %");

  sendData(temperature, humidity, pir, lightLevel);
}