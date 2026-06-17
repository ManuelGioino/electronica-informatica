#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

const char* ssid     = "UA-Alumnos";
const char* password = "41umn05WLC";

const char* mqtt_server  = "54.243.81.47";
const int   mqtt_port    = 1883;
const char* mqtt_topic   = "timbre/boton";
const char* capture_url  = "http://54.243.81.47:8888/capture";

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// Señal que pone app_httpd.cpp cuando detecta una cara
volatile bool capture_requested = false;

// Timeout de face detection: se desactiva si no hay activacion por 60s
#define DETECTION_TIMEOUT_MS 60000UL
static unsigned long last_activacion_ms = 0;
extern int8_t detection_enabled;

void startCameraServer();
void setupLedFlash(int pin);
void setDetectionEnabled(int8_t val);

// ── Capture task ─────────────────────────────────────────────────────────────
// Espera a que app_httpd.cpp marque capture_requested, luego sube UN frame
// al relay en EC2. El notificador lo descarga y lo manda por WhatsApp.
void captureTask(void* parameter) {
  while (true) {
    if (capture_requested && WiFi.status() == WL_CONNECTED) {
      camera_fb_t* fb = esp_camera_fb_get();
      if (fb) {
        WiFiClient wc;
        HTTPClient http;
        http.setTimeout(5000);
        http.begin(wc, capture_url);
        http.addHeader("Content-Type", "image/jpeg");
        int code = http.POST(fb->buf, (int)fb->len);
        http.end();
        esp_camera_fb_return(fb);
        if (code == 200) {
          Serial.println("Foto de deteccion subida al relay.");
        } else {
          Serial.printf("Error subiendo foto: %d\n", code);
        }
      }
      capture_requested = false;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void conectarMQTT() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  while (!mqttClient.connected()) {
    Serial.println("Conectando a MQTT...");
    if (mqttClient.connect("ESP32CAM_Stream")) {
      Serial.println("Conectado al broker MQTT.");
      mqttClient.subscribe("timbre/comando");
    } else {
      Serial.print("Fallo rc=");
      Serial.println(mqttClient.state());
      delay(3000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  Serial.print("Comando recibido: ");
  Serial.println(mensaje);

  if (mensaje == "activar_deteccion") {
    setDetectionEnabled(1);
    last_activacion_ms = millis();
    Serial.println("Face detection ACTIVADA por ultrasonido.");
  } else if (mensaje == "desactivar_deteccion") {
    setDetectionEnabled(0);
    Serial.println("Face detection DESACTIVADA.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size   = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size  = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  IPAddress local_ip(172, 22, 44, 200);
  IPAddress gateway(172, 22, 32, 200);
  IPAddress subnet(255, 255, 0, 0);
  IPAddress dns(8, 8, 8, 8);
  WiFi.config(local_ip, gateway, subnet, dns);

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");

  if (MDNS.begin("portero")) {
    Serial.println("mDNS iniciado. Camara en http://portero.local");
  }

  mqttClient.setCallback(callback);
  conectarMQTT();

  startCameraServer();

  xTaskCreatePinnedToCore(captureTask, "capture", 8192, NULL, 1, NULL, 0);
  Serial.println("Capture task iniciado.");

  Serial.print("Camera lista en: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (!mqttClient.connected()) {
    conectarMQTT();
  }
  mqttClient.loop();

  if (detection_enabled && last_activacion_ms > 0 &&
      (millis() - last_activacion_ms > DETECTION_TIMEOUT_MS)) {
    setDetectionEnabled(0);
    last_activacion_ms = 0;
    Serial.println("Face detection desactivada por timeout (60s sin deteccion).");
  }

  delay(100);
}
