#include <LedControl.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLERemoteCharacteristic.h>

// Pins für MAX7219
#define DIN 3
#define CLK 1
#define CS  2
LedControl lc = LedControl(DIN, CLK, CS, 2); // 2 Displays in Reihe

// Relay
#define RELAY_PIN 10

// ===================== BLE =====================
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

static BLEAdvertisedDevice* myDevice;
static bool doConnect = false;
static bool connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;

// Daten
uint8_t speed = 0;
uint8_t battery = 0;
bool lightOn = false;
uint8_t modeState = 0; // 0: Eco, 1: Drive, 2: Sport
unsigned long lastPacket = 0;

// Display States
bool displayOverride = false;
unsigned long displayOverrideStart = 0;
uint8_t displayOverrideType = 0; // 0: Mode, 1: Light Off
uint8_t lastModeState = 255;
bool lastLightState = false;

// ===================== 3x5 Font =====================
const byte font3x5[13][5] = {
  {B111,B101,B101,B101,B111}, //0
  {B010,B110,B010,B010,B111}, //1
  {B111,B001,B111,B100,B111}, //2
  {B111,B001,B111,B001,B111}, //3
  {B101,B101,B111,B001,B001}, //4
  {B111,B100,B111,B001,B111}, //5
  {B111,B100,B111,B101,B111}, //6
  {B111,B001,B001,B001,B001}, //7
  {B111,B101,B111,B101,B111}, //8
  {B111,B101,B111,B001,B111}, //9
  {B111,B100,B111,B100,B111}, //E (10) - Eco
  {B110,B101,B101,B101,B110}, //D (11) - Drive
  {B111,B100,B111,B001,B111}  //S (12) - Sport
};

// 8x8 Licht Icon
const byte lightIcon[8] = {
  B00011000,
  B00111100,
  B01111110,
  B11111111,
  B11111111,
  B01111110,
  B00111100,
  B00011000
};

// 3x5 "OFF" Text
const byte offText[3][5] = {
  {B111,B101,B101,B101,B111}, // O
  {B111,B100,B111,B100,B100}, // F
  {B111,B100,B111,B100,B100}  // F
};

void drawDigit(int device, int digit, int x, int y) {
  for (int row=0; row<5; row++) {
    byte line = font3x5[digit][row];
    for (int col=0; col<3; col++) {
      lc.setLed(device, y+row, x+col, line & (1<<(2-col)));
    }
  }
}

void drawChar(int device, int character, int x, int y) {
  for (int row=0; row<5; row++) {
    byte line = font3x5[character][row];
    for (int col=0; col<3; col++) {
      lc.setLed(device, y+row, x+col, line & (1<<(2-col)));
    }
  }
}

void showSpeed(uint8_t spd) {
  lc.clearDisplay(0);
  int tens = spd/10;
  int ones = spd%10;
  drawDigit(0, tens, 0, 0);  // links
  drawDigit(0, ones, 4, 0);  // rechts
}

void showStatus(uint8_t bat, bool light) {
  lc.clearDisplay(1);

  // Batterie-Balken unten
  int bars = map(bat, 0, 100, 0, 8);
  bool blink = (bat < 20) && ((millis()/300) % 2 == 0);
  for (int c=0; c<bars; c++) {
    lc.setLed(0, 6, c, !blink);
    lc.setLed(0, 7, c, !blink);
  }
  
  // Licht-Symbol 3x3 oben links
  if (light) {
    int pattern[3][3] = {
      {0,1,0},
      {1,1,1},
      {0,1,0}
    };
    for (int r=0; r<3; r++)
      for (int c=0; c<3; c++)
        if (pattern[r][c]) lc.setLed(1,r,c,true);
  }
}

void showModeIndicator(uint8_t mode) {
  lc.clearDisplay(0);
  uint8_t character;
  
  switch(mode) {
//    case 0: character = 10; break; // E für Eco
    case 1: character = 10; break; // E für Eco
    case 2: character = 11; break; // D für Drive  
//    case 3: character = 11; break; // D für Drive  
    case 3: character = 12; break; // S für Sport
  //  case 5: character = 12; break; // S für Sport
    default: character = 10;
  }
  
  // Zeichen in der Mitte anzeigen (Position 2,1)
  drawChar(0, character, 2, 1);
}

void showLightOffIndicator() {
  lc.clearDisplay(0);
 
  
  // "OFF" Text in der unteren Hälfte
  for (int i=0; i<3; i++) {
    for (int row=0; row<5; row++) {
      byte line = offText[i][row];
      for (int col=0; col<3; col++) {
        lc.setLed(0, row+3, col+1+(i*3), line & (1<<(2-col)));
      }
    }
  }
}

void showError() {
  for(int d=0; d<2; d++) {
    lc.clearDisplay(d);
    for (int i=0;i<8;i++) {
      lc.setLed(d,i,i,true);
      lc.setLed(d,i,7-i,true);
    }
  }
delay(2000);
 ESP.restart(); // Führt einen Software-Reset aus
}

// ===================== BLE Callbacks =====================
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) { 
    connected = true; 
  }
  void onDisconnect(BLEClient* pclient) { 
    connected = false; 
    Serial.println("Verbindung getrennt"); 
  }
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveName() && advertisedDevice.getName() == "ESP32-UART-Dashboard") {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
    }
  }
};

// ------------------- BLE Notify -------------------
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify) {
  if(length < 7) return; // Jetzt 7 Bytes erwartet
  if(pData[0] != 0x5A || pData[1] != 0xA5) return;

  speed = pData[2];
  battery = pData[3];
  lightOn = pData[4];
  modeState = pData[5]; // Neues Byte für Mode State
  lastPacket = millis();

  // Mode-Wechsel erkannt
  if (modeState != lastModeState) {
    displayOverride = true;
    displayOverrideType = 1; // Mode Anzeige
    displayOverrideStart = millis();
      showStatus(battery, lightOn);
    showModeIndicator(modeState);
    lastModeState = modeState;
  }
 /*  // Licht aus erkannt
  if ( lightOn != lastLightState) {
    displayOverride = true;
    displayOverrideType = 1; // Light Off Anzeige
    displayOverrideStart = millis();
  
  // Licht Icon in der oberen Hälfte
  for (int row=0; row<8; row++) {
    for (int col=0; col<8; col++) {
      lc.setLed(0, row, col, lightIcon[row] & (1<<(7-col)));
    }
  }
  }
  // Licht aus erkannt
  if (lastLightState && !lightOn) {
    displayOverride = true;
    displayOverrideType = 1; // Light Off Anzeige
    displayOverrideStart = millis();
    showLightOffIndicator();
  }
  lastLightState = lightOn;

*/
  // Normale Anzeige nur wenn kein Override aktiv
  if (!displayOverride) {
    showSpeed(speed);
    showStatus(battery, lightOn);
  }

  // Relay bei 30 km/h steuern
  static bool lastRelayState = false;
  bool relayState = speed > 30;
  if (relayState != lastRelayState) {
    digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
    Serial.printf("RELAY:%d\n", relayState ? 1 : 0);
    lastRelayState = relayState;
  }
}

// ===================== BLE Verbindung =====================
bool connectToServer() {
  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  if (!pClient->connect(myDevice)) { 
    Serial.println("Verbindung fehlgeschlagen"); 
    return false; 
  }

  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) { 
    Serial.println("Service nicht gefunden"); 
    pClient->disconnect(); 
    return false; 
  }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  if (pRemoteCharacteristic == nullptr) { 
    Serial.println("Characteristic nicht gefunden"); 
    pClient->disconnect(); 
    return false; 
  }

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  }

  return true;
}

// ===================== Setup / Loop =====================
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  for(int d=0; d<2; d++) {
    lc.shutdown(d,false);
    lc.setIntensity(d,12);
    lc.clearDisplay(d);
  }

  BLEDevice::init("BLE-Client");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pScan->setActiveScan(true);
  pScan->setInterval(10);
  pScan->setWindow(99);
  pScan->start(5, false);
}

void loop() {
  if(doConnect) {
    if(connectToServer()) doConnect = false;
    else  delay(500);
  }

  if(!connected) {
    BLEDevice::getScan()->start(2, false);
  }

  // Display Override Timeout (1 Sekunde)
  if (displayOverride && (millis() - displayOverrideStart >= 200)) {
    displayOverride = false;
    showSpeed(speed);
    showStatus(battery, lightOn);
  }

  if(millis() - lastPacket > 2000) showError();
  delay(50);
//if(connectToServer()) doConnect = false;
    //else delay(500);
  

}