#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// -----------------------------
// WiFi and Firebase credentials
// -----------------------------
#define WIFI_SSID "your wifi ssid"
#define WIFI_PASSWORD "your wifi password"
#define API_KEY "api key"
#define DATABASE_URL "database url"
#define DATABASE_SECRET "secret key"

// -----------------------------
// Firebase objects
// -----------------------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define IN1 26
#define IN2 25
#define IN3 27
#define IN4 33
#define ENA 14
#define ENB 12
#define SPEED1 170
#define SPEED2 130

#define SOIL_PIN 32

int p1state = 0, p2state = 0;

// Function declarations
void pumpOn(uint8_t pump_no);
void pumpOff(uint8_t pump_no);
void toggle(uint8_t pump_no);
void soilMoisture();
void pumpAuto();

void setup() {
  Serial.begin(115200);

  // --- Wi-Fi connection ---
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to Wi-Fi");

  // --- Firebase setup ---
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("✅ Connected to Firebase");

  Firebase.RTDB.setInt(&fbdo, "/ESP_32/pump1_status", p1state);
  Firebase.RTDB.setInt(&fbdo, "/ESP_32/pump2_status", p2state);

  // --- Motor setup ---
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  pumpOff(3); // turn off both at start
}


unsigned long previousMillis = 0;
const long interval = 5000; // 10 seconds
int autoState = 0;

void loop() {
  static unsigned long lastCheck = 0;
  const unsigned long checkInterval = 150; // check every 150ms for faster response

  //for soil sensor delay
 

  if (millis() - lastCheck >= checkInterval) {
    lastCheck = millis();


    unsigned long currentMillis = millis();
    Serial.print("difference = ");Serial.print(currentMillis - previousMillis);Serial.println("");
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      
      // To call soil Moisture 
      soilMoisture();
    }

    if (Firebase.RTDB.getString(&fbdo, "/ESP_32/command") && fbdo.dataType() == "string") {
      String command = fbdo.stringData();
      command.replace("\"", "");
      command.replace("\\", "");
      command.trim();

      if (command == "standby") return;

      Serial.print("🔥 Command: ");
      Serial.println(command);

      bool executed = false;

      if (command == "pump1_toggle") {
        toggle(1);
        
        executed = true;

      } else if (command == "pump2_toggle") {
        toggle(2);
        
        executed = true;

      } else if (command == "both_on") {
        pumpOn(3);
        
        executed = true;

      } else if (command == "both_off") {
        pumpOff(3);
        
        executed = true;
      }else if (command == "auto"){
        pumpAuto();
        Firebase.RTDB.setInt(&fbdo, "/ESP_32/autoState", autoState);
        executed = true;
      }

      if (executed) {
        Firebase.RTDB.setString(&fbdo, "/ESP_32/command", "standby");
      }
    }
  }
}

// -----------------------------
// Motor control functions
// -----------------------------
void pumpOn(uint8_t pump_no) {
  Serial.printf("🚿 Pump %d ON\n", pump_no);
  if (pump_no == 1 || pump_no == 3) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, SPEED1);
    p1state = 1;
  }
  if (pump_no == 2 || pump_no == 3) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    analogWrite(ENB, SPEED2);
    p2state = 1;
  }
  Firebase.RTDB.setInt(&fbdo, "/ESP_32/pump1_status", p1state);
  Firebase.RTDB.setInt(&fbdo, "/ESP_32/pump2_status", p2state);
}

void pumpOff(uint8_t pump_no) {
  Serial.printf("🛑 Pump %d OFF\n", pump_no);
  if (pump_no == 1 || pump_no == 3) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, 0);
    p1state = 0;
  }
  if (pump_no == 2 || pump_no == 3) {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    analogWrite(ENB, 0);
    p2state = 0;
  }
  Firebase.RTDB.setInt(&fbdo, "/ESP_32/pump1_status", p1state);
  Firebase.RTDB.setInt(&fbdo, "/ESP_32/pump2_status", p2state);
}

void toggle(uint8_t pump_no) {
  if (pump_no == 1) {
    (p1state) ? pumpOff(1) : pumpOn(1);
  } else if (pump_no == 2) {
    (p2state) ? pumpOff(2) : pumpOn(2);
  }
}



void soilMoisture() {
  int sensorValue = analogRead(SOIL_PIN);
  int moisturePercent;

  // Apply your calibrated ranges
  if (sensorValue > 2000) {
    moisturePercent = map(sensorValue, 4095, 2000, 0, 25);
  } 
  else if (sensorValue > 1300) {
    moisturePercent = map(sensorValue, 2000, 1300, 25, 60);
  } 
  else if (sensorValue > 900) {
    moisturePercent = map(sensorValue, 1300, 900, 60, 90);
  } 
  else {
    moisturePercent = map(sensorValue, 900, 800, 90, 100);
  }

  // Clamp between 0–100
  if (moisturePercent < 0) moisturePercent = 0;
  if (moisturePercent > 100) moisturePercent = 100;

  // --- Send to Firebase ---
  if (Firebase.RTDB.setInt(&fbdo, "/ESP_32/response", moisturePercent)) {
        Serial.print(" Firebase updated");

  } else {
    Serial.print("❌ Firebase update failed: ");
    Serial.println(fbdo.errorReason());
  }
}

void pumpAuto(){
  autoState = 1;
  Firebase.RTDB.setInt(&fbdo, "/ESP_32/autoState", autoState);
  pumpOn(3);
  delay(10000);
  pumpOff(1);
  delay(5000);
  pumpOff(2);
  autoState = 0;
  Firebase.RTDB.setInt(&fbdo, "/ESP_32/autoState", autoState);
}
