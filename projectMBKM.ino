#define BLYNK_TEMPLATE_ID "TMPL6E-LmSuOP"
#define BLYNK_TEMPLATE_NAME "penyiramanIOT"
#define BLYNK_AUTH_TOKEN "gRRmgZMZ4OJwUckS0oKJwcvGXYud1Ha3"
#define DHT_SENSOR_PIN 23
#define DHT_SENSOR_TYPE DHT11
#define ledGreen 21
#define ledRed 22
#define relay 4
#define soilMoisture 34

#include <DHT.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <time.h>

DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);
//firebase
const String USER_EMAIL = "fillahseptian7l24@gmail.com";
const String USER_PASSWORD = "123456";
const String API_KEY = "AIzaSyDJn0WTx2a1VwCX0zUCpNC-G0oh7COVy80";  // Ganti dengan API Key Firebase Anda
const String DATABASE_URL = "https://sensor-suhu-dht11-56540-default-rtdb.firebaseio.com/";  // Ganti dengan URL Database Firebase Anda
String uid;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json;

//set waktu pengiriman data ke firebase
unsigned long previousMillis = 0;
const unsigned long interval = 10000;


bool relayState = false;             // Kondisi Relay
bool ManualButtonCondition = false;  //Kondisi Manual Pompa
bool AutoButtonCondition = false;    //Kondisi Manual Pompa

int moistureLevelLo = 0;  //Kondisi terendah kelembapan
int moistureLevelHi = 0;  //Kondisi tertinggi kelembapan

char ssid[64];  // Menyimpan SSID yang diperoleh dari WiFi Manager
char pass[64];  // Menyimpan password yang diperoleh dari WiFi Manager

float lastTempC = 0.0;
float lastHumidity = 0.0;
float lastSoilMoisture = 0.0;

//variable dateTIme
char data_waktu[50];
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 21600;
const int   daylightOffset_sec = 3600;
//kalibrasi soil
float soilMoistureValue = analogRead(soilMoisture);
float linearA = 1.71307697010454E+01;
float linearB = -6.65566220374560E-03;
float hitung = linearA + (linearB * soilMoistureValue);
void setup() {
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  dht_sensor.begin();
  pinMode(ledGreen, OUTPUT);
  pinMode(ledRed, OUTPUT);
  pinMode(relay, OUTPUT);

  digitalWrite(relay, HIGH);
  
  // Inisialisasi WiFi Manager
  bool res;
  WiFiManager wifiManager;
  res = wifiManager.autoConnect("AutoConnectAP", "password");
  // Menyimpan SSID dan password yang diperoleh dari WiFi Manager
  strncpy(ssid, WiFi.SSID().c_str(), sizeof(ssid));
  strncpy(pass, WiFi.psk().c_str(), sizeof(pass));
  if (!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  } else {
    // Jika Anda terhubung, inisialisasikan Blynk
    Serial.println("Connected...yeey :)");
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  }

  //upload sketch tanpa kabel
  uploadWireless();

  //firebase
  config.database_url = DATABASE_URL;
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;
  Firebase.begin(&config, &auth);

  Serial.println("Getting User ID...");
  while (auth.token.uid == "") {
    Serial.print(".");
    Serial.print(uid);
  }
  uid = auth.token.uid.c_str();
  Serial.println("User Id : ");
  Serial.println(uid);
  //dateTime
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getDateTime();
}

void loop() {
  ArduinoOTA.handle();
  Blynk.run();
  getDateTime();
  
  sendTemperatureData();
  sendHumidityData();
  sendSoilMoistureData();
  //penyiraman otomatis
  if (AutoButtonCondition) {
    controlRelayBasedOnSoilMoisture();
  }else if (ManualButtonCondition) {
    
  } else {
    digitalWrite(relay, HIGH);
    relayState = false;
    Blynk.virtualWrite(V7, "---");
  }

  int value = analogRead(soilMoisture);
  Serial.println(hitung);
  Serial.println("Relay: " + (String)(relayState ? "ON" : "OFF"));
  Serial.println("Moisture Lo: " + (String)moistureLevelLo);
  Serial.println("Moisture Hi: " + (String)moistureLevelHi);
  unsigned long currentMillis = millis();
  //mengirim data ke firebase dengan batasan waktu
  if (currentMillis - previousMillis >= interval) {
    // sendSensorDataToFirebase();
    previousMillis = currentMillis;
  }
  //kalibrasi soil
  soilMoistureValue = analogRead(soilMoisture);
  hitung = linearA + (linearB * soilMoistureValue);
  delay(2500);
}

void sendSensorDataToFirebase() {
  float tempC = dht_sensor.readTemperature();
  float humi = dht_sensor.readHumidity();
  float soilMoistureValue = hitung;

  if (!isnan(tempC) && !isnan(humi) ) {
    // Periksa apakah ada perubahan data sejak yang terakhir dikirim
    if (tempC != lastTempC || humi != lastHumidity || soilMoistureValue != lastSoilMoisture) {
      // Dapatkan timestamp saat ini
      unsigned long timestamp = getTime();

      // Buat path yang rapi dalam Firebase
      String path = "/environment/" + String(timestamp);

      // Kirim data ke Firebase dengan timestamp
      json.set("temperature", tempC);
      json.set("humidity",humi);
      json.set("soil_moisture", soilMoistureValue);
      json.set("createdAt", timestamp);
      Serial.print("Set Json...");
      Serial.print(Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json) ? "Sukses " : fbdo.errorReason().c_str());
      Serial.println(timestamp);

      // Update nilai-nilai terakhir
      lastTempC = tempC;
      lastHumidity = humi;
      lastSoilMoisture = soilMoistureValue;

      Serial.println("Sensor data sent to Firebase with timestamp: " + String(timestamp));
    }
  }
}

unsigned long getTime() {
  time_t now;
  configTime(0, 0, "pool.ntp.org");
  Serial.print("Getting time");

  while (now < (24 * 3600)) {
    Serial.print(".");
    now = time(nullptr);
    delay(1000);
  }
  return now;

}

bool isFirstConnect = true;

BLYNK_CONNECTED() {
  if (isFirstConnect) {
    Blynk.syncAll();
    isFirstConnect = false;
  }
}

void sendTemperatureData() {
  float tempC = dht_sensor.readTemperature();

  if (isnan(tempC)) {
    Serial.println("Failed to read temperature from DHT sensor!");
    return;
  }

  if (tempC != lastTempC) {
    lastTempC = tempC;

    Serial.print("Temperature: ");
    Serial.print(tempC);
    Serial.print("°C");

    Blynk.virtualWrite(V0, tempC);

    Serial.println(" Temperature data sent to Blynk");
  }
}

void sendHumidityData() {
  float humi = dht_sensor.readHumidity();

  if (isnan(humi)) {
    Serial.println("Failed to read humidity from DHT sensor!");
    digitalWrite(ledRed, HIGH);
    digitalWrite(ledGreen, LOW);
    return;
  }

  if (humi != lastHumidity) {
    lastHumidity = humi;

    Serial.print("Humidity: ");
    Serial.print(humi);
    Serial.print("%");

    Blynk.virtualWrite(V1, humi);
    digitalWrite(ledGreen, HIGH);
    digitalWrite(ledRed, LOW);

    Serial.println(" Humidity data sent to Blynk");
  }
}

void sendSoilMoistureData() {

  if (hitung != lastSoilMoisture) {
    lastSoilMoisture = hitung;

    Serial.print("Soil Moisture: ");
    Serial.println(hitung);

    Blynk.virtualWrite(V2, hitung);
    

    Serial.println(" Soil Moisture data sent to Blynk");
  }
}


BLYNK_WRITE(V3) {
  relayState = param.asInt() == 1;
  if (relayState == 1) {
    digitalWrite(relay, LOW);
    ManualButtonCondition = true;
    Blynk.virtualWrite(V8, data_waktu);
  } else {
    digitalWrite(relay, HIGH);
    ManualButtonCondition = false;
  }
}

BLYNK_WRITE(V6) {
  int v = param.asInt();
  if (v == 1) {
    AutoButtonCondition = true;
  } else {
    AutoButtonCondition = false;
  }
}

//otomasi pompa
//batas bawah
BLYNK_WRITE(V5) {
  int v = param.asInt();
  //.........................
  moistureLevelLo = v;
}

//batas atas
BLYNK_WRITE(V4) {
  int v = param.asInt();
  //.................
  moistureLevelHi = v;
}



void getDateTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  strftime(data_waktu, sizeof(data_waktu), "%A, %B %d %Y %H:%M:%S", &timeinfo);
}


void controlRelayBasedOnSoilMoisture() {
  static unsigned long lastTurnOnTime = 0; // Waktu terakhir sistem dihidupkan
  static unsigned long lastTurnOffTime = 0; // Waktu terakhir sistem dimatikan
  static unsigned long lastResetTime = 0;
  unsigned long currentTime = millis() - lastResetTime;;
  unsigned long elapsedTime = currentTime - lastTurnOffTime; // Hitung waktu sejak dimatikan
  Serial.print(currentTime);
  if (ManualButtonCondition) {
    Serial.println("Manual Pompa Dinyalakan");
  } else {
    // Nyalakan pompa jika nilai sensor berubah
    if (hitung < moistureLevelLo && !relayState) {
      digitalWrite(relay, LOW);
      relayState = true;
      Serial.println("Relay dinyalakan karena nilai sensor rendah.");
      Blynk.virtualWrite(V8, data_waktu);
      Blynk.virtualWrite(V7, "Pompa Hidup");
      lastTurnOnTime = currentTime; // Catat waktu terakhir sistem dihidupkan
    }

    // Matikan pompa jika nilai sensor telah mencapai ambang batas atas
    if (hitung >= moistureLevelHi && relayState) {
      digitalWrite(relay, HIGH);
      relayState = false;
      Serial.println("Relay dimatikan karena nilai sensor telah mencapai ambang batas atas.");
      Blynk.virtualWrite(V7, "Penyiraman Berhasil");
      lastTurnOffTime = currentTime; // Catat waktu terakhir sistem dimatikan
    }

    // Cek apakah harus dimatikan selama 1 menit
    if (hitung < moistureLevelHi && elapsedTime >= 60000) {
      digitalWrite(relay, HIGH);
      relayState = false;
      Blynk.virtualWrite(V7, "penyiraman gagal");
      ManualButtonCondition = true;
      Serial.println("Sistem dimatikan selama 1 menit.");
    }
  }
   // Cek apakah harus dihidupkan kembali setelah 1 jam
    if (ManualButtonCondition && currentTime - lastTurnOffTime >= 3600000) {
      Serial.println("Sistem dihidupkan kembali setelah 1 jam.");
      Blynk.virtualWrite(V7, "---");
      lastResetTime = millis();
      ManualButtonCondition = false;
    }
}


void uploadWireless() {
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else  // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}