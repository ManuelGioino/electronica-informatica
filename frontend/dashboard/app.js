const state = {
  streamUrl: "",
  faceEnabled: false,
  autoMode: false,
  visitorPresent: false,
  distanceCm: null,
  doorState: "Locked",
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
  doorPill: document.getElementById("door-pill"),
  faceToggle: document.getElementById("face-toggle"),
  autoToggle: document.getElementById("auto-toggle"),
  presenceLabel: document.getElementById("presence-label"),
  faceLabel: document.getElementById("face-label"),
  presenceValue: document.getElementById("presence-value"),
  distanceValue: document.getElementById("distance-value"),
  faceValue: document.getElementById("face-value"),
  doorValue: document.getElementById("door-value"),
  eventList: document.getElementById("event-list"),
  loadStream: document.getElementById("load-stream"),
  authorizeButton: document.getElementById("authorize-button"),
  denyButton: document.getElementById("deny-button"),
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

  applyPill(
    refs.doorPill,
    state.doorState,
    state.doorState === "Authorized" ? "success" : state.doorState === "Denied" ? "danger" : "neutral"
  );

  refs.faceToggle.checked = state.faceEnabled;
  refs.autoToggle.checked = state.autoMode;
  refs.faceLabel.textContent = state.faceEnabled ? "Face Detection On" : "Face Detection Off";
  refs.faceValue.textContent = state.faceEnabled ? "Enabled" : "Inactive";
  refs.presenceLabel.textContent = state.visitorPresent ? "Visitor present" : "No visitor";
  refs.presenceValue.textContent = state.visitorPresent ? "Yes" : "No";
  refs.distanceValue.textContent = state.distanceCm == null ? "-- cm" : `${state.distanceCm} cm`;
  refs.doorValue.textContent = state.doorState;
}

refs.loadStream.addEventListener("click", () => {
  const url = refs.streamUrl.value.trim();
  state.streamUrl = url;

  if (!url) {
    refs.streamImage.removeAttribute("src");
    refs.streamImage.style.display = "none";
    refs.streamEmpty.style.display = "grid";
    state.cameraState = "Idle";
    addEvent("Stream cleared");
    render();
    return;
  }

  refs.streamImage.src = url;
  refs.streamImage.style.display = "block";
  refs.streamEmpty.style.display = "none";
  state.cameraState = "Streaming";
  state.visitorPresent = true;
  state.distanceCm = 58;
  addEvent("Stream URL loaded");
  render();
});

refs.faceToggle.addEventListener("change", (event) => {
  state.faceEnabled = event.target.checked;
  addEvent(state.faceEnabled ? "Face recognition enabled" : "Face recognition disabled");
  render();
});

refs.autoToggle.addEventListener("change", (event) => {
  state.autoMode = event.target.checked;
  addEvent(state.autoMode ? "Auto mode enabled" : "Auto mode disabled");
  render();
});

refs.authorizeButton.addEventListener("click", () => {
  state.doorState = "Authorized";
  addEvent("Owner authorized access");
  render();
});

refs.denyButton.addEventListener("click", () => {
  state.doorState = "Denied";
  addEvent("Owner denied access");
  render();
});

addEvent("Dashboard scaffold created");
render();
