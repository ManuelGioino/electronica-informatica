#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

const char* ssid     = "UA-Alumnos";
const char* password = "41umn05WLC";


const char* mqtt_server = "54.243.81.47";
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "timbre/boton";

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

void startCameraServer();
void setupLedFlash(int pin);
void setDetectionEnabled(int8_t val);

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

  IPAddress local_IP(172, 22, 32, 100);
  IPAddress gateway(172, 22, 32, 200);
  IPAddress subnet(255, 255, 240, 0);
  IPAddress dns(8, 8, 8, 8);

  if (!WiFi.config(local_IP, gateway, subnet, dns)) {
    Serial.println("Error configurando IP fija");
  }

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");

  mqttClient.setCallback(callback);
  conectarMQTT();

  startCameraServer();

  Serial.print("Camera lista en: http://");
  Serial.println(WiFi.localIP());
  Serial.println("Activa Face Detection desde la pagina.");
}

void loop() {
  if (!mqttClient.connected()) {
    conectarMQTT();
  }
  mqttClient.loop();
  delay(100);
}