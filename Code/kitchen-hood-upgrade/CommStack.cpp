#include <Arduino.h>
#include "CommStack.h"

int FanSpeed = 0;
int FanLed = 0;

// POPRAVLJENO: Vraćene uglaste zagrade [3] za niz elemenata
uint32_t FanCommTelegram[3] = { 0, 0, 0 };

uint32_t FirstPacketLedOff = 0b0000101000000000;
uint32_t SecondPacketLedOff = 0b0000111000000000;
uint32_t ThirdPacketLedOff = 0b0001010000000000;

uint32_t FirstPacketLedOn = 0;
uint32_t SecondPacketLedOn = 0;
uint32_t ThirdPacketLedOn = 0;

const uint32_t speedMasks[] = {
  0,                                         // Brzina 0
  (1 << 0) | (1 << 8),                       // Brzina 1
  (0 << 0) | (1 << 1) | (1 << 8),            // Brzina 2
  (1 << 0) | (1 << 1) | (1 << 8),            // Brzina 3
  (1 << 0) | (0 << 1) | (1 << 2) | (1 << 8), // Brzina 4
  (0 << 0) | (1 << 1) | (1 << 2) | (1 << 8), // Brzina 5
  (1 << 0) | (1 << 1) | (1 << 2) | (1 << 8)  // Brzina 6
};

void FanCommFormPacket(int FanSpeed, int FanLed) {
  // POPRAVLJENO: Vraćeni indeksi niza, [1] i [0]
  FanCommTelegram[2] = FirstPacketLedOff | speedMasks[FanSpeed];
  FanCommTelegram[1] = SecondPacketLedOff;
  FanCommTelegram[0] = ThirdPacketLedOff;
}

void FanCommSendBit(uint32_t FanCommPacket, int BitPosition) {
  if (((FanCommPacket >> BitPosition) & 1) == 1) {
    digitalWrite(FanCommPin, HIGH);
    delayMicroseconds(1380);
    digitalWrite(FanCommPin, LOW);
    delayMicroseconds(640);
  }
  if (((FanCommPacket >> BitPosition) & 1) == 0) {
    digitalWrite(FanCommPin, HIGH);
    delayMicroseconds(520);
    digitalWrite(FanCommPin, LOW);
    delayMicroseconds(1530);
  }
}

void FanCommSendTelegram(int FanSpeed, int FanLed) {
  for (int i = 2; i >= 0; i--) {
    delayMicroseconds(5100);
    digitalWrite(FanCommPin, HIGH);
    delayMicroseconds(1920);
    digitalWrite(FanCommPin, LOW);
    delayMicroseconds(1154);
    for (int j = 15; j >= 0; j--) {
      FanCommSendBit(FanCommTelegram[i], j);
    }
  }
}
