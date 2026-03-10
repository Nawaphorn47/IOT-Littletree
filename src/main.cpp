#include <Arduino.h>
#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <PubSubClient.h>

#define RAIN_PIN 36   
#define SOIL_PIN 34   
#define EC_PIN 35   
#define PUMP2_PIN 16  

#define TFT_CLK    18 
#define TFT_MOSI   23 
#define TFT_MISO   19 
#define TFT_CS     33 
#define TFT_DC     25  
#define TFT_RST    -1  

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

#define WIFI_SSID "K"
#define WIFI_PASSWORD "12345678"

#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "x5MZQdYA2jGPv4Qmuyc18m_t3mjzNl8wDiz6ahuKOjOiNIRieHLClWYlL5CfNWzhQUBJLY_ux0K91Rt5498o_g=="
#define INFLUXDB_ORG "littletree" 
#define INFLUXDB_BUCKET "farm_data" 
#define TZ_INFO "UTC-7"

const char* mqtt_server = "broker.hivemq.com"; 
const int mqtt_port = 1883;
const char* mqtt_topic = "SmartFarm/Pump2/Control";

WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

Point sensorRain("rain_sensor");
Point sensorSoil("soil_sensor");
Point sensorEC("ec_sensor");

// 🌟 ตั้งค่าเริ่มต้นให้เป็น Manual และปิดปั๊มน้ำไว้ก่อน (ปั๊มจะไม่ติดตอนบูท)
bool isAutoMode = false;  
static bool pumpState = false; 

void wakeUpScreen() {
  delay(500); 
  tft.begin();
  tft.setRotation(1); 
  tft.fillScreen(ILI9341_BLACK); 
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);  tft.println("Smart Farm System");
  tft.setCursor(10, 50);  tft.print("Rain:");
  tft.setCursor(10, 90);  tft.print("Soil:");
  tft.setCursor(10, 130); tft.print("EC  :");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  Serial.println("MQTT Received: " + message);

  if (message == "ON") {
    isAutoMode = false; 
    digitalWrite(PUMP2_PIN, HIGH); 
    pumpState = true;
    Serial.println("👉 Manual Mode: Pump ON");
    wakeUpScreen(); 
  } 
  else if (message == "OFF") {
    isAutoMode = false; 
    digitalWrite(PUMP2_PIN, LOW); 
    pumpState = false;
    Serial.println("👉 Manual Mode: Pump OFF");
    wakeUpScreen(); 
  } 
  else if (message == "AUTO") {
    isAutoMode = true; 
    Serial.println("🤖 Switched to AUTO Mode");
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    // 🌟 ใช้ MAC Address เป็น ID เพื่อป้องกันการเตะออกซ้ำซ้อน
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String clientId = "Farm-" + mac;
    
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      mqttClient.subscribe(mqtt_topic); 
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // 🌟 สั่งปิดปั๊มทันทีที่บอร์ดตื่น
  pinMode(PUMP2_PIN, OUTPUT);
  digitalWrite(PUMP2_PIN, LOW); 

  pinMode(RAIN_PIN, INPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(EC_PIN, INPUT); 

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (wifiMulti.run() != WL_CONNECTED) { delay(500); }
  
  configTime(7 * 3600, 0, "time.google.com", "0.th.pool.ntp.org");
  client.setInsecure();

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, TFT_CS);
  wakeUpScreen(); 
}

void loop() {
  if (wifiMulti.run() == WL_CONNECTED) {
    if (!mqttClient.connected()) reconnectMQTT();
    mqttClient.loop(); 
  }

  int rawRain = analogRead(RAIN_PIN);
  int rawSoil = analogRead(SOIL_PIN);
  int rawEC = analogRead(EC_PIN);

  int rainPercent = map(rawRain, 4095, 0, 0, 100);     
  int soilPercent = map(rawSoil, 3265, 0, 0, 100);  
  int ecPercent   = map(rawEC, 3682, 4095, 0, 100);    

  rainPercent = constrain(rainPercent, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);
  ecPercent   = constrain(ecPercent, 0, 100);

  if (isAutoMode) {
    if (soilPercent < 30 && rainPercent < 10) { 
      if (!pumpState) { 
        digitalWrite(PUMP2_PIN, HIGH); 
        Serial.println("💦 AUTO: Pump ON");
        pumpState = true;
        wakeUpScreen(); 
      }
    } 
    else if (soilPercent > 60 || rainPercent >= 10) { 
      if (pumpState) { 
        digitalWrite(PUMP2_PIN, LOW); 
        Serial.println("🛑 AUTO: Pump OFF");
        pumpState = false;
        wakeUpScreen(); 
      }
    }
  }

  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK); 
  tft.setTextSize(3);
  tft.setCursor(100, 45);  tft.printf("%-3d%%", rainPercent); 
  tft.setCursor(100, 85);  tft.printf("%-3d%%", soilPercent);
  tft.setCursor(100, 125); tft.printf("%-3d%%", ecPercent);

  static unsigned long lastInfluxUpdate = 0;
  if (millis() - lastInfluxUpdate >= 5000) { 
    lastInfluxUpdate = millis();
    sensorRain.clearFields();   sensorRain.addField("value", rainPercent);
    sensorSoil.clearFields();   sensorSoil.addField("value", soilPercent);
    sensorEC.clearFields();     sensorEC.addField("value", ecPercent);
    client.writePoint(sensorRain);
    client.writePoint(sensorSoil);
    client.writePoint(sensorEC);
  }
}