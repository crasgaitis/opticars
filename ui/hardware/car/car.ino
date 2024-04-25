/*  car.ino
 *  @file      car.ino
 *  @author    Peyton Anton Rapo
 *  @date      11-March-2024
 *  @brief     Code to run on car for use in the final project (Part C) of lab 4
 *   
 *  This code will read in commands from serial and will handle these commands accordingly. Currently supports:
 *  - Movement Commands (CMD:MOVE x,y) where x,y is the percentage of max to move in that direction (-1,1) for each
 *  - Debug Enable Commands (CMD:DEBUG TRUE)
 *  - Debug Enable Commands (CMD:DEBUG FALSE)
 * 
 *  Acknowledgments: 
 *  - FreeRTOS Blink_AnalogRead example served as the starting point for this code
 *  - FreeRTOS Documentation
 *  - Arduino Documentation
 *  - FreeRTOS debugging tips (provided by course staff)
 *  - HC06 Tutorial: https://www.instructables.com/Tutorial-Using-HC06-Bluetooth-to-Serial-Wireless-U-1/
 *  - Dual Bridge Motor Driver Tutorial: https://www.bananarobotics.com/shop/How-to-use-the-L298N-Dual-H-Bridge-Motor-Driver
 */

#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <SoftwareSerial.h>

////////////////////////////////////////////////
// BLUETOOTH SETUP
////////////////////////////////////////////////

int RxD = 2;
int TxD = 3;
SoftwareSerial HC06(RxD,TxD);

////////////////////////////////////////////////
// DEBUG VARIABLES
////////////////////////////////////////////////

bool DEBUG = false;

////////////////////////////////////////////////
// TASK SKELETONS
////////////////////////////////////////////////

void TaskCommunicationManager( void *pvParameters );

void TaskCarManager( void *pvParameters );

////////////////////////////////////////////////
// COMMUNICATION MANAGER VARIABLES
////////////////////////////////////////////////

// Pointer to TaskUpdateMovement so I can start/suspend it as needed
TaskHandle_t updatePtr = NULL;

// TaskCommunicationManager will add updates to the updateQueue
// TaskMoveCar will read from the updateQueue and update its current movement order
QueueHandle_t updateQueue = NULL;

////////////////////////////////////////////////
// CAR MANAGER VARIABLES
////////////////////////////////////////////////

// Pins for reading from the thumbstick
#define IN1 4
#define IN2 5
#define IN3 6
#define IN4 7

// Definition of a movement order
struct movementOrder {
  float xVal; // -x is turn left and +x is turn right
  float yVal; // -y is reverse and +y is forward
};

// How long a car can go without a new update before stopping itself
// This is in ticks which I think are 1 ms, so this is a timeout of 100 ms.
// The CommunicationManager tries to read a message every 10 ms
// so this means it'd have to fail 10 times in a row to trigger this.
#define TIMEOUT 100 

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
  // Start bluetooth serial on baud rate of 9600. Can be interfaced with only by the python server over bluetooth
  
  // Use serial for debugging
  if (DEBUG) {
    Serial.begin(9600);
      
    while (!Serial) {
      ; // wait for bluetooth to connect. Needed for native USB, on LEONARDO, MICRO, YUN, and other 32u4 based boards.
    }
  } else {
    HC06.begin(9600);  

    while (!HC06) {
      ; // wait for bluetooth to connect. Needed for native USB, on LEONARDO, MICRO, YUN, and other 32u4 based boards.
    }
  }

  // Want both tasks to always be running and to be equal priority so they wait till they delay
  xTaskCreate(
    TaskCommunicationManager
    ,  "CommunicationManager"
    ,  128
    ,  NULL
    ,  0
    ,  NULL );

  xTaskCreate(
    TaskCarManager
    ,  "CarManager"
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
 * @brief Reads CMD messages from bluetooth and handles the messages accordingly. Supports movement and debug enable/disable commands.
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

  updatePtr = NULL;

  // Always want to be in a loop of waiting for CMDs from the server and then updating the current movement order and sending back an ACK
  for (;;) {
    // Wait for a CMD message from bluetooth serial (Blocking)
    if (DEBUG) {
      while(!Serial.available()) { // block until there is data to read
        // Checking at a rate of 100Hz (100 times per second) so should be fast enough for real time, while waiting some time
        vTaskDelay( 10 / portTICK_PERIOD_MS ); // wait for 10 ms, which frees up the time for the car to move
      }
    } else {
      while(!HC06.available()) { // block until there is data to read
        // Checking at a rate of 100Hz (100 times per second) so should be fast enough for real time, while waiting some time
        vTaskDelay( 10 / portTICK_PERIOD_MS ); // wait for 10 ms, which frees up the time for the car to move
      }
    }

    // Handle CMD message (in the form of "CMD:command\n")
    String cmd = "";
    if (DEBUG) {
      cmd = Serial.readStringUntil('\n');
    } else {
      cmd = HC06.readStringUntil('\n');
    }
    debug(cmd);

    // for each CMD message we must reply with a ACK message
    // since the bluetooth chip isn't great and we aren't processing ACK commands rn
    // it is not necesarry to reply.
    if (cmd.startsWith("CMD:MOVEORDER ")) { // command to update movement order
      debug("Recevied Command: MOVEORDER");
      processMoveOrder(cmd);
      // HC06.println("ACK: MOVEORDER UPDATED");
      debug("ACK: MOVEORDER UPDATED");
    } else if (cmd.equals("CMD:DEBUG TRUE")) { // command to enable debug mode
      DEBUG = true; 
      // HC06.println("ACK:DEBUG MODE ENABLED");
      debug("ACK:DEBUG MODE ENABLED");
    } else if (cmd.equals("CMD:DEBUG FALSE")) { // command to disable debug mode
      DEBUG = false;
      // HC06.println("ACK:DEBUG MODE DISABLED");
      debug("ACK:DEBUG MODE DISABLED");
    } else { // unknown command
      debug("Error Recevied Unknown Command: " + cmd);
      debug("ACK:UNKNOWN CMD");
      // HC06.println("ACK:UNKNOWN CMD");
    }
  }
}

////////////////////////////////////////////////
// CAR MANAGER TASK
////////////////////////////////////////////////

/**
 * @brief Will constantly move car according to the current move order. Reads from the updateQueue to update move order accordingly
 * 
 * REFERENCES: Used Blink_AnalogRead FreeRTOS example as the starter code for this method, FreeRTOS documentation, and the FreeRTOS debugging tips provided by the course staff.
 */
void TaskCarManager(void *pvParameters)
{
  (void) pvParameters;

  // wait for 10 ms as recommended in the FreeRTOS debugging tips 
  vTaskDelay( 10 / portTICK_PERIOD_MS );

  // this will only be run once since TaskCarManager is only called once
  carManagerSetup();

  // run movement test. Could make this its own CMD
  // movementTest();

  // Want the car doing nothing intially
  movementOrder currOrder = {0.0, 0.0};

  TickType_t lastUpdate = xTaskGetTickCount();

  for(;;) {
    // if there is a new movement order, update current order
    movementOrder newOrder; // place to put new order. Would move into currOrder but might overwrite even if unsuccessful
    if (xQueueReceive(updateQueue, &newOrder, (TickType_t) 0) == pdPASS) { // want a wait of 0 ticks since we want it to never block on waiting
      currOrder = newOrder; // update order
      lastUpdate = xTaskGetTickCount(); // update the last time an update has happened
    } else {
      TickType_t currTick = xTaskGetTickCount();
      // if we haven't had an update in a while, stop the car as there has most likely been an error
      if (currTick - lastUpdate >= TIMEOUT) {
        currOrder = {0.0, 0.0};
        stopCar(); // make sure the car is stopped lol
      }
    }

    // process current movement order and convert to a duty cycle
    float xVal = currOrder.xVal; // -x is turn left and +x is turn right
    float yVal = currOrder.yVal; // -y is reverse and +y is forward

    // The sign of the values determine direction & the value determines the percentage of the duty cycle to run for
    // In this case we set the period to be 10 ms since that's the current message communication wait time (so we can treat that as the movement time).
    // The range of xVal and yVal are [-1,1] on increments of +-0.1 This means we can multiply by 10 to get which milliseconds to run up to
    // i.e. if xVal = 0.8 for the first 8 ms for this current millis() frame we will run it. 
    
    // since we didn't manage to figure out rotation and forward/back at the same time we will alternate who is activating each tick by delaying for 1 ms for each instruction.
    // This does mean there will be a 2 ms delay between an update and it being applied potentially.

    unsigned long time; // container for time

    // check if xVal is in this current ms
    time = millis();
    if (abs(xVal) * 10 <= (time % 11)) { // if we don't pass the car is already stopped
      if (xVal < 0) { // turn left aka counter clockwise
        rotateCounterClockwise();
      } else { // turn right aka clockwise
        rotateClockwise();
      }
    } else {
      stopCar();
    }
    vTaskDelay( 1 / portTICK_PERIOD_MS ); // delay for 1 ms

    // check if yVal is in this current ms
    time = millis();
    if (abs(yVal) * 10 <= (time % 11)) { // if we don't pass the car is already stopped
      if (yVal < 0) { // move backward
        moveBackward();
      } else { // move forward
        moveForward();
      }
    } else {
      stopCar();
    }
    vTaskDelay( 1 / portTICK_PERIOD_MS ); // delay for 1 ms
  }
}


////////////////////////////////////////////////
// COMMUNICATION MANAGER HELPER FUNCTIONS
////////////////////////////////////////////////

/**
 * @brief Setups up everything the communication manager needs. Only the updateQueue as of now.
 * 
 * REFERENCE: FreeRTOS documentation
 */
void communicationManagerSetup() {
  // Really only needs to be size 1 since we are working with sending things one at a time
  // could maybe even use a global variable. 
  updateQueue = xQueueCreate(
    1,
    sizeof(movementOrder)
  );
}

/**
 * @brief Proceses a move order and sends it to the update queue
 * 
 * REFERENCE: FreeRTOS documentation and Arduino documentation
 */
void processMoveOrder(String cmd) {
  float xVal = 0.0;
  float yVal = 0.0;

  // Parse cmd for xVal and yVal
  // cmd is in the form "CMD:MOVEORDER x.x,y.y"
  // x is indices [14, 18]
  // y is indices [19, 23]
  xVal = cmd.substring(14, 17).toFloat();
  yVal = cmd.substring(18, 22).toFloat();

  // Create movement order
  movementOrder order = {xVal, yVal};

  // timeout = 0 aka an error has occured if we ever have to wait.
  if (xQueueSendToBack(updateQueue, (void *) &order, (TickType_t) 0) != pdPASS) {
    // error occured. For now there is no error handling, but can maybe try to restart the system as an expansion upon this project.
    debug("Error: Can't send data back to updateQueue");
  }
  // // wait for 10 ms, which frees up the time for the car to move
  // vTaskDelay( 10 / portTICK_PERIOD_MS );
}

////////////////////////////////////////////////
// CAR MANAGER HELPER FUNCTIONS
////////////////////////////////////////////////

/**
 * @brief Setups up everything the car manager needs. Currently this entails setting the motors as outputs.
 */
void carManagerSetup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
}

/**
 * @brief Sets the wheels of the car to rotate the car counter clockwise
 * 
 * REFERENCE: Dual Bridge Motor Driver Tutorial
 */
void rotateCounterClockwise() {
  // Left side of the car: going forward
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  // Right side of the car: going backward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
}

/**
 * @brief Sets the wheels of the car to rotate the car clockwise
 * 
 * REFERENCE: Dual Bridge Motor Driver Tutorial
 */
void rotateClockwise() {
  // Left side of the car: going backward
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  // Right side of the car: going forward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
}

/**
 * @brief Sets the wheels of the car to be moving forward
 * 
 * REFERENCE: Dual Bridge Motor Driver Tutorial
 */
void moveForward() {
  // Left side of the car: going forward
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW); 

  // Right side of the car: going backward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
}

/**
 * @brief Sets the wheels of the car to be moving backward
 * 
 * REFERENCE: Dual Bridge Motor Driver Tutorial
 */
void moveBackward() {
  // Left side of the car: going backward
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH); 

  // Right side of the car: going backward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
}

/**
 * @brief Stop all of the wheels of the car
 * 
 * REFERENCE: Dual Bridge Motor Driver Tutorial
 */
void stopCar() {
  // Left side of the car: Stopped
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW); 

  // Right side of the car: Stopped
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

/**
 * @brief Test that all directions are currently working and moving the correct way. For use in debugging only.
 */
void movementTest() {
  stopCar(); // stop card so there is a bit of a delay before the test starts
  vTaskDelay( 1000 / portTICK_PERIOD_MS );
  rotateCounterClockwise();
  vTaskDelay( 500 / portTICK_PERIOD_MS );
  rotateClockwise();
  vTaskDelay( 500 / portTICK_PERIOD_MS );
  moveForward();
  vTaskDelay( 500 / portTICK_PERIOD_MS );
  moveBackward();
  vTaskDelay( 500 / portTICK_PERIOD_MS );
  stopCar();
}

////////////////////////////////////////////////
// UTIL FUNCTIONS
////////////////////////////////////////////////

/**
 * @brief Prints to both serial ports if DEBUG mode is enabled
 */
void debug(String msg) {
  if (DEBUG) {
    Serial.println(msg);
  } 
}

