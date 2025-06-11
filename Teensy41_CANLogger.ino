#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

#define CAN_BAUD     500000
#define SD_CS        10
#define STATUS_LED   13
#define UART_BAUD    115200
#define LOG_ROTATE_MS 60000
#define LOG_MAX_SIZE (1024UL*1024UL*5)

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;
File logFile;
uint32_t logIndex = 0;
unsigned long lastRotate = 0;

/* Simple anomaly detection using running average */
uint32_t idSum = 0; uint32_t idCount = 0;
bool checkAnomaly(const CAN_message_t &msg){
  idSum += msg.id; idCount++;
  uint32_t avg = idSum / idCount;
  return (msg.id > avg * 2); // trivial check
}

void rotateLog(){
  if(logFile) logFile.close();
  char name[20];
  sprintf(name, "log_%lu.csv", logIndex++);
  logFile = SD.open(name, FILE_WRITE);
  if(logFile){
    logFile.println("time,id,len,data");
  }
}

void setup(){
  pinMode(STATUS_LED, OUTPUT);
  Serial.begin(UART_BAUD);
  Serial1.begin(UART_BAUD);
  while(!Serial && millis() < 3000);

  Can0.begin();
  Can0.setBaudRate(CAN_BAUD);

  if(!SD.begin(SD_CS)){
    Serial.println("SD init failed");
  }else{
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
      if(logFile.size() > LOG_MAX_SIZE) rotateLog();
    }
    // forward important messages
    if(checkAnomaly(msg)){
      Serial1.print("ID:");
      Serial1.print(msg.id, HEX);
      Serial1.print(" ");
      for(int i=0;i<msg.len;i++){
        Serial1.print(msg.buf[i], HEX); Serial1.print(' ');
      }
      Serial1.println();
    }
    digitalWrite(STATUS_LED, LOW);
  }

  if(millis() - lastRotate > LOG_ROTATE_MS){
    lastRotate = millis();
    rotateLog();
  }
}
