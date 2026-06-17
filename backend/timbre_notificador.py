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
POLICIA_WHATSAPP = os.getenv("POLICIA_WHATSAPP", "whatsapp:+5491160434379")

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

# IP pública del EC2 para construir la URL de la foto
EC2_IP = os.getenv("EC2_IP", "54.243.81.47")

ultima_foto_url = None

webhook_app = Flask("webhook")


def obtener_url_foto():
    try:
        resp = requests.get("http://relay:8888/capture-filename", timeout=5)
        if resp.status_code != 200:
            print("No hay foto disponible en el relay")
            return None
        filename = resp.text.strip()
        url = f"http://{EC2_IP}:8888/fotos/{filename}"
        print(f"URL de foto: {url}")
        return url
    except Exception as e:
        print(f"Error obteniendo URL de foto: {e}")
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


def enviar_whatsapp(texto, media_url=None, to=None):
    try:
        twilio_client = Client(TWILIO_SID, TWILIO_TOKEN)
        kwargs = {
            "body":  texto,
            "from_": TWILIO_WHATSAPP,
            "to":    to or TU_WHATSAPP,
        }
        if media_url:
            kwargs["media_url"] = [media_url]
        twilio_client.messages.create(**kwargs)
        print(f"WhatsApp enviado a {kwargs['to']}: {texto}")
    except Exception as e:
        print(f"Error Twilio: {e}")


@webhook_app.route("/webhook", methods=["POST"])
def whatsapp_webhook():
    body = request.form.get("Body", "").strip().upper()
    print(f"Respuesta WhatsApp recibida: {body}")

    if body == "POLICIA":
        guardar_historial("respuesta_policia")
        enviar_whatsapp("✅ Acción registrada. Llamá al 911.")
        enviar_whatsapp(
            "⚠️ Hay alguien en la puerta de mi casa que no conozco.",
            media_url=ultima_foto_url,
            to=POLICIA_WHATSAPP,
        )
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
    mensaje = msg.payload.decode()
    print(f"Mensaje recibido: {mensaje}")

    if mensaje == "cara_detectada":
        global ultima_foto_url
        guardar_historial(mensaje)

        time.sleep(2)
        foto_url = obtener_url_foto()
        ultima_foto_url = foto_url

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
