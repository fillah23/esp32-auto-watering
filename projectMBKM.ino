#define BLYNK_TEMPLATE_ID "TMPL6E-LmSuOP"
#define BLYNK_TEMPLATE_NAME "penyiramanIOT"
#define BLYNK_AUTH_TOKEN "gRRmgZMZ4OJwUckS0oKJwcvGXYud1Ha3"
#define DHT_SENSOR_PIN 25
#define DHT_SENSOR_TYPE DHT11
#define ledGreen 21
#define ledRed 22
#define relay 4
#define soilMoisture 34
#define LED_ESP32 2

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <time.h>

//set waktu pengiriman data ke server
unsigned long previousMillis = 0;
const long interval = 20000; // 1 minute in milliseconds
char server[64] = "";  // Buffer untuk menyimpan IP server
int port = 0;          // Variabel untuk menyimpan port
const char* path = "/api/blynk-data";

bool relayState = false;             // Kondisi Relay
bool ManualButtonCondition = false;  //Kondisi Manual Pompa
bool AutoButtonCondition = false;    //Kondisi Manual Pompa

int moistureLevelLo = 0;  //Kondisi terendah kelembapan
int moistureLevelHi = 0;  //Kondisi tertinggi kelembapan

char ssid[64];  // Menyimpan SSID yang diperoleh dari WiFi Manager
char pass[64];  // Menyimpan password yang diperoleh dari WiFi Manager

float lastSoilMoisture = 0.0;

//variable dateTIme
char data_waktu[50];
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 21600;
const int   daylightOffset_sec = 3600;
//kalibrasi soil
float soilMoistureValue = analogRead(soilMoisture);
float linearA = -0.01597; 
float linearB = 41.0936;
// Menghitung nilai kalibrasi
float hitung = (linearA * soilMoistureValue) + linearB;


static unsigned long lastTurnOnTime = 0; // Waktu terakhir sistem dihidupkan
static unsigned long lastTurnOffTime = 0; // Waktu terakhir sistem dimatikan
static unsigned long lastResetTime = 0;

//Mengambil waktu sejak device menyala
unsigned long currentTime = millis();



void sendSensorData(float soil) {
  // Create an HTTP client object
  WiFiClient client;
  
  // Construct the URL
  String url = "http://" + String(server) + ":" + String(port) + String(path);
  url += "?soil=" + String(soil);

  // Send the GET request to the server
  if (client.connect(server, port)) {
    Serial.println("Sending data to server");
    client.println("GET " + url + " HTTP/1.1");
    client.println("Host: " + String(server));
    client.println("Connection: close");
    client.println();
    delay(10);
    client.stop();
  } else {
    Serial.println("Failed to connect to server");
  }
}


bool isFirstConnect = true;

BLYNK_CONNECTED() {
  if (isFirstConnect) {
    Blynk.syncAll();
    isFirstConnect = false;
  }
}


void sendSoilMoistureData() {

  if (hitung != lastSoilMoisture) {
    lastSoilMoisture = hitung;

    Serial.print("Soil Moisture: ");
    Serial.println(hitung);

    Blynk.virtualWrite(V2, hitung);
    Serial.print("Soil analog: ");
    Serial.println(soilMoistureValue);

    Blynk.virtualWrite(V1, soilMoistureValue);
    

    Serial.println(" Soil Moisture data sent to Blynk");
  }
}


BLYNK_WRITE(V3) {
  relayState = param.asInt() == 1;
  if (relayState == 1) {
    digitalWrite(relay, HIGH);
    ManualButtonCondition = true;
    Blynk.virtualWrite(V8, data_waktu);
  } else {
    digitalWrite(relay, LOW);
    ManualButtonCondition = false;
  }
}

BLYNK_WRITE(V6) {
  int v = param.asInt();
  if (v == 1) {
    AutoButtonCondition = true;
    lastTurnOffTime = currentTime;
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
BLYNK_WRITE(V9) {
  String v = param.asStr();  // Baca nilai sebagai string dari Blynk
  v.toCharArray(server, sizeof(server));  // Salin string ke buffer 'server'
  Serial.print("Server IP updated to: ");
  Serial.println(server);
}

BLYNK_WRITE(V8) {
  int v = param.asInt();  // Baca nilai sebagai integer dari Blynk
  port = v;               // Simpan nilai port
  Serial.print("Port updated to: ");
  Serial.println(port);
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

  //Mengambil waktu sejak 
  unsigned long elapsedTime = currentTime - lastTurnOffTime; // Hitung waktu sejak dimatikan

  Serial.printf("Current Time: %3d, elapsedTime: %3d, lastTurnOffTime: %3d", currentTime, elapsedTime, lastTurnOffTime);
  if (ManualButtonCondition) {
    Serial.println("Manual Pompa Dinyalakan");
  } else {
    // Nyalakan pompa jika nilai sensor berubah
    if (hitung < moistureLevelLo && !relayState) {
      digitalWrite(relay, HIGH);
      relayState = true;
      Serial.println("Relay dinyalakan karena nilai sensor rendah.");
      Blynk.virtualWrite(V0, data_waktu);
      Blynk.virtualWrite(V7, "Pompa Hidup");

      float soilMoistureValue = hitung;
      sendSensorData(soilMoistureValue);
      lastTurnOnTime = currentTime; // Catat waktu terakhir sistem dihidupkan
    }

    // Matikan pompa jika nilai sensor telah mencapai ambang batas atas
    if (hitung >= moistureLevelHi && relayState) {
      digitalWrite(relay, LOW);
      relayState = false;
      Serial.println("Relay dimatikan karena nilai sensor telah mencapai ambang batas atas.");
      Blynk.virtualWrite(V7, "Penyiraman Berhasil");
      lastTurnOffTime = currentTime; // Catat waktu terakhir sistem dimatikan
    }

    // Cek apakah harus dimatikan selama 1 menit
    if (hitung < moistureLevelHi && elapsedTime >= 60000) {
      digitalWrite(relay, LOW);
      relayState = false;
      Blynk.virtualWrite(V7, "penyiraman gagal");
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
void blinkLED(void *parameter) {
  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      digitalWrite(LED_ESP32, HIGH);
      vTaskDelay(500 / portTICK_PERIOD_MS);  // Tunggu 500ms
      digitalWrite(LED_ESP32, LOW);
      vTaskDelay(500 / portTICK_PERIOD_MS);  // Tunggu 500ms
    } else {
      digitalWrite(LED_ESP32, HIGH);  // Jika tidak terhubung, LED tetap menyala
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

void setup() {
  WiFi.mode(WIFI_STA);
  Serial.begin(9600);
  pinMode(ledGreen, OUTPUT);
  pinMode(ledRed, OUTPUT);
  pinMode(relay, OUTPUT);
  pinMode(LED_ESP32,OUTPUT);

  digitalWrite(relay, LOW);
  
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
    // digitalWrite(LED_ESP32, LOW);
  } else {
    // Jika Anda terhubung, inisialisasikan Blynk
    Serial.println("Connected...yeey :)");
    // digitalWrite(LED_ESP32, HIGH);
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  }
  xTaskCreatePinnedToCore(
    blinkLED,   // Nama fungsi
    "BlinkTask", // Nama Task
    1000,        // Stack size
    NULL,        // Parameter
    1,           // Prioritas
    NULL,        // Handle
    0            // Core (gunakan core 0)
  );
  //upload sketch tanpa kabel
  uploadWireless();

  //dateTime
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getDateTime();
}

void loop() {
  ArduinoOTA.handle();
  Blynk.run();
  getDateTime();
  
  sendSoilMoistureData();

  
  //penyiraman otomatis
  if (AutoButtonCondition) {
    controlRelayBasedOnSoilMoisture();
  }else if (ManualButtonCondition) {
    digitalWrite(relay, HIGH);
    relayState = true;
  }else{
    digitalWrite(relay, LOW);
    relayState = false;
    Blynk.virtualWrite(V7, "---");
  }

  int value = analogRead(soilMoisture);
  Serial.println(hitung);
  Serial.println("Relay: " + (String)(relayState ? "ON" : "OFF"));
  Serial.println("Moisture Lo: " + (String)moistureLevelLo);
  Serial.println("Moisture Hi: " + (String)moistureLevelHi);
  unsigned long currentMillis = millis();
  
  //kalibrasi soil
  soilMoistureValue = analogRead(soilMoisture);
  hitung = (linearA * soilMoistureValue) + linearB;

  delay(2500);

  currentTime = millis();
}
