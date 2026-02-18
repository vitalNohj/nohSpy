/**
 * NohJEye - ESP32 Camera Streaming Server
 *
 * AP always active at 4.3.2.1 with captive portal.
 * STA connects to router if WIFI_SSID is configured.
 * MJPEG stream available on both interfaces at port 81.
 *
 * https://docs.freenove.com/projects/fnk0060/en/latest/fnk0060/codes/C.html
 */
#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <DNSServer.h>
#include "SD_MMC.h"
#include "SPIFFS.h"
#include "board_config.h"

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
#ifndef AP_PASSWORD
#define AP_PASSWORD "nohjeye123"
#endif

#define STA_CONNECT_TIMEOUT_MS 15000
#define AP_CHANNEL 1
#define AP_MAX_CONN 4
#define RECONNECT_INTERVAL_MS 30000
#define DNS_PORT 53

static const IPAddress AP_IP(4, 3, 2, 1);
static const IPAddress AP_GATEWAY(4, 3, 2, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

void startCameraServer();
void setupLedFlash();

static camera_config_t cam_cfg;
static bool sta_configured = false;
static DNSServer dnsServer;

static void print_board_info() {
  Serial.println();
  Serial.println("============================================");
  Serial.println("       NohJEye - ESP32 Camera Server");
  Serial.println("============================================");

  Serial.println("\n[Chip]");
  Serial.printf("  Model:      %s rev %u\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("  Cores:      %u @ %u MHz\n", ESP.getChipCores(), ESP.getCpuFreqMHz());
  Serial.printf("  SDK:        %s\n", ESP.getSdkVersion());

  Serial.println("\n[Flash]");
  Serial.printf("  Size:       %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("  Speed:      %u MHz\n", ESP.getFlashChipSpeed() / 1000000);

  Serial.println("\n[PSRAM]");
  if (psramFound()) {
    Serial.printf("  Size:       %u bytes (%u KB)\n", ESP.getPsramSize(), ESP.getPsramSize() / 1024);
    Serial.printf("  Free:       %u bytes\n", ESP.getFreePsram());
  } else {
    Serial.println("  NOT DETECTED - high-res streaming will be limited");
  }

  Serial.println("\n[Heap]");
  Serial.printf("  Total:      %u bytes\n", ESP.getHeapSize());
  Serial.printf("  Free:       %u bytes\n", ESP.getFreeHeap());

  Serial.println("\n[Network]");
  Serial.printf("  STA MAC:    %s\n", WiFi.macAddress().c_str());
  Serial.printf("  AP MAC:     %s\n", WiFi.softAPmacAddress().c_str());
}

static void init_camera() {
  cam_cfg.ledc_channel = LEDC_CHANNEL_0;
  cam_cfg.ledc_timer   = LEDC_TIMER_0;
  cam_cfg.pin_d0       = Y2_GPIO_NUM;
  cam_cfg.pin_d1       = Y3_GPIO_NUM;
  cam_cfg.pin_d2       = Y4_GPIO_NUM;
  cam_cfg.pin_d3       = Y5_GPIO_NUM;
  cam_cfg.pin_d4       = Y6_GPIO_NUM;
  cam_cfg.pin_d5       = Y7_GPIO_NUM;
  cam_cfg.pin_d6       = Y8_GPIO_NUM;
  cam_cfg.pin_d7       = Y9_GPIO_NUM;
  cam_cfg.pin_xclk     = XCLK_GPIO_NUM;
  cam_cfg.pin_pclk     = PCLK_GPIO_NUM;
  cam_cfg.pin_vsync    = VSYNC_GPIO_NUM;
  cam_cfg.pin_href     = HREF_GPIO_NUM;
  cam_cfg.pin_sccb_sda = SIOD_GPIO_NUM;
  cam_cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cam_cfg.pin_pwdn     = PWDN_GPIO_NUM;
  cam_cfg.pin_reset    = RESET_GPIO_NUM;
  cam_cfg.pixel_format = PIXFORMAT_JPEG;

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  cam_cfg.xclk_freq_hz = 20000000;
#else
  cam_cfg.xclk_freq_hz = 10000000;
#endif

  if (psramFound()) {
    cam_cfg.frame_size  = FRAMESIZE_VGA;
    cam_cfg.fb_location = CAMERA_FB_IN_PSRAM;
    cam_cfg.jpeg_quality = 10;
    cam_cfg.fb_count    = 2;
    cam_cfg.grab_mode   = CAMERA_GRAB_LATEST;
  } else {
    cam_cfg.frame_size  = FRAMESIZE_QVGA;
    cam_cfg.fb_location = CAMERA_FB_IN_DRAM;
    cam_cfg.jpeg_quality = 12;
    cam_cfg.fb_count    = 1;
    cam_cfg.grab_mode   = CAMERA_GRAB_WHEN_EMPTY;
  }
}

static void init_wifi() {
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char ap_ssid[32];
  snprintf(ap_ssid, sizeof(ap_ssid), "NohJEye-%02X%02X", mac[4], mac[5]);

  WiFi.softAP(ap_ssid, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN);
  delay(100);

  dnsServer.start(DNS_PORT, "*", AP_IP);

  Serial.printf("\n[WiFi AP] SSID: '%s'  Pass: '%s'\n", ap_ssid, AP_PASSWORD);
  Serial.printf("[WiFi AP] IP:   %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("[WiFi AP] MAC:  %s\n", WiFi.softAPmacAddress().c_str());

  String ssid = WIFI_SSID;
  if (ssid.length() > 0 && ssid != "********") {
    sta_configured = true;
    Serial.printf("[WiFi STA] Connecting to '%s'", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < STA_CONNECT_TIMEOUT_MS) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WiFi STA] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("[WiFi STA] RSSI: %d dBm\n", WiFi.RSSI());
    } else {
      Serial.println("[WiFi STA] Connection failed - AP only for now");
    }
  } else {
    Serial.println("[WiFi STA] No SSID configured - AP-only mode");
  }
}

static bool init_sdcard() {
  Serial.println("\n[SD Card]");
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("  Not mounted (no card or mount failed)");
    return false;
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("  No card detected");
    return false;
  }
  const char *type = cardType == CARD_MMC  ? "MMC"  :
                     cardType == CARD_SD   ? "SDSC" :
                     cardType == CARD_SDHC ? "SDHC" : "Unknown";
  Serial.printf("  Type: %s\n", type);
  Serial.printf("  Size: %llu MB\n", SD_MMC.cardSize() / (1024 * 1024));
  Serial.printf("  Used: %llu MB\n", SD_MMC.usedBytes() / (1024 * 1024));
  return true;
}

static bool init_spiffs() {
  Serial.println("\n[SPIFFS]");
  if (!SPIFFS.begin(true)) {
    Serial.println("  Mount failed");
    return false;
  }
  Serial.printf("  Total: %u bytes\n", SPIFFS.totalBytes());
  Serial.printf("  Used:  %u bytes\n", SPIFFS.usedBytes());
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(500);

  print_board_info();

  // WiFi first so AP is always reachable even if camera fails
  init_wifi();

  init_sdcard();
  init_spiffs();

  // Camera init - non-fatal so server starts regardless
  init_camera();
  esp_err_t err = esp_camera_init(&cam_cfg);
  bool camera_ok = (err == ESP_OK);

  if (camera_ok) {
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
      Serial.printf("\n[Camera] PID=0x%02X VER=0x%02X MIDH=0x%02X MIDL=0x%02X\n",
        s->id.PID, s->id.VER, s->id.MIDH, s->id.MIDL);
      s->set_vflip(s, 0);
      s->set_hmirror(s, 0);
      s->set_brightness(s, 1);
      s->set_saturation(s, -1);
    }
  } else {
    Serial.printf("\n[ERROR] Camera init failed: 0x%x - server starts without video\n", err);
  }

  setupLedFlash();
  startCameraServer();

  Serial.println("\n============================================");
  Serial.println("         NohJEye Server Ready");
  Serial.println("============================================");
  Serial.println("  AP:     http://4.3.2.1  (captive portal)");
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("  STA:    http://%s\n", WiFi.localIP().toString().c_str());
  if (camera_ok)
    Serial.println("  Stream: :81/stream");
  else
    Serial.println("  Camera: OFFLINE - check wiring / PSRAM");
  Serial.println("  Info:   /info");
  Serial.println("============================================\n");

  Serial.printf("[Memory] Heap: %u free | PSRAM: %u free\n",
    ESP.getFreeHeap(), psramFound() ? ESP.getFreePsram() : 0);
}

void loop() {
  dnsServer.processNextRequest();

  static unsigned long lastCheck = 0;
  unsigned long now = millis();
  if (sta_configured && (now - lastCheck > RECONNECT_INTERVAL_MS)) {
    lastCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi STA] Reconnecting...");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
  delay(10);
}
