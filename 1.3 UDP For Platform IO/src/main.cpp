// ======= ESP32-CAM UDP JPEG Streamer (Optimized + CRC16 + CLKRC tweak) =======
// Model: AI THINKER (OV2640)
// Streams compressed JPEG frames via UDP in chunks, sends CRC16 after frame.
// - Uses CAMERA_FB_IN_PSRAM, fb_count=4
// - Applies OV2640 CLKRC register tweak for higher internal pixel clock
// - Keeps XCLK safe at 20 MHz (with CLKRC doubling effect)

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_wifi.h"

#pragma GCC optimize ("O3","unroll-loops","inline")

// ----------------- USER CONFIG -----------------
const char* ssid = "RC_Config";
const char* password = "12345678";

// Destination (set to your PC / phone IP on same LAN)
IPAddress dest_ip(192,168,137,1);   // <-- REPLACE with your receiver IP if needed
const uint16_t dest_port = 2222;    // must match python viewer

// Camera / stream parameters
#define FRAME_SIZE   FRAMESIZE_QVGA   // try QVGA if network weak
#define JPEG_QUALITY 20
#define FB_COUNT     4
#define CHUNK_SIZE   1024            // UDP packet payload size (bytes)
#define LOCAL_UDP_PORT 12345         // local UDP port used by ESP32 (arbitrary)

// AI-Thinker pinout
#define PWDN_GPIO_NUM       32
#define RESET_GPIO_NUM      -1
#define XCLK_GPIO_NUM       0
#define SIOD_GPIO_NUM       26
#define SIOC_GPIO_NUM       27
#define Y9_GPIO_NUM         35
#define Y8_GPIO_NUM         34
#define Y7_GPIO_NUM         39
#define Y6_GPIO_NUM         36
#define Y5_GPIO_NUM         21
#define Y4_GPIO_NUM         19
#define Y3_GPIO_NUM         18
#define Y2_GPIO_NUM         5
#define VSYNC_GPIO_NUM      25
#define HREF_GPIO_NUM       23
#define PCLK_GPIO_NUM       22

// ------------------------------------------------
WiFiUDP udp;

// Forward declaration (task)
void udpStreamerTask(void *pv);

// ---------------- CRC16 (polynomial 0xA001) ----------------
uint16_t crc16(const uint8_t *data, uint32_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= *data++;
    for (int i = 0; i < 8; i++) {
      if (crc & 1) crc = (crc >> 1) ^ 0xA001;
      else crc = (crc >> 1);
    }
  }
  return crc;
}

// ---------------- Camera init with safe XCLK + params ----------------
void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;

  // SAFE XCLK: 20 MHz recommended with CLKRC doubling tweak
  config.xclk_freq_hz = 37000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAME_SIZE;
  config.jpeg_quality = JPEG_QUALITY;
  config.fb_count = FB_COUNT;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed!");
    ESP.restart();
  }

  // Set sensor parameters and apply OV2640 register tweak (CLKRC)
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_framesize(s, FRAME_SIZE);
    s->set_quality(s, JPEG_QUALITY);

    // Try to enable internal clock doubler via CLKRC (reg 0x11 = 0x80)
    // This increases internal pixel timing and can raise FPS for smaller resolutions.
    // We already use XCLK = 20MHz; the sensor will double internally if this succeeds.
    if (s->set_reg(s, 0x11, 0xFF, 0x80) == 0) {
      Serial.println("INFO: Applied CLKRC (0x11=0x80) high-FPS register tweak.");
    } else {
      Serial.println("WARNING: Failed to apply CLKRC high-FPS tweak.");
    }
  }
}

// ---------------- UDP streaming task ----------------
// Sends:
// 1) 4-byte header (frame length, little-endian)
// 2) N chunks of JPEG data (CHUNK_SIZE each except final)
// 3) 2-byte CRC16 (little-endian)
void udpStreamerTask(void *pv) {
  Serial.printf("UDP streamer running on core %d → %s:%d\n",
                xPortGetCoreID(), dest_ip.toString().c_str(), dest_port);

  for (;;) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      vTaskDelay(10);
      continue;
    }

    uint32_t len = fb->len;
    uint8_t *buf = fb->buf;
    uint16_t crc = crc16(buf, len);

    // --- header: total frame length (4 bytes little-endian)
    udp.beginPacket(dest_ip, dest_port);
    udp.write((uint8_t*)&len, 4);
    udp.endPacket();

    // --- send in CHUNK_SIZE pieces
    for (uint32_t sent = 0; sent < len; sent += CHUNK_SIZE) {
      size_t remaining = (len - sent);
      size_t n = (remaining < (size_t)CHUNK_SIZE) ? remaining : (size_t)CHUNK_SIZE;
      udp.beginPacket(dest_ip, dest_port);
      udp.write(buf + sent, n);
      udp.endPacket();
    }

    // --- send CRC16 (2 bytes little-endian)
    udp.beginPacket(dest_ip, dest_port);
    udp.write((uint8_t*)&crc, 2);
    udp.endPacket();

    esp_camera_fb_return(fb);

    // small yield; the camera + wifi will govern real throughput
    vTaskDelay(1);
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nESP32-CAM UDP Streamer (Optimized + CRC16 + CLKRC)");

  // Camera
  setupCamera();

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Improve wireless stability
  esp_wifi_set_max_tx_power(78);   // ~20.5 dBm (max)
  WiFi.setSleep(false);            // disable wifi modem sleep

  Serial.printf("Connecting to WiFi SSID: %s", ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
    if (millis() - start > 20000) { // 20s timeout
      Serial.println("\nWiFi connect timed out, restarting...");
      ESP.restart();
    }
  }

  Serial.println("\nConnected!");
  Serial.print("ESP32 IP: "); Serial.println(WiFi.localIP());

  // UDP begin (bind local port)
  udp.begin(LOCAL_UDP_PORT);

  // create streamer task pinned to Core 1
  BaseType_t res = xTaskCreatePinnedToCore(udpStreamerTask, "UDPStream", 8192, NULL, 1, NULL, 1);
  if (res != pdPASS) {
    Serial.println("Failed to create UDPStream task!");
  }
}

void loop() {
  // nothing here; streamer runs in task
  vTaskDelay(1000);
}
