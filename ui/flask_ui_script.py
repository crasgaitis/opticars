from flask import Flask, render_template
from flask_socketio import SocketIO
import threading
import eye_tracking
from utils import build_dataset_from_csv, gaze_id

app = Flask(__name__)
socketio = SocketIO(app, async_mode='eventlet')

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('get_eye_tracking_data')
def get_eye_tracking_data():

    # eye_tracking_data = eye_tracking.eye_tracking_data
    # get a dataframe from the build dataset function -> from csv just for testing
    df = build_dataset_from_csv('cat_looking_updown.csv', 'cat')
    data = gaze_id(df)
    
    # print(eye_tracking_data)
    socketio.emit('update_eye_tracking_data_', {'data': data})

if __name__ == '__main__':
    # start the eye tracking script
    threading.Thread(target=eye_tracking.update_eye_tracking_data).start()

    # start the Flask app
    socketio.run(app, debug=True, port=4999)
