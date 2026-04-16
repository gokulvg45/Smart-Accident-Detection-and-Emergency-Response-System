/*
 Accident Detection System
 Uses:
 - ESP8266
 - MPU6050
 - GPS
 - GSM
 - LCD

 Features:
 - Detects accidents
 - Sends SMS alerts
 - Finds nearest hospital
*/

#include <ESP8266WiFi.h>
#include <math.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include <MPU6050.h>
#include <LiquidCrystal_I2C.h>

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// WiFi
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
String server = "http://YOUR_SERVER_IP:5000/data";



WiFiClient client;

// GPS
SoftwareSerial gpsSerial(D5, D6);
TinyGPSPlus gps;

// GSM
SoftwareSerial gsm(D7, D8);

// MPU6050
MPU6050 mpu;

// Pins
#define BUTTON D3
#define BUZZER D4

// Numbers
String num1 = "+91XXXXXXXXXX";
String num2 = "+91XXXXXXXXXX";
String hospital1 = "+91XXXXXXXXXX";
String hospital2 = "+91XXXXXXXXXX";

// Location
float lat = 0.0;
float lon = 0.0;

// ✅ BALANCED VALUES
const float acc_threshold = 1.3;
const float gyro_threshold = 80;

bool accidentDetected = false;

void setup() {

  Serial.begin(9600);
  Serial.println("🚀 System Starting...");

  lcd.init();
  lcd.backlight();
  lcd.print("Accident System");
  lcd.setCursor(0,1);
  lcd.print("Starting...");
  delay(2000);
  lcd.clear();

  Wire.begin(D2, D1);
  mpu.initialize();

  gpsSerial.begin(9600);
  gsm.begin(9600);

  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  lcd.print("Connecting WiFi");
  WiFi.begin(ssid, password);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    count++;
    if (count > 20) {
  Serial.println("WiFi Failed");
  break;
  }
}

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected");
    lcd.print("WiFi Connected");
  } else {
    Serial.println("\nOffline Mode");
    lcd.print("Offline Mode");
  }

  delay(2000);
  lcd.clear();
  lcd.print("System Ready");
}

// 📩 SMS
void sendSMS(String number, String message) {

  Serial.println("📩 Sending SMS to: " + number);

  lcd.clear();
  lcd.print("Sending SMS");

  gsm.println("AT+CMGF=1");
  delay(1000);

  gsm.print("AT+CMGS=\"");
  gsm.print(number);
  gsm.println("\"");
  delay(1000);

  gsm.print(message);
  delay(1000);

  gsm.write(26);
  delay(6000);

  gsm.println("AT");
  delay(2000);

  Serial.println("✅ Sent to: " + number);
}

// 📍 GPS
void getGPS() {

  Serial.println("📍 Getting GPS...");
  lcd.clear();
  lcd.print("Getting GPS");

  unsigned long start = millis();

  while (millis() - start < 5000) {
    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
    }
  }

  if (gps.location.isValid()) {

    lat = gps.location.lat();
    lon = gps.location.lng();

    Serial.print("Lat: ");
    Serial.println(lat,6);
    Serial.print("Lon: ");
    Serial.println(lon,6);

    lcd.clear();
    lcd.print("GPS OK");
  } else {
    Serial.println("GPS Failed");
    lcd.clear();
    lcd.print("GPS Failed");
  }

  delay(1500);
}

// 🚨 Accident detection (FILTERED)
bool checkAccident() {

  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  float acc = sqrt(ax*ax + ay*ay + az*az) / 16384.0;

  Serial.print("ACC: ");
  Serial.print(acc);
  Serial.print(" GX: ");
  Serial.print(gx);
  Serial.print(" GY: ");
  Serial.println(gy);

  if (acc > acc_threshold && (abs(gx) > gyro_threshold || abs(gy) > gyro_threshold)) {

    delay(200); // confirm

    int16_t ax2, ay2, az2, gx2, gy2, gz2;
    mpu.getMotion6(&ax2, &ay2, &az2, &gx2, &gy2, &gz2);

    float acc2 = sqrt(ax2*ax2 + ay2*ay2 + az2*az2) / 16384.0;

    if (acc2 > acc_threshold) {
      Serial.println("🚨 Accident Confirmed!");
      return true;
    }
  }

  return false;
}

// 🌐 INTERNET MODE
void internetMode() {

  Serial.println("🌐 Internet Mode");

  HTTPClient http;
  String url = server + "?lat=" + String(lat,6) + "&lon=" + String(lon,6);

  lcd.clear();
  lcd.print("Finding Hospital");

  http.begin(client, url);
  int code = http.GET();

  if (code > 0) {

    String payload = http.getString();
    Serial.println(payload);

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, payload)) return;

    String hname = doc["name"];
    float hlat = doc["lat"];
    float hlon = doc["lon"];

    String msg = "Accident detected! Need help!\nLocation:\n";
    msg += "https://maps.google.com/?q=";
    msg += String(lat,6) + "," + String(lon,6);
    msg += "\nHospital: " + hname;
    msg += "\nhttps://maps.google.com/?q=";
    msg += String(hlat,6) + "," + String(hlon,6);

    String hmsg = "Accident detected!\nLocation:\n";
    hmsg += "https://maps.google.com/?q=";
    hmsg += String(lat,6) + "," + String(lon,6);

    sendSMS(num1, msg); delay(4000);
    sendSMS(num2, msg); delay(4000);
    sendSMS(hospital1, hmsg); delay(4000);
    sendSMS(hospital2, hmsg); delay(4000);
  }

  http.end();
}

// ❌ OFFLINE MODE
void offlineMode() {

  Serial.println("📴 Offline Mode");

  String msg = "Accident detected! Need help!\nLocation:\n";
  msg += "https://maps.google.com/?q=";
  msg += String(lat,6) + "," + String(lon,6);

  String hmsg = "Accident detected!\nLocation:\n";
  hmsg += "https://maps.google.com/?q=";
  hmsg += String(lat,6) + "," + String(lon,6);

  sendSMS(num1, msg); delay(4000);
  sendSMS(num2, msg); delay(4000);
  sendSMS(hospital1, hmsg); delay(4000);
  sendSMS(hospital2, hmsg); delay(4000);
}

// 🔁 LOOP
void loop() {

  if (checkAccident() && !accidentDetected) {

    accidentDetected = true;
    Serial.println("⏱ Countdown Started");

    for (int i = 60; i > 0; i--) {

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Accident!");
      lcd.setCursor(0,1);
      lcd.print("Wait: ");
      lcd.print(i);

      Serial.println(i);

      digitalWrite(BUZZER, HIGH);

      if (digitalRead(BUTTON) == LOW) {
        delay(200);
        if (digitalRead(BUTTON) == LOW) {
          digitalWrite(BUZZER, LOW);
          Serial.println("Cancelled");
          lcd.clear();
          lcd.print("Cancelled");
          accidentDetected = false;
          delay(2000);
          return;
        }
      }

      delay(500);
      digitalWrite(BUZZER, LOW);
      delay(500);
    }

    digitalWrite(BUZZER, LOW);

    getGPS();

    if (WiFi.status() == WL_CONNECTED) {
      internetMode();
    } else {
      offlineMode();
    }

    lcd.clear();
    lcd.print("Alert Sent");
    Serial.println("Alert Sent");

    delay(3000);
    lcd.clear();
    lcd.print("System Ready");

    delay(10000);
    accidentDetected = false;
  }

  delay(500);
}