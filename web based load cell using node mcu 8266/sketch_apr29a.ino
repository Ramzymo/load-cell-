#include <HX711.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define LOADCELL_DOUT_PIN D1
#define LOADCELL_SCK_PIN  D0
const int EEPROM_ADDR = 0;
const int NUM_SAMPLES = 10;

HX711 scale;
float calibrationFactor = 0;
float currentWeight = 0;

ESP8266WebServer server(80);

// ==== Web Interface ==== 
void handleRoot() {
  String html = R"====(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Load Cell - Live Weight</title>
      <style>
        body {
          margin: 0;
          padding: 0;
          background: #1e1e1e;
          color: #4fc3f7;
          font-family: 'Segoe UI', sans-serif;
          text-align: center;
        }
        h1 {
          margin-top: 60px;
          font-size: 2em;
        }
        h2 {
          font-size: 1.2em;
          color: #999;
          margin-bottom: 30px;
        }
        #weight {
          font-size: 3em;
          margin-top: 20px;
        }
      </style>
    </head>
    <body>
      <h1>Load Cell</h1>
      <h2>Live Weight</h2>
      <div id="weight">-- g</div>

      <script>
        const weightDisplay = document.getElementById("weight");
        setInterval(() => {
          fetch("/weight")
            .then(res => res.text())
            .then(data => {
              weightDisplay.innerText = data + " g";
            });
        }, 200);
      </script>
    </body>
    </html>
  )====";
  server.send(200, "text/html", html);
}

void handleWeight() {
  server.send(200, "text/plain", String((int)currentWeight)); // no decimal
}

// ==== Setup ==== 
void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  delay(500);

  Serial.println("\n🚀 RAMZY LOAD CELL STARTING...");

  EEPROM.get(EEPROM_ADDR, calibrationFactor);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale();

  if (isnan(calibrationFactor) || calibrationFactor == 0 || abs(calibrationFactor) > 10000) {
    Serial.println("🛠️ No valid calibration. Calibrating using 1000g reference...");

    scale.tare();  // Zero the scale
    delay(1000);

    long raw = scale.read_average(NUM_SAMPLES);
    calibrationFactor = (float)raw / 1000.0;  // Assume current weight is 1000g
    scale.set_scale(calibrationFactor);

    EEPROM.put(EEPROM_ADDR, calibrationFactor);
    EEPROM.commit();

    Serial.printf("📊 Raw avg: %ld\n", raw);
    Serial.printf("✅ Calibration factor saved: %.4f\n", calibrationFactor);
  } else {
    Serial.printf("✅ Loaded calibration: %.4f\n", calibrationFactor);
    scale.set_scale(calibrationFactor);
    scale.tare();  // Zero the scale if no weight is present
    delay(500);
  }

  // Start WiFi Access Point
  WiFi.softAP("RAMZY LOAD CELL", "12345678");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("📡 WiFi: RAMZY LOAD CELL\n🔗 Open: http://");
  Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/weight", handleWeight);
  server.begin();
}

// ==== Loop ==== 
void loop() {
  server.handleClient();

  if (scale.is_ready()) {
    float grams = scale.get_units(NUM_SAMPLES);
    if (grams < 0) grams = 0;
    currentWeight = grams;
  }
}
