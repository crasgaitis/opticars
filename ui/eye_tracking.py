import time
import threading
from utils import get_tracker, gaze_data, gaze_id

# Placeholder
eye_tracking_data = "init"

def update_eye_tracking_data():
    global eye_tracking_data
    TRACKER = get_tracker()
    # i = 0
    while True:
        # eye_tracking_data = "Updated Eye Tracking Data " + str(i)
        # # print(eye_tracking_data)
        # i += 1
        # time.sleep(0.1)
        out = gaze_data(TRACKER, 1)
        eye_tracking_data = gaze_id(out)

# Start a thread to continuously update eye tracking data
if __name__ == '__main__':
    threading.Thread(target=update_eye_tracking_data).start()
