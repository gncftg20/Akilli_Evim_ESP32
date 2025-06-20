#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLE2902.h>

#define SERVICE_UUID "ebe0ccb0-7a0a-4b0c-8a1a-6ff2997da3a6"
#define WRITE_CHAR_UUID "ebe0ccd8-7a0a-4b0c-8a1a-6ff2997da3a6"
#define TEMP_CHAR_UUID "ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6"
#define DEVICE_MAC "a4:c1:38:d0:a6:e7"  // Mi Thermometer MAC

BLEClient* pClient = nullptr;
BLERemoteService* pServiceMain = nullptr;
BLERemoteCharacteristic* pWriteCharSpeed = nullptr;
BLERemoteCharacteristic* pNotifyCharTemp = nullptr;

bool deviceConnected = false;
bool dataReceived = false;
bool bleInitialized = false;
float Derece = 24;
float Nem = 24;

unsigned long sonsayac = 0;




static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

  if (length >= 3) {

    bool sign = pData[1] & (1 << 7);
    int16_t temp = ((pData[1] & 0x7F) << 8) | pData[0];
    if (sign) temp = temp - 32767;
    Derece = temp / 100.0;
    Nem = pData[2];

    Serial.printf("Sıcaklık: %.2f°C, Nem: %.0f%%\n", Derece, Nem);
    uint8_t header = 0xAA; // Örnek başlık
    Serial2.write(header);
    Serial2.write((uint8_t*)&Derece, sizeof(float));
    Serial2.write((uint8_t*)&Nem, sizeof(float));
    Serial2.flush();
    dataReceived = true;
  }
}



class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    deviceConnected = true;
    Serial.println(F("Cihaza bağlanıldı"));
  }

  void onDisconnect(BLEClient* pclient) {
    pClient = nullptr;
    pNotifyCharTemp = nullptr;
    pWriteCharSpeed = nullptr;
    deviceConnected = false;
    Serial.println(F("Cihaza bağlantı sonlandı. Tekrar bağlanmayı deneyeceğim..."));
  }
};

void miAuthorization() {
  uint8_t authPacket[] = { 0x01, 0x00 };
  if (pWriteCharSpeed != nullptr) {
    pWriteCharSpeed->writeValue(authPacket, sizeof(authPacket), false);
  }
  Serial.println(F("Yetkilendirme gönderildi"));
}

void connectToKnownDevice() {
  if (!bleInitialized) {
    BLEDevice::init("ESP1_BLE_Sender");  // Bu ESP'nin adı
    Serial.println(F("BLEDevice başlatıldı."));
    bleInitialized = true;
  }
  BLEAddress deviceAddress(DEVICE_MAC);
  Serial.printf(F("MAC ile cihaza bağlanılıyor: %s\n"), deviceAddress.toString().c_str());

  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  if (pClient->connect(deviceAddress)) {
    Serial.println(F("Sunucuya bağlanıldı."));

    pServiceMain = pClient->getService(SERVICE_UUID);
    if (pServiceMain == nullptr) {
      Serial.println(F("Servis bulunamadı. Bağlantı kesiliyor."));
      pClient->disconnect();
      return;
    }
    Serial.println(F("Ana servis bulundu."));

    pWriteCharSpeed = pServiceMain->getCharacteristic(WRITE_CHAR_UUID);
    if (pWriteCharSpeed == nullptr) {
      Serial.println(F("Yazma karakteristiği bulunamadı. Bağlantı kesiliyor."));
      pClient->disconnect();
      return;
    }
    Serial.println(F("Yazma karakteristiği bulundu."));

    pNotifyCharTemp = pServiceMain->getCharacteristic(TEMP_CHAR_UUID);
    if (pNotifyCharTemp == nullptr) {
      Serial.println(F("Sıcaklık karakteristiği bulunamadı. Bağlantı kesiliyor."));
      pClient->disconnect();
      return;
    }
    Serial.println(F("Sıcaklık karakteristiği bulundu."));

    if (pNotifyCharTemp->canNotify()) {
      pNotifyCharTemp->registerForNotify(notifyCallback);
      Serial.println(F("Bildirimler için kaydedildi."));
      miAuthorization();
    }
  } else {
    deviceConnected = false;
    Serial.println(F("BLE bağlantısı başarısız oldu."));
    if (pClient != nullptr) {
      delete pClient;
      pClient = nullptr;
    }
    pNotifyCharTemp = nullptr;
    pWriteCharSpeed = nullptr;
  }
  vTaskDelay(pdMS_TO_TICKS(100));
}


void setup() {
  Serial.begin(115200);
  
  connectToKnownDevice();
  Serial2.begin(9600, SERIAL_8N1, -1, 17);
}

void loop() {

  vTaskDelay(pdMS_TO_TICKS(1000));  // Küçük bir gecikme
}
