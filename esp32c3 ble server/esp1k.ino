#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Pins für Blinker und High Beam
#define LEFT_BLINKER_PIN  9
#define RIGHT_BLINKER_PIN 8
#define HIGH_BEAM_PIN     7

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; Serial.println("Client verbunden"); }
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; Serial.println("Client getrennt"); }
};

// Sicherheits-Callback
class SecurityCallback : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() { return 123456; }
    void onPassKeyNotify(uint32_t pass_key) {}
    bool onConfirmPIN(uint32_t pin) { return (pin == 123456); }
    bool onSecurityRequest() { return true; }
    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
        Serial.println(cmpl.success ? "Authentifizierung erfolgreich" : "Authentifizierung fehlgeschlagen");
    }
};

// ===================== Original-Paketparser =====================
enum ParserState { WAIT_HEADER, WAIT_SECOND_HEADER, COLLECT_PACKET };
ParserState state = WAIT_HEADER;
uint8_t packetBuffer[32];
uint8_t packetIndex = 0;
uint8_t payloadLength = 0;

void processPacket() {
  if(packetIndex < 9) return;
  if(packetBuffer[0]!=0x5A || packetBuffer[1]!=0xA5) return;

  payloadLength = packetBuffer[2];
  if(packetIndex < 9 + payloadLength) return;

  // Beispiel: Prüfen, ob es das Dashboard-Paket ist
  if(packetBuffer[3]==0x20 && packetBuffer[4]==0x21 && packetBuffer[5]==0x64) {
    if(packetIndex>=12) {
      uint8_t speed = packetBuffer[11];
      uint8_t lightState = packetBuffer[9];
      uint8_t modeState = packetBuffer[7];
      uint8_t batteryPercent = packetBuffer[8];

      bool leftBlinker = digitalRead(LEFT_BLINKER_PIN) == LOW;
      bool rightBlinker = digitalRead(RIGHT_BLINKER_PIN) == LOW;
      bool highBeam = digitalRead(HIGH_BEAM_PIN) == LOW;

      // Kompaktes 9-Byte-Paket für den Client
      uint8_t dashboardPacket[9];
      dashboardPacket[0] = 0x5A;
      dashboardPacket[1] = 0xA5;
      dashboardPacket[2] = speed;
      dashboardPacket[3] = batteryPercent;
      dashboardPacket[4] = lightState;
      dashboardPacket[5] = modeState;
      dashboardPacket[6] = leftBlinker ? 1 : 0;
      dashboardPacket[7] = rightBlinker ? 1 : 0;
      dashboardPacket[8] = highBeam ? 1 : 0;

      if(deviceConnected) {
        pTxCharacteristic->setValue(dashboardPacket, 9);
        pTxCharacteristic->notify();
      }
    }
  }
  packetIndex = 0;
  state = WAIT_HEADER;
}

// ===================== UART-Lesen und Parser =====================
void handleUART() {
  while(Serial.available()) {
    uint8_t c = Serial.read();
    switch(state) {
      case WAIT_HEADER:
        if(c==0x5A) { packetBuffer[0]=c; packetIndex=1; state=WAIT_SECOND_HEADER; }
        break;
      case WAIT_SECOND_HEADER:
        if(c==0xA5) { packetBuffer[1]=c; packetIndex=2; state=COLLECT_PACKET; }
        else state=WAIT_HEADER;
        break;
      case COLLECT_PACKET:
        if(packetIndex<sizeof(packetBuffer)) packetBuffer[packetIndex++]=c;
        if(packetIndex>=3) {
          payloadLength = packetBuffer[2];
          if(packetIndex >= 9 + payloadLength) processPacket();
        }
        break;
    }
  }
}

// ===================== Setup/Loop =====================
void setup() {
  Serial.begin(115200);
  pinMode(LEFT_BLINKER_PIN, INPUT_PULLUP);
  pinMode(RIGHT_BLINKER_PIN, INPUT_PULLUP);
  pinMode(HIGH_BEAM_PIN, INPUT_PULLUP);

  BLEDevice::init("ESP32-UART-Dashboard");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(new SecurityCallback());

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  pSecurity->setCapability(ESP_IO_CAP_OUT);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());
  pTxCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  pAdvertising->start();

  Serial.println("BLE Dashboard Sender gestartet");
}

void loop() {
  handleUART();
  delay(1);
}
