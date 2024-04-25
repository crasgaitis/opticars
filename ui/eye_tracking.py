import threading
import serial
import numpy as np
from utils import get_tracker, gaze_data, gaze_id, preprocess_gaze, calculatePower_new

def update_eye_tracking_data():
    global eye_tracking_data
    TRACKER = get_tracker()
    
    eye_tracking_data = "init"

    baud = 9600
    bluetoothPort = "COM5"
    car = serial.Serial(bluetoothPort, baud)

    # time.sleep(2)
    # i = 0
    try:
        while True:
            # eye_tracking_data = "Updated Eye Tracking Data " + str(i)
            # # print(eye_tracking_data)
            # i += 1
            # time.sleep(0.1)
            out = gaze_data(TRACKER, 0.7)
            
            if np.isnan(out['left_gaze_point_on_display_area'][0]) and np.isnan(out['right_gaze_point_on_display_area'][0]):
                continue
            
            gazexy = preprocess_gaze(out)
            eye_tracking_data = gaze_id(gazexy)
            
            # stream to bytes
            left, right = calculatePower_new(gazexy)
            cmd = f"CMD: {left + 1.0},{right + 1.0}\n" # format request to controller
            car.write(cmd.encode())
            car.flush() # make sure it all sends before you start reading
            print("Sent: " + cmd)
    except KeyboardInterrupt:
        car.close()

# Start a thread to continuously update eye tracking data
if __name__ == '__main__':
    threading.Thread(target=update_eye_tracking_data).start()
