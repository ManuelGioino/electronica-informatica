const CAMERA_URL_STORAGE_KEY = "portero.cameraBaseUrl";

const state = {
  cameraBaseUrl: "",
  streamUrl: "",
  faceEnabled: false,
  faceDetected: false,
  faceEventCount: null,
  faceStatusTimer: null,
  faceStatusErrorShown: false,
  mqttState: "Pending",
  cameraState: "Idle",
  events: [],
};

const refs = {
  streamImage: document.getElementById("stream-frame"),
  streamEmpty: document.getElementById("stream-empty"),
  streamUrl: document.getElementById("stream-url"),
  mqttPill: document.getElementById("mqtt-pill"),
  cameraPill: document.getElementById("camera-pill"),
  faceToggle: document.getElementById("face-toggle"),
  autoToggle: document.getElementById("auto-toggle"),
  faceLabel: document.getElementById("face-label"),
  eventList: document.getElementById("event-list"),
  loadStream: document.getElementById("load-stream"),
};

function nowLabel() {
  return new Date().toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
  });
}

function addEvent(message) {
  state.events.unshift({ message, at: nowLabel() });
  state.events = state.events.slice(0, 8);
  renderEvents();
}

function normalizeCameraBaseUrl(rawUrl) {
  const withProtocol = /^https?:\/\//i.test(rawUrl) ? rawUrl : `http://${rawUrl}`;
  const url = new URL(withProtocol);

  if (url.port === "81") {
    url.port = "";
  }

  url.pathname = "";
  url.search = "";
  url.hash = "";

  return url.toString().replace(/\/$/, "");
}

function getCameraBaseUrl() {
  if (state.cameraBaseUrl) {
    return state.cameraBaseUrl;
  }

  const rawUrl = refs.streamUrl.value.trim();
  if (!rawUrl) {
    throw new Error("Connect the camera first");
  }

  state.cameraBaseUrl = normalizeCameraBaseUrl(rawUrl);
  return state.cameraBaseUrl;
}

function getCameraStreamUrl() {
  const url = new URL(getCameraBaseUrl());
  url.port = "81";
  url.pathname = "/stream";
  url.search = "";
  url.hash = "";
  return url.toString().replace(/\/$/, "");
}

async function sendCameraControl(variable, value) {
  const baseUrl = getCameraBaseUrl();
  const controlUrl = `${baseUrl}/control?var=${encodeURIComponent(variable)}&val=${encodeURIComponent(value)}`;
  const response = await fetch(controlUrl);

  if (!response.ok) {
    throw new Error(`Camera rejected ${variable}`);
  }
}

async function syncCameraStatus() {
  const baseUrl = getCameraBaseUrl();
  const response = await fetch(`${baseUrl}/status`);

  if (!response.ok) {
    throw new Error("Camera status unavailable");
  }

  const status = await response.json();
  state.faceEnabled = Number(status.face_detect) === 1;
  addEvent("Camera status synced");
  render();
}

async function syncFaceStatus() {
  const baseUrl = getCameraBaseUrl();
  const response = await fetch(`${baseUrl}/face-status`);

  if (!response.ok) {
    throw new Error("Face status unavailable");
  }

  const status = await response.json();
  const nextEventCount = Number(status.event_count) || 0;
  const nextFaceDetected = Number(status.face_detected) === 1;
  const previousFaceDetected = state.faceDetected;
  const previousEventCount = state.faceEventCount;

  state.faceEnabled = Number(status.face_detect) === 1;
  state.faceDetected = nextFaceDetected;

  if (previousEventCount !== null && nextEventCount > previousEventCount) {
    const missedEvents = nextEventCount - previousEventCount;
    addEvent(missedEvents === 1 ? "Face detected at door" : `${missedEvents} face detections received`);
  } else if (!previousFaceDetected && nextFaceDetected) {
    addEvent("Face detected at door");
  }

  state.faceEventCount = nextEventCount;
  state.faceStatusErrorShown = false;
  render();
}

function startFaceStatusPolling() {
  if (state.faceStatusTimer) {
    clearInterval(state.faceStatusTimer);
  }

  syncFaceStatus().catch(() => {
    if (!state.faceStatusErrorShown) {
      addEvent("Face status endpoint unavailable");
      state.faceStatusErrorShown = true;
    }
  });

  state.faceStatusTimer = setInterval(() => {
    syncFaceStatus().catch(() => {
      if (!state.faceStatusErrorShown) {
        addEvent("Face status endpoint unavailable");
        state.faceStatusErrorShown = true;
      }
    });
  }, 1000);
}

function stopFaceStatusPolling() {
  if (state.faceStatusTimer) {
    clearInterval(state.faceStatusTimer);
    state.faceStatusTimer = null;
  }
}

function applyPill(el, label, tone) {
  el.textContent = label;
  el.className = `pill ${tone}`;
}

function renderEvents() {
  refs.eventList.innerHTML = "";

  if (!state.events.length) {
    const item = document.createElement("li");
    item.innerHTML = "<span>No events yet</span><time>--:--</time>";
    refs.eventList.appendChild(item);
    return;
  }

  state.events.forEach((entry) => {
    const item = document.createElement("li");
    const text = document.createElement("span");
    const time = document.createElement("time");
    text.textContent = entry.message;
    time.textContent = entry.at;
    item.append(text, time);
    refs.eventList.appendChild(item);
  });
}

function render() {
  applyPill(
    refs.mqttPill,
    state.mqttState === "Connected" ? "MQTT Connected" : "MQTT Pending",
    state.mqttState === "Connected" ? "success" : "neutral"
  );

  applyPill(
    refs.cameraPill,
    state.cameraState === "Streaming" ? "Camera Live" : "Camera Idle",
    state.cameraState === "Streaming" ? "success" : "neutral"
  );

  refs.faceToggle.checked = state.faceEnabled;
  refs.faceLabel.textContent = state.faceEnabled ? "Face Detection On" : "Face Detection Off";
}

function disconnectCamera() {
  refs.streamImage.removeAttribute("src");
  refs.streamImage.style.display = "none";
  refs.streamEmpty.style.display = "grid";
  state.cameraBaseUrl = "";
  state.streamUrl = "";
  state.cameraState = "Idle";
  state.faceDetected = false;
  state.faceEventCount = null;
  state.faceStatusErrorShown = false;
  stopFaceStatusPolling();
  addEvent("Camera disconnected");
  render();
}

function connectCamera() {
  const rawUrl = refs.streamUrl.value.trim();

  if (!rawUrl) {
    refs.streamImage.removeAttribute("src");
    refs.streamImage.style.display = "none";
    refs.streamEmpty.style.display = "grid";
    state.cameraBaseUrl = "";
    state.streamUrl = "";
    state.cameraState = "Idle";
    state.faceDetected = false;
    state.faceEventCount = null;
    state.faceStatusErrorShown = false;
    stopFaceStatusPolling();
    localStorage.removeItem(CAMERA_URL_STORAGE_KEY);
    addEvent("Camera URL cleared");
    render();
    return;
  }

  try {
    state.cameraBaseUrl = normalizeCameraBaseUrl(rawUrl);
    state.streamUrl = getCameraStreamUrl();
  } catch (error) {
    addEvent("Camera URL is invalid");
    render();
    return;
  }

  refs.streamUrl.value = state.cameraBaseUrl;
  localStorage.setItem(CAMERA_URL_STORAGE_KEY, state.cameraBaseUrl);
  refs.streamImage.src = state.streamUrl;
  refs.streamImage.style.display = "block";
  refs.streamEmpty.style.display = "none";
  state.cameraState = "Streaming";
  state.faceEventCount = null;
  state.faceStatusErrorShown = false;
  addEvent("Camera connected");
  render();

  syncCameraStatus().catch(() => {
    addEvent("Camera status could not be read");
  });
  startFaceStatusPolling();
}

refs.loadStream.addEventListener("click", connectCamera);

refs.streamUrl.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    connectCamera();
  }
});

refs.faceToggle.addEventListener("change", async (event) => {
  const previousValue = state.faceEnabled;
  const nextValue = event.target.checked;

  refs.faceToggle.disabled = true;
  state.faceEnabled = nextValue;
  render();

  try {
    await sendCameraControl("face_detect", nextValue ? 1 : 0);
    addEvent(nextValue ? "Face detection enabled on camera" : "Face detection disabled on camera");
    if (nextValue) {
      setTimeout(() => {
        syncFaceStatus().catch(() => {
          if (!state.faceStatusErrorShown) {
            addEvent("Face status endpoint unavailable");
            state.faceStatusErrorShown = true;
          }
        });
      }, 700);
    } else {
      state.faceDetected = false;
      state.faceEventCount = null;
    }
  } catch (error) {
    state.faceEnabled = previousValue;
    addEvent("Face detection command failed");
  } finally {
    refs.faceToggle.disabled = false;
    render();
  }
});

addEvent("Dashboard iniciado");
const savedCameraUrl = localStorage.getItem(CAMERA_URL_STORAGE_KEY);
if (savedCameraUrl) {
  refs.streamUrl.value = savedCameraUrl;
  connectCamera();
}
render();
