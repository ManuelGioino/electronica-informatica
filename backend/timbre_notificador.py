import paho.mqtt.client as mqtt
from twilio.rest import Client
from flask import Flask, request
import mysql.connector
import requests
import threading
import os
import time

# Configuración Twilio
TWILIO_SID      = os.getenv("TWILIO_SID")
TWILIO_TOKEN    = os.getenv("TWILIO_TOKEN")
TWILIO_WHATSAPP = os.getenv("TWILIO_WHATSAPP")
TU_WHATSAPP     = os.getenv("TU_WHATSAPP")

# Configuración MySQL
DB_CONFIG = {
    "host":     os.getenv("DB_HOST", "172.31.63.142"),
    "user":     os.getenv("DB_USER", "timbre_user"),
    "password": os.getenv("DB_PASSWORD", "timbre_pass"),
    "database": os.getenv("DB_NAME", "timbre"),
}

# Configuración MQTT
BROKER = os.getenv("MQTT_BROKER", "54.243.81.47")
TOPIC  = os.getenv("MQTT_TOPIC", "timbre/boton")

# Cooldown: mínimo 60s entre notificaciones
COOLDOWN_SEGUNDOS = 60
ultimo_envio = 0

webhook_app = Flask("webhook")


def capturar_y_subir_foto():
    try:
        resp = requests.get("http://relay:8888/capture", timeout=5)
        if resp.status_code != 200:
            print("No hay foto disponible en el relay")
            return None
        upload = requests.put(
            "https://transfer.sh/capture.jpg",
            data=resp.content,
            headers={"Content-Type": "image/jpeg"},
            timeout=10,
        )
        if upload.status_code == 200:
            url = upload.text.strip()
            print(f"Foto subida: {url}")
            return url
        print(f"Error al subir foto, status: {upload.status_code}")
        return None
    except Exception as e:
        print(f"Error al capturar/subir foto: {e}")
        return None


def guardar_historial(mensaje):
    try:
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()
        cursor.execute(
            "INSERT INTO historial (mensaje, notificado) VALUES (%s, %s)",
            (mensaje, True),
        )
        conn.commit()
        cursor.close()
        conn.close()
        print("Guardado en base de datos")
    except Exception as e:
        print(f"Error DB: {e}")


def enviar_whatsapp(texto, media_url=None):
    try:
        twilio_client = Client(TWILIO_SID, TWILIO_TOKEN)
        kwargs = {
            "body":  texto,
            "from_": TWILIO_WHATSAPP,
            "to":    TU_WHATSAPP,
        }
        if media_url:
            kwargs["media_url"] = [media_url]
        twilio_client.messages.create(**kwargs)
        print(f"WhatsApp enviado: {texto}")
    except Exception as e:
        print(f"Error Twilio: {e}")


@webhook_app.route("/webhook", methods=["POST"])
def whatsapp_webhook():
    body = request.form.get("Body", "").strip().upper()
    print(f"Respuesta WhatsApp recibida: {body}")

    if body == "POLICIA":
        guardar_historial("respuesta_policia")
        enviar_whatsapp("✅ Acción registrada. Llamá al 911.")
    elif body == "NADA":
        guardar_historial("respuesta_nada")
        enviar_whatsapp("✅ Acción registrada. Ignorando visita.")
    else:
        enviar_whatsapp("No entendí la respuesta. Respondé POLICIA o NADA.")

    return "<Response/>", 200, {"Content-Type": "text/xml"}


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Conectado al broker MQTT ({BROKER})")
        client.subscribe(TOPIC)
        print(f"Suscripto a {TOPIC}")
    else:
        print(f"Error al conectar, rc={rc}")


def on_message(client, userdata, msg):
    global ultimo_envio
    mensaje = msg.payload.decode()
    print(f"Mensaje recibido: {mensaje}")

    if mensaje == "cara_detectada":
        ahora = time.time()
        if ahora - ultimo_envio < COOLDOWN_SEGUNDOS:
            restante = int(COOLDOWN_SEGUNDOS - (ahora - ultimo_envio))
            print(f"Cooldown activo, próxima notificación en {restante}s")
            return

        ultimo_envio = ahora
        guardar_historial(mensaje)

        time.sleep(2)
        foto_url = capturar_y_subir_foto()

        if foto_url:
            enviar_whatsapp(
                "🔔 ¡Alguien está en la puerta!\nRespondé POLICIA o NADA.",
                media_url=foto_url,
            )
        else:
            enviar_whatsapp("🔔 ¡Alguien está en la puerta!\nRespondé POLICIA o NADA.")


mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(BROKER, 1883, 60)

threading.Thread(
    target=lambda: webhook_app.run(host="0.0.0.0", port=8889, use_reloader=False),
    daemon=True,
).start()
print("Webhook Flask escuchando en :8889")

mqtt_client.loop_forever()
