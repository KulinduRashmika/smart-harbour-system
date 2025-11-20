#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>

// -------- WiFi ----------
#define WIFI_SSID "vivo Y29"
#define WIFI_PASSWORD "00001111"

// -------- Firebase --------
#define API_KEY "AIzaSyCL44i_1iyrdc7wBAWjiHHkJCf68Bl3Xgg"
#define DATABASE_URL "https://dht22-404f0-default-rtdb.asia-southeast1.firebasedatabase.app/"   // Must end with /

// -------- Login ----------
#define USER_EMAIL "danujadewnith@gmail.com"
#define USER_PASSWORD "Danuja2003@"

// -------- DHT22 Sensor --------
#define DHTPIN D2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// -------- Sensors & Buzzer --------
#define VIBRATION_PIN D5
#define GAS_PIN A0        // Analog pin for gas sensor
#define BUZZER_PIN D4

// -------- Gas sensor threshold --------
#define GAS_THRESHOLD 160  // 0-1023, adjust as needed

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);

  pinMode(VIBRATION_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  dht.begin();

  // WiFi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi Connected!");
  Serial.println(WiFi.localIP());

  // Firebase setup
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Serial.println("Initializing Firebase...");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase Ready!");
}

void loop() {
  // Read DHT22
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Read sensors
  int vibration = digitalRead(VIBRATION_PIN);
  int gasValue = analogRead(GAS_PIN); // 0-1023
  int gasDetected = (gasValue > GAS_THRESHOLD) ? 1 : 0; // 1 = gas detected, 0 = safe

  // Check DHT error
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT22!");
    return;
  }

  // Print values
  Serial.print("Temp: "); Serial.print(t);
  Serial.print("°C  |  Humidity: "); Serial.print(h);
  Serial.print("%  |  Vibration: "); Serial.print(vibration);
  Serial.print("  |  Gas Value: "); Serial.print(gasValue);
  Serial.print("  |  Gas Detected: "); Serial.println(gasDetected);

  // Send to Firebase
  Firebase.RTDB.setFloat(&fbdo, "/DHT22/Temperature", t);
  Firebase.RTDB.setFloat(&fbdo, "/DHT22/Humidity", h);
  Firebase.RTDB.setInt(&fbdo, "/Vibration/State", vibration);
  Firebase.RTDB.setInt(&fbdo, "/Gas/State", gasDetected);

  // Activate buzzer if vibration or gas detected
  // Buzzer logic (replace your current digitalWrite)
if (vibration == HIGH || gasDetected == 1) {
  tone(BUZZER_PIN, 1000); // 1000 Hz beep
  Serial.println("⚠️ Alert! Sensor triggered!");
} else {
  noTone(BUZZER_PIN);     // stop buzzer
}


  delay(300); // Update every 3 seconds
}
