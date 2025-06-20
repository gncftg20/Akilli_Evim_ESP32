#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "ArduinoJson.h"

#define API_KEY "AIzaSyBKbClIvNrmJdzq5Gy97MI6mssqk31A77o"
#define DATABASE_URL "https://esp32-4d97e-default-rtdb.firebaseio.com/"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_MOSI 23
#define OLED_CLK 18
#define OLED_DC 17
#define OLED_CS 13  //boş pin
#define OLED_RST 5

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RST, OLED_CS);

const int relayPins[] = { 0, 4, 16, 19, 21 };
unsigned long sonsayac = 0;
const unsigned long interval = 5000;
const unsigned long BILDIRIM_SURESI = 3000;
const unsigned long SICAKLIK_GUNCELLEME_SURESI = 10000;

bool dataReceived = false;
bool bildirimGosteriliyor = false;
bool ekoMod = false;
bool kombiDurum = false;
float hedefDerece = 24;
float Derece = 24;
float Nem = 24;

struct cihazlar {
  bool durum;
  String oncekiAd;
  String ad;
};

cihazlar cihazlar[4] = {
  { false, "Cihaz 1", "Cihaz 1" },
  { false, "Cihaz 2", "Cihaz 2" },
  { false, "Cihaz 3", "Cihaz 3" },
  { false, "Cihaz 4", "Cihaz 4" }
};

char firebase_email[64];
char firebase_password[64];
char uid[128];
String Email = "abdurrahmantekkoyun@gmail.com";
String Password = "12345678";
String userId = "Ms7nx5me17RAnfL7JK3n4ZgWuFf1";
String newEmail = "";
String newPassword = "";
String newUid = "";

FirebaseData stream;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

Preferences preferences;

void gosterBildirim(String mesaj) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  // Mesajı satırlara böl
  int maxKarakter = 21;  // Bir satırda gösterilebilecek maksimum karakter
  for (int i = 0; i < mesaj.length(); i += maxKarakter) {
    // min fonksiyonunu düzelt
    int sonIndex = (i + maxKarakter > mesaj.length()) ? mesaj.length() : i + maxKarakter;
    String satir = mesaj.substring(i, sonIndex);
    display.println(satir);
  }

  display.display();
  sonsayac = millis();
  bildirimGosteriliyor = true;
}
void durumsorgula() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);  // En üste alalım biraz
  display.print(cihazlar[0].ad + ": ");
  display.println(cihazlar[0].durum ? "ACIK" : "KAPALI");
  display.print(cihazlar[1].ad + ": ");
  display.println(cihazlar[1].durum ? "ACIK" : "KAPALI");
  display.print(cihazlar[2].ad + ": ");
  display.println(cihazlar[2].durum ? "ACIK" : "KAPALI");
  display.print(cihazlar[3].ad + ": ");
  display.println(cihazlar[3].durum ? "ACIK" : "KAPALI");

  display.setCursor(0, 35);  // Biraz aşağıya alalım
  display.print("Sicaklik: ");
  display.print(Derece, 1);  // 1 ondalık basamak
  display.println(" C");

  display.print("Nem: ");
  display.print(Nem, 0);  // 0 ondalık basamak
  display.println(" %");

  display.print("Hedef:");
  display.print(hedefDerece, 1);

  display.setCursor(90, 35);  // En alta kombi durumu
  display.print("Kombi:");
  display.setCursor(90, 45);
  display.print(kombiDurum ? "ACIK" : "KAPALI");
  display.display();  // Ekranı güncelle
}
void Termostat() {
  float aralık = ekoMod ? 0.5 : 0.3;

  if (Derece <= (hedefDerece - aralık)) {
    Serial.println("Kombi AÇILIYOR");
    digitalWrite(relayPins[4], HIGH);

  } else if (Derece <= (hedefDerece - aralık)) {
    digitalWrite(relayPins[4], LOW);
    Serial.println("Kombi KAPANIYOR");

  } else {
    Serial.println("Kombi mevcut aralıkta, değişiklik yok.");
  }
}
void streamCallback(FirebaseStream data) {
  Serial.printf("Stream verisi geldi! Yol: %s, Tip: %s, Veri: %s\n",
                data.dataPath().c_str(),
                data.dataType().c_str(),
                data.stringData().c_str());

  String path = data.dataPath();

  // --- CİHAZ DURUM KONTROLLERİ ---
  for (int i = 0; i < 4; i++) {
    String cihazDurumPath = "/cihaz" + String(i + 1) + "/durum";
    String cihazAdPath = "/cihaz" + String(i + 1) + "/ad";

    if (path.equals(cihazDurumPath)) {
      if (data.dataType() == "boolean") {
        bool yeniDurum = data.boolData();
        if (cihazlar[i].durum != yeniDurum) {
          cihazlar[i].durum = yeniDurum;
          digitalWrite(relayPins[i], yeniDurum ? LOW : HIGH);
          String bildirim = cihazlar[i].ad + " " + (yeniDurum ? "Acildi" : "Kapatildi");
          gosterBildirim(bildirim);
          Serial.println("Röle Durumu Güncellendi: " + bildirim);
        }
      }
    } else if (path.equals(cihazAdPath)) {
      if (data.dataType() == "string") {
        String yeniAd = data.stringData();
        if (cihazlar[i].ad != yeniAd) {
          cihazlar[i].oncekiAd = cihazlar[i].ad;
          cihazlar[i].ad = yeniAd;
          String bildirim = cihazlar[i].oncekiAd + " adli cihaz artik\n" + yeniAd;
          gosterBildirim(bildirim);
          Serial.println("Cihaz Adı Güncellendi: " + bildirim);
        }
      }
    }
  }

  // --- TERMOSTAT KONTROLLERİ ---
  if (path.equals("/termostat/ekomod")) {
    if (data.dataType() == "boolean") {
      if (ekoMod != data.boolData()) {
        ekoMod = data.boolData();
        gosterBildirim(String("Eko Mod ") + (ekoMod ? "Acildi" : "Kapatildi"));
        Serial.println("Eko Mod Güncellendi: " + String(ekoMod));
      }
    }
  }
  if (path.equals("/termostat/kombionoff")) {
    if (data.dataType() == "boolean") {
      if (ekoMod != data.boolData()) {
        kombiDurum = data.boolData();
        gosterBildirim(String("Kombi ") + (kombiDurum ? "Acildi" : "Kapatildi"));
        Serial.println("Eko Mod Güncellendi: " + String(kombiDurum));
      }
    }
  }

  if (path.equals("/termostat/hedefderece")) {
    if (data.dataType() == "number" || data.dataType() == "int" || data.dataType() == "float") {
      float yeniHedef = data.floatData();
      if (hedefDerece != yeniHedef) {
        hedefDerece = yeniHedef;
        gosterBildirim("Hedef Sicaklik:\n" + String(hedefDerece, 1) + " C");
        Serial.println("Hedef Sicaklik:\n" + String(hedefDerece, 1) + " C");
      }
    }
  }
  // ... Diğer termostat kontrolleri için de benzer debug çıktılar ekle ...
  Serial.println("------------------------------------");
}

void fetchInitialFirebaseConfig() {
  Serial.println("Firebase'den başlangıç yapılandırma verileri çekiliyor...");
  String basePath = "users/" + userId + "/Kontrol/";

  for (int i = 0; i < 4; i++) {
    String pathDurum = basePath + "cihaz" + String(i + 1) + "/durum";
    if (Firebase.RTDB.getBool(&fbdo, pathDurum)) {
      cihazlar[i].durum = fbdo.boolData();
      digitalWrite(relayPins[i], cihazlar[i].durum ? LOW : HIGH);
      Serial.printf("Cihaz %d durum: %s\n", i + 1, cihazlar[i].durum ? "ACIK" : "KAPALI");
    } else {
      Serial.printf("Cihaz %d durum çekilemedi: %s\n", i + 1, fbdo.errorReason().c_str());
    }

    String pathAd = basePath + "cihaz" + String(i + 1) + "/ad";
    if (Firebase.RTDB.getString(&fbdo, pathAd)) {
      cihazlar[i].ad = fbdo.stringData();
      cihazlar[i].oncekiAd = cihazlar[i].ad;  // Başlangıçta aynı yap
      Serial.printf("Cihaz %d adı: %s\n", i + 1, cihazlar[i].ad.c_str());
    } else {
      Serial.printf("Cihaz %d adı çekilemedi: %s\n", i + 1, fbdo.errorReason().c_str());
    }
  }

  // Termostat ayarları
  if (Firebase.RTDB.getBool(&fbdo, basePath + "termostat/ekomod")) {
    ekoMod = fbdo.boolData();
    Serial.printf("Eko Mod: %s\n", ekoMod ? "ACIK" : "KAPALI");
  } else {
    Serial.println("Eko Mod çekilemedi: " + fbdo.errorReason());
  }

  if (Firebase.RTDB.getBool(&fbdo, basePath + "termostat/kombionoff")) {
    kombiDurum = fbdo.boolData();
    // Kombi rölesi varsa burada kontrol et
    Serial.printf("Kombi Durumu: %s\n", kombiDurum ? "ACIK" : "KAPALI");
  } else {
    Serial.println("Kombi durumu çekilemedi: " + fbdo.errorReason());
  }

  if (Firebase.RTDB.getFloat(&fbdo, basePath + "termostat/hedefderece")) {
    hedefDerece = fbdo.floatData();
    Serial.printf("Hedef Derece: %.1f\n", hedefDerece);
  } else {
    Serial.println("Hedef Derece çekilemedi: " + fbdo.errorReason());
  }

  Serial.println("Başlangıç yapılandırması tamamlandı.");
}

void streamTimeoutCallback(bool timeout) {
  if (timeout)
    Serial.println("stream timed out, resuming...\n");

  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void firebaseTask(void* parameter) {
  // Firebase objeleri için BSSL (güvenli soket katmanı) tampon boyutunu ayarla
  // Bu, OOM hatalarını azaltmaya yardımcı olabilir
  fbdo.setBSSLBufferSize(2048, 1024);    // Varsayılanı artırdık
  stream.setBSSLBufferSize(2048, 1024);  // Varsayılanı artırdık

  // Firebase yapılandırmasını ayarla
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Kimlik doğrulama bilgileri
  auth.user.email = "abdurrahmantekkoyun@gmail.com";
  auth.user.password = "12345678";

  // Firebase'i başlat
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);



  String streamPath = "users/" + userId + "/Kontrol";
  Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
  if (!Firebase.RTDB.beginStream(&stream, streamPath)) {
    Serial.println("Stream baslatilamadi: " + stream.errorReason());
  }
  Serial.println("Firebase Stream dinleniyor: " + streamPath);

  // Başlangıç yapılandırmasını çek (Firebase hazırsa)
  if (Firebase.ready()) {
    Serial.println("Firebase: Kimlik doğrulama başarılı veya hazır.");
    fetchInitialFirebaseConfig();
  } else {
    Serial.println("Firebase: Başlangıçta hazır değil. Başlangıç verileri çekilemedi.");
  }

  // Görev döngüsü: Firebase bağlantısını ve periyodik güncellemeleri yönet
  unsigned long lastTempUpdate = millis();  // Görevin kendi sayacını kullan
  while (true) {
    // Firebase'in hazır olup olmadığını kontrol et ve eğer bağlantı koptuysa yeniden başlatmayı dene
    if (!Firebase.ready()) {
      Serial.println("Firebase bağlantısı koptu veya hazır değil, yeniden deneniyor...");
      vTaskDelay(pdMS_TO_TICKS(1000));
      Firebase.begin(&config, &auth);                         // Tekrar dene
      if (!Firebase.RTDB.beginStream(&stream, streamPath)) {  // Stream'i de tekrar başlat
        Serial.println("Stream yeniden baslatilamadi: " + stream.errorReason());
      }
    }

    // Sıcaklık ve Nem verilerini Firebase'e periyodik gönderme
    if (millis() - sonsayac >= 10000) {
      if (Firebase.ready() && dataReceived) {
        Termostat();
        Serial.println("Sıcaklık ve Nem Firebase'e güncelleniyor...");
        String tempPath = "users/" + userId + "/Kontrol/termostat/mevcutderece";
        String humPath = "users/" + userId + "/Kontrol/termostat/nem";

        if (Firebase.RTDB.setFloat(&fbdo, tempPath, Derece)) {
          Serial.println("Sıcaklık güncellendi.");
        } else {
          Serial.println("Sıcaklık güncelleme başarısız: " + fbdo.errorReason());
        }

        if (Firebase.RTDB.setFloat(&fbdo, humPath, Nem)) {
          Serial.println("Nem güncellendi.");
        } else {
          Serial.println("Nem güncelleme başarısız: " + fbdo.errorReason());
        }

        dataReceived = false;  // Veri gönderildikten sonra sıfırla, yeni veri bekleniyor
      } else {
        if (!Firebase.ready()) Serial.println("Firebase hazır olmadığı için sıcaklık/nem güncellenemedi.");
        if (!dataReceived) Serial.println("BLE'den sıcaklık/nem verisi alınamadığı için güncellenemedi.");
      }
      sonsayac = millis();  // Sayacı sıfırla
    }

    vTaskDelay(pdMS_TO_TICKS(100));  // Görevi duraklat, CPU'yu diğer görevlere bırak
  }
}
void GetFirebaseInfo() {
  preferences.begin("config", true);  // read-only
  Email = preferences.getString("email").c_str();
  Password = preferences.getString("password").c_str();
  userId = preferences.getString("uid").c_str();
  preferences.end();
}
void WiFiManagerCustom() {
  WiFiManager wm;
  WiFiManagerParameter custom_firebase_email("email", "Firebase Email", firebase_email, 64);
  WiFiManagerParameter custom_firebase_password("password", "Firebase Password", firebase_password, 64, "type=\"password\"");
  WiFiManagerParameter custom_userId("uid", "User UID", uid, 128);

  wm.addParameter(&custom_firebase_email);
  wm.addParameter(&custom_firebase_password);
  wm.addParameter(&custom_userId);

  IPAddress dns1(8, 8, 8, 8);  // Google DNS
  IPAddress dns2(1, 1, 1, 1);  // Cloudflare DNS
  WiFi.setDNS(dns1, dns2);

  wm.setConnectTimeout(20);

  if (!wm.autoConnect("ESP32_Config", "12345678")) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Cihaz kurulum moduna geçiyor...");
    display.setCursor(0, 15);
    display.println("Wifi: ESP32_Config");
    display.setCursor(0, 30);
    display.println("Şifre: 12345678");
    display.display();
    delay(3000);
    ESP.restart();
  } else {
    Serial.println("WiFi bağlı!");
    Serial.println(WiFi.localIP());
  }

  Serial.println("✅ WiFi bağlantısı başarılı.");
  Serial.println(WiFi.localIP());
  Serial.println(Email);
  Serial.println(Password);
  Serial.println(userId);

  newEmail = custom_firebase_email.getValue();
  newPassword = custom_firebase_password.getValue();
  newUid = custom_userId.getValue();
  /*
  bool changes = false;
  if (newEmail != Email || newPassword != Password || newUid != userId) {
    preferences.begin("config", false);
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Kullanici bilgileri güncellendi.");
    display.display();
    if (newEmail != Email) {
      preferences.putString("email", newEmail);
      Serial.println("📥 Yeni email kaydedildi.");
      changes = true;
    }
    if (newPassword != Password) {
      preferences.putString("password", newPassword);
      Serial.println("📥 Yeni şifre kaydedildi.");
      changes = true;
    }
    if (newUid != userId) {
      preferences.putString("uid", newUid);
      Serial.println("📥 Yeni UID kaydedildi.");
      changes = true;
    }
    preferences.end();
  }

  if (!changes) {
    Serial.println("ℹ Veriler güncel, değişiklik yapılmadı.");
  }*/
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 22, -1);  // RX=16, TX kullanılmıyor
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  for (int i = 0; i < 5; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }
  digitalWrite(relayPins[4], LOW);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Sistem Baslatiliyor...");
  display.display();


  // GetFirebaseInfo();

  WiFiManagerCustom();
  delay(100);
  xTaskCreatePinnedToCore(firebaseTask, "FirebaseTask", 50000, NULL, 1, NULL, 1);

  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  if (bildirimGosteriliyor && (millis() - sonsayac >= BILDIRIM_SURESI)) {
    bildirimGosteriliyor = false;
  }
  if (!bildirimGosteriliyor) {
    durumsorgula();
    delay(100);
  }
  if (Serial2.available() >= 1 && Serial2.read() == 0xAA) {
    if (Serial2.available() >= sizeof(float) * 2) {
      Serial2.readBytes((uint8_t*)&Derece, sizeof(float));
      Serial2.readBytes((uint8_t*)&Nem, sizeof(float));
      Serial.print("Sıcaklık: ");
      Serial.println(Derece);
      Serial.print("Nem: ");
      Serial.println(Nem);
      dataReceived = true;
    }
  }
}