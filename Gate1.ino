#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <LittleFS.h>
#include <time.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// ---------------- RFID + SERVO ---------------
#define SS_PIN   D8
#define RST_PIN  D3
#define SERVO_PIN D1

// ---------------- BUZZER ----------------
#define BUZZER_PIN D2

// ---------------- WIFI ----------------
const char* ssid = "Galaxy A21s";
const char* password = "12345678";

// ---------------- MASTER PASSWORD ----------------
const String masterPassword = "1234";

// ---------------- OBJECTS ----------------
MFRC522 mfrc522(SS_PIN, RST_PIN);
ESP8266WebServer server(80);
Servo doorServo;

// ---------------- AUTHORIZED CARDS ----------------
String authorizedUIDs[] = {"DD7AD505", "9A5AA904"};
String authorizedNames[] = {"Danuja", "kalpa"};
int totalCards = 2;

// ---------------- STATE ----------------
String lockStatus = "Locked";
String lastPerson = "None";
bool emergencyLock = false;

// ---------------- LOG FILE ----------------
const char *LOG_PATH = "/access_log.csv";

// ---------------- FIREBASE ----------------
String firebaseURL = "https://gateentrylist-33fad-default-rtdb.asia-southeast1.firebasedatabase.app/entries.json";

// =======================================================
// TIME
// =======================================================
String getTimestamp() {
  time_t now = time(nullptr);

  if (now < 1600000000) {
    unsigned long sec = millis() / 1000;
    unsigned long mins = (sec / 60) % 60;
    unsigned long hours = (sec / 3600) % 24;
    char buf[32];
    sprintf(buf, "uptime %02lu:%02lu:%02lu", hours, mins, sec % 60);
    return String(buf);
  }

  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char buf[32];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday,
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec);
  return String(buf);
}

// =======================================================
// FIREBASE UPLOAD
// =======================================================
void sendToFirebase(String name) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Firebase skipped: WiFi not connected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.begin(client, firebaseURL);

  String json = "{\"name\":\"" + name + "\",\"time\":\"" + getTimestamp() + "\"}";
  https.addHeader("Content-Type", "application/json");

  int httpCode = https.POST(json);

  if (httpCode > 0) Serial.println("Firebase => " + https.getString());
  else Serial.println("Firebase Error: " + String(httpCode));

  https.end();
}

// =======================================================
// LITTLEFS LOGGING
// =======================================================
void appendLog(const String &name) {
  if (!LittleFS.begin()) return;

  File f = LittleFS.open(LOG_PATH, "a");
  if (!f) { LittleFS.end(); return; }

  f.println(name + "," + getTimestamp());
  f.close();
  LittleFS.end();
}

String readLogAsJSON() {
  String json = "[";

  if (!LittleFS.begin()) return "[]";
  File f = LittleFS.open(LOG_PATH, "r");
  if (!f) { LittleFS.end(); return "[]"; }

  bool first = true;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;

    int c = line.indexOf(',');
    String name = line.substring(0, c);
    String ts = line.substring(c + 1);

    if (!first) json += ",";
    json += "{\"name\":\"" + name + "\",\"time\":\"" + ts + "\"}";
    first = false;
  }

  f.close();
  LittleFS.end();
  json += "]";
  return json;
}

// =======================================================
// BUZZER FUNCTIONS
// =======================================================

void denyBeep() {  
  digitalWrite(BUZZER_PIN, HIGH); delay(200);
  digitalWrite(BUZZER_PIN, LOW);  delay(200);
  digitalWrite(BUZZER_PIN, HIGH); delay(200);
  digitalWrite(BUZZER_PIN, LOW);
}


void wrongCardAlarm() {
  Serial.println("!!! WRONG RFID - BUZZER ALARM ACTIVATED !!!");

  digitalWrite(BUZZER_PIN, HIGH);   
  unsigned long start = millis();

  while (millis() - start < 30000) { 
    server.handleClient();  
    delay(10);
  }

  digitalWrite(BUZZER_PIN, LOW);    
}

void successBeep() {
  digitalWrite(BUZZER_PIN, HIGH); delay(150);
  digitalWrite(BUZZER_PIN, LOW);  delay(150);
  digitalWrite(BUZZER_PIN, HIGH); delay(150);
  digitalWrite(BUZZER_PIN, LOW);
}


// DOOR CONTROL

void unlockDoor(String name) {
  if (emergencyLock) return;

  lockStatus = "Unlocked by " + name;
  lastPerson = name;

  appendLog(name);
  sendToFirebase(name);

  successBeep();

  doorServo.write(180);
  delay(5000);
  doorServo.write(0);

  lockStatus = "Locked";
}

String checkAuthorization(String tagUID) {
  tagUID.toUpperCase();
  for (int i = 0; i < totalCards; i++)
    if (tagUID == authorizedUIDs[i]) return authorizedNames[i];

  return "";
}

// =======================================================
// ADMIN PASSWORD UNLOCK
// =======================================================
void handlePasswordUnlock() {
  String pw = server.arg("pw");

  if (pw == masterPassword) {
    emergencyLock = false;
    lockStatus = "Unlocked by Admin";
    lastPerson = "Admin";

    appendLog("Admin Password");
    sendToFirebase("Admin Password");

    successBeep();

    doorServo.write(180);
    delay(5000);
    doorServo.write(0);

    lockStatus = "Locked";

    server.send(200, "text/plain", "Unlocked");
  }
  else {
    server.send(200, "text/plain", "Wrong Password!");
  }
}

// =======================================================
// HTML PAGE
// =======================================================
String htmlPage() {
  String html =
"<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>RFID Door</title>"
"<style>"
"body{font-family:Arial;background:#0D1B2A;color:white;text-align:center}"
".card{background:#1B263B;margin:20px;padding:20px;border-radius:12px}"
".status{padding:12px;border-radius:10px;font-size:20px}"
".locked{background:#B00020}.unlocked{background:#2E7D32}.lockdown{background:#FFA000;color:black}"
"table{width:100%;margin-top:10px;border-collapse:collapse}"
"th,td{padding:8px;border-bottom:1px solid #243447;text-align:left}"
".popup{position:fixed;top:0;left:0;width:100%;height:100%;background:#000a;display:flex;justify-content:center;align-items:center;display:none}"
".box{background:white;color:black;padding:20px;border-radius:10px;width:300px}"
"input{width:100%;padding:10px;margin-top:10px;font-size:16px}"
"</style></head><body>"

"<div class='card'>"
"<h2>RFID Door Dashboard</h2>"
"<div id='status' class='status locked'>Loading...</div>"
"<div><b>Last:</b> <span id='last'>-</span></div>"

"<button onclick=\"fetch('/unlock')\">Unlock</button>"
"<button onclick=\"fetch('/lock')\">Lock</button>"
"<button onclick=\"fetch('/lockdown')\">Lockdown</button>"
"<button onclick=\"document.getElementById('pop').style.display='flex'\">Admin Unlock</button>"

"<div id='pop' class='popup'>"
" <div class='box'>"
"   <h3>Enter Admin Password</h3>"
"   <input id='pw' type='password' placeholder='1234'/>"
"   <button onclick='sendPW()'>Unlock</button>"
"   <button onclick=\"document.getElementById('pop').style.display='none'\">Cancel</button>"
" </div>"
"</div>"

"<h3>Access History</h3>"
"<div id='history'>Loading...</div>"

"<script>"
"function sendPW(){"
" let p=document.getElementById('pw').value;"
" fetch('/pwunlock?pw='+p).then(r=>r.text()).then(t=>{alert(t);document.getElementById('pop').style.display='none';});"
"}"

"function update(){"
"fetch('/data').then(r=>r.json()).then(d=>{"
"document.getElementById('status').innerText=d.status;"
"document.getElementById('last').innerText=d.last;"
"document.getElementById('status').className='status '+d.class;"
"});"

"fetch('/logjson').then(r=>r.json()).then(arr=>{"
"let t='<table><tr><th>Name</th><th>Date & Time</th></tr>';"
"for(let i=arr.length-1;i>=0;i--) t+=`<tr><td>${arr[i].name}</td><td>${arr[i].time}</td></tr>`;"
"t+='</table>'; document.getElementById('history').innerHTML=t;"
"});"
"}"

"setInterval(update,1000); update();"
"</script></div></body></html>";

  return html;
}

// =======================================================
// HTTP HANDLERS
// =======================================================
void handleRoot() { server.send(200, "text/html", htmlPage()); }

void handleData() {
  String cls = emergencyLock ? "lockdown" :
               (lockStatus.startsWith("Unlocked") ? "unlocked" : "locked");

  String json = "{\"status\":\"" + lockStatus +
                "\",\"last\":\"" + lastPerson +
                "\",\"class\":\"" + cls + "\"}";

  server.send(200, "application/json", json);
}

void handleUnlock() {
  if (emergencyLock) { server.send(200,"text/plain","EMERGENCY LOCKDOWN!"); return; }
  unlockDoor("Manual Unlock");
  server.send(200, "text/plain", "OK");
}

void handleLock() {
  doorServo.write(0);
  lockStatus="Locked";
  server.send(200,"text/plain","OK");
}

void handleLockdown() {
  emergencyLock=true;
  lockStatus="EMERGENCY LOCKDOWN";
  server.send(200,"text/plain","OK");
}

void handleLogJson() { server.send(200, "application/json", readLogAsJSON()); }

void handleDownload() {
  if (!LittleFS.begin()) { server.send(500, "text/plain", "FS Failed"); return; }
  if (!LittleFS.exists(LOG_PATH)) { server.send(404,"text/plain","No Log"); return; }

  File f = LittleFS.open(LOG_PATH, "r");
  server.streamFile(f, "text/csv");
  f.close();
  LittleFS.end();
}

// =======================================================
// SETUP
// =======================================================
void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  doorServo.attach(SERVO_PIN);
  doorServo.write(0);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (LittleFS.begin()) {
    if (!LittleFS.exists(LOG_PATH)) {
      File f = LittleFS.open(LOG_PATH, "w");
      f.println("Name,Timestamp");
      f.close();
    }
    LittleFS.end();
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  configTime(19800, 0, "pool.ntp.org");

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/unlock", handleUnlock);
  server.on("/lock", handleLock);
  server.on("/lockdown", handleLockdown);
  server.on("/pwunlock", handlePasswordUnlock);
  server.on("/logjson", handleLogJson);
  server.on("/download", handleDownload);

  server.begin();
}

// =======================================================
// LOOP
// =======================================================
void loop() {
  server.handleClient();

  if (emergencyLock) return;

  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  Serial.println("Scanned UID: " + uid);

  String name = checkAuthorization(uid);

  if (name != "") {
    unlockDoor(name);
  } 
  else {
    Serial.println("Access Denied!");
    wrongCardAlarm();   
  }

  mfrc522.PICC_HaltA();
}
