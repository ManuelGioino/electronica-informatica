# Portero Digital

Monorepo for the digital doorman project built around two ESP32 nodes:

- an `ESP32-CAM` node for live video and face detection
- an `ESP32` door node for ultrasonic sensing / actuator logic
- a lightweight frontend placeholder for the owner console

## Repository Layout

```text
portero-digital/
  docs/
  firmware/
    esp32-cam/
      CameraWebServer/
    esp32-door-node/
      ultrasonico_deteccion_distancia/
  frontend/
    dashboard/
```

## Current Modules

### Camera node

Path: `firmware/esp32-cam/CameraWebServer/`

This is the camera firmware you were already using, relocated into an Arduino-friendly sketch folder so it can live cleanly inside the monorepo.

### Door node

Path: `firmware/esp32-door-node/ultrasonico_deteccion_distancia/`

This folder contains the second firmware sketch you provided. It is kept as-is for now so the hardware work stays traceable while the full integration plan is defined.

### Frontend placeholder

Path: `frontend/dashboard/`

Open `frontend/dashboard/index.html` in a browser to view the first owner dashboard placeholder. It already includes:

- a stream URL field for the ESP32-CAM MJPEG feed
- face-recognition enable/disable UI
- authorize / deny controls
- system status chips
- a local event log stub

## Next Steps

1. Merge camera detection + MQTT into the camera node.
2. Replace or complete the door node with the actual ultrasonic / relay logic.
3. Wire the dashboard to MQTT over WebSockets and the camera stream endpoint.
4. Define the final topic contract in `docs/mqtt-topics.md`.

## Notes

- This repo was initialized locally and is ready to be pushed to GitHub later.
- WiFi credentials are still hard-coded in the imported sketches and should be moved to a safer config flow before production.
