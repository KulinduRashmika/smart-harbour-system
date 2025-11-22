#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// WiFi Login

#define WIFI_SSID "vivo Y29"
#define WIFI_PASSWORD "00001111"


// Pins

#define LDR_PIN D0
#define LED_PIN D1

ESP8266WebServer server(80);
bool manualControl = false;  
bool ledState = LOW;          

void handleRoot() {
  int ldrValue = digitalRead(LDR_PIN);


  String lightStatus = (ldrValue == HIGH) ? "DARK (Night)" : "BRIGHT (Morning)";
  String ledStatus = (digitalRead(LED_PIN) == HIGH) ? "ON" : "OFF";


  String html = "<html><head>"
                "<meta http-equiv='refresh' content='1'/>"
                "<style>"
                "body { font-family: Arial; background:#111; color:#0f0; text-align:center; padding-top:40px; }"
                ".box { background:#222; padding:20px; border-radius:10px; width:300px; margin:auto; }"
                "h1 { color:#00ff6a; }"
                "button { padding:10px 20px; margin:5px; border:none; border-radius:5px; cursor:pointer; }"
                ".on { background:#0f0; color:#000; }"
                ".off { background:#f00; color:#fff; }"
                "</style></head><body>"
                "<div class='box'>"
                "<h1>LDR Status</h1>"
                "<p><b>LDR Value:</b> " + String(ldrValue) + "</p>"
                "<p><b>Light Status:</b> " + lightStatus + "</p>"
                "<p><b>LED Status:</b> " + ledStatus + "</p>"
                "<form action='/led/on'><button class='on'>Turn LED ON</button></form>"
                "<form action='/led/off'><button class='off'>Turn LED OFF</button></form>"
                "<form action='/led/auto'><button class='on'>Resume Auto</button></form>"
                "</div></body></html>";

  server.send(200, "text/html", html);
}

// Turn LED ON manually
void handleLedOn() {
  manualControl = true;
  digitalWrite(LED_PIN, HIGH);
  ledState = HIGH;
  server.sendHeader("Location", "/");
  server.send(303);
}

// Turn LED OFF manually 
void handleLedOff() {
  manualControl = true;
  digitalWrite(LED_PIN, LOW);
  ledState = LOW;
  server.sendHeader("Location", "/");
  server.send(303); 
}

//  LDR control
void handleLedAuto() {
  manualControl = false;
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Web server routes
  server.on("/", handleRoot);
  server.on("/led/on", handleLedOn);
  server.on("/led/off", handleLedOff);
  server.on("/led/auto", handleLedAuto);

  server.begin();
}

void loop() {
  int ldrValue = digitalRead(LDR_PIN);

  if (!manualControl) {
    if (ldrValue == HIGH) {         // Morning for LED ON
      digitalWrite(LED_PIN, HIGH);
      ledState = HIGH;
    } else {                        // Night for LED OFF
      digitalWrite(LED_PIN, LOW);
      ledState = LOW;
    }
  }

  server.handleClient();
}
