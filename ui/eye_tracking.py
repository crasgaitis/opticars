import threading
import serial
import numpy as np
from utils import get_tracker, gaze_data, gaze_id, preprocess_gaze, calculatePower_new3
import tobii_research as tr
import time

def gaze_data_callback(out):
    # if np.isnan(out['left_gaze_point_on_display_area'][0]) and np.isnan(out['right_gaze_point_on_display_area'][0]):
    #     return
            
    gazexy = preprocess_gaze(out)
    # print(gazexy)
    # print(np.round(gazexy, 1))
    # print('\n')
    # time.sleep(0.5)
    # eye_tracking_data = gaze_id(gazexy)
    
    # # stream to bytes
    # left, right = calculatePower_new2(gazexy)
    left, right = calculatePower_new3(gazexy)  # try

    # print(f"{left}, {right}")
    cmd = f"CMD: {round(((left)), 2)},{round((right), 2)}\n" # format request to controller
    # cmd=f"CMD: 0.1, 1.9"
    # print(cmd)

    car.write(cmd.encode())
    car.flush() # make sure it all sends before you start reading
    print(cmd)
    
def update_eye_tracking_data():
    global car

    baud = 9600
    bluetoothPort = "COM14"
    car = serial.Serial(bluetoothPort, baud)

    TRACKER = get_tracker()
    
    TRACKER.subscribe_to(tr.EYETRACKER_GAZE_DATA, gaze_data_callback, as_dictionary=True)

    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("ouch")
        TRACKER.unsubscribe_from(tr.EYETRACKER_GAZE_DATA, gaze_data_callback)

        car.close() 

# Start a thread to continuously update eye tracking data
if __name__ == '__main__':
    threading.Thread(target=update_eye_tracking_data).start()
