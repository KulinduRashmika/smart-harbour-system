#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <time.h>

// ---------- PINS ----------
#define SS_PIN     D2
#define RST_PIN    D1
#define SERVO_PIN  D3
#define BUZZER_PIN D8
#define LED        D4 // Green LED for valid user

// ---------- WIFI ----------
const char* WIFI_SSID = "vivo Y29";
const char* WIFI_PASSWORD = "00001111";

// ---------- FIREBASE CONFIG ----------
#define API_KEY "AIzaSyDYFcMA_Y3Bw1fbI5qE29itBjr0KfzajW4"
#define DATABASE_URL "https://exit-gate-714e9-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const char* USER_EMAIL = "danujadewnith@gmail.com";
const char* USER_PASSWORD = "Danuja2003@";

// ---------- RFID AUTH ----------
String authorizedUIDs[] = {"DD7AD505", "D9F26A05"};
String authorizedNames[] = {"Danuja", "Kulindu"};
int totalCards = 2;

// ---------- OBJECTS ----------
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo gateServo;

// ---------- BUZZER TIMERS ----------
bool invalidBuzzerActive = false;
unsigned long invalidBuzzerStart = 0;
const unsigned long invalidBuzzerDuration = 2000; // 2 seconds for invalid user

// ---------- FUNCTIONS ----------
String checkAuthorization(String uid) {
  uid.toUpperCase();
  for (int i = 0; i < totalCards; i++) {
    if (uid == authorizedUIDs[i]) return authorizedNames[i];
  }
  return "";
}

String getTimestamp() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char buffer[30];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
          t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
          t->tm_hour, t->tm_min, t->tm_sec);
  return String(buffer);
}

// ---------- FIREBASE ----------
void sendToFirebase(String name, String uid) {
  // Current status
  String livePath = "/CurrentStatus/" + uid;
  FirebaseJson liveData;
  liveData.set("name", name);
  liveData.set("uid", uid);
  liveData.set("time", getTimestamp());
  liveData.set("status", "EXIT");
  Firebase.RTDB.setJSON(&fbdo, livePath.c_str(), &liveData);

  // Exit logs
  String logPath = "/ExitLogs/" + String(millis());
  FirebaseJson logData;
  logData.set("name", name);
  logData.set("uid", uid);
  logData.set("time", getTimestamp());
  logData.set("status", "EXIT");
  Firebase.RTDB.setJSON(&fbdo, logPath.c_str(), &logData);
}

// ---------- BUZZER ----------
void startInvalidBuzzer() {
  tone(BUZZER_PIN, 2000);
  invalidBuzzerActive = true;
  invalidBuzzerStart = millis();
}

void handleBuzzers() {
  if (invalidBuzzerActive && millis() - invalidBuzzerStart >= invalidBuzzerDuration) {
    noTone(BUZZER_PIN);
    invalidBuzzerActive = false;
  }
}

// ---------- GATE ----------
void openGate(String name, String uid) {
  Serial.println("Opening gate for: " + name);
  sendToFirebase(name, uid);
  gateServo.write(180);
  delay(3000);
  gateServo.write(0);
}

// ---------- HANDLE WEB DOOR TOGGLE ----------
void handleDoorCommand() {
  if (Firebase.RTDB.getInt(&fbdo, "/door/status")) {
    int cmd = fbdo.intData();
    static int lastCmd = -1;
    if (cmd != lastCmd) {
      if (cmd == 1) {          // Open gate
        Serial.println("Gate opened from web!");
        gateServo.write(180);
        delay(3000);
        gateServo.write(0);
      } else if (cmd == 0) {   // Close gate
        Serial.println("Gate closed from web!");
        gateServo.write(0);
      }
      lastCmd = cmd;
    }
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW); // LED off initially

  SPI.begin();
  mfrc522.PCD_Init();
  delay(100);

  gateServo.attach(SERVO_PIN);
  gateServo.write(0);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nConnected: " + WiFi.localIP().toString());

  configTime(19800, 0, "pool.ntp.org");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase Ready");
}

// ---------- LOOP ----------
void loop() {
  handleBuzzers();       // handle invalid buzzer timing
  handleDoorCommand();   // check web toggle

  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  Serial.println("RFID EXIT SCAN: " + uid);

  String name = checkAuthorization(uid);

  if (name != "") {
    Serial.println("EXIT Granted: " + name);
    digitalWrite(LED, HIGH);   // Turn on green LED
    openGate(name, uid);
    digitalWrite(LED, LOW);    // Turn off LED after gate closes
  } else {
    Serial.println("EXIT DENIED");
    startInvalidBuzzer(); // Buzzer for invalid user
  }

  mfrc522.PICC_HaltA();
}
