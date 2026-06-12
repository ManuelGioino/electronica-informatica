from flask import Flask, Response, request, send_from_directory
import threading
import time
import os

app = Flask(__name__)
latest_frame = None
captured_frame = None
latest_capture_filename = None
lock = threading.Lock()

FOTOS_DIR = "/app/fotos"
os.makedirs(FOTOS_DIR, exist_ok=True)


@app.route('/frame', methods=['POST'])
def receive_frame():
    global latest_frame
    with lock:
        latest_frame = request.data
    return 'OK', 200


@app.route('/capture', methods=['POST'])
def store_capture():
    global captured_frame, latest_capture_filename
    data = request.data
    filename = f"capture_{int(time.time())}.jpg"
    with lock:
        captured_frame = data
        latest_capture_filename = filename
    with open(os.path.join(FOTOS_DIR, filename), 'wb') as f:
        f.write(data)
    return 'OK', 200


@app.route('/capture-filename')
def get_capture_filename():
    with lock:
        fn = latest_capture_filename
    if fn is None:
        return 'No capture yet', 503
    return fn, 200


@app.route('/fotos/<filename>')
def serve_foto(filename):
    return send_from_directory(FOTOS_DIR, filename)


@app.route('/health')
def health():
    return 'OK', 200


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8888, threaded=True)
