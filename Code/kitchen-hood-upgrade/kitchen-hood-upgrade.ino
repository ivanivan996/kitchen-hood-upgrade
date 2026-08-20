
// Matter Managerc:\Users\Ivan\git\kitchen-hood-upgrade\Code\kitchen-hood-upgrade\Inc\CommStack.h
#include <Arduino.h>
#include <Matter.h>
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>

#include "CommStack.h"
// List of Matter Endpoints for this Nodec:\Users\Ivan\git\kitchen-hood-upgrade\Code\kitchen-hood-upgrade\Src\CommStack.c
// Fan Endpoint - On/Off control + Speed Percent Control + Fan Modes
MatterFan Fan;

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to comc:\Users\Ivan\git\kitchen-hood-upgrade\Code\kitchen-hood-upgrade\src\Src\CommStack.cppmission the Matter Network
// WiFi is manually set and started
const char *ssid = "ganewlan_Plus";          // Change this to your WiFi SSID
const char *password = "ganewlan1";  // Change this to c:\Users\Ivan\git\kitchen-hood-upgrade\Code\kitchen-hood-upgrade\src\src\CommStack.cppyour WiFi password

// set your board USER BUTTON pin here - used for toggling On/Off and decommission the Matter Node
const int buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t debouceTime = 250;              // button debouncing time (ms)
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

int FanSpeedStatus = 0;

extern int FanSpeed;
extern int FanLed;

// set your board Analog Pin here - used for changing the Fan speed
const uint8_t analogPin = A0;  // Analog Pin depends on each board

void setup() {
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
    // setting speed to Zero, while the Fan is ON, shall turn the Fan OFF
    if (speedPercent == MatterFan::OFF_SPEED && Fan.getMode() != MatterFan::FAN_MODE_OFF) {
      // ATTR_SET do not update the attribute, just SET it to avoid infinite loop
      return Fan.setOnOff(false, Fan.ATTR_SET);
    }
    // changing the speed to higher than Zero, while the Fan is OFF, shall turn the Fan ON
    if (speedPercent > MatterFan::OFF_SPEED && Fan.getMode() == MatterFan::FAN_MODE_OFF) {
      // ATTR_SET do not update the attribute, just SET it to avoid infinite loop
      return Fan.setOnOff(true, Fan.ATTR_SET);
    }
    // for other case, just return true
    return true;
  });

  // This callback will be executed whenever the fan mode matter attribute is updated
  // This will take action when user APP starts the Fan by changing the mode
  Fan.onChangeMode([](MatterFan::FanMode_t fanMode) {
    // when the Fan is turned ON using Mode Selection, while it is OFF, shall start it by setting the speed to 50%
    if (Fan.getSpeedPercent() == MatterFan::OFF_SPEED && fanMode != MatterFan::FAN_MODE_OFF) {
      Serial.printf("Fan set to %s mode -- speed percentage will go to 50%%\r\n", Fan.getFanModeString(fanMode));
      // ATTR_SET do not update the attribute, just SET it to avoid infinite loop
      return Fan.setSpeedPercent(50, Fan.ATTR_SET);
    }
    return true;
  });

  // Generic callback will be executed as soon as a single feature callback is done
  // In this example, it will just print status messages
  Fan.onChange([](MatterFan::FanMode_t fanMode, uint8_t speedPercent) {
    // just report state
    Serial.printf("Fan State: Mode %s | %u%% speed.\r\n", Fan.getFanModeString(fanMode), speedPercent);
    // drive the Fan DC motor
    //fanDCMotorDrive(fanMode != MatterFan::FAN_MODE_OFF, speedPercent);
    // returns success
    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }
}

void loop() {
  // 1. NEBLOKIRAJUĆA PROVERA ZA MATTER UPARIVANJE
  if (!Matter.isDeviceCommissioned()) {
    // Koristimo static tajmer umesto while petlje da ne bismo blokirali pinove
    static uint32_t vremeIspisa = 0;
    if (millis() - vremeIspisa > 5000) { // Ispisuje stanje svakih 5 sekundi
      vremeIspisa = millis();
      Serial.println("\r\nMatter Node nije uparen! Cekam povezivanje na aplikaciju...");
      Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
      Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    }
  }

  // 2. VAŠ TEST TOGGLE - Sada se izvršava bez obzira na stanje uparivanja!
  // Smanjili smo učestalost slanja na svake 2 sekunde da ne zagušimo ESP32-C6 i Matter pozadinu
  //static uint32_t vremeSlanja = 0;
  //if (millis() - vremeSlanja > 5) { 
  //  vremeSlanja = millis();
    
  //  Serial.println("-> Izvrsavam test toggle 100ms i saljem protokol...");
    
    // Punjenje niza novim bitovima i slanje kroz CommStack
    FanCommFormPacket(0, FanLed);
    FanCommCycle();
  //}
}
