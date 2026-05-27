#include <WiFi.h>
#include <PubSubClient.h>

// ===== WiFi =====
const char* ssid     = "UA-Alumnos";
const char* password = "41umn05WLC";

// ===== MQTT =====
const char* mqtt_server = "54.243.81.47";
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "timbre/boton";

// ===== Pines ultrasónico =====
#define TRIG_PIN 5
#define ECHO_PIN 18

// ===== Config =====
const float  DISTANCIA_UMBRAL_CM  = 100.0;
const unsigned long COOLDOWN_MS   = 5000;
const unsigned long AUSENCIA_MS   = 10000; // apaga detección tras 10s sin presencia

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);
unsigned long ultimaDeteccion  = 0;
unsigned long ultimaPresencia   = 0;
bool          deteccionActiva   = false;

void conectarWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado. IP: " + WiFi.localIP().toString());
}

void conectarMQTT() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  while (!mqttClient.connected()) {
    Serial.println("Conectando a MQTT...");
    if (mqttClient.connect("ESP32_DoorNode")) {
      Serial.println("Conectado al broker MQTT.");
    } else {
      Serial.print("Fallo rc=");
      Serial.println(mqttClient.state());
      delay(3000);
    }
  }
}

float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duracion == 0) return -1;
  return duracion / 58.0;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  conectarWiFi();
  conectarMQTT();
  Serial.println("Sistema listo.");
}

void loop() {
  if (!mqttClient.connected()) {
    conectarMQTT();
  }
  mqttClient.loop();

  float distancia = medirDistancia();
  unsigned long ahora = millis();

  if (distancia > 0) {
    Serial.printf("Distancia: %.1f cm\n", distancia);

    if (distancia < DISTANCIA_UMBRAL_CM) {
      ultimaPresencia = ahora;

      if (!deteccionActiva && ahora - ultimaDeteccion > COOLDOWN_MS) {
        mqttClient.publish("timbre/comando", "activar_deteccion");
        Serial.println("Publicado: activar_deteccion");
        deteccionActiva = true;
        ultimaDeteccion = ahora;
      }
    }
  }

  // Desactivar si no hay presencia por AUSENCIA_MS
  if (deteccionActiva && ahora - ultimaPresencia > AUSENCIA_MS) {
    mqttClient.publish("timbre/comando", "desactivar_deteccion");
    Serial.println("Publicado: desactivar_deteccion");
    deteccionActiva = false;
  }

  delay(200);
}
