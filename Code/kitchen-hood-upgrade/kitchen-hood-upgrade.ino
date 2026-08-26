#include <Arduino.h>
#include <Matter.h>
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>

#include "CommStack.h"

// List of Matter Endpoints for this Node
// Fan Endpoint - On/Off control + Speed Percent Control + Fan Modes
MatterFan Fan;

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
// WiFi is manually set and started
const char *ssid = "ganewlan";               // Change this to your WiFi SSID
const char *password = "ganewlan1";          // Change this to your WiFi password

// set your board USER BUTTON pin here - used for toggling On/Off and decommission the Matter Node
const int buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t debouceTime = 250;              // button debouncing time (ms)
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

int FanSpeedStatus = 0;

// definisano u CommStack.cpp
extern int FanSpeed;
extern int FanLed;

// set your board Analog Pin here - used for changing the Fan speed
const uint8_t analogPin = A0;  // Analog Pin depends on each board

// hardverski tajmer koji poziva FanCommCycle() svakih 100us
hw_timer_t *FanCommTimer = NULL;

// sprečava kružno okidanje onChangeSpeedPercent <-> onChangeMode preko ATTR_SET
volatile bool fanCallbackGuard = false;

// signal iz Matter callback-a ka loop() da treba formirati novi telegram
// (posao se NE radi direktno u Matter/CHIP task kontekstu, samo se postavi flag)
volatile bool fanNeedsUpdate = false;
volatile int lastFanSpeed = -1;  // -1 = forsira prvo formiranje paketa

void setup() {
  // Inicijalizacija komunikacionog pina prema fan kontroleru (reset puls)
  pinMode(FanCommPin, OUTPUT);
  digitalWrite(FanCommPin, HIGH);
  delay(1000);
  digitalWrite(FanCommPin, LOW);

  // Initialize the USER BUTTON (Boot button) GPIO that will toggle the Fan (On/Off) and decommission the Matter Node
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the Analog Pin A0 used to read input voltage and to set the Fan speed accordingly

  Serial.begin(115200);

  // CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  // Manually connect to WiFi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\r\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);

  // On Boot or Reset, Fan is set at 0% speed, OFF, changing between OFF, ON, SMART and HIGH
  Fan.begin(0, MatterFan::FAN_MODE_OFF, MatterFan::FAN_MODE_SEQ_OFF_HIGH);

  // callback functions would control Fan motor
  // the Matter Controller will send new data whenever the User APP or Automation request

  // single feature callbacks take place before the generic (all features) callback
  // This callback will be executed whenever the speed percent matter attribute is updated
  Fan.onChangeSpeedPercent([](uint8_t speedPercent) {
    if (fanCallbackGuard) {
      // već smo unutar lanca callback-ova (pozvano iz onChangeMode preko ATTR_SET) - izađi bez daljeg okidanja
      return true;
    }
    fanCallbackGuard = true;

    bool result = true;
    // setting speed to Zero, while the Fan is ON, shall turn the Fan OFF
    if (speedPercent == MatterFan::OFF_SPEED && Fan.getMode() != MatterFan::FAN_MODE_OFF) {
      // ATTR_SET do not update the attribute, just SET it to avoid infinite loop
      result = Fan.setOnOff(false, Fan.ATTR_SET);
    }
    // changing the speed to higher than Zero, while the Fan is OFF, shall turn the Fan ON
    else if (speedPercent > MatterFan::OFF_SPEED && Fan.getMode() == MatterFan::FAN_MODE_OFF) {
      // ATTR_SET do not update the attribute, just SET it to avoid infinite loop
      result = Fan.setOnOff(true, Fan.ATTR_SET);
    }

    fanCallbackGuard = false;
    return result;
  });

  // This callback will be executed whenever the fan mode matter attribute is updated
  // This will take action when user APP starts the Fan by changing the mode
  Fan.onChangeMode([](MatterFan::FanMode_t fanMode) {
    if (fanCallbackGuard) {
      // već smo unutar lanca callback-ova (pozvano iz onChangeSpeedPercent preko ATTR_SET) - izađi bez daljeg okidanja
      return true;
    }
    fanCallbackGuard = true;

    bool result = true;
    // when the Fan is turned ON using Mode Selection, while it is OFF, shall start it by setting the speed to 50%
    if (Fan.getSpeedPercent() == MatterFan::OFF_SPEED && fanMode != MatterFan::FAN_MODE_OFF) {
      Serial.printf("Fan set to %s mode -- speed percentage will go to 50%%\r\n", Fan.getFanModeString(fanMode));
      // ATTR_SET do not update the attribute, just SET it to avoid infinite loop
      result = Fan.setSpeedPercent(50, Fan.ATTR_SET);
    }

    fanCallbackGuard = false;
    return result;
  });

  // Generic callback will be executed as soon as a single feature callback is done
  // Namerno je MINIMALAN - samo upiše novu brzinu i postavi flag.
  // Stvarni posao (FanCommFormPacket, Serial.printf) se radi u loop(), van CHIP task konteksta,
  // da bi se izbeglo trošenje/akumuliranje steka CHIP task-a kod brzih uzastopnih Matter komandi.
  Fan.onChange([](MatterFan::FanMode_t fanMode, uint8_t speedPercent) {
    int newSpeed;
    if (fanMode == MatterFan::FAN_MODE_OFF || speedPercent == MatterFan::OFF_SPEED) {
      newSpeed = 0;
    } else {
      newSpeed = map(speedPercent, 1, 100, 1, 6);
      if (newSpeed < 1) newSpeed = 1;
      if (newSpeed > 6) newSpeed = 6;
    }

    FanSpeed = newSpeed;
    fanNeedsUpdate = true;  // loop() će formirati paket kad stigne red

    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }

  // Formiraj početni telegram pre nego što tajmer krene da ga šalje
  FanCommFormPacket(FanSpeed, 0);
  lastFanSpeed = FanSpeed;

  // Pokreni tajmer koji poziva FanCommCycle() svakih 100us
  FanCommTimer = timerBegin(1000000);  // 1 MHz -> 1 tick = 1us
  timerAttachInterrupt(FanCommTimer, &FanCommCycle);
  timerAlarm(FanCommTimer, 100, true, 0);  // 100us, autoreload, beskonačno
}

void loop() {
  // NEBLOKIRAJUĆA PROVERA ZA MATTER UPARIVANJE
  if (!Matter.isDeviceCommissioned()) {
    // Koristimo static tajmer umesto while petlje da ne bismo blokirali pinove
    static uint32_t vremeIspisa = 0;
    if (millis() - vremeIspisa > 5000) {  // Ispisuje stanje svakih 5 sekundi
      vremeIspisa = millis();
      Serial.println("\r\nMatter Node nije uparen! Cekam povezivanje na aplikaciju...");
      Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
      Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    }
  }

  // Obradi update brzine van CHIP task konteksta, i samo ako se vrednost stvarno promenila
  if (fanNeedsUpdate) {
    fanNeedsUpdate = false;
    int speedNow = FanSpeed;
    if (speedNow != lastFanSpeed) {
      lastFanSpeed = speedNow;
      FanCommFormPacket(speedNow, 0);
      Serial.printf("Fan speed updated to %d\r\n", speedNow);
    }
  }

  // Slanje protokola prema fan kontroleru se odvija automatski u pozadini
  // preko hardverskog tajmera (FanCommCycle, pozivan svakih 100us).
}