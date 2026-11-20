
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ===== WiFi Configuration =====
const char* ssid = "SF";
const char* password = "00000000";

// ===== Web Server =====
WebServer server(80);

// ===== Sensor Data =====
float temperature = -999;
float humidity = -999;
int airQuality = -1;
int soundLevel = -1;
int motion = -1;
float light = -1;

bool dataReceived = false;
unsigned long lastReceived = 0;

// ===== Alert Thresholds (ปรับเทียบภายหลัง) =====
const float TEMP_MAX = 35.0;
const float HUMIDITY_MAX = 80.0;
const int AIR_MAX = 2000;
const int SOUND_MAX = 2000;
const float LIGHT_MIN = 100.0;

// ===== Dashboard HTML =====
const char dashboard[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>IoT Environmental Dashboard</title>
<style>
*{box-sizing:border-box}
body{
  margin:0;padding:20px;
  font-family:Arial,sans-serif;
  background:#101827;color:#f8fafc
}
.container{max-width:1000px;margin:auto}
h1{margin-bottom:5px}
.sub{color:#94a3b8;margin-bottom:25px}
.status{
  padding:12px;border-radius:10px;
  background:#253449;margin-bottom:20px
}
.grid{
  display:grid;
  grid-template-columns:repeat(auto-fit,minmax(220px,1fr));
  gap:15px
}
.card{
  background:#1e293b;padding:22px;
  border-radius:14px;border:1px solid #334155
}
.label{color:#94a3b8;font-size:14px}
.value{font-size:32px;font-weight:bold;margin:12px 0}
.unit{font-size:14px;color:#94a3b8}
.alert{
  background:#1e293b;padding:18px;
  border-radius:14px;margin-top:20px
}
.good{color:#4ade80}
.bad{color:#fb7185}
footer{color:#64748b;margin-top:20px;font-size:12px}
</style>
</head>
<body>
<div class="container">
<h1>IoT Environmental Dashboard</h1>
<div class="sub">ESP32 Gateway Monitoring System</div>

<div class="status" id="status">Connecting to Gateway...</div>

<div class="grid">
  <div class="card">
    <div class="label">Temperature</div>
    <div class="value" id="temp">--</div>
    <div class="unit">°C</div>
  </div>
  <div class="card">
    <div class="label">Humidity</div>
    <div class="value" id="humidity">--</div>
    <div class="unit">%</div>
  </div>
  <div class="card">
    <div class="label">Air Quality</div>
    <div class="value" id="air">--</div>
    <div class="unit">ADC Reading</div>
  </div>
  <div class="card">
    <div class="label">Sound Level</div>
    <div class="value" id="sound">--</div>
    <div class="unit">ADC Reading</div>
  </div>
  <div class="card">
    <div class="label">Motion Detection</div>
    <div class="value" id="motion">--</div>
    <div class="unit">PIR Sensor</div>
  </div>
  <div class="card">
    <div class="label">Light Intensity</div>
    <div class="value" id="light">--</div>
    <div class="unit">Lux</div>
  </div>
</div>

<div class="alert">
  <h3>Alert & System Status</h3>
  <div id="alerts">Waiting for sensor data...</div>
</div>

<footer>
  Last update: <span id="updated">--</span>
  <br>Auto refresh every 2 seconds
</footer>
</div>

<script>
async function updateDashboard(){
  try{
    const response = await fetch('/data');
    if(!response.ok) throw new Error('HTTP error');

    const d = await response.json();

    document.getElementById('status').innerHTML =
      d.dataReceived
      ? '<span class="good">● Sensor connected</span>'
      : '<span class="bad">● Waiting for ESP1 data</span>';

    document.getElementById('temp').textContent =
      d.temperature < -100 ? '--' : d.temperature.toFixed(1);

    document.getElementById('humidity').textContent =
      d.humidity < 0 ? '--' : d.humidity.toFixed(1);

    document.getElementById('air').textContent =
      d.airQuality < 0 ? '--' : d.airQuality;

    document.getElementById('sound').textContent =
      d.sound < 0 ? '--' : d.sound;

    document.getElementById('motion').textContent =
      d.motion < 0 ? '--' : (d.motion ? 'DETECTED' : 'CLEAR');

    document.getElementById('light').textContent =
      d.light < 0 ? '--' : d.light.toFixed(1);

    document.getElementById('updated').textContent =
      d.dataReceived ? new Date().toLocaleTimeString() : '--';

    let a = [];
    if(d.dataReceived){
      if(d.temperature > 35) a.push('High temperature');
      if(d.humidity > 80) a.push('High humidity');
      if(d.airQuality > 2000) a.push('Air quality threshold exceeded');
      if(d.sound > 2000) a.push('High sound reading');
      if(d.motion === 1) a.push('Motion detected');
      if(d.light >= 0 && d.light < 100) a.push('Low light');
    }

    document.getElementById('alerts').innerHTML =
      a.length
      ? a.map(x => '<div class="bad">⚠ '+x+'</div>').join('')
      : (d.dataReceived
          ? '<span class="good">No active alerts</span>'
          : 'Waiting for sensor data...');

  }catch(e){
    document.getElementById('status').innerHTML =
      '<span class="bad">● Cannot connect to Gateway</span>';
  }
}
updateDashboard();
setInterval(updateDashboard,2000);
</script>
</body>
</html>
)rawliteral";

// ===== Receive Sensor Data =====
void handleSensorData(){
  if(server.method() != HTTP_POST){
    server.send(405,"text/plain","Method Not Allowed");
    return;
  }

  JsonDocument doc;
  DeserializationError error =
    deserializeJson(doc,server.arg("plain"));

  if(error){
    server.send(400,"application/json",
                "{\"error\":\"Invalid JSON\"}");
    return;
  }

  if(!doc["temperature"].is<float>() ||
     !doc["humidity"].is<float>() ||
     !doc["air_quality"].is<int>() ||
     !doc["sound"].is<int>() ||
     !doc["motion"].is<int>() ||
     !doc["light"].is<float>()){
    server.send(400,"application/json",
                "{\"error\":\"Missing or invalid fields\"}");
    return;
  }

  float newTemp = doc["temperature"].as<float>();
  float newHumidity = doc["humidity"].as<float>();
  int newAir = doc["air_quality"].as<int>();
  int newSound = doc["sound"].as<int>();
  int newMotion = doc["motion"].as<int>();
  float newLight = doc["light"].as<float>();

  if(newTemp < -999 || newTemp > 100 ||
     newHumidity < -999 || newHumidity > 100 ||
     newAir < -1 || newAir > 4095 ||
     newSound < -1 || newSound > 4095 ||
     (newMotion != 0 && newMotion != 1) ||
     newLight < -1 || newLight > 100000){
    server.send(400,"application/json",
                "{\"error\":\"Value out of range\"}");
    return;
  }

  temperature = newTemp;
  humidity = newHumidity;
  airQuality = newAir;
  soundLevel = newSound;
  motion = newMotion;
  light = newLight;

  dataReceived = true;
  lastReceived = millis();

  Serial.println("Sensor data received!");

  server.send(200,"application/json",
              "{\"status\":\"OK\"}");
}

// ===== Dashboard Data API =====
void handleData(){
  bool connected = dataReceived &&
                   (millis() - lastReceived < 10000);

  JsonDocument doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["airQuality"] = airQuality;
  doc["sound"] = soundLevel;
  doc["motion"] = motion;
  doc["light"] = light;
  doc["dataReceived"] = connected;

  String output;
  serializeJson(doc,output);

  server.send(200,"application/json",output);
}

// ===== Setup =====
void setup(){
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting ESP32 Gateway...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);

  Serial.print("Connecting WiFi");

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.localIP());

  server.on("/",HTTP_GET,[](){
    server.send_P(200,"text/html",dashboard);
  });

  server.on("/sensor-data",HTTP_POST,handleSensorData);
  server.on("/data",HTTP_GET,handleData);

  server.begin();

  Serial.println("HTTP Server Started");
  Serial.println("Dashboard is ready!");
}

// ===== Loop =====
void loop(){
  server.handleClient();
}