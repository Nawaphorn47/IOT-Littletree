#include <Arduino.h>
#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// --- ขาเซนเซอร์ ---
#define RAIN_PIN 36   
#define SOIL_PIN 34   
#define EC_PIN 35   
#define PUMP2_PIN 16  // 🌟 เปลี่ยนเป็น 16 เพื่อไม่ให้ชนกับจอ TFT ครับ

// --- ขา SPI จอ ---
#define TFT_CLK    18 
#define TFT_MOSI   23 
#define TFT_MISO   19 
#define TFT_CS     33 
#define TFT_DC     25  
#define TFT_RST    -1  

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// --- ตั้งค่า Wi-Fi ---
#define WIFI_SSID "K"
#define WIFI_PASSWORD "12345678"

// --- ตั้งค่า InfluxDB ---
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "x5MZQdYA2jGPv4Qmuyc18m_t3mjzNl8wDiz6ahuKOjOiNIRieHLClWYlL5CfNWzhQUBJLY_ux0K91Rt5498o_g=="
#define INFLUXDB_ORG "littletree" 
#define INFLUXDB_BUCKET "farm_data" 
#define TZ_INFO "UTC-7"

WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point sensorRain("rain_sensor");
Point sensorSoil("soil_sensor");
Point sensorEC("ec_sensor");

void setup() {
  Serial.begin(115200);

  // 🌟 ตั้งค่าขามอเตอร์ปั๊มน้ำ
  pinMode(PUMP2_PIN, OUTPUT);
  digitalWrite(PUMP2_PIN, HIGH); // ปิดปั๊มไว้ก่อนตอนเริ่ม (Relay แยกมักเป็น Active Low: HIGH=ปิด, LOW=เปิด)

  pinMode(RAIN_PIN, INPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(EC_PIN, INPUT); 

  // ------------------------------------
  // 1. จัดการ Wi-Fi และ InfluxDB
  // ------------------------------------
  Serial.println("\n--- Starting Network Setup ---");
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected");

  Serial.print("Syncing time...");
  configTime(7 * 3600, 0, "time.google.com", "0.th.pool.ntp.org", "time.uni.net.th");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nTime synchronized!");

  client.setInsecure();

  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // ------------------------------------
  // 2. เปิดหน้าจอ TFT
  // ------------------------------------
  Serial.println("\n--- Starting TFT Screen ---");
  SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.setRotation(1); 
  tft.fillScreen(ILI9341_BLACK); 

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Smart Farm System");
  
  tft.setTextSize(2);
  tft.setCursor(10, 50);  tft.print("Rain:");
  tft.setCursor(10, 90);  tft.print("Soil:");
  tft.setCursor(10, 130); tft.print("EC  :");

  tft.setCursor(10, 200);
  tft.setTextColor(ILI9341_YELLOW);
  tft.print("Status: System Ready!");
}

void loop() {
  // 1. อ่านค่าดิบจากเซนเซอร์
  int rawRain = analogRead(RAIN_PIN);
  int rawSoil = analogRead(SOIL_PIN);
  int rawEC = analogRead(EC_PIN);

  // 2. แปลงค่าดิบ (Raw) ให้เป็นเปอร์เซ็นต์ (0 - 100%)
  int rainPercent = map(rawRain, 4095, 0, 0, 100);     
  int soilPercent = map(rawSoil, 3265, 0, 0, 100);  
  int ecPercent   = map(rawEC, 3682, 4095, 0, 100);    

  // 3. ป้องกันไม่ให้ตัวเลขทะลุ 0 หรือ 100 
  rainPercent = constrain(rainPercent, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);
  ecPercent   = constrain(ecPercent, 0, 100);

  Serial.printf("Percent -> Rain: %d%% | Soil: %d%% | EC: %d%%\n", rainPercent, soilPercent, ecPercent);

// ------------------------------------
  // 🌟 3.5 เงื่อนไขรดน้ำอัตโนมัติ (แก้ตรรกะให้ถูกต้อง) 🌟
  // ------------------------------------
  static bool pumpState = false; // ตัวแปรเก็บสถานะปั๊ม

  if (soilPercent < 30 && rainPercent < 10) {
    // --- กรณีดินแห้ง (< 30%) และ ฝนไม่ตก ---
    if (!pumpState) { 
      digitalWrite(PUMP2_PIN, LOW); 
      Serial.println("💦 Auto Pump: ON");
      pumpState = true;
      
      // หน่วงเวลาให้ไฟนิ่ง แล้ววาดจอใหม่ให้ครบทุกบรรทัด
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
  } else {
    // --- กรณีดินชื้นแล้ว (>= 30%) หรือ ฝนตก ---
    if (pumpState) { 
      digitalWrite(PUMP2_PIN, HIGH); 
      Serial.println("🛑 Auto Pump: OFF");
      pumpState = false;
      
      // หน่วงเวลาให้ไฟนิ่ง แล้ววาดจอใหม่ให้ครบทุกบรรทัด
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
  }

  // ------------------------------------
  // 4. แสดงผล "เปอร์เซ็นต์" บนหน้าจอ TFT
  // ------------------------------------
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK); 
  tft.setTextSize(3);
  
  tft.setCursor(100, 45);  tft.printf("%-3d%%", rainPercent); 
  tft.setCursor(100, 85);  tft.printf("%-3d%%", soilPercent);
  tft.setCursor(100, 125); tft.printf("%-3d%%", ecPercent);

  // 5. เตรียมข้อมูลส่งขึ้น InfluxDB 
  sensorRain.clearFields();   sensorRain.addField("value", rainPercent);
  sensorSoil.clearFields();   sensorSoil.addField("value", soilPercent);
  sensorEC.clearFields();     sensorEC.addField("value", ecPercent);

  client.writePoint(sensorRain);
  client.writePoint(sensorSoil);
  client.writePoint(sensorEC);

  // ------------------------------------
  // 6. ส่งข้อมูลเข้า InfluxDB และอัปเดตสถานะบนจอ
  // ------------------------------------
  tft.setTextSize(2);
  tft.setCursor(10, 200);

  if (wifiMulti.run() == WL_CONNECTED) {
    if (client.flushBuffer()) {
      Serial.println("ส่งข้อมูลขึ้น InfluxDB สำเร็จ!");
      tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
      tft.print("Status: InfluxDB OK!   ");
    } else {
      Serial.println(client.getLastErrorMessage());
      tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
      tft.print("Status: Send Error!    ");
    }
  } else {
      Serial.println("WiFi หลุด");
      tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
      tft.print("Status: WiFi Offline!  ");
  }

  delay(5000);
}