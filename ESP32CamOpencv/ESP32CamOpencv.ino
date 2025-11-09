// ======= ESP32-CAM UDP JPEG Streamer =======
// Streams compressed JPEG frames via UDP
// Viewer: see Python script on PC side

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>

#pragma GCC optimize ("O3","unroll-loops","inline")

// ---------- Wi-Fi ----------
const char* ssid     = "ESP32_CAM_RELAY";
const char* password = "camera123";

// ---------- Destination (your PC IP + port) ----------
IPAddress dest_ip(192,168,137,1);   // 🖥️ Replace with your PC’s IP
const uint16_t dest_port = 2222;

// ---------- Camera settings ----------
#define FRAME_SIZE   FRAMESIZE_VGA
#define JPEG_QUALITY 20
#define FB_COUNT     4
#define CHUNK_SIZE   1024  // bytes per UDP packet

// ---------- AI-Thinker pins ----------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WiFiUDP udp;

// ---------- Forward declaration ----------
void udpStreamerTask(void *pv); 

// ---------- Setup camera ----------
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
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 37000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAME_SIZE;
  config.jpeg_quality = JPEG_QUALITY;
  config.fb_count     = FB_COUNT;
  config.grab_mode    = CAMERA_GRAB_LATEST;
  config.fb_location  = CAMERA_FB_IN_PSRAM;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    ESP.restart();
  }
}

// ---------- UDP streaming loop ----------
void udpStreamerTask(void *pv) {
  Serial.printf("UDP streamer running on core %d → %s:%d\n",
                xPortGetCoreID(), dest_ip.toString().c_str(), dest_port);

  for (;;) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { vTaskDelay(10); continue; }

    uint32_t len = fb->len;
    uint8_t *buf = fb->buf;

    // --- Send frame header (4 bytes = length) ---
    udp.beginPacket(dest_ip, dest_port);
    udp.write((uint8_t*)&len, 4);
    udp.endPacket();

    // --- Send image in 1 KB chunks ---
    for (uint32_t sent = 0; sent < len; sent += CHUNK_SIZE) {
      size_t n = min((size_t)CHUNK_SIZE, (size_t)(len - sent));
      udp.beginPacket(dest_ip, dest_port);
      udp.write(buf + sent, n);
      udp.endPacket();
    }

    esp_camera_fb_return(fb);
    vTaskDelay(1);
  }
}

// ---------- Main setup ----------
void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32-CAM UDP JPEG Streamer Starting…");

  setupCamera();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to %s", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  udp.begin(WiFi.localIP(), 12345); // local port

  xTaskCreatePinnedToCore(udpStreamerTask, "UDPStream", 8192, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelay(1000);
}
