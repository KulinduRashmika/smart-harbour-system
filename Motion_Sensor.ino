#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>


#define WIFI_SSID      "Redmi 9T"
#define WIFI_PASSWORD  "12345678"


#define API_KEY        "AIzaSyA-2nDmVJpaFg1L36MIIbOaX7sMQf_NbGE"
#define DATABASE_URL   "https://motionsensor-7ad60-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


#define PIR_PIN D5
#define BUZZER_PIN D6


LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- NTP CONFIG (REAL TIME) ----------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800); 



bool lastMotionState = false;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 500;

bool systemEnabled = true; 

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
}


void setupFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = "kulindurashmika5@gmail.com";
  auth.user.password = "Rashmika@12";

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Connecting to Firebase...");
  unsigned long start = millis();
  while (!Firebase.ready() && millis() - start < 10000) {
    delay(100);
    Serial.print(".");
  }

  Serial.println();
  Serial.println(Firebase.ready() ? "Firebase Ready!" : "Firebase FAILED!");
}


String getFormattedDateTime() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();

  int yearVal = year(epochTime);
  int monthVal = month(epochTime);
  int dayVal = day(epochTime);
  int hourVal = hour(epochTime);
  int minuteVal = minute(epochTime);
  int secondVal = second(epochTime);

  String ampm = "AM";
  int displayHour = hourVal;

  if (hourVal == 0) displayHour = 12;
  else if (hourVal == 12) ampm = "PM";
  else if (hourVal > 12) {
    displayHour = hourVal - 12;
    ampm = "PM";
  }

  char buffer[30];
  sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d %s", 
          dayVal, monthVal, yearVal, displayHour, minuteVal, secondVal, ampm.c_str());

  return String(buffer);
}


void checkSystemState() {
  if (Firebase.getBool(fbdo, "/motion/systemEnabled")) {
    systemEnabled = fbdo.boolData();
  }
}


void sendMotionToFirebase(bool motionState) {
  if (!Firebase.ready()) return;

  Firebase.setBool(fbdo, "/motion/status", motionState);

  String logText = motionState ? "Shipped Arrived" : "No Ship Arrived";
  String timeStamp = getFormattedDateTime();

  FirebaseJson json;
  json.set("event", logText);
  json.set("time", timeStamp);

  Firebase.pushJSON(fbdo, "/motion/logs", json);

  Serial.print("Logged: ");
  Serial.print(logText);
  Serial.print(" at ");
  Serial.println(timeStamp);
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Motion System");
  delay(1500);

  connectToWiFi();
  setupFirebase();

  timeClient.begin();
  timeClient.update();
}


void loop() {
  checkSystemState(); 

  //  SYSTEM STOPPED MODE
  if (!systemEnabled) {
    digitalWrite(BUZZER_PIN, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Stopped");
    delay(300);
    return; 
  }

  // SYSTEM ACTIVE MODE
  bool motionDetected = digitalRead(PIR_PIN);

  unsigned long now = millis();
  if (motionDetected != lastMotionState || now - lastSendTime > sendInterval) {
    lastMotionState = motionDetected;
    lastSendTime = now;

    if (motionDetected) {
      Serial.println("Shipped Arrived");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Shipped Arrived");
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      Serial.println("No Ship Arrived");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("No Ship Arrived");
      digitalWrite(BUZZER_PIN, LOW);
    }

    sendMotionToFirebase(motionDetected);
  }

  delay(50);
}