#include <WiFiManager.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoOTA.h>
#include <TimeLib.h>

// Definisikan kredensial Firebase
#define FIREBASE_HOST "https://smart-garden-de8cb-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "AIzaSyB7Qqag_YkLcXvFT137yzmKFXL0tu4K034"

// Inisialisasi Firebase
FirebaseData firebaseData;

const int soilMoisturePins[] = {34, 35, 36};  // Pin untuk sensor kelembaban tanah
const int relayPin = 27;                      // Pin untuk mengendalikan relay
const int DHTPIN = 32;                        // Pin untuk sensor DHT11
const int LDRPIN = 39;                        // Pin untuk sensor LDR
const int wifiLedPin = 2;                     // Pin untuk indikator LED WiFi

// Konfigurasi sensor DHT11
#define DHTTYPE DHT11   // Jenis sensor DHT11 (DHT11/DHT22/AM2302)
DHT dht(DHTPIN, DHTTYPE);

// Inisialisasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Ganti alamat jika perlu

struct Schedule {
  int day;
  int month;
  int year;
  int hour;
  int minute;
  int duration;
};

Schedule schedule;
String schedulePath = "/coordinates/-O-pQm2xT5sz9VCLqyVB/schedule";

// List of paths to send sensor data to
String sensorDataPaths[] = {
  "/coordinates/-O-pQm2xT5sz9VCLqyVB",
  "/coordinates/-O-pVM2HHnyoXBl7a76R",
  "/coordinates/-O-pYTzf2uHRX96gHMt9",
  "/coordinates/-O-p_V19qSighqbzDGH3",
  "/coordinates/-O-pa4sAoqaLUM6ibPhz"
};

void setup() {
  Serial.begin(115200);

  pinMode(wifiLedPin, OUTPUT);
  digitalWrite(wifiLedPin, LOW);  // Matikan LED WiFi pada awal

  // Inisialisasi WiFiManager
  WiFiManager wm;
  // Remove the resetSettings call to prevent resetting WiFi settings each time
  // wm.resetSettings();
  if (!wm.autoConnect("SmartGarden", "smartgarden")) {
    Serial.println("Failed to connect to WiFi");
    digitalWrite(wifiLedPin, LOW);  // Matikan LED WiFi jika gagal terhubung
    delay(3000);
    // Reset board jika gagal terhubung
    ESP.restart();
  }
  Serial.println("Connected to WiFi");
  digitalWrite(wifiLedPin, HIGH);  // Nyalakan LED WiFi jika terhubung

  // Inisialisasi ArduinoOTA
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");

  // Sambungkan ke Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);  // Matikan pompa air pada awal

  // Inisialisasi sensor DHT11
  dht.begin();

  // Inisialisasi LCD
  lcd.begin();  // Menggunakan init() alih-alih begin()
  lcd.backlight();
  lcd.clear();
  
  // Menampilkan tulisan "UM HIJAU" pada baris pertama LCD
  lcd.setCursor(0, 0);
  lcd.print("UM HIJAU");

  lcd.setCursor(0, 1);
  lcd.print("UM HIJAU");
  delay(2000);  // Tampilkan selama 2 detik

  // Menampilkan label pada baris pertama LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Temp  Hum  Soil");

  // Inisialisasi waktu
  configTime(0, 0, "pool.ntp.org");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  Serial.println("Time synchronized");

  // Ambil path jadwal dari Firebase
  loadSchedule();
  Firebase.setStreamCallback(firebaseData, scheduleCallback, scheduleStreamTimeout);
  Firebase.beginStream(firebaseData, schedulePath);
}

void loop() {
  static bool showPumpStatus = false;
  static unsigned long lastSwitchTime = 0;
  unsigned long currentTime = millis();

  ArduinoOTA.handle();  // Tambahkan ini untuk menangani OTA

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.reconnect();
    delay(10000);
    return;
  }

  int totalSoilMoistureValue = 0;
  int soilMoistureValues[3];
  for (int i = 0; i < 3; i++) {
    soilMoistureValues[i] = analogRead(soilMoisturePins[i]);
    totalSoilMoistureValue += soilMoistureValues[i];
  }
  int averageSoilMoistureValue = totalSoilMoistureValue / 3;
  float soilMoisturePercentage = map(averageSoilMoistureValue, 4095, 0, 0, 100);
  soilMoisturePercentage += 30; // Tambahkan 30 persen

  // Baca suhu dan kelembaban dari sensor DHT11
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  // Baca nilai LDR
  int ldrValue = analogRead(LDRPIN);

  // Kirim data ke Firebase
  bool dataSent = true;
  for (String path : sensorDataPaths) {
    if (!Firebase.setInt(firebaseData, path + "/soilMoisture1", soilMoistureValues[0])) {
      dataSent = false;
    }
    if (!Firebase.setInt(firebaseData, path + "/soilMoisture2", soilMoistureValues[1])) {
      dataSent = false;
    }
    if (!Firebase.setInt(firebaseData, path + "/soilMoisture3", soilMoistureValues[2])) {
      dataSent = false;
    }
    if (!Firebase.setFloat(firebaseData, path + "/averageSoilMoisture", soilMoisturePercentage)) {
      dataSent = false;
    }
    if (!Firebase.setFloat(firebaseData, path + "/humidity", humidity)) {
      dataSent = false;
    }
    if (!Firebase.setFloat(firebaseData, path + "/temperature", temperature)) {
      dataSent = false;
    }
    if (!Firebase.setInt(firebaseData, path + "/ldrValue", ldrValue)) {
      dataSent = false;
    }
  }
  if (dataSent) {
    Serial.println("Data sent to Firebase successfully");
  }

  // Kontrol pompa air berdasarkan rata-rata kelembaban tanah dan nilai LDR
  bool pumpActive = false;
  if (averageSoilMoistureValue > 3200) {  // Sesuaikan nilai threshold sesuai kebutuhan
    digitalWrite(relayPin, LOW);  // Nyalakan pompa air
    pumpActive = true;
    Firebase.setBool(firebaseData, "/pumpActive", true);
  } else {
    digitalWrite(relayPin, HIGH);  // Matikan pompa air
    pumpActive = false;
    Firebase.setBool(firebaseData, "/pumpActive", false);
  }

  // Cek jadwal dan nyalakan pompa jika sesuai jadwal
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);

  if (p_tm->tm_year + 1900 == schedule.year && p_tm->tm_mon + 1 == schedule.month && p_tm->tm_mday == schedule.day &&
      p_tm->tm_hour == schedule.hour && p_tm->tm_min == schedule.minute && p_tm->tm_sec == 0) {
    Serial.println("Jadwal penyiraman dimulai");
    digitalWrite(relayPin, LOW);  // Nyalakan pompa air
    delay(schedule.duration * 1000);  // Penyiraman selama durasi yang ditentukan (dalam detik)
    digitalWrite(relayPin, HIGH);  // Matikan pompa air
    delay(1000);  // Tunggu 1 detik sebelum pengecekan ulang
  }

  // Tampilkan data di LCD secara bergantian
  if (currentTime - lastSwitchTime >= (showPumpStatus ? 4000 : 2000)) { // Menampilkan data sensor selama 4 detik dan status pompa selama 2 detik
    lcd.clear();
    if (showPumpStatus) {
      if (pumpActive) {
        lcd.setCursor(0, 0);
        lcd.print("SEDANG  MENYIRAM");
      } else {
        lcd.setCursor(0, 0);
        lcd.print(" POMPA AIR MATI ");
      }
      showPumpStatus = false;
    } else {
      lcd.setCursor(0, 0);
      lcd.print(" Temp  Hum  Soil");
      lcd.setCursor(0, 1);
      lcd.printf("%4.1fC %3.0f%% %4.0f%%", temperature, humidity, soilMoisturePercentage);
      showPumpStatus = true;
    }
    lastSwitchTime = currentTime;
  }

  delay(1000);  // Kurangi delay agar responsif, baca dan kirim data lebih sering
}

void loadSchedule() {
  if (Firebase.getJSON(firebaseData, schedulePath.c_str())) {
    FirebaseJson& json = firebaseData.jsonObject();
    FirebaseJsonData result;

    json.get(result, "year");
    schedule.year = result.intValue;

    json.get(result, "month");
    schedule.month = result.intValue;

    json.get(result, "day");
    schedule.day = result.intValue;

    json.get(result, "hour");
    schedule.hour = result.intValue;

    json.get(result, "minute");
    schedule.minute = result.intValue;

    json.get(result, "duration");
    schedule.duration = result.intValue;

    Serial.printf("Jadwal penyiraman: %02d-%02d-%04d %02d:%02d selama %d detik\n", schedule.day, schedule.month, schedule.year, schedule.hour, schedule.minute, schedule.duration);
  } else {
    Serial.println("Failed to get schedule from Firebase");
    Serial.println(firebaseData.errorReason());
  }
}

void scheduleCallback(StreamData data) {
  Serial.println("Schedule updated");
  loadSchedule();
}

void scheduleStreamTimeout(bool timeout) {
  if (timeout) {
    Serial.println("Stream timeout, resuming...");
    Firebase.beginStream(firebaseData, schedulePath.c_str());
  }
}