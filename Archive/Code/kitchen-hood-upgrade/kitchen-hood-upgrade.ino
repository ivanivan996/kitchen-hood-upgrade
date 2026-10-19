#include <Arduino.h>
#include <WiFi.h>
#include <ModbusIP_ESP8266.h>
#include "CommStack.h"

const char *ssid = "HUAWEI-2.4G-wXp9";
const char *password = "W7j2T58C";

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

#define BUTTON_LED 1
#define BUTTON_UP 2
#define BUTTON_DOWN 3

#define REG_LED_COMMAND 0
#define REG_LED_STATUS 1
#define REG_SPEED_COMMAND 2
#define REG_SPEED_STATUS 3

uint32_t lastButtonTime = 0;
const uint32_t debounceTime = 800;

void setFanSpeed(int speed) {
  if (speed < 0) speed = 0;
  if (speed > 6) speed = 6;

  lastSpeed = speed;
  FanSpeed = speed;

  FanCommFormPacket(FanSpeed, FanLed);

  mb.Hreg(REG_SPEED_COMMAND, speed);
  mb.Hreg(REG_SPEED_STATUS, speed);
}

void setFanLed(int led) {
  led = led ? 1 : 0;

  lastLed = led;
  FanLed = led;

  FanCommFormPacket(FanSpeed, FanLed);

  mb.Hreg(REG_LED_COMMAND, led);
  mb.Hreg(REG_LED_STATUS, led);
}

void setup() {
  Serial.begin(115200);

  pinMode(FanCommPin, OUTPUT);
  digitalWrite(FanCommPin, HIGH);
  delay(1000);
  digitalWrite(FanCommPin, LOW);

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_LED, INPUT_PULLUP);

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

  mb.addHreg(REG_LED_COMMAND, 0);
  mb.addHreg(REG_LED_STATUS, 0);
  mb.addHreg(REG_SPEED_COMMAND, 0);
  mb.addHreg(REG_SPEED_STATUS, 0);

  FanCommTimer = timerBegin(1000000);
  timerAttachInterrupt(FanCommTimer, &FanCommCycle);
  timerAlarm(FanCommTimer, 100, true, 0);
}

void loop() {
  mb.task();

  int modbusSpeed = mb.Hreg(REG_SPEED_COMMAND);

  if (modbusSpeed > 6) modbusSpeed = 6;
  if (modbusSpeed < 0) modbusSpeed = 0;

  if (modbusSpeed != lastSpeed) {
    setFanSpeed(modbusSpeed);
  }

  int modbusLed = mb.Hreg(REG_LED_COMMAND);

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
    else if (digitalRead(BUTTON_DOWN) == LOW) {
      lastButtonTime = millis();
      setFanSpeed(lastSpeed - 1);
    }
    else if (digitalRead(BUTTON_LED) == LOW) {
      lastButtonTime = millis();
      setFanLed(!lastLed);
    }
  }

  delay(1);
}