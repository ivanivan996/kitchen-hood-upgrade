#include <Arduino.h>
#include "CommStack.h"

int FanSpeed = 0;
int FanLed = 0;

enum FanCommState {
  DECIDE = 0,
  SEND_PREAMBLE_HIGH = 1,
  SEND_PREAMBLE_LOW = 2,
  SEND_HIGH_HIGH = 3,
  SEND_HIGH_LOW = 4,
  SEND_LOW_HIGH = 5,
  SEND_LOW_LOW = 6,
  SEND_PAUSE = 7
};

FanCommState FanCommCurrentState = DECIDE;
int32_t FanCommTicks = 0;
int32_t FanCommTargetTicks = 0;

int32_t FanCommTelegram[3] = { 0, 0, 0 };
int32_t FanCommPacketCount = 2;
int32_t FanCommBitCount = 16;

void FanCommFormPacket(int FanSpeed, int FanLed) {
  FanCommTelegram[2] = FirstPacketLedOff | speedMasks[FanSpeed];
  FanCommTelegram[1] = SecondPacketLedOff;
  FanCommTelegram[0] = ThirdPacketLedOff;
}

void IRAM_ATTR FanCommCycle() {
  FanCommTicks++;

  switch (FanCommCurrentState) {
    case DECIDE:
      if (FanCommBitCount == 16) {
        FanCommCurrentState = SEND_PREAMBLE_HIGH;
        FanCommTicks = 0;
        FanCommTargetTicks = FirstPulseHighTime / 100;
      } else if (FanCommBitCount == -1) {
        FanCommPacketCount--;
        if (FanCommPacketCount < 0) {
          FanCommPacketCount = 2;
        }
        FanCommBitCount = 16;
        FanCommTicks = 0;
        FanCommTargetTicks = PauseTime / 100;
        FanCommCurrentState = SEND_PAUSE;
      } else {
        if (((FanCommTelegram[FanCommPacketCount] >> FanCommBitCount) & 1) == 1) {
          FanCommTicks = 0;
          FanCommTargetTicks = HighPulseHighTime / 100;
          FanCommCurrentState = SEND_HIGH_HIGH;
        } else if (((FanCommTelegram[FanCommPacketCount] >> FanCommBitCount) & 1) == 0) {
          FanCommTicks = 0;
          FanCommTargetTicks = LowPulseHighTime / 100;
          FanCommCurrentState = SEND_LOW_HIGH;
        }
      }
      break;

    case SEND_PAUSE:
      digitalWrite(FanCommPin, LOW);
      if (FanCommTicks >= FanCommTargetTicks) {
        FanCommCurrentState = DECIDE;
      }
      break;

    case SEND_PREAMBLE_HIGH:
      digitalWrite(FanCommPin, HIGH);
      if (FanCommTicks >= FanCommTargetTicks) {
        FanCommTicks = 0;
        FanCommTargetTicks = FirstPulseLowTime / 100;
        FanCommCurrentState = SEND_PREAMBLE_LOW;
      }
      break;

    case SEND_PREAMBLE_LOW:
      digitalWrite(FanCommPin, LOW);
      if (FanCommTicks >= FanCommTargetTicks) {
        FanCommBitCount--;
        FanCommCurrentState = DECIDE;
      }
      break;

    case SEND_HIGH_HIGH:
      digitalWrite(FanCommPin, HIGH);
      if (FanCommTicks >= FanCommTargetTicks) {
        FanCommTicks = 0;
        FanCommTargetTicks = HighPulseLowTime / 100;
        FanCommCurrentState = SEND_HIGH_LOW;
      }
      break;

    case SEND_HIGH_LOW:
      digitalWrite(FanCommPin, LOW);
      if (FanCommTicks >= FanCommTargetTicks) {
        FanCommBitCount--;
        FanCommCurrentState = DECIDE;
      }
      break;

    case SEND_LOW_HIGH:
      digitalWrite(FanCommPin, HIGH);
      if (FanCommTicks >= FanCommTargetTicks) {
        FanCommTicks = 0;
        FanCommTargetTicks = LowPulseLowTime / 100;
        FanCommCurrentState = SEND_LOW_LOW;
      }
      break;

    case SEND_LOW_LOW:
      digitalWrite(FanCommPin, LOW);
      if (FanCommTicks >= FanCommTargetTicks) {
        FanCommBitCount--;
        FanCommCurrentState = DECIDE;
      }
      break;
  }
}