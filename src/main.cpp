#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // ต้องเพิ่ม Library นี้ใน platformio.ini

// --- การตั้งค่า Network ---
const char* mqtt_server = "192.168.0.196";
const char* ssid = "K";
const char* password = "12345678";


WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;

void setup_wifi() {
  delay(10);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32_Mock_Client")) {
      Serial.println("MQTT connected");
    } else {
      delay(5000);
    }
  }
}

void setup() {
  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 5000) { // ส่งข้อมูลจำลองทุก 5 วินาที
    lastMsg = now;

    // --- ส่วนการสร้าง Mock Data (จำลองข้อมูล) ---
    int mockSoil = random(1500, 3500); // สุ่มค่าความชื้น 1500 - 3500
    int mockRain = random(0, 4095);    // สุ่มค่าฝน 0 - 4095
    String pumpStatus = (mockSoil > 3000) ? "ON" : "OFF";

    // --- สร้าง JSON Payload ---
    StaticJsonDocument<200> doc;
    doc["sensor"] = "LittleTree-01";
    doc["soil_moisture"] = mockSoil;
    doc["rain_value"] = mockRain;
    doc["pump_status"] = pumpStatus;

    char buffer[200];
    serializeJson(doc, buffer);

    // --- ส่งข้อมูลไปยัง Topic หลัก ---
    client.publish("plant/data/json", buffer);

    Serial.print("Published JSON: ");
    Serial.println(buffer);
  }
}