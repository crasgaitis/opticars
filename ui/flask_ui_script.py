from flask import Flask, render_template
from flask_socketio import SocketIO
import threading
from utils import gaze_id, get_tracker, gaze_data

app = Flask(__name__)
app.logger.disabled = True
socketio = SocketIO(app, async_mode='eventlet')

lock = threading.Lock()

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('get_eye_tracking_data')
def get_eye_tracking_data():
    while True:
        lock.acquire()
        
        try:
            TRACKER = get_tracker()
            out = gaze_data(TRACKER, 0.3)
            data = gaze_id(out)
            
            socketio.emit('update_eye_tracking_data', {'data': data})
        finally:
            lock.release()

# def get_eye_tracking_data():
#     TRACKER = get_tracker()
#     out = gaze_data(TRACKER, 0.3)
#     data = gaze_id(out)
    
#     socketio.emit('update_eye_tracking_data', {'data': data})

if __name__ == '__main__':
    
    # start the eye tracking script
    threading.Thread(target=get_eye_tracking_data).start()
    
    # start the Flask app
    socketio.run(app, debug=True, port=4999)