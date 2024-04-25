import serial
import time

baud = 9600
bluetoothPort = "COM11"

print("Attempting to connect")
carSerial = serial.Serial(bluetoothPort, baud)
# give time to connect
time.sleep(2)
print("Connected")
while True:
    print("Sending a ping")
    msg = 'ping\n'.encode()
    carSerial.write(msg)
    carSerial.flush()
    print("ping success")
    
    time.sleep(1)
    
    # print("Receiving a pong")
    # msg = carSerial.readline()
    # print(f"Pong success: {msg}")