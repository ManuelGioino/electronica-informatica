# MQTT Topics

Initial topic draft for the unified portero project.

## Device Availability

- `portero/frontdoor/cam/availability`
- `portero/frontdoor/door/availability`

Payload example:

```json
{ "state": "online" }
```

## Camera State

- `portero/frontdoor/cam/status`
- `portero/frontdoor/cam/face`

Payload examples:

```json
{ "stream": "ready", "ip": "192.168.1.40", "faceDetection": true }
```

```json
{ "detected": true, "count": 1 }
```

## Door / Sensor State

- `portero/frontdoor/door/distance`
- `portero/frontdoor/door/presence`
- `portero/frontdoor/door/state`

Payload examples:

```json
{ "cm": 46.2 }
```

```json
{ "present": true }
```

```json
{ "lock": "closed", "authorization": "idle" }
```

## Commands

- `portero/frontdoor/cmd/face-detection`
- `portero/frontdoor/cmd/authorize`
- `portero/frontdoor/cmd/deny`
- `portero/frontdoor/cmd/auto-mode`

Payload examples:

```json
{ "enabled": true }
```

```json
{ "actor": "owner-dashboard", "durationMs": 3000 }
```
