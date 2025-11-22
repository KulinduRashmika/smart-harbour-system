#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

// -------- WiFi --------
#define WIFI_SSID "vivo Y29"
#define WIFI_PASSWORD "00001111"

// -------- Firebase --------
#define API_KEY "AIzaSyBtYYTT6L-b3JLbOfQ2BMBcsCz2SonhOyo"
#define DATABASE_URL "https://waterlevel-fbd94-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const char* USER_EMAIL = "danujadewnith@gmail.com";
const char* USER_PASSWORD = "Danuja2003@";

// -------- Sensor & LED Pins --------
#define WATER_SENSOR A0        // Analog water level sensor
#define GREEN_LED D7           // Normal water level
#define RED_LED D8             // High water level
#define BUZZER D4              // Buzzer for HIGH water or danger
#define ULTRASONIC_TRIG D5     // Ultrasonic TRIG
#define ULTRASONIC_ECHO D6     // Ultrasonic ECHO
#define DANGER_LED D3          // New LED for ship too close

// -------- Thresholds --------
float lowLevel = 300;     // LOW water threshold
float highLevel = 700;    // HIGH water threshold
float waterLevel;
long dangerDistance = 5;  // Danger zone distance in cm

// -------- NTP for timestamp --------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800); // GMT+5:30

void setup() {
  Serial.begin(115200);

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Pins
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(DANGER_LED, OUTPUT);
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);

  // NTP
  timeClient.begin();
}

long readUltrasonicCM() {
  digitalWrite(ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);
  long duration = pulseIn(ULTRASONIC_ECHO, HIGH);
  long distance = duration * 0.034 / 2; // cm
  return distance;
}

void loop() {
  timeClient.update();
  String timestamp = timeClient.getFormattedTime();

  // Read water level
  waterLevel = analogRead(WATER_SENSOR);

  // Read ultrasonic distance
  long distanceCM = readUltrasonicCM();

  Serial.println("Water Level: " + String(waterLevel) + " | Distance: " + String(distanceCM) + " cm at " + timestamp);

  // LED & Buzzer logic for water
  String waterAlert = "";
  if (waterLevel < lowLevel) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER, LOW);
    waterAlert = "Water level LOW!";
  } else if (waterLevel <= highLevel) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);
    waterAlert = "Water level NORMAL";
  } else {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER, LOW);
    waterAlert = "Water level HIGH!";
  }

  // Danger zone for ship
  String dangerAlert = "";
  if (distanceCM <= dangerDistance) {
    digitalWrite(DANGER_LED, HIGH);
    digitalWrite(BUZZER, HIGH);
    dangerAlert = "Danger! Ship too close!";
  } else {
    digitalWrite(DANGER_LED, LOW);
    if(waterLevel <= highLevel) digitalWrite(BUZZER, LOW); // Only high water triggers buzzer
    dangerAlert = "Safe distance";
  }

  // Prepare JSON to send to Firebase
  FirebaseJson json;
  json.set("waterLevel", waterLevel);
  json.set("waterAlert", waterAlert);
  json.set("distanceCM", distanceCM);
  json.set("dangerAlert", dangerAlert);
  json.set("timestamp", timestamp);

  // Update current value
  if (Firebase.RTDB.setJSON(&fbdo, "/harbour/current", &json)) {
    Serial.println("[Firebase Current] ✅ Updated successfully");
  } else {
    Serial.println("[Firebase Current] ❌ Update failed: " + fbdo.errorReason());
  }

  // Add to history for graph
  if (Firebase.RTDB.pushJSON(&fbdo, "/harbour/history", &json)) {
    Serial.println("[Firebase History] ✅ Pushed successfully");
  } else {
    Serial.println("[Firebase History] ❌ Push failed: " + fbdo.errorReason());
  }

  Serial.println("----------------------------------------------------");
  delay(1000);
}
