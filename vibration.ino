#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>

// -------- WiFi ----------
#define WIFI_SSID "vivo Y29"
#define WIFI_PASSWORD "00001111"

// -------- Firebase ----------
#define API_KEY "AIzaSyCCY4Puk13qOIMhLr7gkhudmCMimC3-7mw"
#define DATABASE_URL "https://my-project-de1f7-default-rtdb.asia-southeast1.firebasedatabase.app" 
#define USER_EMAIL "pasindumadusanka041@gmail.com"
#define USER_PASSWORD "Pasindu2002"

// -------- Vibration Sensor ----------
#define VIBRATION_PIN D5

// -------- LED for Door State ----------
#define DOOR_LED_PIN D7

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);

  pinMode(VIBRATION_PIN, INPUT);
  pinMode(DOOR_LED_PIN, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi Connected!");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  int vibration = digitalRead(VIBRATION_PIN);
  Serial.print("Vibration: "); Serial.println(vibration);

  Firebase.RTDB.setInt(&fbdo, "/Vibration/State", vibration);

  // --- Door LED Control ---
  if (Firebase.RTDB.getInt(&fbdo, "/EmergencyDoor/State")) {
    int doorState = fbdo.intData();
    digitalWrite(DOOR_LED_PIN, doorState == 1 ? HIGH : LOW);
  }

  delay(500);
}
