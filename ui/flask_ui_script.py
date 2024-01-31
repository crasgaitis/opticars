from flask import Flask, render_template
from flask_socketio import SocketIO
import threading
import eye_tracking

app = Flask(__name__)
socketio = SocketIO(app, async_mode='eventlet')

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('get_eye_tracking_data')
def get_eye_tracking_data():
    eye_tracking_data = eye_tracking.eye_tracking_data
    # print(eye_tracking_data)
    socketio.emit('update_eye_tracking_data', {'data': eye_tracking_data})

if __name__ == '__main__':
    # start the eye tracking script
    threading.Thread(target=eye_tracking.update_eye_tracking_data).start()

    # start the Flask app
    socketio.run(app, debug=True, port=4999)
