
#include <TFT_eSPI.h>
#include <SPI.h>
#include <pgmspace.h>
#include <BLEDevice.h>
#include <BLEAdvertisedDevice.h>
#include "logo.h"
#include <PNGdec.h> // PNG decoder library

#include "xbackground_image.h" // PNG data for background image

PNG png; // PNG decoder instance
int16_t xpos = 0; // X position for image drawing
int16_t ypos = 0; // Y position for image drawing
#define MAX_IMAGE_WDITH 320 // Maximum image width for display

// ===================== Display =====================
#define SCR_WD 240
#define SCR_HT 320
TFT_eSPI tft = TFT_eSPI();

// --------------------- Pins ---------------------
#define RELAY_PIN         10
#define LEFT_BLINKER_PIN 9
#define RIGHT_BLINKER_PIN 8
#define HIGH_BEAM_PIN     7

// --------------------- Farben ---------------------
#define AUDI_BLACK      tft.color565(0,0,0)
#define AUDI_WHITE      tft.color565(255,255,255)
#define AUDI_RED        tft.color565(230,0,0)
#define AUDI_GRAY       tft.color565(60,60,60)
#define AUDI_BLUE       tft.color565(0,100,255)
#define AUDI_YELLOW     tft.color565(255,200,0)
#define AUDI_GREEN      tft.color565(0,255,0)

#define BLACK AUDI_BLACK
#define WHITE AUDI_WHITE

// --------------------- BLE ---------------------
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
bool leftBlinkerState = false;
bool rightBlinkerState = false;
bool highBeamState = false;

static BLEAdvertisedDevice* myDevice;
static bool doConnect = false;
static bool connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;

// --------------------- 7-Segment ---------------------
uint16_t SEGMENT_COLOR = AUDI_WHITE;
const uint16_t SEGMENT_BG = AUDI_BLACK;
const int SEGMENT_WIDTH   = 40;
const int SEGMENT_HEIGHT  = 12;
const int SEGMENT_SPACING = 10;
const int DIGIT_WIDTH  = SEGMENT_WIDTH + 2*SEGMENT_SPACING;
const int DIGIT_HEIGHT = 2*SEGMENT_WIDTH + 3*SEGMENT_SPACING;

const byte digitPatterns[10] = {
  B1111110,B0110000,B1101101,B1111001,B0110011,
  B1011011,B1011111,B1110000,B1111111,B1111011
};

// ===================== Display Functions =====================
void drawSegment(int x, int y, bool horizontal, bool on) {
  tft.fillRoundRect(
    x, y,
    horizontal ? SEGMENT_WIDTH  : SEGMENT_HEIGHT,
    horizontal ? SEGMENT_HEIGHT : SEGMENT_WIDTH,
    3, on ? SEGMENT_COLOR : SEGMENT_BG
  );
}

void drawDigit(int x, int y, int number) {
  byte pattern = digitPatterns[number];
  drawSegment(x + SEGMENT_SPACING, y, true, pattern & B1000000);
  drawSegment(x + SEGMENT_WIDTH + SEGMENT_SPACING, y + SEGMENT_SPACING, false, pattern & B0100000);
  drawSegment(x + SEGMENT_WIDTH + SEGMENT_SPACING, y + SEGMENT_WIDTH + 2*SEGMENT_SPACING, false, pattern & B0010000);
  drawSegment(x + SEGMENT_SPACING, y + 2*SEGMENT_WIDTH + 2*SEGMENT_SPACING, true, pattern & B0001000);
  drawSegment(x, y + SEGMENT_WIDTH + 2*SEGMENT_SPACING, false, pattern & B0000100);
  drawSegment(x, y + SEGMENT_SPACING, false, pattern & B0000010);
  drawSegment(x + SEGMENT_SPACING, y + SEGMENT_WIDTH + SEGMENT_SPACING, true, pattern & B0000001);
}

void drawSpeedDisplay(int x, int y, int speed, uint16_t color = AUDI_WHITE) {
  speed = constrain(speed, 0, 199);
  int tens = speed / 10;
  int units = speed % 10;
  uint16_t originalColor = SEGMENT_COLOR;
  SEGMENT_COLOR = color;
  tft.startWrite();
  drawDigit(x, y, tens);
  drawDigit(x + DIGIT_WIDTH + 10, y, units);
  tft.endWrite();
  SEGMENT_COLOR = originalColor;
}

void drawBatterySymbol(int x, int y, int percent) {
  int width = 74, height = 18, tipWidth = 3, tipHeight = 10;

  // Rahmen
  tft.drawRoundRect(x, y, width, height, 2, AUDI_WHITE);
  tft.fillRoundRect(x + width, y + (height - tipHeight)/2, tipWidth, tipHeight, 1, AUDI_WHITE);

  // Innenbereich schwarz machen
  tft.fillRect(x + 2, y + 2, width - 4, height - 4, AUDI_BLACK);

  // Füllstand berechnen
  int fillWidth = map(percent, 0, 100, 0, width - 4);
  if (fillWidth > 0) {
    uint16_t fillColor = (percent > 60) ? AUDI_GREEN : (percent > 20) ? AUDI_YELLOW : AUDI_RED;
    tft.fillRect(x + 2, y + 2, fillWidth, height - 4, fillColor);
  }
}

void drawLightIndicator(int x, int y, bool isOn) {
  tft.fillCircle(x, y, 10, isOn ? AUDI_YELLOW : AUDI_BLACK);
  tft.drawCircle(x, y, 10, AUDI_WHITE);
}

void drawBlinkerIndicator(int x, int y, bool isLeft, bool isActive) {
  int w = 20, h = 12;
  if (isLeft) {
    tft.fillTriangle(x, y + h/2, x + w, y, x + w, y + h, isActive ? AUDI_YELLOW : AUDI_GRAY);
    tft.drawTriangle(x, y + h/2, x + w, y, x + w, y + h, AUDI_WHITE);
  } else {
    tft.fillTriangle(x, y, x, y + h, x + w, y + h/2, isActive ? AUDI_YELLOW : AUDI_GRAY);
    tft.drawTriangle(x, y, x, y + h, x + w, y + h/2, AUDI_WHITE);
  }
}

void drawHighBeamIndicator(int x, int y, bool isActive) {
  tft.fillCircle(x, y, 8, isActive ? AUDI_BLUE : AUDI_BLACK);
  tft.drawCircle(x, y, 8, AUDI_WHITE);
}


void drawWiringIndicator(int x, int y, bool isDeltaMode) {
  tft.fillRoundRect(x, y, 78, 35, 5, AUDI_GRAY);
  tft.setTextSize(1);
  tft.setTextColor(AUDI_WHITE);
  tft.setCursor(x + 5, y + 5);
  tft.print("WIRING");
  tft.setTextSize(2);
  tft.setTextColor(isDeltaMode ? AUDI_BLUE : AUDI_WHITE);
  tft.setCursor(x + 15, y + 15);
  tft.print(isDeltaMode ? "DELTA" : "STAR");
}

void drawDriveMode(int x, int y, int mode) {
  const char* modeText = "";
  uint16_t color = AUDI_WHITE;
  if (mode == 4 || mode == 5) {
    modeText = "DYNAMIC";
    color = AUDI_RED;
  } else if (mode == 2 || mode == 3) {
    modeText = "ECO";
    color = AUDI_BLUE;
  } else {
    modeText = "COMFORT";
  }
  tft.setTextSize(2);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(modeText);
}

void drawGearIndicator(int x, int y, String gear) {
  tft.fillRoundRect(x, y, 40, 40, 5, AUDI_GRAY);
  tft.setTextSize(3);
  tft.setTextColor(AUDI_WHITE);
  tft.setCursor(x + 12, y + 10);
  tft.print(gear);
}


void drawBitmap(int x, int y, const uint8_t* bitmap, int w, int h, uint16_t color) {
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      uint8_t byteVal = pgm_read_byte(&bitmap[j * (w / 8) + i / 8]);
      if (byteVal & (1 << (i % 8))) {
        tft.drawPixel(x + i, y + j, color);
      }
    }
  }
}
void drawBitmapTransparent(int x, int y, const uint8_t* bitmap, int w, int h, uint16_t color) {
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      uint8_t byteVal = pgm_read_byte(&bitmap[j * (w / 8) + i / 8]);
      if (byteVal & (1 << (7 - (i % 8)))) { // Beachte Bit-Reihenfolge (MSB first)
        tft.drawPixel(x + i, y + j, color);
      }
    }
  }
}

int pngDraw(PNGDRAW * pDraw) {
    uint16_t lineBuffer[MAX_IMAGE_WDITH];
    png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
    tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer);
    return 1; // Wichtig! int zurückgeben
}


void drawDashboardFrame() {
 
 

  /*
  tft.drawFastHLine(0, 120, 240, AUDI_GRAY);
  tft.drawFastHLine(0, 200, 240, AUDI_GRAY);
  tft.drawFastVLine(20, 0, 320, AUDI_GRAY);
  tft.drawFastVLine(220, 0, 320, AUDI_GRAY);
  tft.fillRect(0, 250, 240, 70, AUDI_BLACK);
  tft.drawFastHLine(0, 250, 240, AUDI_RED);
 //  drawBitmapTransparent(0,0, background_image, 240, 320, tft.color565(255,255,255));
*/
}

void showSplashScreen() {
  tft.fillScreen(BLACK);
  delay(200);
  // Logo mit ansteigender Helligkeit zeichnen (ohne Hintergrund)
  for (int brightness = 0; brightness <= 255; brightness += 7) {
    drawBitmapTransparent(SCR_WD/2 - 64, SCR_HT/2 - 32, ninebotLogo, 128, 64, tft.color565(brightness, brightness, brightness));
  }

  // Text nach und nach anzeigen
  const char* text = "D45H804RD";
  int textWidth = strlen(text) * 18;
  tft.setTextSize(3);
  tft.setTextColor(WHITE);
  
  for (size_t i = 0; i <= strlen(text); i++) {
    tft.fillRect(SCR_WD/2 - textWidth/2, SCR_HT/2 + 40, textWidth, 24, BLACK);
    tft.setCursor(SCR_WD/2 - textWidth/2, SCR_HT/2 + 40);
    for (size_t j = 0; j < i; j++) {
      tft.print(text[j]);
    }
    delay(60);
  }
  
  delay(100);
  tft.fillScreen(BLACK);
   tft.setTextSize(1);
}



// ===================== BLE Callbacks =====================
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) { connected = true; Serial.println("Verbunden mit BLE Server"); }
  void onDisconnect(BLEClient* pclient) { connected = false; Serial.println("Verbindung getrennt"); }
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveName() && advertisedDevice.getName() == "ESP32-UART-Dashboard") {
    BLEDevice::getScan()->stop();
    myDevice = new BLEAdvertisedDevice(advertisedDevice);
    doConnect = true;
    Serial.println("ESP32-UART-Dashboard gefunden");
}

  }
};

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify) {
    if(length < 9) return; // Paket zu klein
    if(pData[0] != 0x5A || pData[1] != 0xA5) return; // Header check

    uint8_t speed = pData[2];
    uint8_t battery = pData[3];
    bool light = pData[4];
    uint8_t mode = pData[5];

    static uint8_t lastSpeed=255, lastBattery=255, lastMode=255;
    static bool lastLight=2;
    static bool lastDeltaMode = false;

    bool deltaMode = speed > 30;

    // ----------------- Display -----------------
    if(speed != lastSpeed || deltaMode != lastDeltaMode) {
      tft.fillRect(54, 130, 160, 70, AUDI_BLACK);
      drawSpeedDisplay(54, 130, speed, deltaMode ? AUDI_BLUE : AUDI_WHITE);
    //  drawGearIndicator(100, 30, speed == 0 ? "P" : "D");
      lastSpeed = speed;
      lastDeltaMode = deltaMode;
    }

    if(battery != lastBattery) {
      tft.fillRect(90, 290, 55, 23, AUDI_BLACK);
      drawBatterySymbol(90, 290, battery);
      lastBattery = battery;
    }

    if(light != lastLight) {
      drawLightIndicator(30, 50, light);
      lastLight = light;
    }

    if(mode != lastMode) {
      tft.fillRect(90, 270, 100, 20, AUDI_BLACK);
      drawDriveMode(90, 270, mode);
      lastMode = mode;
    }

    drawWiringIndicator(5, 270, deltaMode);

    // ----------------- Relay an Nano senden -----------------
    static bool lastRelayState = false;
    if(deltaMode != lastRelayState) {
      Serial.printf("RELAY:%d\n", deltaMode ? 1 : 0);
      lastRelayState = deltaMode;
    }
}


// ===================== BLE Verbindung =====================
bool connectToServer() {
  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  if (!pClient->connect(myDevice)) { Serial.println("Verbindung fehlgeschlagen"); return false; }

  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) { Serial.println("Service nicht gefunden"); pClient->disconnect(); return false; }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  if (pRemoteCharacteristic == nullptr) { Serial.println("Characteristic nicht gefunden"); pClient->disconnect(); return false; }

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Notify registriert");
  }

  return true;
}
void readNanoInputs() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.startsWith("LEFT:")) leftBlinkerState = line.substring(5).toInt();
    else if (line.startsWith("RIGHT:")) rightBlinkerState = line.substring(6).toInt();
    else if (line.startsWith("HIGH:")) highBeamState = line.substring(5).toInt();
  }
}

// ===================== Setup/Loop =====================
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(LEFT_BLINKER_PIN, INPUT_PULLUP);
  pinMode(RIGHT_BLINKER_PIN, INPUT_PULLUP);
  pinMode(HIGH_BEAM_PIN, INPUT_PULLUP);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(AUDI_BLACK);
  showSplashScreen();
  //drawDashboardFrame();

 tft.fillScreen(TFT_BLACK);
  int16_t rc_mainbg = png.openFLASH((uint8_t * ) background_image, sizeof(background_image), pngDraw);
  if (rc_mainbg == PNG_SUCCESS) {
    tft.startWrite();
    rc_mainbg = png.decode(NULL, 0);
    tft.endWrite();
  }
  png.close();


  drawBatterySymbol(90, 290, 60);
  drawLightIndicator(30, 50, false);
  drawSpeedDisplay(54, 130, 0);
  drawDriveMode(90, 270, 0);
 // drawGearIndicator(100, 30, "P");
  drawWiringIndicator(5, 270, false);
  
  drawBlinkerIndicator(30, 10, true, false);
  drawBlinkerIndicator(190, 10, false, false);
  drawHighBeamIndicator(215, 50, false);

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

  // ----------------- Nano Inputs lesen -----------------
  readNanoInputs();

  // ----------------- Dashboard aktualisieren -----------------
  static bool lastLeft = 2, lastRight = 2, lastHigh = 2;
  if(leftBlinkerState != lastLeft) {
    drawBlinkerIndicator(30, 10, true, leftBlinkerState);
    lastLeft = leftBlinkerState;
  }
  if(rightBlinkerState != lastRight) {
    drawBlinkerIndicator(190, 10, false, rightBlinkerState);
    lastRight = rightBlinkerState;
  }
  if(highBeamState != lastHigh) {
    drawHighBeamIndicator(215, 50, highBeamState);
    lastHigh = highBeamState;
  }

  delay(10);
}
