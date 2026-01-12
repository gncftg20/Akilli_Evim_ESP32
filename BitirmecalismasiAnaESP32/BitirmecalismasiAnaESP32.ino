#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "ArduinoJson.h"
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <esp_task_wdt.h>
#include "addons/TokenHelper.h" // Kütüphane içinde gelir, eklemeyi unutma
#include "addons/RTDBHelper.h"


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3 * 3600);

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

const int relayPins[] = { 0, 4, 16, 19, 15 };
unsigned long sonsayac = 0;
unsigned long sonPeriyodikLogZamani = 0;
const unsigned long interval = 5000;
const unsigned long BILDIRIM_SURESI = 3000;
const unsigned long SICAKLIK_GUNCELLEME_SURESI = 10000;
const unsigned long PERIYODIK_logJson_ARALIGI = 900000;
unsigned long sonEkranGuncelleme = 0;

bool wifiConnected = false;
bool shouldSaveConfig = false;
bool dataReceived = false;
bool bildirimGosteriliyor = false;
bool ekoMod = false;
bool kombiacik = false;
bool kombiDurum= false;
bool ai_setting = false;
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

char firebase_email_setting[64];
char firebase_password[64];
char uid[128];
String Email= "";
String Password = "";
String userId = "";
String newemail = "";
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


  int maxKarakter = 21;
  for (int i = 0; i < mesaj.length(); i += maxKarakter) {
    int sonIndex = (i + maxKarakter > mesaj.length()) ? mesaj.length() : i + maxKarakter;
    String satir = mesaj.substring(i, sonIndex);
    display.println(satir);
  }

  display.display();
  sonsayac = millis();
  bildirimGosteriliyor = true;
}
/*
void durumsorgula() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(cihazlar[0].ad + ": ");
  display.println(cihazlar[0].durum ? "ACIK" : "KAPALI");
  display.print(cihazlar[1].ad + ": ");
  display.println(cihazlar[1].durum ? "ACIK" : "KAPALI");
  display.print(cihazlar[2].ad + ": ");
  display.println(cihazlar[2].durum ? "ACIK" : "KAPALI");
  display.print(cihazlar[3].ad + ": ");
  display.println(cihazlar[3].durum ? "ACIK" : "KAPALI");

  display.setCursor(0, 35);
  display.print("Sicaklik: ");
  display.print(Derece, 1);
  display.println(" C");

  display.print("Nem: ");
  display.print(Nem, 0);
  display.println(" %");

  display.print("Hedef:");
  display.print(hedefDerece, 1);

  display.setCursor(90, 35);
  display.print("Kombi:");
  display.setCursor(90, 45);
  display.print(kombiacik ? "ACIK" : "KAPALI");
  display.display();
}*/
void durumsorgula() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  for(int i=0; i<4; i++) {
     display.print(cihazlar[i].ad.substring(0, 7) + ":"); // İsimleri ekrana sığdır
     display.println(cihazlar[i].durum ? "ACIK" : "KPL");
  }

  display.drawLine(0, 33, 128, 33, WHITE);

  display.setCursor(0, 35);
  display.print("Sicaklik: ");
  display.print(Derece, 1);
  display.println(" C");

  display.print("Nem: ");
  display.print(Nem, 0);
  display.println(" %");

  display.setCursor(0, 55);
  display.print("Hdf:");
  display.print(hedefDerece, 1);

  display.setCursor(70, 55);
  display.print(wifiConnected ? "WIFI:OK" : "WIFI:YOK");

  display.setCursor(90, 35);
  display.print("Kombi:");
  display.setCursor(90, 45);
  display.print(kombiDurum ? "ATESLE" : "BEKLE");
  
  display.display();
}
void Termostat() {
  float aralık = ekoMod ? 0.5 : 0.3;
  if(kombiacik){
    if (Derece <= (hedefDerece - aralık)) {
    Serial.println("Kombi AÇILIYOR");
    digitalWrite(relayPins[4], HIGH);
    kombiDurum=true;
  } else if (Derece >= (hedefDerece + aralık)) {
    digitalWrite(relayPins[4], LOW);
    Serial.println("Kombi KAPANIYOR");
    kombiDurum=false;
  } else {
    Serial.println("Kombi mevcut aralıkta, değişiklik yok.");
  }
  }else{
    Serial.println("Kombi yaz modunda.");
  }
  
}
void periyodikSistemlogJsonu() {
    Serial.println("--- 15 Dakikalık Periyodik Sistem logJsonu Başladı ---");
    FirebaseJson logJson;
    logJson.add("mevcutDerece", Derece);
    logJson.add("nem", Nem);
    logJson.add("hedefDerece", hedefDerece);
    logJson.add("kombionoff", kombiacik); // Röle 32'nin durumu
    logJson.add("kombianlıkdurum", kombiDurum); // Kullanıcı ana düğme durumu
    logJson.add("ekomod", ekoMod);

    for (int i = 0; i < 4; i++) {
        logJson.add("cihaz" + String(i + 1)+" durum", cihazlar[i].durum);
    }
  
    timeClient.update();
    logJson.add("saat", timeClient.getFormattedTime());
    logJson.add("gun", timeClient.getDay()); // 0=Pazar, 1=Pazartesi...

    // 4. logJsonu Firebase'e Yaz
    // logJson yolu: users/{userId}/logJsons/Periyodik/{timestamp_millis}
    String logJsonPath = "users/" + userId + "/Periyodik/" + timeClient.getEpochTime(); 

    if (Firebase.RTDB.setJSON(&fbdo, logJsonPath, &logJson)) {
        Serial.println("Periyodik logJsonlama başarıyla tamamlandı.");
    } else {
        Serial.println("Periyodik logJsonlama başarısız: " + fbdo.errorReason());
    }

    sonPeriyodikLogZamani = millis();
    Serial.println("-------------------------------------------------");
}
void saveConfigCallback () {
  Serial.println(">>> DUĞMEYE BASILDI: shouldSaveConfig = true yapildi!");
  shouldSaveConfig = true;
}

String urlDecode(String str) {
  String encodedString = str;
  String decodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < encodedString.length(); i++) {
    c = encodedString.charAt(i);
    if (c == '+') {
      decodedString += ' ';
    } else if (c == '%') {
      i++;
      code0 = encodedString.charAt(i);
      i++;
      code1 = encodedString.charAt(i);
      c = (h2int(code0) << 4) | h2int(code1);
      decodedString += c;
    } else {
      decodedString += c;
    }
  }
  return decodedString;
}

unsigned char h2int(char c) {
  if (c >= '0' && c <= '9') {
    return ((unsigned char)c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return ((unsigned char)c - 'a' + 10);
  }
  if (c >= 'A' && c <= 'F') {
    return ((unsigned char)c - 'A' + 10);
  }
  return (0);
}

void streamCallback(FirebaseStream data) {
  Serial.printf("Stream verisi geldi! Yol: %s, Tip: %s, Veri: %s\n",
                data.dataPath().c_str(),
                data.dataType().c_str(),
                data.stringData().c_str());

  String path = data.dataPath();


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
      if (kombiacik != data.boolData()) {
        kombiacik = data.boolData();
        gosterBildirim(String("Kombi ") + (kombiacik ? "Acildi" : "Kapatildi"));
        Serial.println("Eko Mod Güncellendi: " + String(kombiacik));
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
  if (path.equals("/ai_setting/ESPLogenabled")) {
    if (data.dataType() == "boolean") {
      if (ai_setting != data.boolData()) {
        ai_setting = data.boolData();
        gosterBildirim(String("AI ") + (ai_setting ? "Acildi" : "Kapatildi"));
        Serial.println("AI Mod Güncellendi: " + String(ai_setting));
      }
    }
  }
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
      cihazlar[i].oncekiAd = cihazlar[i].ad;
      Serial.printf("Cihaz %d adı: %s\n", i + 1, cihazlar[i].ad.c_str());
    } else {
      Serial.printf("Cihaz %d adı çekilemedi: %s\n", i + 1, fbdo.errorReason().c_str());
    }
  }


  if (Firebase.RTDB.getBool(&fbdo, basePath + "termostat/ekomod")) {
    ekoMod = fbdo.boolData();
    Serial.printf("Eko Mod: %s\n", ekoMod ? "ACIK" : "KAPALI");
  } else {
    Serial.println("Eko Mod çekilemedi: " + fbdo.errorReason());
  }
  if (Firebase.RTDB.getBool(&fbdo, basePath + "termostat/kombionoff")) {
    kombiacik = fbdo.boolData();
    Serial.printf("Kombi Durumu: %s\n", kombiacik ? "ACIK" : "KAPALI");
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
  if (Firebase.RTDB.getBool(&fbdo, basePath + "ai_setting/ESPLogenabled")) {
    ai_setting = fbdo.boolData();
    Serial.printf("AI %s\n", ai_setting ? "ACIK" : "KAPALI");
  } else {
    Serial.println("AI Mod çekilemedi: " + fbdo.errorReason());
  }
}
  

void streamTimeoutCallback(bool timeout) {
  if (timeout)
    Serial.println("stream timed out, resuming...\n");

  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void firebaseTask(void* parameter) {
  if (userId.length() == 0 ||Email.length() == 0 || Password.length() == 0) {
    Serial.println("❌ HATA: Kullanıcı bilgileri Email/Pass/UID) eksik! Firebase başlatılmıyor.");
    vTaskDelete(NULL); 
  }

  fbdo.setBSSLBufferSize(2048, 1024); 
  stream.setBSSLBufferSize(2048, 1024);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = Email.c_str();
  auth.user.password = Password.c_str();

  config.timeout.socketConnection = 10000;
  config.timeout.wifiReconnect = 10000;
  config.timeout.serverResponse = 10000;
  config.timeout.rtdbKeepAlive = 45 * 1000;
  config.token_status_callback = tokenStatusCallback;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  String streamPath = "users/" + userId + "/Kontrol";
  
  unsigned long startAttempt = millis();
  while (!Firebase.ready() && millis() - startAttempt < 10000) {
      vTaskDelay(100);
  }
  
  Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
  if (Firebase.ready()) {
      fetchInitialFirebaseConfig();
      Serial.println("Firebase Hazır! Stream başlatılıyor...");
      if (!Firebase.RTDB.beginStream(&stream, streamPath)) {
          Serial.println("Stream başlatılamadı: " + stream.errorReason());
      } else {
          Serial.println("Stream dinleniyor: " + streamPath);
      }
  }
  
/*
  if (!Firebase.RTDB.beginStream(&stream, streamPath)) {
    Serial.println("Stream baslatilamadi: " + stream.errorReason());
  }
  Serial.println("Firebase Stream dinleniyor: " + streamPath);

  if (Firebase.ready()) {
    Serial.println("Firebase: Kimlik doğrulama başarılı veya hazır.");
    fetchInitialFirebaseConfig();
  } else {
    Serial.println("Firebase: Başlangıçta hazır değil. Başlangıç verileri çekilemedi.");
  }
*/

  
  while (true) {
    if (Firebase.ready() && !Firebase.RTDB.readStream(&stream)) { 
        Serial.println("Stream koptu, tekrar başlatılıyor...");
        if (!Firebase.RTDB.beginStream(&stream, streamPath)) {
            Serial.println("Stream hatası: " + stream.errorReason());
        }
    }
    if (Firebase.ready()&&ai_setting){
    if (millis() - sonPeriyodikLogZamani > 30000) {
        periyodikSistemlogJsonu();
    }
    }
    if (millis() - sonsayac >= 10000) {
      if (Firebase.ready() && dataReceived) {
        Termostat();
        Serial.println("Sıcaklık ve Nem Firebase'e güncelleniyor...");
        String tempPath = "users/" + userId + "/Kontrol/termostat/mevcutderece";
        String humPath = "users/" + userId + "/Kontrol/termostat/nem";
        String kombidurumPath = "users/" + userId + "/Kontrol/termostat/kombiDurum";
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
        if (Firebase.RTDB.setBool(&fbdo, kombidurumPath, kombiDurum)) {
          Serial.println("Kombi Durumu güncellendi.");
        } else {
          Serial.println("Kombi Durumu güncelleme başarısız: " + fbdo.errorReason());
        }
        dataReceived = false;
      } else {
        if (!Firebase.ready()) Serial.println("Firebase hazır olmadığı için sıcaklık/nem güncellenemedi.");
        if (!dataReceived) Serial.println("BLE'den sıcaklık/nem verisi alınamadığı için güncellenemedi.");
      }
      sonsayac = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void GetFirebaseInfo() {
  preferences.begin("config", true);
  Email = preferences.getString("email");
  Password = preferences.getString("password");
  userId = preferences.getString("uid");
  preferences.end();
}
void saveConfig(String email, String password, String uid) {
  preferences.begin("config", false);

  preferences.putString("email",email);
  preferences.putString("password", password);
  preferences.putString("uid", uid);

  preferences.end();

  Serial.println("CONFIG KAYDEDILDI:");
  Serial.println(email);
  Serial.println(uid);
}
/*
void WiFiManagerCustom() {
  WiFiManager wm;
  WiFiManagerParameter custom_firebaseemail_setting(email_setting", "Firebaseemail_setting", "", 64);
  WiFiManagerParameter custom_firebase_password("password", "Firebase Password", "", 64, "type=\"password\"");
  WiFiManagerParameter custom_userId("uid", "User UID", "", 128);

  wm.addParameter(&custom_firebaseemail_setting);
  wm.addParameter(&custom_firebase_password);
  wm.addParameter(&custom_userId);

  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(1, 1, 1, 1);
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

  Serial.println("WiFi bağlantısı başarılı.");
  Serial.println(WiFi.localIP());
  Serial.printlnemail_setting);
  Serial.println(Password);
  Serial.println(userId);

  /*wm.setSaveConfigCallback([&]() {
  Serial.println("CONFIG CALLBACK TETIKLENDI");
  neemail_setting = custom_firebaseemail_setting.getValue();
  newPassword = custom_firebase_password.getValue();
  newUid = custom_userId.getValue();

  bool changes = false;
  if (neemail_setting !=email_setting || newPassword != Password || newUid != userId) {
    preferences.begin("config", false);
    if (neemail_setting !=email_setting) {
      preferences.putString(email_setting", neemail_setting);
      Serial.println("Yeniemail_setting kaydedildi.");
      changes = true;
    }
    if (newPassword != Password) {
      preferences.putString("password", newPassword);
      Serial.println("Yeni şifre kaydedildi.");
      changes = true;
    }
    if (newUid != userId) {
      preferences.putString("uid", newUid);
      Serial.println("Yeni UID kaydedildi.");
      changes = true;
    }
    preferences.end();
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Kullanici bilgileri güncellendi.");
    display.display();
  }
 
  if (!changes) {
    Serial.println("ℹ Veriler güncel, değişiklik yapılmadı.");
  }
}
}
*/
void WiFiManagerCustom() {
  WiFiManager wm;
  /*
  preferences.begin("config", true); 
  String kayitlemail_setting = preferences.getString(email_setting", "");
  String kayitliPass = preferences.getString("password", "");
  String kayitliUid = preferences.getString("uid", "");
  preferences.end();
  */

  WiFiManagerParameter custom_email("email", "Firebase Email",Email.c_str(), 64);
  WiFiManagerParameter custom_pass("password", "Firebase Password", Password.c_str(), 64);
  WiFiManagerParameter custom_userId("uid", "User UID", userId.c_str(), 128);

  wm.addParameter(&custom_email);
  wm.addParameter(&custom_pass);
  wm.addParameter(&custom_userId);

  wm.setSaveConfigCallback(saveConfigCallback);

  wm.setConnectTimeout(20); 
  wm.setConfigPortalTimeout(60); 
  wm.setBreakAfterConfig(true); 
  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(1, 1, 1, 1);
  WiFi.setDNS(dns1, dns2);

  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WIFI BAGLANAMADI!");
    display.println("AG: ESP32_Termostat");
    display.println("SIFRE: 12345678");
    display.println("IP: 192.168.4.1");
    display.display();
  });
  if (!wm.autoConnect("ESP32_Termostat", "12345678")) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Ayari Yok!");
    display.println("Offline Mod");
    display.println("Aktiflesiyor...");
    display.display();
    delay(2000);
  } else {
    Serial.println("WiFi bağlı!");
    Serial.println(WiFi.localIP());
  }
  newemail = custom_email.getValue();
  newPassword = custom_pass.getValue();
  newUid = custom_userId.getValue();

  bool changes = false;
  if (newemail !=Email || newPassword != Password || newUid != userId) {
    preferences.begin("config", false);
    if (newemail !=Email) {
      preferences.putString("email", newemail);
      Serial.println("Yeni email kaydedildi.");
      changes = true;
    }
    if (newPassword != Password) {
      preferences.putString("password", newPassword);
      Serial.println("Yeni şifre kaydedildi.");
      changes = true;
    }
    if (newUid != userId) {
      preferences.putString("uid", newUid);
      Serial.println("Yeni UID kaydedildi.");
      changes = true;
    }
    preferences.end();
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Kullanici bilgileri güncellendi.");
    display.display();
  }
 
  if (!changes) {
    Serial.println("ℹ Veriler güncel, değişiklik yapılmadı.");
  }
  if (WiFi.status() == WL_CONNECTED) {
  wifiConnected = true;
  }
  else{wifiConnected = false;}
   //wm.resetSettings();
  /*
  String gelenUid = String(custom_uid.getValue());
  
  if (shouldSaveConfig || (gelenUid.length() > 5 && gelenUid != kayitliUid)) {
    
    String haemail_setting = String(customemail_setting.getValue());
    String yeniPass = String(custom_pass.getValue());
    String yeniUid  = String(custom_uid.getValue());

    // %40'ı @ işaretine çevir
    String yenemail_setting = urlDecode(haemail_setting);

    Serial.println("Gelen Hamemail_setting: " + haemail_setting);
    Serial.println("Duzeltilmisemail_setting: " + yenemail_setting);
    Serial.println("Gelen UID: " + yeniUid);

    if(yeniUid.length() > 0) {
      preferences.begin("config", false);
      
      preferences.putString(email_setting", yenemail_setting);
      preferences.putString("password", yeniPass);
      preferences.putString("uid", yeniUid);
      delay(100); 
      preferences.end();
      
      Serial.println("✅ HAFIZAYA YAZILDI!");
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("Ayarlar Kaydedildi!");
      display.println("Yeniden Baslatiliyor");
      display.display();
      
      delay(2000); 
      ESP.restart(); 
    } else {
      Serial.println("❌ HATA: UID boş geldi, kaydedilmedi.");
    }
  } 
  else {
    Serial.println("ℹ Yeni ayar girilmedi veya Save tuşuna basılmadı.");
  }*/
}
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 22, -1);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  for (int i = 0; i < 5; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }
  digitalWrite(relayPins[4], HIGH);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Sistem Baslatiliyor...");
  display.display();

  GetFirebaseInfo();
  Serial.println("PREFERENCES OKUNDU:");
  Serial.println(Email);
  Serial.println(Password);
  Serial.println(userId);
  WiFiManagerCustom();
  WiFi.setSleep(false);
  delay(5000);
  if (userId.length() > 5 &&Email.length() > 5) {
     Serial.println("Bilgiler mevcut, Firebase Gorevi baslatiliyor...");
     xTaskCreatePinnedToCore(firebaseTask, "FirebaseTask", 50000, NULL, 1, NULL, 1);
  }
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  
}

void loop() {
  if (bildirimGosteriliyor && (millis() - sonsayac >= BILDIRIM_SURESI)) {
    bildirimGosteriliyor = false;
    
  }
  
  if (!bildirimGosteriliyor && (millis() - sonEkranGuncelleme >= 1000)) {
    durumsorgula();
    sonEkranGuncelleme = millis();
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
  delay(10);
}