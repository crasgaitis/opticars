/*  controller.ino
 *  @file      controller.ino
 *  @author    Peyton Anton Rapo
 *  @date      11-March-2024
 *  @brief     Code to run on controller for use in controlling car for final project (Part C) of lab 4
 *   
 *  This code will read in requests from serial and will handle these requests accordingly. Currently supports:
 *  - Data Requests (REQ:DATA)
 *  - Debug Enable Requests (REQ:DEBUG TRUE)
 *  - Debug Enable Requests (REQ:DEBUG FALSE)
 *  
 *  Note: This code probably doesn't need to rely upon FreeRTOS, but serves as good practice so using it.
 *        Also, car.ino will feature very similar logic as they are both designed in the same way to interface with the python server.
 * 
 *  Acknowledgments: 
 *  - FreeRTOS Blink_AnalogRead example served as the starting point for this code
 *  - FreeRTOS Documentation
 *  - Arduino Documentation
 *  - FreeRTOS debugging tips (provided by course staff)
 */

#include <Arduino_FreeRTOS.h>
#include <queue.h>

////////////////////////////////////////////////
// DEBUG VARIABLES
////////////////////////////////////////////////
bool DEBUG = false;

////////////////////////////////////////////////
// TASK SKELETONS
////////////////////////////////////////////////

void TaskCommunicationManager( void *pvParameters );

void TaskAnalogRead( void *pvParameters );

////////////////////////////////////////////////
// COMMUNICATION MANAGER VARIABLES
////////////////////////////////////////////////

// Pointer to TaskAnalogRead so I can start/suspend it as needed
TaskHandle_t readPtr = NULL;

// TaskAnalogRead fills the commQueue will data to send
// TaskCommunicationManager will read from commQueue and send to the arduino via serial
QueueHandle_t commQueue = NULL;

////////////////////////////////////////////////
// ANALOG READ VARIABLES
////////////////////////////////////////////////

// Pins for reading from the thumbstick
#define VRx A0
#define VRy A1

struct thumbstickData {
  int xVal;
  int yVal;
};

////////////////////////////////////////////////
// ARDUINO SETUP & LOOP
////////////////////////////////////////////////

/**
 * @brief will run once at the beginning of the arduino life cycle. Assumes connected to computer to start
 * 
 * REFERENCES: Used Blink_AnalogRead FreeRTOS example as the starter code for this method
 */
void setup() {
  // Want this to occur before anything else, and not multithreaded.
  // Start serial on baud rate of 9600. Can be interfaced with from serial monitor (for debugging purposes) or from the python server
  Serial.begin(9600);
  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB, on LEONARDO, MICRO, YUN, and other 32u4 based boards.
  }

  // Only the communication manager is initially active since we need the python server to initiate a data request
  xTaskCreate(
    TaskCommunicationManager
    ,  "CommunicationManager"
    ,  128
    ,  NULL
    ,  0
    ,  NULL );

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
}

/**
 * @brief run as often as possible on the arduino. Not used for FreeRTOS.
 * 
 * REFERENCES: Used Blink_AnalogRead FreeRTOS example as the starter code for this method
 */
void loop()
{
  // Empty. Things are done in Tasks.
}

////////////////////////////////////////////////
// COMMUNICATION MANAGER TASK
////////////////////////////////////////////////

/**
 * @brief Reads REQ messages from Serial (either serial monitor, if debugging, or from the python server over usb) and handles the messages accordingly. Supports data and debug enable/disable requests.
 * 
 * REFERENCES: Used Blink_AnalogRead FreeRTOS example as the starter code for this method, Arduino and FreeRTOS documentation, and the FreeRTOS debugging tips provided by the course staff.
 */
void TaskCommunicationManager(void *pvParameters)
{
  (void) pvParameters;

  // wait for 10 ms as recommended in the FreeRTOS debugging tips 
  vTaskDelay( 10 / portTICK_PERIOD_MS );

  // this will only be run once since TaskCommunicationManager is only called once
  communicationManagerSetup();

  readPtr = NULL;
  // Always want to be in a loop of reading data from thumbstick, sending to serial, and waiting for an ACK
  for (;;) {
    // Wait for a REQ message from Serial (Blocking)
    while(!Serial.available()) { // block until there is data to read
      // Checking at a rate of 100Hz (100 times per second) so should be fast enough for real time, while waiting some time
      vTaskDelay( 10 / portTICK_PERIOD_MS ); // wait for 10 ms
    }

    // Handle REQ message (in the form of "REQ:request\n")
    String req = Serial.readStringUntil('\n');

    // for each REQ message we must reply with a RESP message
    if (req.equals("REQ:DATA")) { // request for thumbstick data
      debug("Recevied Request: DATA");
      thumbstickData data = getThumbstickData();
      Serial.println("RESP:" + String(data.xVal) + "," + String(data.yVal));
    } else if (req.equals("REQ:DEBUG TRUE")) { // request to enable debug mode
      DEBUG = true; 
      Serial.println("RESP:DEBUG MODE ENABLED");   
    } else if (req.equals("REQ:DEBUG FALSE")) { // request to disable debug mode
      DEBUG = false;
      Serial.println("RESP:DEBUG MODE DISABLED");
    } else { // unknown request
      debug("Error Recevied Unknown Request: " + req);
      Serial.println("RESP:UNKNOWN REQ");
    }
  }
}

////////////////////////////////////////////////
// ANALOG READ TASK
////////////////////////////////////////////////

/**
 * @brief Reads in X and Y data from the analog thumbstick and adds it to the commQueue. Relies on TaskCommunicationManager to start this task.
 * 
 * REFERENCES: Used Blink_AnalogRead FreeRTOS example as the starter code for this method, FreeRTOS documentation, and the FreeRTOS debugging tips provided by the course staff.
 */
void TaskAnalogRead(void *pvParameters)
{
  (void) pvParameters;

  // wait for 10 ms as recommended in the FreeRTOS debugging tips 
  vTaskDelay( 10 / portTICK_PERIOD_MS );

  // initial values for X and Y
  int xVal = 0;
  int yVal = 0;
  
  // Want to keep looping, but will shut itself down after every data point collected for efficiency (might be overkill though)
  for (;;)
  {
    // read X and Y values from the analog thumbstick
    xVal = analogRead(VRx);
    yVal = analogRead(VRy);

    thumbstickData data = {xVal, yVal}; // wrapper data structure for the thumbstick
    
    // timeout = 0 aka an error has occured if we ever have to wait.
    if (xQueueSendToBack(commQueue, (void *) &data, (TickType_t) 0) != pdPASS) {
      // error occured. For now there is no error handling, but can maybe try to restart the system as an expansion upon this project.
      debug("Error: Can't send data back to commQueue");
    }

    vTaskSuspend( NULL ); // suspends itself to save CPU space. Will need to awoken by the communication manager
  }
}

////////////////////////////////////////////////
// COMMUNICATION MANAGER HELPER FUNCTIONS
////////////////////////////////////////////////

/**
 * @brief Setups up everything the communication manager needs. Only the commQueue as of now.
 * 
 * REFERENCE: FreeRTOS documentation
 */
void communicationManagerSetup() {
  // Really only needs to be size 1 since we are working with sending things one at a time
  // could maybe even use a global variable. 
  commQueue = xQueueCreate(
    1,
    sizeof(thumbstickData)
  );
}

/**
 * @brief Handles an incoming data request. Will start the thumbstick task and try to read data from it and then send it over the serial.
 * 
 * REFERENCE: FreeRTOS documentation
 */
thumbstickData getThumbstickData() {
  // Once request received get data from analog thumbstick
  // will effectively only run the first time this loop is called
  if (readPtr == NULL) {
    xTaskCreate(
      TaskAnalogRead
      ,  "AnalogRead"
      ,  128
      ,  NULL
      ,  1 // want this to take precedence over the communication manager. Is fine since we are expecting only 1 message at a time.
      ,  &readPtr ); // setup a task handle to the task so we can suspend and restart it when necessary
  } else {
    vTaskResume(readPtr); // will resume the thumbstick task and it will self suspend once finished.
  }

  // Block until we get data from the thumbstick
  // In an expanded version of this project we could throw some errors when blocking too long
  thumbstickData data;
  while (xQueueReceive(commQueue, &data, (TickType_t) 10) != pdPASS) { debug("Waiting for data from thumbstick"); }

  return data;
}

////////////////////////////////////////////////
// UTIL FUNCTIONS
////////////////////////////////////////////////

/**
 * @brief Prints to serial if DEBUG mode is enabled
 */
void debug(String msg) {
  if (DEBUG) Serial.println(msg);
}

