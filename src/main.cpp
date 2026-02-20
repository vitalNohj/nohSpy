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
#define WIFI_PASSWORD "********"`
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

static const char *camera_model_name() {
#if defined(CAMERA_MODEL_WROVER_KIT)
  return "WROVER_KIT";
#elif defined(CAMERA_MODEL_ESP32S3_EYE)
  return "ESP32S3_EYE";
#elif defined(CAMERA_MODEL_ESP32S3_CAM_LCD)
  return "ESP32S3_CAM_LCD";
#elif defined(CAMERA_MODEL_M5STACK_CAMS3_UNIT)
  return "M5STACK_CAMS3_UNIT";
#elif defined(CAMERA_MODEL_XIAO_ESP32S3)
  return "XIAO_ESP32S3";
#elif defined(CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3)
  return "DFRobot_FireBeetle2_ESP32S3";
#elif defined(CAMERA_MODEL_DFRobot_Romeo_ESP32S3)
  return "DFRobot_Romeo_ESP32S3";
#else
  return "UNKNOWN";
#endif
}

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
  Serial.printf("\n[Camera Profile] %s\n", camera_model_name());
  Serial.println("\n[Camera Pins]");
  Serial.printf("  XCLK: %d   PCLK: %d\n", XCLK_GPIO_NUM, PCLK_GPIO_NUM);
  Serial.printf("  SIOD: %d   SIOC: %d\n", SIOD_GPIO_NUM, SIOC_GPIO_NUM);
  Serial.printf("  VSYNC: %d  HREF: %d\n", VSYNC_GPIO_NUM, HREF_GPIO_NUM);
  Serial.printf("  D0-D7: %d %d %d %d %d %d %d %d\n",
    Y2_GPIO_NUM, Y3_GPIO_NUM, Y4_GPIO_NUM, Y5_GPIO_NUM,
    Y6_GPIO_NUM, Y7_GPIO_NUM, Y8_GPIO_NUM, Y9_GPIO_NUM);
  Serial.printf("  PWDN: %d   RESET: %d\n", PWDN_GPIO_NUM, RESET_GPIO_NUM);

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
  cam_cfg.xclk_freq_hz = 20000000;
  cam_cfg.pixel_format = PIXFORMAT_JPEG;
  cam_cfg.frame_size   = FRAMESIZE_SVGA;
  cam_cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  cam_cfg.fb_location  = CAMERA_FB_IN_PSRAM;
  cam_cfg.jpeg_quality = 12;
  cam_cfg.fb_count     = 2;

  if (psramFound()) {
    Serial.println("  FB: PSRAM (SVGA, 2 buffers, GRAB_LATEST)");
    cam_cfg.jpeg_quality = 10;
    cam_cfg.fb_count     = 2;
    cam_cfg.grab_mode    = CAMERA_GRAB_LATEST;
  } else {
    Serial.println("  FB: DRAM (SVGA, 1 buffer, GRAB_WHEN_EMPTY) -- no PSRAM");
    cam_cfg.fb_count     = 1;
    cam_cfg.fb_location  = CAMERA_FB_IN_DRAM;
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
  SD_MMC.setPins(39 /*CLK*/, 38 /*CMD*/, 40 /*D0*/);
  if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)) {
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
  Serial.println("[Serial] Baud=115200");
  delay(500);

  print_board_info();

  // Camera first (Freenove order). Allow power to settle before SCCB probe.
  init_camera();
  delay(200);
  esp_err_t err = ESP_FAIL;
  bool camera_ok = false;

  static const int xclk_options[] = { 10000000, 20000000, 16000000 };
  for (int attempt = 0; attempt < 3 && !camera_ok; attempt++) {
    cam_cfg.xclk_freq_hz = xclk_options[attempt];
    if (attempt > 0) {
      Serial.printf("[Camera] Retry %d/3 – trying XCLK %d MHz...\n",
                    attempt + 1, cam_cfg.xclk_freq_hz / 1000000);
      delay(1000);
    } else {
      Serial.printf("  XCLK: %d MHz\n", cam_cfg.xclk_freq_hz / 1000000);
    }
    err = esp_camera_init(&cam_cfg);
    camera_ok = (err == ESP_OK);
    if (!camera_ok) {
      Serial.printf("[Camera] Attempt %d failed: 0x%x\n", attempt + 1, err);
      esp_camera_deinit();
    }
  }

  if (camera_ok) {
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
      Serial.printf("\n[Camera OK] PID=0x%02X VER=0x%02X MIDH=0x%02X MIDL=0x%02X\n",
        s->id.PID, s->id.VER, s->id.MIDH, s->id.MIDL);
      s->set_vflip(s, 0);
      s->set_hmirror(s, 0);
      s->set_brightness(s, 1);
      s->set_saturation(s, -1);
    }
  } else {
    Serial.println("\n[Camera FAILED] All 3 attempts failed. Checklist:");
    Serial.println("  1. Ribbon cable firmly seated BOTH ends; contacts face PCB.");
    Serial.println("  2. Try Freenove Sketch_25.1_CameraWebServer in Arduino IDE (same board/camera).");
    Serial.println("     If that works, the issue is PlatformIO/build; if not, hardware.");
    Serial.printf("  3. Pin profile: %s (see include/camera_pins.h)\n", camera_model_name());
    Serial.println("  4. PSRAM must show detected above; if not, set board_build.arduino.memory_type = dio_opi");
  }

  // Bring network and storage up after camera probe.
  init_wifi();
  init_sdcard();
  init_spiffs();

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
