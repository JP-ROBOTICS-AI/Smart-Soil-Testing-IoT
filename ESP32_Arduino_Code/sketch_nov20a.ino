

// ===== BLYNK CONFIGURATION =====
#define BLYNK_TEMPLATE_ID "TMPL****"  // ADD YOUR TEMPLATE ID HERE!
#define BLYNK_TEMPLATE_NAME "soil monitor"
#define BLYNK_AUTH_TOKEN "Wce1eoW8IhucQdcgPWVTVu5qhIWlXv-w"
#define BLYNK_PRINT Serial

// ===== LIBRARY INCLUDES =====
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ModbusMaster.h>
#include <DHT.h>
#include <HTTPClient.h>

// ===== WIFI CONFIGURATION =====
char ssid[] = "realme 6";
char pass[] = "12345dkb";

// ===== GOOGLE SHEETS CONFIGURATION =====
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbzwrcSTmQf0ng1Cy1YPV_atRiFrc0d5PjVvgcBOQkP0DyTabZGQVKPfB6HDwT1mGTuY/exec";
const char* secretKey = "CHANGE_ME_SECRET_12345";

// ===== PIN DEFINITIONS =====
#define MAX485_RO 16  // MAX485 RO → ESP32 GPIO16 (RX2)
#define MAX485_DI 17  // MAX485 DI → ESP32 GPIO17 (TX2)
#define MAX485_DE 25  // MAX485 DE → ESP32 GPIO25
#define MAX485_RE 26  // MAX485 RE → ESP32 GPIO26
#define DHTPIN 19     // DHT22 OUT → ESP32 GPIO19
#define DHTTYPE DHT22

// ===== SENSOR OBJECTS =====
DHT dht(DHTPIN, DHTTYPE);
ModbusMaster node;
#define RS485Serial Serial2

// ===== GLOBAL VARIABLES =====
float temperature = 0;
float humidity = 0;
int soilMoisture = 0;
int nitrogen = 0;
int phosphorus = 0;
int potassium = 0;
float pH = 0;
int ec = 0;

unsigned long lastSensorRead = 0;
unsigned long lastBlynkUpdate = 0;
unsigned long lastGoogleSheetUpdate = 0;

const unsigned long SENSOR_INTERVAL = 5000;         // Read sensors every 5 seconds
const unsigned long BLYNK_INTERVAL = 2000;          // Update Blynk every 2 seconds
const unsigned long GOOGLE_SHEET_INTERVAL = 300000; // Update Google Sheets every 5 minutes (300000ms)

// ===== MAX485 CONTROL FUNCTIONS =====
void preTransmission() {
  digitalWrite(MAX485_DE, HIGH);
  digitalWrite(MAX485_RE, HIGH);
  delayMicroseconds(100);
}

void postTransmission() {
  digitalWrite(MAX485_DE, LOW);
  digitalWrite(MAX485_RE, LOW);
  delayMicroseconds(100);
}

// ===== SETUP FUNCTION =====
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n=================================");
  Serial.println("ESP32 Soil Monitor Starting...");
  Serial.println("=================================");

  // Configure MAX485 control pins
  pinMode(MAX485_DE, OUTPUT);
  pinMode(MAX485_RE, OUTPUT);
  digitalWrite(MAX485_DE, LOW);
  digitalWrite(MAX485_RE, LOW);

  // Initialize DHT22
  dht.begin();
  Serial.println("✓ DHT22 initialized");

  // Initialize RS485 Serial
  Serial.println("Initializing RS485 (Serial2)...");
  RS485Serial.begin(4800, SERIAL_8N1, MAX485_RO, MAX485_DI);
  delay(50);

  // Setup Modbus
  node.begin(1, RS485Serial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  Serial.println("✓ MAX485 (Modbus) initialized");

  // Connect to WiFi and Blynk
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("✓ WiFi Connected!");
  Serial.println("✓ Blynk Connected!");

  Serial.println("\n=================================");
  Serial.println("Setup Complete! Starting monitoring...");
  Serial.println("Google Sheets logging every 2 minutes");
  Serial.println("=================================\n");
}

// ===== MAIN LOOP =====
void loop() {
  Blynk.run();
  unsigned long currentMillis = millis();

  // Read sensors periodically
  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;
    Serial.println("\n--- Reading Sensors ---");
    readDHT22();
    readSoilSensor();
    Serial.println("----------------------\n");
  }

  // Update Blynk periodically
  if (currentMillis - lastBlynkUpdate >= BLYNK_INTERVAL) {
    lastBlynkUpdate = currentMillis;
    updateBlynk();
  }

  // Send data to Google Sheets every 2 minutes
  if (currentMillis - lastGoogleSheetUpdate >= GOOGLE_SHEET_INTERVAL) {
    lastGoogleSheetUpdate = currentMillis;
    sendToGoogleSheets();
  }
}

// ===== READ DHT22 SENSOR =====
void readDHT22() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (isnan(h) || isnan(t)) {
    Serial.println("❌ Failed to read DHT22 sensor!");
    return;
  }
  
  temperature = t;
  humidity = h;
  
  Serial.println("DHT22 Data:");
  Serial.print("  Temperature: ");
  Serial.print(temperature, 1);
  Serial.println(" °C");
  Serial.print("  Humidity: ");
  Serial.print(humidity, 1);
  Serial.println(" %");
}

// ===== READ SOIL NPK SENSOR =====
void readSoilSensor() {
  uint8_t result = node.readHoldingRegisters(0x0000, 7);
  
  if (result == node.ku8MBSuccess) {
    soilMoisture = node.getResponseBuffer(0);
    temperature = node.getResponseBuffer(1) / 10.0;
    ec = node.getResponseBuffer(2);
    pH = node.getResponseBuffer(3) / 10.0;
    nitrogen = node.getResponseBuffer(4);
    phosphorus = node.getResponseBuffer(5);
    potassium = node.getResponseBuffer(6);
    
    Serial.println("NPK Soil Sensor Data:");
    Serial.print("  Moisture: ");
    Serial.print(soilMoisture);
    Serial.println(" %");
    Serial.print("  Nitrogen (N): ");
    Serial.print(nitrogen);
    Serial.println(" mg/kg");
    Serial.print("  Phosphorus (P): ");
    Serial.print(phosphorus);
    Serial.println(" mg/kg");
    Serial.print("  Potassium (K): ");
    Serial.print(potassium);
    Serial.println(" mg/kg");
    Serial.print("  pH: ");
    Serial.println(pH, 1);
    Serial.print("  EC: ");
    Serial.print(ec);
    Serial.println(" μS/cm");
  } else {
    Serial.print("❌ Soil sensor read FAILED! Error code: ");
    Serial.println(result);
  }
}

// ===== UPDATE BLYNK CLOUD =====
void updateBlynk() {
  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V2, soilMoisture);
  Blynk.virtualWrite(V3, nitrogen);
  Blynk.virtualWrite(V4, phosphorus);
  Blynk.virtualWrite(V5, potassium);
  Blynk.virtualWrite(V6, pH);
  Blynk.virtualWrite(V7, ec);
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("📤 Data sent to Blynk Cloud");
  } else {
    Serial.println("⚠️ WiFi not connected - cannot send to Blynk");
  }
}

// ===== SEND DATA TO GOOGLE SHEETS =====
void sendToGoogleSheets() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi not connected - cannot send to Google Sheets");
    return;
  }

  HTTPClient http;
  
  // Build URL with parameters
  String url = String(googleScriptURL) + "?secret=" + secretKey;
  url += "&temperature=" + String(temperature, 1);
  url += "&humidity=" + String(humidity, 1);
  url += "&moisture=" + String(soilMoisture);
  url += "&nitrogen=" + String(nitrogen);
  url += "&phosphorus=" + String(phosphorus);
  url += "&potassium=" + String(potassium);
  url += "&ph=" + String(pH, 1);
  url += "&ec=" + String(ec);

  Serial.println("\n📊 Sending data to Google Sheets...");
  Serial.println("URL: " + url);

  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    Serial.print("✓ HTTP Response code: ");
    Serial.println(httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Response: " + payload);
      Serial.println("✓ Data successfully logged to Google Sheets!");
    }
  } else {
    Serial.print("❌ HTTP Request failed, error: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }
  
  http.end();
}

// ===== BLYNK CONNECTED EVENT =====
BLYNK_CONNECTED() {
  Serial.println("\n✓ Successfully connected to Blynk Cloud!");
  Blynk.syncAll();
}
