// ESP32-C3 + NEO-6M GPS → BLE で緯度経度を送信
// 必要ライブラリ: TinyGPSPlus（Arduino IDEのライブラリマネージャで追加）
// 配線: GPS TX → GPS_RX_PIN / GPS RX → GPS_TX_PIN / VCC → 3.3V(or 5V、モジュール仕様に従う) / GND → GND
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <TinyGPSPlus.h>

#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHAR_UUID    "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

// ボードに合わせて変更（XIAO ESP32C3 は D7=GPIO20(RX), D6=GPIO21(TX)）
#define GPS_RX_PIN 20
#define GPS_TX_PIN 21

TinyGPSPlus gps;
BLECharacteristic *pChar;
volatile bool connected = false;

class CB : public BLEServerCallbacks {
  void onConnect(BLEServer*) { connected = true; }
  void onDisconnect(BLEServer* s) { connected = false; s->startAdvertising(); }
};

void setup() {
  Serial1.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // NEO-6Mの標準は9600bps

  BLEDevice::init("Kickboard-C3");
  BLEDevice::setMTU(185);  // 長めの文字列を送れるようにする
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new CB());
  BLEService *svc = server->createService(SERVICE_UUID);
  pChar = svc->createCharacteristic(CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pChar->addDescriptor(new BLE2902());
  svc->start();
  server->getAdvertising()->addServiceUUID(SERVICE_UUID);
  server->getAdvertising()->start();
}

void loop() {
  // GPSデータは常に読み続ける（delayで止めない）
  while (Serial1.available()) gps.encode(Serial1.read());

  static uint32_t last = 0;
  if (connected && millis() - last >= 1000) {
    last = millis();
    char buf[48];
    if (gps.location.isValid() && gps.location.age() < 3000) {
      double acc = gps.hdop.isValid() ? gps.hdop.hdop() * 2.5 : 10;  // HDOPからの概算精度(m)
      snprintf(buf, sizeof(buf), "%.6f,%.6f,%.0f", gps.location.lat(), gps.location.lng(), acc);
    } else {
      strcpy(buf, "NOFIX");  // GPS未測位
    }
    pChar->setValue((uint8_t*)buf, strlen(buf));
    pChar->notify();
  }
}
