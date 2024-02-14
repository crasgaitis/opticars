from flask import Flask, render_template
from flask_socketio import SocketIO
import threading
import eye_tracking
from utils import build_dataset, gaze_id, get_tracker, gaze_data

app = Flask(__name__)
socketio = SocketIO(app, async_mode='eventlet')

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('get_eye_tracking_data')
def get_eye_tracking_data():

    # eye_tracking_data = eye_tracking.eye_tracking_data
    # get a dataframe from the build dataset function -> from csv just for testing
    tracker = get_tracker()
    test = gaze_data(tracker, 0.01)
    df = build_dataset(tracker, 'test', time_step_sec = 0.1, tot_time_min=0.15)
    data = gaze_id(df)
    
    # print(eye_tracking_data)
    socketio.emit('update_eye_tracking_data_', {'data': data})

if __name__ == '__main__':
    # start the eye tracking script
    threading.Thread(target=get_eye_tracking_data).start()

    # start the Flask app
    socketio.run(app, debug=True, port=4999)
