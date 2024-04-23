'''
    server.py
    @file      server.py
    @author    Peyton Anton Rapo
    @date      11-March-2024
    @brief     Code to handle communication between the controller and the car

    This code will request data from the controller and then send that data to the car.
    Handles messages one at a time and uses a handshake type system where every message must be met
    with either a RESP or ACK message.

    Note: This code could probably run on an arduino if you have two arduinos with bluetooth.

    Acknowledgments: 
    - pySerial documenation: Referenced this when using the serial port communication methods
        - link: https://pyserial.readthedocs.io/en/latest/index.html
'''

import serial
import time
import tobii_research as tr

def get_tracker():
  all_eyetrackers = tr.find_all_eyetrackers()

  for tracker in all_eyetrackers:
    # print("Model: " + tracker.model)
    # print("Serial number: " + tracker.serial_number) 
    # print(f"Can stream eye images: {tr.CAPABILITY_HAS_EYE_IMAGES in tracker.device_capabilities}")
    # print(f"Can stream gaze data: {tr.CAPABILITY_HAS_GAZE_DATA in tracker.device_capabilities}")
    return tracker


################################################
# ARDUINO (KINDA) SETUP & LOOP
################################################

'''
    @brief Setups up serial connections with controller & car.

    @return The serial connection for the controller and the serial connection for the car.
'''
def setup():
    # GIANT TODO: have this create controller / car objects / classes that I can call send message on
    # GIANT TODO 2: Need to have a test that gets the bounds of the eye tracker.
    # trying to mimic arduino code for readability
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
    carSerial = serial.Serial(bluetoothPort, baud)
    # give time to connect
    time.sleep(2)

    # return eye tracker & serial port
    return tracker, carSerial

'''
    @brief Loop a cycle of instructions: read data from the thumbstick and send a command to the car.

    @param controller Serial connection for controller.
    @param car Serial connection for car.
    @param debug Whether the controller and car are in debug mode or not. Defaults to False.
'''
def loop(eyeTracker, car, debug=False):
    # Will process messages one at a time and will require an RESP or ACK message from controller or car respectively

    # # Send get data request and get response from controller
    # req = "REQ:DATA\n"
    # resp = sendReq(controller, req, debug)
    
    # cmd = sendReq(controller)
    data, _ = build_dataset(eyeTracker, 'cat')
    data2 = pd.DataFrame(data.iloc[0]).transpose()
    left, right = calculatePower(data2)
    cmd = f"CMD: {left},{right}\n" # format request to controller

    # # create cmd message from this resp
    # cmd = createCmd(resp)

    # Send cmd message to the car
    # Right now we don't really need to handle the ack message but in the future could support better error handling
    sendCmd(car, cmd, debug)

################################################
# SENDING MESSAGES
################################################

'''
    @brief Sends the given request message to the controller and returns its response

    @param controller Serial connection for the controller.
    @param req The request message to send.
    @param debug Whether debug mode is enabled or not. Defaults to False.

    @return The resp message from the controller. Will start with "RESP:"
'''
def sendReq(controller, req, debug=False):     
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

# def sendReq(controller):
#     data, dict_list = build_dataset(tracker, 'cat', time_step_sec=0.15, tot_time_min=0.0025)
#     data2 = pd.DataFrame(data.iloc[0]).transpose()
#     print(data)
#     mag, dir = detect_movement_example_with_scaling(data2)
#     print(f"Mag: {mag}, Dir: {dir}")

'''
    @brief Sends the given command message to the car and returns its acknowledgement (if applicable)

    @param car Serial connection for the car.
    @param cmd The command message to send.
    @param debug Whether debug mode is enabled or not. Defaults to False.

    @return The ack message from the car. Will start with "ACK:"
'''
def sendCmd(car, cmd, debug=False):     
    # Send cmd message to the car
    car.write(cmd.encode())
    car.flush() # make sure it all sends before you start reading
    print("Sent: " + cmd)

    # for now no ACK messages necesarry
    # # Read ack from car
    # ack = car.readline().decode()
    # print("Received: " + ack)

    # if debug:
    #     while not ack.startswith("ACK:"): # if debug is enabled may receive non-ack messages
    #         ack = car.readline().decode()
    #         print("Received: " + ack)

    # return ack

################################################
# HELPER FUNCTIONS
################################################

'''
    @brief Creates a cmd message to send to the car.

    @param dataResp A response message from a data request

    @return The cmd message to send to the car which starts with "CMD:"
'''
def createCmd(dataResp):
    cmdHeader = "CMD:MOVEORDER "
    (x,y) = dataResp.split(':')[1].split(',') # seperate RESP and then split the x and y into 2 variables

    # convert strings to ints
    x = int(x)
    y = int(y)

    # Control system: +x is turn right, -x is turn left
    # Control system: +y is forward, -y is reverse
    y = -y # since techincally down on the thumbstick is positive

    # Center the data points: x default is 517, y default is 519 but it's okay
    midpoint = 512 # 1023 / 2
    x -= midpoint
    y += midpoint

    # Normalize so now it is a value between -1 and (basically) 1 in both directions
    x /= midpoint
    y /= midpoint

    # Round values so string isn't too long. Only need 1 decimal place since we are dealing with 1 ms chunks
    x = round(x, 1)
    y = round(y, 1)

    # Append to cmd header and convert back to strings
    return cmdHeader + str(x) + ',' + str(y)

'''
    @brief Enables debug mode on the controller and car.

    @param controller Serial connection for controller.
    @param car Serial connection for car.
'''
def enableDebug(controller, car):
    # # Enable debug mode for the controller
    # sendReq(controller, "REQ:DEBUG TRUE\n")

    # Enable debug mode for the car
    sendCmd(car, "CMD:DEBUG TRUE\n")

'''
    @brief Disable debug mode on the controller and car.

    @param controller Serial connection for controller.
    @param car Serial connection for car.
'''
def disableDebug(controller, car):
    # Disable debug mode for the controller
    sendReq(controller, "REQ:DEBUG FALSE\n")

    # Disable debug mode for the car
    sendCmd(car, "CMD:DEBUG FALSE\n")

################################################
# MAIN METHOD
################################################

'''
    @brief Main function for python. Will call setup() once and then repeat loop() forever.
'''
def main():
    eyeTracker, car = setup()
    
    debug = False
    # if debug:
    #     enableDebug(eyeTracker, car)
    # else:
    #     disableDebug(eyeTracker, car)

    # want to be able to catch keyboard interrupt exceptions so we can safely close serial ports
    try:
        while True:
            loop(eyeTracker, car, debug)
            
    # when we want to end the program safely close the serial ports
    except KeyboardInterrupt:
        # controller.close()
        car.close()

# python code so the script of the file is only run when run as main
if __name__ == "__main__":
    main()


