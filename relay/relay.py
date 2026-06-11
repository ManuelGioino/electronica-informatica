from flask import Flask, Response, request
import threading
import time

app = Flask(__name__)
latest_frame = None
lock = threading.Lock()


@app.route('/frame', methods=['POST'])
def receive_frame():
    global latest_frame
    with lock:
        latest_frame = request.data
    return 'OK', 200


def generate_stream():
    while True:
        with lock:
            frame = latest_frame
        if frame:
            yield (
                b'--frame\r\n'
                b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n'
            )
        time.sleep(0.05)  # ~20 FPS


@app.route('/stream')
def stream():
    return Response(
        generate_stream(),
        mimetype='multipart/x-mixed-replace; boundary=frame'
    )


@app.route('/health')
def health():
    return 'OK', 200


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8888, threaded=True)
