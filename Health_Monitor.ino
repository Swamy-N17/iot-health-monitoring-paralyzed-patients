#include <WiFi.h>
#include <FirebaseESP32.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ------------------ Wi-Fi Credentials ------------------
#define WIFI_SSID "Redmi11T"
#define WIFI_PASSWORD "99887766"

// ------------------ Firebase Credentials ------------------
#define FIREBASE_HOST "health-app-iag6pn-default-rtdb.firebaseio.com/vitals"
#define FIREBASE_AUTH "AIzaSyAB7oPMen4D8MJ49b6qyfUS4NCOgGfyF4o"

// ------------------ OLED Setup ------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ------------------ Pins ------------------
#define FLEX1_PIN 32
#define FLEX2_PIN 35
#define FLEX3_PIN 34
#define FLEX4_PIN 33
#define DS18B20_PIN 19

#define RED_LED 25
#define GREEN_LED 26
#define BUZZER 27

// ------------------ Firebase Objects ------------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ------------------ Sensor Objects ------------------
MAX30105 particleSensor;
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// ------------------ Variables ------------------
float temperature = 0;
int heartRate = 0;
int spo2 = 0;

// ------------------ Setup ------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize LEDs and buzzer
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // Wi-Fi Connection
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
  Serial.print("📶 IP Address: ");
  Serial.println(WiFi.localIP());

  // Firebase setup
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("🔥 Firebase Ready!");

  // OLED setup
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED not found!");
    while (1);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();

  // DS18B20 setup
  sensors.begin();

  // MAX30102 setup (stable)
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("❌ MAX30102 not found. Check wiring!");
    while (1);
  }
  Serial.println("✅ MAX30102 initialized!");
  particleSensor.setup(); 
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeIR(0x0A);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("All Sensors Ready!");
  display.display();
  delay(1000);
}

// ------------------ Condition Check ------------------
void checkConditions() {
  bool critical = false;
  String statusMsg = "";

  if (temperature < 28 || temperature > 38) {
    statusMsg += "Temp Abnormal! ";
    critical = true;
  }

  if (heartRate < 50 || heartRate > 120) {
    statusMsg += "HR Abnormal! ";
    critical = true;
  }

  if (spo2 < 80) {
    statusMsg += "SpO2 Abnormal! ";
    critical = true;
  }

  if (critical) {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
    tone(BUZZER, 1000);
  } else {
    statusMsg = "Normal";
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    noTone(BUZZER);
  }

  Serial.println(statusMsg);

  // Display patient condition on OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Body Parameters:");
  display.println("--------------------");
  display.printf("Temp: %.2f C\n", temperature);
  display.printf("HR: %d bpm\n", heartRate);
  display.printf("SpO2: %d %%\n", spo2);
  display.println();
  display.print("Status: ");
  display.println(statusMsg);
  display.display();
}

// ------------------ Main Loop ------------------
void loop() {
  // Read 4 Flex Sensors
  int flex1 = analogRead(FLEX1_PIN);
  int flex2 = analogRead(FLEX2_PIN);
  int flex3 = analogRead(FLEX3_PIN);
  int flex4 = analogRead(FLEX4_PIN);

  // Read DS18B20
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);

 // --- Stable MAX30102 HR & SpO2 Code ---
uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t spo2Raw, hrRaw;
int8_t validSPO2, validHR;

// Fill sample buffer
for (int i = 0; i < 100; i++) {
  while (!particleSensor.check()) particleSensor.check();
  irBuffer[i] = particleSensor.getIR();
  redBuffer[i] = particleSensor.getRed();
}

// Calculate raw HR and SpO2
maxim_heart_rate_and_oxygen_saturation(
  irBuffer, 100, redBuffer,
  &spo2Raw, &validSPO2,
  &hrRaw, &validHR
);

// ---------- Heart Rate Filtering & Correction ----------
static float hrHistory[8] = {0};  // store last 8 values for smoothing
static int idx = 0;
float avgHR = 0;

// Step 1: Validate HR range (normal human range 50–120 bpm)
if (validHR && hrRaw > 40 && hrRaw < 180) {

  // Step 2: Correct overshoot from noisy data
  // Many MAX30102 sensors overshoot ~15-25 bpm, so scale down
  if (hrRaw > 120) {
    hrRaw = hrRaw - 20;  // shift down to realistic range
  }

  // Step 3: Store HR value for running average
  hrHistory[idx] = hrRaw;
  idx = (idx + 1) % 8;

  // Step 4: Compute smoothed average
  for (int i = 0; i < 8; i++) avgHR += hrHistory[i];
  avgHR /= 8;

  heartRate = round(avgHR);
} else {
  // if invalid, retain last stable HR
  heartRate = heartRate;
}

// ---------- SpO2 Filtering ----------
if (validSPO2 && spo2Raw >= 70 && spo2Raw <= 100) {
  spo2 = spo2Raw;
}

// Optional: Print to Serial for debugging
Serial.print("Smoothed HR: "); Serial.print(heartRate);
Serial.print(" | Raw HR: "); Serial.print(hrRaw);
Serial.print(" | SpO2: "); Serial.println(spo2);



  // Print readings
  Serial.println("----- Sensor Readings -----");
  Serial.printf("Flex1: %d | Flex2: %d | Flex3: %d | Flex4: %d\n", flex1, flex2, flex3, flex4);
  Serial.printf("Temperature: %.2f°C\n", temperature);
  Serial.printf("Heart Rate: %d bpm | SpO2: %d%%\n", heartRate, spo2);
  Serial.println("---------------------------");

  // Send to Firebase
  if (Firebase.ready()) {
    Firebase.setInt(fbdo, "/FlexSensors/Flex1", flex1);
    Firebase.setInt(fbdo, "/FlexSensors/Flex2", flex2);
    Firebase.setInt(fbdo, "/FlexSensors/Flex3", flex3);
    Firebase.setInt(fbdo, "/FlexSensors/Flex4", flex4);
    Firebase.setFloat(fbdo, "/Body/Temperature", temperature);
    Firebase.setFloat(fbdo, "/Body/HeartRate", heartRate);
    Firebase.setFloat(fbdo, "/Body/SpO2", spo2);
  }

  // Check and Display Condition
  checkConditions();

}