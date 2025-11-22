/*****************************************************
 *  NodeMCU + HX711 Load Cell + LCD + Web Page
 *  DT  -> D5
 *  SCK -> D4
 *  SDA -> D2
 *  SCL -> D1
 *****************************************************/

#include <ESP8266WiFi.h>
#include "HX711.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- LCD ----------
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change address if needed

// ---------- HX711 ----------
#define DT_PIN D5
#define SCK_PIN D4
HX711 scale;

// ---------- WiFi ----------
const char* ssid = "vivo Y29";
const char* password = "00001111";

WiFiServer server(80);

// ---------- CALIBRATION ----------
float calibration_factor = -7050;  // Adjust for your load cell

void setup() {
  Serial.begin(115200);

  // LCD init
  Wire.begin(D2, D1); // SDA, SCL
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Load Cell Boot");
  delay(1000);
  lcd.clear();

  // HX711 init
  scale.begin(DT_PIN, SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare();

  Serial.println("HX711 Ready");
  lcd.setCursor(0,0);
  lcd.print("HX711 Ready");
  delay(1000);
  lcd.clear();

  // WiFi connect
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  lcd.setCursor(0,0);
  lcd.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP().toString());
  delay(1500);
  lcd.clear();

  // Start Web Server
  server.begin();
}

void loop() {
  // Read weight
  float weight = scale.get_units(5);

  // ---------- Serial ----------
  Serial.print("Weight: ");
  Serial.print(weight, 2);
  Serial.println(" g");

  // ---------- LCD ----------
  lcd.setCursor(0,0);
  lcd.print("Weight:        "); // clear text
  lcd.setCursor(0,1);
  lcd.print("                "); // clear line
  lcd.setCursor(0,1);
  lcd.print(weight, 2);
  lcd.print(" g");

  // ---------- Web Page ----------
  WiFiClient client = server.available();
  if (client) {
    while (!client.available()) delay(1);

    client.readStringUntil('\r'); // ignore request

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Refresh: 1"); // auto-refresh every second
    client.println();
    client.println("<html><head><title>Load Cell</title></head>");
    client.println("<body style='text-align:center;font-family:Arial;'>");
    client.println("<h1>Load Cell Weight</h1>");
    client.print("<h2 style='font-size:60px;'>");
    client.print(weight, 2);
    client.println(" g</h2>");
    client.println("<p>Auto refresh every 1 second</p>");
    client.println("</body></html>");

    delay(10);
  }

  delay(1000); // 1 second update
}
