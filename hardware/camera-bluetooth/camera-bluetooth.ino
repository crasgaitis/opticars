// ArduCAM Mini demo (C)2018 Lee
// Web: http://www.ArduCAM.com

// The demo sketch will do the following tasks:
// 1. Set the camera to JPEG output mode.
// 2. Read data from Serial port and deal with it
// 3. If receive 0x00-0x08,the resolution will be changed.
// 4. If receive 0x10,camera will capture a JPEG photo and buffer the image to FIFO.Then write datas to Serial port.
// 5. If receive 0x20,camera will capture JPEG photo and write datas continuously.Stop when receive 0x21.
// 6. If receive 0x30,camera will capture a BMP  photo and buffer the image to FIFO.Then write datas to Serial port.
// 7. If receive 0x11 ,set camera to JPEG output mode.
// 8. If receive 0x31 ,set camera to BMP  output mode.

#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include <SoftwareSerial.h>

int RxD = 2;
int TxD = 3;

// Definitions Arduino pins connected to input H Bridge
int IN1 = 4;
int IN2 = 5;
int IN3 = 6;
int IN4 = 7;

SoftwareSerial HC06(RxD,TxD);


#define BMPIMAGEOFFSET 66
const char bmp_header[BMPIMAGEOFFSET] PROGMEM =
{
  0x42, 0x4D, 0x36, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x28, 0x00,
  0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00,
  0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00,
  0x00, 0x00
};
// set pin 7 as the slave select for the digital pot:
const int CS = 8;
bool is_header = false;
int mode = 0;
uint8_t start_capture = 0;
ArduCAM myCAM( OV2640, CS );
uint8_t read_fifo_burst(ArduCAM myCAM);

void setup() {
  // put your setup code here, to run once:
  uint8_t vid, pid;
  uint8_t temp;

  // Set the output pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  Wire.begin();
  Serial.begin(57600);
  HC06.begin(57600);

  Serial.println(F("ACK CMD ArduCAM Start! END"));
  // HC06.println(F("ACK CMD ArduCAM Start! END"));

  // set the CS as an output:
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);
  // initialize SPI:
  SPI.begin();
    //Reset the CPLD
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  delay(100);
  
  while (1) {
    //Check if the ArduCAM SPI bus is OK
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55) {
      Serial.println(F("ACK CMD SPI interface Error!END"));
      // HC06.println(F("ACK CMD SPI interface Error!END"));

      delay(1000); continue;
    } else {
      Serial.println(F("ACK CMD SPI interface OK.END")); 
      // HC06.println(F("ACK CMD SPI interface OK.END"));
      break;
    }
  }

#if defined (OV2640_MINI_2MP_PLUS)
  while (1) {
    //Check if the camera module type is OV2640
    myCAM.wrSensorReg8_8(0xff, 0x01);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))) {
      Serial.println(F("ACK CMD Can't find OV2640 module!"));
      // HC06.println(F("ACK CMD Can't find OV2640 module!"));

      delay(1000); continue;
    }
    else {
      Serial.println(F("ACK CMD OV2640 detected.END")); break;
      // HC06.println(F("ACK CMD OV2640 detected.END")); break;

    }
  }
#endif
  //Change to JPEG capture mode and initialize the OV5642 module
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
#if defined (OV2640_MINI_2MP_PLUS)
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
#endif
  delay(1000);
  myCAM.clear_fifo_flag();
}

void loop() {
  // put your main code here, to run repeatedly:
  uint8_t temp = 0xff, temp_last = 0;
  bool is_header = false;
  if (HC06.available())
  {
    temp = HC06.read();
    switch (temp)
    {
      case 0:
        myCAM.OV2640_set_JPEG_size(OV2640_160x120); delay(1000);
        Serial.println(F("ACK CMD switch to OV2640_160x120END"));
        HC06.println(F("ACK CMD switch to OV2640_160x120END"));

        temp = 0xff;
        break;
      case 1:
        myCAM.OV2640_set_JPEG_size(OV2640_176x144); delay(1000);
        Serial.println(F("ACK CMD switch to OV2640_176x144END"));
        HC06.println(F("ACK CMD switch to OV2640_176x144END"));

        temp = 0xff;
        break;
      case 2:
        myCAM.OV2640_set_JPEG_size(OV2640_320x240); delay(1000);
        Serial.println(F("ACK CMD switch to OV2640_320x240END"));
        HC06.println(F("ACK CMD switch to OV2640_320x240END"));

        temp = 0xff;
        break;
      case 3:
        temp = 0xff;
        myCAM.OV2640_set_JPEG_size(OV2640_352x288); delay(1000);
        Serial.println(F("ACK CMD switch to OV2640_352x288END"));
        HC06.println(F("ACK CMD switch to OV2640_352x288END"));

        break;
      case 4:
        temp = 0xff;
        myCAM.OV2640_set_JPEG_size(OV2640_640x480); delay(1000);
        Serial.println(F("ACK CMD switch to OV2640_640x480END"));
        HC06.println(F("ACK CMD switch to OV2640_640x480END"));


        break;
      case 5:
        temp = 0xff;
        myCAM.OV2640_set_JPEG_size(OV2640_800x600); delay(1000);
        Serial.println(F("ACK CMD switch to OV2640_800x600END"));
        HC06.println(F("ACK CMD switch to OV2640_800x600END"));

        break;
      case 6:
        temp = 0xff;
        myCAM.OV2640_set_JPEG_size(OV2640_1024x768); delay(1000);
        Serial.println(F("ACK CMD switch to OV2640_1024x768END"));
        HC06.println(F("ACK CMD switch to OV2640_1024x768END"));


        break;
      case 7:
        temp = 0xff;
        myCAM.OV2640_set_JPEG_size(OV2640_1280x1024); delay(1000);
        Serial.println(F("ACK CMD switch to OV2640_1280x1024END"));
        HC06.println(F("ACK CMD switch to OV2640_1280x1024END"));


        break;
      case 8:
        temp = 0xff;
        myCAM.OV2640_set_JPEG_size(OV2640_1600x1200); delay(1000);
        Serial.println(F("ACK CMD switch to OV2640_1600x1200END"));
        HC06.println(F("ACK CMD switch to OV2640_1600x1200END"));


        break;
      case 0x10:
        mode = 1;
        temp = 0xff;
        start_capture = 1;
        Serial.println(F("ACK CMD CAM start single shoot.END"));
        HC06.println(F("ACK CMD CAM start single shoot.END"));

        break;
      default:
        break;
    }
  }

  if (start_capture == 1)
  {
    myCAM.flush_fifo();
    myCAM.clear_fifo_flag();
    //Start capture
    myCAM.start_capture();
    start_capture = 0;
  }
  if (myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
  {
    Serial.println(F("ACK CMD CAM Capture Done.END"));
    HC06.println(F("ACK CMD CAM Capture Done.END"));

    delay(50);
    read_fifo_burst(myCAM);
    //Clear the capture done flag
    myCAM.clear_fifo_flag();
    Serial.println(F("ACK CMD CAM end single shoot.END"));
    HC06.println(F("ACK CMD CAM end single shoot.END"));

  }
}
uint8_t read_fifo_burst(ArduCAM myCAM)
{
  uint8_t temp = 0, temp_last = 0;
  uint32_t length = 0;
  length = myCAM.read_fifo_length();
  Serial.println(length, DEC);
  HC06.println(length, DEC);

  if (length >= MAX_FIFO_SIZE) //512 kb
  {
    Serial.println(F("ACK CMD Over size.END"));
    HC06.println(F("ACK CMD Over size.END"));

    return 0;
  }
  if (length == 0 ) //0 kb
  {
    Serial.println(F("ACK CMD Size is 0.END"));
    HC06.println(F("ACK CMD Size is 0.END"));

    return 0;
  }
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();//Set fifo burst mode
  temp =  SPI.transfer(0x00);
  length --;
  while ( length-- )
  {
    temp_last = temp;
    temp =  SPI.transfer(0x00);
    if (is_header == true)
    {
      // Serial.write(temp);
      HC06.write(temp);

    }
    else if ((temp == 0xD8) & (temp_last == 0xFF))
    {
      is_header = true;
      Serial.println(F("ACK CMD IMG END"));
      // Serial.write(temp_last);
      // Serial.write(temp);
      HC06.println(F("ACK CMD IMG END"));
      HC06.write(temp_last);
      HC06.write(temp);
    }
    if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
      break;
    delayMicroseconds(15);
  }
  Serial.println();
  HC06.println();

  myCAM.CS_HIGH();
  is_header = false;
  return 1;
}

void forward() {
 digitalWrite(IN1, HIGH);
digitalWrite(IN2, LOW);
digitalWrite(IN3, LOW);
digitalWrite(IN4, HIGH); 
}
