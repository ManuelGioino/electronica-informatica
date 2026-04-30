#include "esp_camera.h"
#include "fd_forward.h"
#include "fr_forward.h"
#include "img_converters.h"
#include "fb_gfx.h"

// ===== Pines AI Thinker ESP32-CAM =====
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

mtmn_config_t mtmn_config = {0};

void liberarNetBoxes(box_array_t *boxes) {
  if (!boxes) return;
  free(boxes->score);
  free(boxes->box);
  free(boxes->landmark);
  free(boxes);
}

bool iniciarCamara() {
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

  // Para detección facial conviene RGB565 y tamaño chico/cuadrado
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_240X240;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.print("Error al iniciar camara. Codigo 0x");
    Serial.println(err, HEX);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_framesize(s, FRAMESIZE_240X240);
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
  }

  return true;
}

void configurarDetector() {
  mtmn_config = mtmn_init_config();
  mtmn_config.type = FAST;
  mtmn_config.min_face = 80;
  mtmn_config.pyramid = 0.707f;
  mtmn_config.pyramid_times = 4;
  mtmn_config.p_threshold.score = 0.6f;
  mtmn_config.p_threshold.nms = 0.7f;
  mtmn_config.p_threshold.candidate_number = 20;
  mtmn_config.r_threshold.score = 0.7f;
  mtmn_config.r_threshold.nms = 0.7f;
  mtmn_config.r_threshold.candidate_number = 10;
  mtmn_config.o_threshold.score = 0.7f;
  mtmn_config.o_threshold.nms = 0.7f;
  mtmn_config.o_threshold.candidate_number = 1;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nIniciando deteccion facial...");

  if (!psramFound()) {
    Serial.println("Aviso: no se detecto PSRAM. Puede fallar o ir muy lento.");
  } else {
    Serial.println("PSRAM detectada.");
  }

  if (!iniciarCamara()) {
    Serial.println("No se pudo iniciar la camara.");
    while (true) {
      delay(1000);
    }
  }

  configurarDetector();
  Serial.println("Camara lista. Mostrare por Serial si detecto cara.");
}

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("No se pudo capturar frame.");
    delay(200);
    return;
  }

  dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
  if (!image_matrix) {
    Serial.println("No se pudo asignar memoria para image_matrix.");
    esp_camera_fb_return(fb);
    delay(200);
    return;
  }

  bool convertido = false;

  if (fb->format == PIXFORMAT_RGB565) {
    fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item);
    convertido = true;
  } else {
    convertido = fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item);
  }

  if (!convertido) {
    Serial.println("No se pudo convertir la imagen.");
    dl_matrix3du_free(image_matrix);
    esp_camera_fb_return(fb);
    delay(200);
    return;
  }

  box_array_t *net_boxes = face_detect(image_matrix, &mtmn_config);

  if (net_boxes && net_boxes->len > 0) {
    Serial.print("CARA DETECTADA. Cantidad: ");
    Serial.println(net_boxes->len);
  } else {
    Serial.println("No hay cara.");
  }

  liberarNetBoxes(net_boxes);
  dl_matrix3du_free(image_matrix);
  esp_camera_fb_return(fb);

  delay(500);
}

const char* ssid = "UA-Alumnos";
const char* password = "41umn05WLC";

