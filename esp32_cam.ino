#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h> 
#include "secret_config.h"

// ===== 카메라 모델 선택 (AI Thinker ESP32-CAM 사용 시) =====
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// ===== 함수 선언 =====
void cameraInit();
bool connectWiFi();
void sendCapturedImage();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("\n[System] Booting...");

  // ===== 카메라 초기화 =====
  cameraInit();

  // ===== Wi-Fi 연결 =====
  if (!connectWiFi()) {
    Serial.println("[WiFi] 연결 실패! 재부팅합니다.");
    ESP.restart(); // Wi-Fi 연결 실패 시 자동 재부팅
  }
}

void loop() {
  // ===== 주기적 촬영 및 업로드 =====
  sendCapturedImage();
  delay(5000); // 5초마다 업로드
}

// ===== 카메라 초기화 함수 =====
void cameraInit() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;

  // 해상도 및 화질 설정
  config.frame_size = FRAMESIZE_VGA;  // UXGA(1600x1200), SVGA(800x600), QVGA(320x240)
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 12; 
  config.fb_count = 1;

  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }

  // 카메라 초기화
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[Camera] Init failed! Error 0x%x\n", err);
    while (true) {
      delay(1000);
    }
  }

  // 카메라 센서 옵션
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);      // 이미지 상하 반전
  s->set_brightness(s, 1); // 밝기 조정 (-2~2)
  s->set_saturation(s, 0); // 채도 조정 (-2~2)

  Serial.println("[Camera] Init success!");
}

// ===== Wi-Fi 연결 함수 (Debug & 타임아웃 추가) =====
bool connectWiFi() {
  Serial.printf("[WiFi] Connecting to SSID: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  const unsigned long timeout = 15000; // 15초 타임아웃

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] 연결 성공!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n[WiFi] 연결 실패 (타임아웃)");
    return false;
  }
}

// ===== 이미지 촬영 및 업로드 함수 =====
void sendCapturedImage() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[Camera] Capture failed!");
    return;
  }

  Serial.printf("[Camera] Captured image size: %u bytes\n", fb->len);

  // Wi-Fi 연결 확인
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] 연결 끊김, 재연결 시도...");
    if (!connectWiFi()) {
      Serial.println("[WiFi] 재연결 실패! 이미지 업로드 건너뜀.");
      esp_camera_fb_return(fb);
      return;
    }
  }

  // HTTP 업로드
  HTTPClient http;
  String uploadUrl = String(serverUrl) + "?ts=" + String(millis());
  http.begin(uploadUrl);
  http.addHeader("Content-Type", "image/jpeg");

  int res = http.POST(fb->buf, fb->len);
  if (res > 0) {
    Serial.printf("[Upload] Response code: %d\n", res);
  } else {
    Serial.printf("[Upload] Failed! Error code: %d\n", res);
  }
  http.end();

  esp_camera_fb_return(fb);
}