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

// -------- Gas Sensor ----------
#define GAS_PIN A0
#define GAS_THRESHOLD 300

// -------- Buzzer ----------
#define BUZZER_PIN D4

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);

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
  int gasValue = analogRead(GAS_PIN);
  int gasDetected = (gasValue > GAS_THRESHOLD) ? 1 : 0;

  Serial.print("Gas Value: ");
  Serial.print(gasValue);
  Serial.print(" | Gas Detected: ");
  Serial.println(gasDetected);

  Firebase.RTDB.setInt(&fbdo, "/Gas/State", gasDetected);

  if (gasDetected == 1) {
    tone(BUZZER_PIN, 1000);
    Serial.println("⚠️ GAS ALERT!");
  } else {
    noTone(BUZZER_PIN);
  }

  delay(500);
}
