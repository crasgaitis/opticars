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

// int RxD = 0;
// int TxD = 1;
// SoftwareSerial BTSerial(RxD, TxD);

// void setup() {
//   pinMode(RxD, INPUT);
//   pinMode(TxD, OUTPUT);
//   BTSerial.begin(9600); // Added semicolon here
//   Serial.begin(9600);   // Added semicolon here
// }

// void loop() {
//   Serial.println(BTSerial.available());
// }

SoftwareSerial HC06(RxD,TxD);



// ////////////////////////////////////////////////
// // DEBUG VARIABLES
// ////////////////////////////////////////////////

bool DEBUG = true;

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

#define IN1 6
#define IN2 7
#define IN3 4
#define IN4 5
#define ENA 9
#define ENB 3

// Definition of a movement order
struct movementOrder {
  float xVal; // -x is turn left and +x is turn right
  float yVal; // -y is reverse and +y is forward
};

// How long a car can go without a new update before stopping itself
// This is in ticks which I think are 1 ms, so this is a timeout of 100 ms.
// The CommunicationManager tries to read a message every 10 ms
// so this means it'd have to fail 10 times in a row to trigger this.
#define TIMEOUT 20

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

  debug("Connected");

  // TEST THE COMMUNICATION MANAGER
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
    if (cmd.startsWith("CMD: ")) { // command to update movement order
      debug("Recevied Command");
      processMoveOrder(cmd);
      debug("ACK: MOVEORDER UPDATED");
    } else { // unknown command
      debug("Error Recevied Unknown Command: " + cmd);
      debug("ACK:UNKNOWN CMD");
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

  // Want the car doing nothing intially
  movementOrder currOrder = {0.0, 0.0};

  TickType_t lastUpdate = xTaskGetTickCount();

  for(;;) {
    // if there is a new movement order, update current order
    movementOrder newOrder; // place to put new order. Would move into currOrder but might overwrite even if unsuccessful
    if (updateQueue != NULL && xQueueReceive(updateQueue, &newOrder, (TickType_t) 0) == pdPASS) { // want a wait of 0 ticks since we want it to never block on waiting
      currOrder = newOrder; // update order
      lastUpdate = xTaskGetTickCount(); // update the last time an update has happened
    } else {
      TickType_t currTick = xTaskGetTickCount();
      // if we haven't had an update in a while, stop the car as there has most likely been an error
      if (currTick - lastUpdate >= TIMEOUT) {
        currOrder = {0.0, 0.0};
      }
    }

    // process current movement order and convert to a duty cycle
    float xVal = currOrder.xVal; // -x is turn left and +x is turn right
    float yVal = currOrder.yVal; // -y is reverse and +y is forward

    leftControl(xVal);
    rightControl(yVal);
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
  // cmd is in the form "CMD: x.x,y.y"
  // x is indices [5, 7]
  // y is indices [9, 11]
  xVal = cmd.substring(5, 8).toFloat() - 1.0;
  yVal = cmd.substring(9, 12).toFloat() - 1.0;

  // Create movement order
  movementOrder order = {xVal, yVal};

  // timeout = 0 aka an error has occured if we ever have to wait.
  if (xQueueSendToBack(updateQueue, (void *) &order, (TickType_t) 0) != pdPASS) {
    // error occured. For now there is no error handling, but can maybe try to restart the system as an expansion upon this project.
    debug("Dropped Movement Order");
  }
  // wait for 10 ms, which frees up the time for the car to move
  // vTaskDelay( 10 / portTICK_PERIOD_MS );
}

////////////////////////////////////////////////
// CAR MANAGER HELPER FUNCTIONS
////////////////////////////////////////////////

/**
 * @brief Setups up everything the car manager needs. Currently this entails setting the motors as outputs.
 */
void carManagerSetup() {
  // put your setup code here, to run once:

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
}

void leftControl(float x) {
  if (x > 0) {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH); 
  } else {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW); 
  }
  int mag = (int) (255.0 * abs(x));
  analogWrite(ENA, mag);
}

void rightControl(float y) {
  if (y > 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW); 
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH); 
  }
  int mag = (int) (255.0 * abs(y));
  analogWrite(ENB, mag);
}


////////////////////////////////////////////////
// UTIL FUNCTIONS
////////////////////////////////////////////////

/**
 * @brief Prints to both serial ports if DEBUG mode is enabled
 */
void debug(String msg) {
  // If using car connected comment this out since you don't want it to be sending data backwards
  // if (DEBUG) {
  //   Serial.println(msg);
  // } 
}