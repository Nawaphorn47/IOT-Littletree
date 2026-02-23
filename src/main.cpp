#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- การตั้งค่า Pin (อ้างอิงตาม diagram.json ที่จัดให้) ---
const int SOIL_PIN = 34;   // Capacitive Soil Moisture Sensor (สีแดง)
const int RAIN_PIN = 35;   // Analog Rain Drop Sensor (สีน้ำเงิน)
const int PUMP_PIN = 18;   // Relay สำหรับปั๊มน้ำ (สีม่วง)

// --- การตั้งค่า Network (แก้ไขตามที่ใช้งานจริง) ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_BROKER_IP"; // ไอพีเครื่องที่ลง Node-RED

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;

void setup_wifi() {
  delay(10);
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

// ฟังก์ชันรับคำสั่งจาก Node-RED (Dashboard)
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // ถ้า Node-RED ส่ง "ON" มาที่ topic plant/pump/control
  if (String(topic) == "plant/pump/control") {
    if (message == "ON") {
      digitalWrite(PUMP_PIN, HIGH);
      client.publish("plant/pump/status", "Pumping...");
    } else {
      digitalWrite(PUMP_PIN, LOW);
      client.publish("plant/pump/status", "Off");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32_Plant_Client")) {
      Serial.println("connected");
      client.subscribe("plant/pump/control"); // รอรับคำสั่งเปิดปิดปั๊ม
    } else {
      delay(5000);
    }
  }
}

void setup() {
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW); // ปิดปั๊มไว้ก่อนตอนเริ่ม
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000) { // ส่งข้อมูลทุกๆ 2 วินาที
    lastMsg = now;

    // อ่านค่า Analog
    int soilValue = analogRead(SOIL_PIN);
    int rainValue = analogRead(RAIN_PIN);

    // แปลงค่าเป็น String เพื่อส่ง MQTT
    client.publish("plant/soil", String(soilValue).c_str());
    client.publish("plant/rain", String(rainValue).c_str());

    Serial.print("Soil: "); Serial.print(soilValue);
    Serial.print(" | Rain: "); Serial.println(rainValue);
  }
}