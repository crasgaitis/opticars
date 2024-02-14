# import time
# import threading

# # placeholder
# eye_tracking_data = "init"

# def update_eye_tracking_data():
#     global eye_tracking_data
#     i = 0
#     while True:
#         eye_tracking_data = "Updated Eye Tracking Data " + str(i)
#         # print(eye_tracking_data)
#         i += 1
#         time.sleep(0.1)

# # thread to continuously update eye tracking data
# if __name__ == '__main__':
#     threading.Thread(target=update_eye_tracking_data).start()
