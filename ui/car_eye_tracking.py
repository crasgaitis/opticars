import pandas as pd
from hardware.server import enableDebug, disableDebug, setup, loop
from utils import build_dataset, get_tracker, calculatePower

def sendReq(controller, debug=False):   

    data, _ = build_dataset(tracker, 'cat')
    data2 = pd.DataFrame(data.iloc[0]).transpose()
    left, right = calculatePower(data2)
    req = f"req: {left}, {right}" # format request to controller

    # Send data request to the controller
    controller.write(req.encode())
    controller.flush() # make sure it all sends before you start reading
    print("Sent: " + req)

    # Read resp from controller
    resp = controller.readline().decode()
    print("Received: " + resp)

    if debug:
        while not resp.startswith("RESP:"): # if debug is enabled may receive non-response messages
            resp = controller.readline().decode()
            print("Received: " + resp)

    return resp


controller, car = setup()

debug = False
if debug:
    enableDebug(controller, car)
else:
    disableDebug(controller, car)

# want to be able to catch keyboard interrupt exceptions so we can safely close serial ports
try:
    while True:
        loop(controller, car, debug)
        
# when we want to end the program safely close the serial ports
except KeyboardInterrupt:
    # controller.close()
    car.close()
        
tracker = get_tracker()

while True:
    sendReq(controller)