#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; Serial.println("Client verbunden"); }
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; Serial.println("Client getrennt"); }
};

// ==== Fake Fahrdaten ====
uint8_t speed = 0;
bool accelerating = true;
uint8_t battery = 100;
bool lightsOn = false;
uint8_t modeState = 1;
bool leftBlinker = false;
bool rightBlinker = false;
bool highBeam = false;
unsigned long lastUpdate = 0;

void simulate() {
  unsigned long now = millis();
  if (now - lastUpdate < 200) return; // alle 200ms update
  lastUpdate = now;

  // Geschwindigkeit 0–72 km/h simulieren
  if (accelerating) {
    if (speed < 72) speed++;
    else accelerating = false;
  } else {
    if (speed > 0) speed--;
    else accelerating = true;
  }

  // Akku langsam entladen
  if (battery > 0 && now % 1000 < 200) battery--;

  // Licht toggeln alle 5 Sekunden
  if ((now / 5000) % 2 == 0) lightsOn = true;
  else lightsOn = false;

  // Mode-State zwischen 1–3 wechseln
  modeState = ((now / 10000) % 3) + 1;

  // Blinker abwechselnd
  leftBlinker = ((now / 2000) % 2 == 0);
  rightBlinker = !leftBlinker;

  // Fernlicht ab und zu an
  highBeam = ((now / 15000) % 2 == 0);

  // 9-Byte-Paket bauen
  uint8_t dashboardPacket[9];
  dashboardPacket[0] = 0x5A;
  dashboardPacket[1] = 0xA5;
  dashboardPacket[2] = speed;
  dashboardPacket[3] = battery;
  dashboardPacket[4] = lightsOn ? 1 : 0;
  dashboardPacket[5] = modeState;
  dashboardPacket[6] = leftBlinker ? 1 : 0;
  dashboardPacket[7] = rightBlinker ? 1 : 0;
  dashboardPacket[8] = highBeam ? 1 : 0;

  if(deviceConnected) {
    pTxCharacteristic->setValue(dashboardPacket, 9);
    pTxCharacteristic->notify();
  }

  // Debug auf Serial
  Serial.printf("Speed:%3d  Batt:%3d%%  Light:%d  Mode:%d  L:%d  R:%d  HB:%d\n",
    speed, battery, lightsOn, modeState, leftBlinker, rightBlinker, highBeam);
}

void setup() {
  Serial.begin(115200);

 BLEDevice::init("ESP32-UART-Dashboard");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("Dashboard-Simulator gestartet");
}

void loop() {
  simulate();
  delay(10);
}
