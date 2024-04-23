import pandas as pd
from utils import build_dataset, get_tracker, calculatePower
import time, serial

# divide numbers by 10 and make sure they're floats (-1.0, 1.1)

# def sendReq(controller, debug=False):   

#     data, _ = build_dataset(tracker, 'cat')
#     data2 = pd.DataFrame(data.iloc[0]).transpose()
#     left, right = calculatePower(data2)
#     req = f"req: {left}, {right}" # format request to controller

#     # Send data request to the controller
#     controller.write(req.encode())
#     controller.flush() # make sure it all sends before you start reading
#     print("Sent: " + req)

#     # Read resp from controller
#     resp = controller.readline().decode()
#     print("Received: " + resp)

#     if debug:
#         while not resp.startswith("RESP:"): # if debug is enabled may receive non-response messages
#             resp = controller.readline().decode()
#             print("Received: " + resp)

#     return resp


baud = 9600

# TODO: Make each controller / car setup functions for better modular code
# # set up serial port to communicate with the controller via usb
# controllerPort = "/dev/cu.usbmodem14101"
# controllerSerial = serial.Serial(controllerPort, baud)
# # give time to connect
# time.sleep(2)
tracker = get_tracker()


# set up serial port to communicate with the car over bluetooth
bluetoothPort = "COM10"
car = serial.Serial(bluetoothPort, baud)
# give time to connect
time.sleep(2)

# want to be able to catch keyboard interrupt exceptions so we can safely close serial ports
try:
    while True:
        data, _ = build_dataset(tracker, 'cat')
        data2 = pd.DataFrame(data.iloc[0]).transpose()
        left, right = calculatePower(data2)
        cmd = f"CMD:MOVEORDER {left},{right}\n" # format request to controller

        # # create cmd message from this resp
        # cmd = createCmd(resp)

        # Send cmd message to the car
        # Right now we don't really need to handle the ack message but in the future could support better error handling
        car.write(cmd.encode())
        car.flush() # make sure it all sends before you start reading
        print("Sent: " + cmd)
        
# when we want to end the program safely close the serial ports
except KeyboardInterrupt:
    # controller.close()
    car.close()