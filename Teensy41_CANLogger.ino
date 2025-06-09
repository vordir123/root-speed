#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <SD.h>
#include <SPI.h>

#define CAN_BAUD 500000
#define SD_CS 10
#define STATUS_LED 13
#define UART_BAUD 115200

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;
File logFile;
uint32_t logIndex = 0;
unsigned long lastRotate = 0;
const unsigned long rotateInterval = 60000; // 1 min

void rotateLog(){
  if(logFile){
    logFile.close();
  }
  char name[20];
  sprintf(name, "log_%lu.csv", logIndex++);
  logFile = SD.open(name, FILE_WRITE);
}

void setup(){
  pinMode(STATUS_LED, OUTPUT);
  Serial.begin(UART_BAUD);
  while(!Serial && millis() < 3000); // Wait for Serial

  Can0.begin();
  Can0.setBaudRate(CAN_BAUD);

  if(!SD.begin(SD_CS)){
    Serial.println("SD init failed");
  } else {
    rotateLog();
  }
}

void loop(){
  CAN_message_t msg;
  if(Can0.read(msg)){
    digitalWrite(STATUS_LED, HIGH);
    if(logFile){
      logFile.printf("%lu,%X,%d,", millis(), msg.id, msg.len);
      for(int i=0;i<msg.len;i++) logFile.printf("%02X", msg.buf[i]);
      logFile.println();
    }
    // forward to ESP32 via Serial1
    Serial1.print("ID:");
    Serial1.print(msg.id, HEX);
    Serial1.print(" ");
    for(int i=0;i<msg.len;i++){
      Serial1.print(msg.buf[i], HEX); Serial1.print(' ');
    }
    Serial1.println();
    digitalWrite(STATUS_LED, LOW);
  }

  if(millis() - lastRotate > rotateInterval){
    lastRotate = millis();
    rotateLog();
  }
}


