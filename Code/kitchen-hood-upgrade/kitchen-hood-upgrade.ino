#include <Arduino.h>
#include <WiFi.h>
#include <ModbusIP_ESP8266.h>

#include "CommStack.h"

const char *ssid = "ganewlan";
const char *password = "ganewlan1";

IPAddress local_IP(192, 168, 100, 90);
IPAddress gateway(192, 168, 100, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 100, 1);

ModbusIP mb;

extern int FanSpeed;
extern int FanLed;

hw_timer_t *FanCommTimer = NULL;

int lastSpeed = 0;
int lastLed = 0;

#define BUTTON_UP 6
#define BUTTON_DOWN 7

#define REG_LED 0
#define REG_SPEED 1
#define REG_STATUS 2

uint32_t lastButtonTime = 0;
const uint32_t debounceTime = 200;

void setFanSpeed(int speed) {
  if (speed < 0) speed = 0;
  if (speed > 6) speed = 6;

  if (speed == lastSpeed) return;

  lastSpeed = speed;
  FanSpeed = speed;

  FanCommFormPacket(FanSpeed, FanLed);

  mb.Hreg(REG_SPEED, speed);
  mb.Hreg(REG_STATUS, speed);
}

void setFanLed(int led) {
  led = led ? 1 : 0;

  if (led == lastLed) return;

  lastLed = led;
  FanLed = led;

  FanCommFormPacket(FanSpeed, FanLed);

  mb.Hreg(REG_LED, led);
}

void setup() {
  Serial.begin(115200);

  pinMode(FanCommPin, OUTPUT);
  digitalWrite(FanCommPin, HIGH);
  delay(1000);
  digitalWrite(FanCommPin, LOW);

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);

  FanSpeed = 0;
  FanLed = 0;

  FanCommFormPacket(FanSpeed, FanLed);

  if (!WiFi.config(local_IP, gateway, subnet, dns)) {
    Serial.println("Static IP configuration failed");
  }

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  mb.server();

  mb.addHreg(REG_LED, 0);
  mb.addHreg(REG_SPEED, 0);
  mb.addHreg(REG_STATUS, 0);

  FanCommTimer = timerBegin(1000000);
  timerAttachInterrupt(FanCommTimer, &FanCommCycle);
  timerAlarm(FanCommTimer, 100, true, 0);
}

void loop() {
  mb.task();

  int modbusSpeed = mb.Hreg(REG_SPEED);

  if (modbusSpeed > 6) modbusSpeed = 6;
  if (modbusSpeed < 0) modbusSpeed = 0;

  if (modbusSpeed != lastSpeed) {
    setFanSpeed(modbusSpeed);
  }

  int modbusLed = mb.Hreg(REG_LED);

  if (modbusLed > 1) modbusLed = 1;
  if (modbusLed < 0) modbusLed = 0;

  if (modbusLed != lastLed) {
    setFanLed(modbusLed);
  }

  if (millis() - lastButtonTime >= debounceTime) {
    if (digitalRead(BUTTON_UP) == LOW) {
      lastButtonTime = millis();
      setFanSpeed(lastSpeed + 1);
    }

    if (digitalRead(BUTTON_DOWN) == LOW) {
      lastButtonTime = millis();
      setFanSpeed(lastSpeed - 1);
    }
  }

  delay(1);
}