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
unsigned long lastPacket = 0;

// ===================== 3x5 Font =====================
const byte font3x5[10][5] = {
  {B111,B101,B101,B101,B111}, //0
  {B010,B110,B010,B010,B111}, //1
  {B111,B001,B111,B100,B111}, //2
  {B111,B001,B111,B001,B111}, //3
  {B101,B101,B111,B001,B001}, //4
  {B111,B100,B111,B001,B111}, //5
  {B111,B100,B111,B101,B111}, //6
  {B111,B001,B001,B001,B001}, //7
  {B111,B101,B111,B101,B111}, //8
  {B111,B101,B111,B001,B111}  //9
};

void drawDigit(int device, int digit, int x, int y) {
  for (int row=0; row<5; row++) {
    byte line = font3x5[digit][row];
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
  for (int c=0; c<bars; c++) lc.setLed(0, 6, c, !blink);
  for (int c=0; c<bars; c++) lc.setLed(0, 7, c, !blink);
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

void showError() {
  for(int d=0; d<2; d++) {
    lc.clearDisplay(d);
    for (int i=0;i<8;i++) {
      lc.setLed(d,i,i,true);
      lc.setLed(d,i,7-i,true);
    }
  }
}

// ===================== BLE Callbacks =====================
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) { 
    connected = true; 
  //  Serial.println("Verbunden mit BLE Server"); 
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
     // Serial.println("ESP32-UART-Dashboard gefunden");
    }
  }
};

// ------------------- BLE Notify -------------------
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify) {
  if(length < 6) return;
  if(pData[0] != 0x5A || pData[1] != 0xA5) return;

  speed = pData[2];
  battery = pData[3];
  lightOn = pData[4];
  lastPacket = millis();

  //Serial.printf("Speed: %d, Battery: %d, Light: %d\n", speed, battery, lightOn);

  showSpeed(speed);
  showStatus(battery, lightOn);

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
   // Serial.println("Notify registriert");
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
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(5, false);
}

void loop() {
  if(doConnect) {
    if(connectToServer()) doConnect = false;
    else delay(2000);
  }

  if(!connected) {
    BLEDevice::getScan()->start(2, false);
  }

  if(millis() - lastPacket > 2000) showError();
  delay(50);
}
