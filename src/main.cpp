/**
 * Freenove OV2640 Camera Web Server – streaming + snapshot + controls
 * Supports: Freenove ESP32-WROVER (FNK0060) and Freenove ESP32-S3 CAM
 * Based on Freenove FNK0060 C examples (Camera Web Server / Video Web Server).
 * https://docs.freenove.com/projects/fnk0060/en/latest/fnk0060/codes/C.html
 */
#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "board_config.h"

// WiFi: define in include/wifi_config.h (copy from wifi_config.h.example) or here
#ifdef __has_include
#if __has_include("wifi_config.h")
#include "wifi_config.h"
#endif
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "********"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "********"
#endif

void startCameraServer();
void setupLedFlash();
void camera_init();

camera_config_t config;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("Freenove OV2640 Camera Web Server");

  camera_init();

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_vflip(s, 0);
    s->set_hmirror(s, 0);
    s->set_brightness(s, 1);
    s->set_saturation(s, -1);
  }

  setupLedFlash();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera ready. Open in browser: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  delay(10000);
}

void camera_init() {
  config.ledc_channel   = LEDC_CHANNEL_0;
  config.ledc_timer     = LEDC_TIMER_0;
  config.pin_d0         = Y2_GPIO_NUM;
  config.pin_d1         = Y3_GPIO_NUM;
  config.pin_d2         = Y4_GPIO_NUM;
  config.pin_d3         = Y5_GPIO_NUM;
  config.pin_d4         = Y6_GPIO_NUM;
  config.pin_d5         = Y7_GPIO_NUM;
  config.pin_d6         = Y8_GPIO_NUM;
  config.pin_d7         = Y9_GPIO_NUM;
  config.pin_xclk       = XCLK_GPIO_NUM;
  config.pin_pclk       = PCLK_GPIO_NUM;
  config.pin_vsync      = VSYNC_GPIO_NUM;
  config.pin_href       = HREF_GPIO_NUM;
  config.pin_sccb_sda   = SIOD_GPIO_NUM;
  config.pin_sccb_scl   = SIOC_GPIO_NUM;
  config.pin_pwdn       = PWDN_GPIO_NUM;
  config.pin_reset      = RESET_GPIO_NUM;
  config.xclk_freq_hz   = 10000000;
  config.frame_size     = FRAMESIZE_SVGA;
  config.pixel_format   = PIXFORMAT_JPEG;
  config.grab_mode      = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location    = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality    = 10;
  config.fb_count       = 2;
}
