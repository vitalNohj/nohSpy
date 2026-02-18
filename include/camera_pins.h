/**
 * Camera pin definitions for Freenove OV2640 boards.
 * Set one of CAMERA_MODEL_WROVER_KIT or CAMERA_MODEL_ESP32S3_EYE in build_flags.
 *
 * - CAMERA_MODEL_WROVER_KIT: Freenove ESP32-WROVER (FNK0060) + extension board
 * - CAMERA_MODEL_ESP32S3_EYE: Freenove ESP32-S3 CAM (same as Espressif ESP32-S3-EYE)
 */
#pragma once

#if defined(CAMERA_MODEL_WROVER_KIT)
/* Freenove FNK0060 – ESP32-WROVER with OV2640 extension board */
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  21
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27

#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    19
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    5
#define Y2_GPIO_NUM    4
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

#elif defined(CAMERA_MODEL_ESP32S3_EYE)
/* Freenove ESP32-S3 CAM – same pinout as Espressif ESP32-S3-EYE */
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  4
#define SIOC_GPIO_NUM  5

#define Y2_GPIO_NUM    11
#define Y3_GPIO_NUM    9
#define Y4_GPIO_NUM    8
#define Y5_GPIO_NUM    10
#define Y6_GPIO_NUM    12
#define Y7_GPIO_NUM    18
#define Y8_GPIO_NUM    17
#define Y9_GPIO_NUM    16

#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13

#else
#error "Define CAMERA_MODEL_WROVER_KIT or CAMERA_MODEL_ESP32S3_EYE in build_flags (see platformio.ini)"
#endif
