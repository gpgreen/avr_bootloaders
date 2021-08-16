/*
  */

// the sensor communicates using SPI, so include the library:
#include <SPI.h>

// pins used for the connection with the bootloader test board
// the other you need are controlled by the SPI library):
const int selectBootloaderPin = 2;
const int dataReadyPin = 7;
const int chipSelectPin = 10;
const int resetPin = 3;
const int ledPin = 4;

byte data[4];

void setup() {
  Serial.begin(115200);

  pinMode(chipSelectPin, OUTPUT);
  // start the SPI library:
  SPI.begin();
  
  // initalize the  data ready and chip select pins:
  pinMode(selectBootloaderPin, OUTPUT);
  pinMode(dataReadyPin, INPUT);
  pinMode(resetPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
      
  // select the bootloader
  digitalWrite(selectBootloaderPin, HIGH);
  digitalWrite(ledPin, HIGH);
  
  Serial.println("Resetting device");
  
  // take device into bootloader
  digitalWrite(resetPin, LOW);
  delay(100);
  digitalWrite(resetPin, HIGH);

  for (int i=0; i<10; i++) {
    // now see if anyone is home
    digitalWrite(ledPin, HIGH);
    while (digitalRead(dataReadyPin) == HIGH);
    digitalWrite(ledPin, LOW);
    // send hello
    data[0] = '0';
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    writeSPI(data);
    digitalWrite(ledPin, HIGH);
    while (digitalRead(dataReadyPin) == HIGH);
    digitalWrite(ledPin, LOW);
    readSPI(data);
    if (data[0] == 0x14 && data[1] == '0' && data[2] == 0x10 && data[3] == 0) {
      Serial.println("Got hello from device");
      break;
    } else {
      Serial.print("Try:"); Serial.print(i); Serial.println();
    }
    delay(100);
  }
}

void loop() {
  digitalWrite(selectBootloaderPin, LOW);
}

void readSPI(byte* data) {
  // take the chip select low to select the device:
  digitalWrite(chipSelectPin, LOW);
  SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
  for (int i=0; i<4; i++)
    data[i] = SPI.transfer(0x00);
  SPI.endTransaction();
  // take the chip select high to de-select:
  digitalWrite(chipSelectPin, HIGH);
}


void writeSPI(byte* data) {
  // take the chip select low to select the device:
  digitalWrite(chipSelectPin, LOW);
  SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
  for (int i=0; i<4; i++)
    SPI.transfer(data[i]);
  SPI.endTransaction();
  // take the chip select high to de-select:
  digitalWrite(chipSelectPin, HIGH);
}
