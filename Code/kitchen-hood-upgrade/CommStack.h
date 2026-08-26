#ifndef COMMSTACK_H
#define COMMSTACK_H

#include <Arduino.h>

// Govori i glavnom fajlu i .cpp fajlu da postoji deljeni int za pin
#define FanCommPin 8

#define FirstPulseHighTime 1920
#define FirstPulseLowTime 1154
#define HighPulseHighTime 1380
#define HighPulseLowTime 640
#define LowPulseHighTime 520
#define LowPulseLowTime 1530
#define PauseTime 5100

const uint32_t FirstPacketLedOff = 0b0000101000000000;
const uint32_t SecondPacketLedOff = 0b0000111000000000;
const uint32_t ThirdPacketLedOff = 0b0001010000000000;

const uint32_t FirstPacketLedOn = 0;
const uint32_t SecondPacketLedOn = 0;
const uint32_t ThirdPacketLedOn = 0;

const uint32_t speedMasks[] = {
  0,                                          // Brzina 0
  (1 << 0) | (1 << 8),                        // Brzina 1
  (0 << 0) | (1 << 1) | (1 << 8),             // Brzina 2
  (1 << 0) | (1 << 1) | (1 << 8),             // Brzina 3
  (1 << 0) | (0 << 1) | (1 << 2) | (1 << 8),  // Brzina 4
  (0 << 0) | (1 << 1) | (1 << 2) | (1 << 8),  // Brzina 5
  (1 << 0) | (1 << 1) | (1 << 2) | (1 << 8)   // Brzina 6
};

// Prototipovi funkcija
void FanCommFormPacket(int FanSpeed, int FanLed);
void IRAM_ATTR FanCommCycle();

#endif