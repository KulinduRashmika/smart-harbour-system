#include "esp_camera.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "base64.h"
#include <time.h>  

// CAMERA MODEL
#define CAMERA_MODEL_AI_THINKER

// CAMERA PINS 
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_PIN 4

// WIFI
const char* ssid = "vivo Y29";
const char* password = "00001111";

// FIREBASE
#define API_KEY "AIzaSyA5yRrPS3yJpjeyxmDx2liLIstOsS3XsDw"
#define DATABASE_URL "https://esp32-camera-project-f9162-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "test@test.com"
#define USER_PASSWORD "12345678"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String lastFlashState = "OFF";

// Get current UNIX time in seconds
long getTimestampSeconds() {
  time_t now;
  time(&now);
  return (long)now;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, LOW);

  // CAMERA SETUP
  camera_config_t cam;
  cam.ledc_channel = LEDC_CHANNEL_0;
  cam.ledc_timer = LEDC_TIMER_0;
  cam.pin_d0 = Y2_GPIO_NUM;
  cam.pin_d1 = Y3_GPIO_NUM;
  cam.pin_d2 = Y4_GPIO_NUM;
  cam.pin_d3 = Y5_GPIO_NUM;
  cam.pin_d4 = Y6_GPIO_NUM;
  cam.pin_d5 = Y7_GPIO_NUM;
  cam.pin_d6 = Y8_GPIO_NUM;
  cam.pin_d7 = Y9_GPIO_NUM;
  cam.pin_xclk = XCLK_GPIO_NUM;
  cam.pin_pclk = PCLK_GPIO_NUM;
  cam.pin_vsync = VSYNC_GPIO_NUM;
  cam.pin_href = HREF_GPIO_NUM;
  cam.pin_sscb_sda = SIOD_GPIO_NUM;
  cam.pin_sscb_scl = SIOC_GPIO_NUM;
  cam.pin_pwdn = PWDN_GPIO_NUM;
  cam.pin_reset = RESET_GPIO_NUM;
  cam.xclk_freq_hz = 20000000;
  cam.pixel_format = PIXFORMAT_JPEG;

  // SUPER LOW SIZE = small Base64 = NO SSL ERROR
  cam.frame_size = FRAMESIZE_QQVGA; // 160x120
  cam.jpeg_quality = 45;            // higher = more compression
  cam.fb_count = 1;

  if (esp_camera_init(&cam) != ESP_OK) {
    Serial.println("❌ Camera failed");
    return;
  }

  // WIFI
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }
  Serial.println("\nWiFi OK");
  WiFi.setSleep(false);

  // FIREBASE
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Firebase.RTDB.setString(&fbdo, "/control/flash", "OFF");

  // NTP TIME SYNC (UTC)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Syncing time");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n⏰ Time synchronized!");
}

// ---------------- FLASH CONTROL --------------------
void checkFlashControl() {
  if (!Firebase.ready()) return;

  if (Firebase.RTDB.getString(&fbdo, "/control/flash")) {
    String s = fbdo.stringData();
    s.trim();
    digitalWrite(FLASH_PIN, s == "ON" ? HIGH : LOW);

    if (s != lastFlashState) {
      lastFlashState = s;
      Serial.println("Flash: " + s);
    }
  }
}

// ------------- SUPER SAFE CHUNK UPLOAD -------------
void uploadChunks(String base64) {
  int chunkSize = 800;   
  int len = base64.length();

  long ts = getTimestampSeconds();          
  String basePath = "/images/" + String(ts); 

  Serial.println("Uploading img at ts=" + String(ts) + ", len=" + String(len));

  for (int i = 0; i < len; i += chunkSize) {
    String part = base64.substring(i, min(i + chunkSize, len));
    String path = basePath + "/chunk" + String(i / chunkSize);

    if (!Firebase.RTDB.setString(&fbdo, path.c_str(), part)) {
      Serial.println("❌ Chunk FAILED: " + fbdo.errorReason());
      return;
    }

    delay(50);   
  }

  Serial.println("✅ Upload complete");
}

// -------------------- CAPTURE -----------------------
void sendImage() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Capture fail");
    return;
  }

  String b64 = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  uploadChunks(b64);
}

void loop() {
  checkFlashControl();
  sendImage();
  delay(8000); 
}
