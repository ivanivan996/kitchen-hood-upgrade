#ifndef COMMSTACK_H
#define COMMSTACK_H

#include <Arduino.h>

// Govori i glavnom fajlu i .cpp fajlu da postoji deljeni int za pin
#define FanCommPin 8

// Prototipovi funkcija
void FanCommSendTelegram(int FanSpeed, int FanLed);
void FanCommFormPacket(int FanSpeed, int FanLed); // Promenjeno u void jer ne vraća vrednost
void FanCommSendBit(uint32_t FanCommPacket, int BitPosition);

#endif
