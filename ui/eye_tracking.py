import threading
import serial
import numpy as np
from utils import get_tracker, gaze_data, gaze_id, preprocess_gaze, calculatePower_new
import tobii_research as tr
import time

def gaze_data_callback(out):
    if np.isnan(out['left_gaze_point_on_display_area'][0]) and np.isnan(out['right_gaze_point_on_display_area'][0]):
        return
            
    gazexy = preprocess_gaze(out)
    eye_tracking_data = gaze_id(gazexy)
    
    # stream to bytes
    left, right = calculatePower_new(gazexy)
    cmd = f"CMD: {left + 1.0},{right + 1.0}\n" # format request to controller
    car.write(cmd.encode())
    car.flush() # make sure it all sends before you start reading
    print(f"Sent {left},{right}")
    
def update_eye_tracking_data():
    global car

    baud = 9600
    bluetoothPort = "COM3"
    car = serial.Serial(bluetoothPort, baud)

    TRACKER = get_tracker()
    
    TRACKER.subscribe_to(tr.EYETRACKER_GAZE_DATA, gaze_data_callback, as_dictionary=True)

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        TRACKER.unsubscribe_from(tr.EYETRACKER_GAZE_DATA, gaze_data_callback)

        car.close()

# Start a thread to continuously update eye tracking data
if __name__ == '__main__':
    threading.Thread(target=update_eye_tracking_data).start()
