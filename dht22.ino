#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>

// -------- WiFi ----------
#define WIFI_SSID "vivo Y29"
#define WIFI_PASSWORD "00001111"

// -------- Firebase ----------
#define API_KEY "AIzaSyCCY4Puk13qOIMhLr7gkhudmCMimC3-7mw"
#define DATABASE_URL "https://my-project-de1f7-default-rtdb.asia-southeast1.firebasedatabase.app" 
#define USER_EMAIL "pasindumadusanka041@gmail.com"
#define USER_PASSWORD "Pasindu2002"

// -------- DHT22 ----------
#define DHTPIN D2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);
  dht.begin();

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
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("DHT ERROR!");
    return;
  }

  Serial.print("Temp: "); Serial.print(t);
  Serial.print("°C | Humidity: "); Serial.println(h);

  Firebase.RTDB.setFloat(&fbdo, "/DHT22/Temperature", t);
  Firebase.RTDB.setFloat(&fbdo, "/DHT22/Humidity", h);

  delay(1000);
}
