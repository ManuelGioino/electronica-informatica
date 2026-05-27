import paho.mqtt.client as mqtt
from twilio.rest import Client
import mysql.connector
import os

# Configuración Twilio
TWILIO_SID      = os.getenv("TWILIO_SID")
TWILIO_TOKEN    = os.getenv("TWILIO_TOKEN")
TWILIO_WHATSAPP = os.getenv("TWILIO_WHATSAPP")
TU_WHATSAPP     = os.getenv("TU_WHATSAPP")

# Configuración MySQL (instancia privada AWS)
DB_CONFIG = {
    "host":     os.getenv("DB_HOST", "172.31.63.142"),
    "user":     os.getenv("DB_USER", "timbre_user"),
    "password": os.getenv("DB_PASSWORD", "timbre_pass"),
    "database": os.getenv("DB_NAME", "timbre"),
}

# Configuración MQTT
BROKER = os.getenv("MQTT_BROKER", "54.243.81.47")
TOPIC  = os.getenv("MQTT_TOPIC", "timbre/boton")


def guardar_historial(mensaje):
    try:
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()
        cursor.execute(
            "INSERT INTO historial (mensaje, notificado) VALUES (%s, %s)",
            (mensaje, True)
        )
        conn.commit()
        cursor.close()
        conn.close()
        print("Guardado en base de datos")
    except Exception as e:
        print(f"Error DB: {e}")


def enviar_whatsapp(texto):
    try:
        client = Client(TWILIO_SID, TWILIO_TOKEN)
        client.messages.create(
            body=texto,
            from_=TWILIO_WHATSAPP,
            to=TU_WHATSAPP,
        )
        print(f"WhatsApp enviado: {texto}")
    except Exception as e:
        print(f"Error Twilio: {e}")


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
        guardar_historial(mensaje)
        enviar_whatsapp("🔔 ¡Alguien está en la puerta! Se detectó una cara.")


client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, 1883, 60)
client.loop_forever()
