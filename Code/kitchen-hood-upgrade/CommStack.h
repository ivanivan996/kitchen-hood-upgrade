#ifndef COMMSTACK_H
#define COMMSTACK_H

#include <Arduino.h>

#define FanCommPin 8

#define FirstPulseHighTime 1920
#define FirstPulseLowTime 1152
#define HighPulseHighTime 1408
#define HighPulseLowTime 640
#define LowPulseHighTime 512
#define LowPulseLowTime 1536
#define PauseTime 5640

const uint32_t FirstPacketLedOff = 0b0000101000000000;
const uint32_t SecondPacketLedOff = 0b0000111000000000;
const uint32_t ThirdPacketLedOff = 0b0001010000000000;

const uint32_t FirstPacketLedOn = 0b0000101100000000;
const uint32_t SecondPacketLedOn = 0b0001010100000101;
const uint32_t ThirdPacketLedOn = 0b0000110000000101;

const uint32_t speedMasksLedOff[] = {
  0,
  (1 << 0) | (1 << 8),
  (1 << 1) | (1 << 8),
  (1 << 0) | (1 << 1) | (1 << 8),
  (1 << 0) | (1 << 2) | (1 << 8),
  (1 << 1) | (1 << 2) | (1 << 8),
  (1 << 0) | (1 << 1) | (1 << 2) | (1 << 8)
};

const uint32_t speedMasksLedOn[] = {
  0,
  (1 << 0),
  (1 << 1),
  (1 << 0) | (1 << 1),
  (1 << 0) | (1 << 2),
  (1 << 1) | (1 << 2),
  (1 << 0) | (1 << 1) | (1 << 2)
};

void FanCommFormPacket(int FanSpeed, int FanLed);
void IRAM_ATTR FanCommCycle();

#endif