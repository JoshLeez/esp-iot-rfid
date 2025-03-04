#define RXD2 16
#define BUZZER_PIN 18  // Buzzer Pin
#define SWITCH_PIN 13  // Switch Pin

#include <LiquidCrystal_I2C.h>  // Library for LCD
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);  // I2C address 0x27, 20 columns and 4 rows

const char* ssid = "Production FTY A";   // Replace with your Wi-Fi SSID
const char* password = "Wifi.gla@2025";    // Replace with your Wi-Fi password

String url = "http://192.168.5.105/cutting-app/public/api";

bool cardPresent = false;
bool cardPreviouslyDetected = false;
String lastUID = "";
unsigned long lastDetectionTime = 0;
const unsigned long cardRemovalTimeout = 2000;

bool lastSwitchState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;  // 50ms debounce time

// Global variables to cache description in Stock IN mode
String cachedDescription = "";
bool descriptionFetched = false;

// Helper function to sound error beeps (error pattern on buzzer)
void errorBeep() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

// Helper function to handle error responses: beep and show error message on LCD.
void handleError(int httpResponseCode, HTTPClient &http) {
  String payload = http.getString();
  errorBeep();
  DynamicJsonDocument errDoc(512);
  DeserializationError err = deserializeJson(errDoc, payload);
  
  if (!err) {
    const char* errMessage = errDoc["message"];
    lcd.setCursor(0, 2);
    lcd.print(errMessage);
  } else {
    lcd.setCursor(0, 2);
    lcd.print("Error: " + String(httpResponseCode));
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, -1);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to Wi-Fi");

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(0, 1);
    lcd.print("Connecting...");
  }

  // Once connected, display "Connected"
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wi-Fi Connected");
  lcd.setCursor(0, 1);
  lcd.print("IP: ");
  lcd.print(WiFi.localIP());
  lcd.setCursor(0, 2);
  lcd.print("Mac:");
  lcd.print(WiFi.macAddress());
  delay(2000);  // Show the IP address for 2 seconds

  // Display initial text on the LCD
  lcd.clear();
  updateLCD();  // Initialize LCD with correct state
}

void loop() {
  static unsigned long lastReadTime = millis();
  static unsigned long lastSwitchCheck = 0;  // Timestamp for switch debounce

  bool switchState = digitalRead(SWITCH_PIN);

  // Debounce switch input and detect mode changes
  if (millis() - lastSwitchCheck > debounceDelay) {
    if (switchState != lastSwitchState) {
      // Reset card state when switching mode so that previous scan isn’t reused
      cardPresent = false;
      cardPreviouslyDetected = false;
      lastUID = "";

      // Additionally, when switching to LOW (Stock IN), reset the cached description
      if (switchState == LOW) {
        cachedDescription = "";
        descriptionFetched = false;
      }

      lastSwitchState = switchState;
      updateLCD();
      Serial.println(switchState == HIGH ? "Mode: Menunggu Kartu" : "Mode: Stock IN");
    }
    lastSwitchCheck = millis();
  }

  // Check for RFID scan via Serial2
  if (Serial2.available()) {
    char buffer[14];
    int index = 0;
    delay(50);
    while (Serial2.available() && index < 14) {
      buffer[index] = Serial2.read();
      index++;
    }
    buffer[index] = '\0';

    if (buffer[0] == 0x02 && buffer[13] == 0x03) {
      String uidHex = String(buffer).substring(1, 11);
      String uidHex4Byte = uidHex.substring(2, 10);

      if (!cardPresent) {
        cardPresent = true;
        lastUID = uidHex4Byte;
        lastDetectionTime = millis();
        cardPreviouslyDetected = true;

        // Convert UID to Decimal (if needed)
        unsigned long decimalUID = strtoul(uidHex4Byte.c_str(), NULL, 16);

        // Normal beep for a valid scan
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);

        // Update LCD Based on Switch State
        updateLCD();

        Serial.print("\nRFID UID HEX (4 Byte): ");
        Serial.println(uidHex4Byte);
        Serial.print("RFID UID Decimal (4 Byte): ");
        Serial.println(decimalUID);
      }
      lastReadTime = millis();
    }
  } else {
    // Clear card state after timeout if no new scan is detected
    if (cardPreviouslyDetected && millis() - lastReadTime > cardRemovalTimeout) {
      cardPresent = false;
      cardPreviouslyDetected = false;
      lastUID = "";
    }
  }
}

void updateLCD() {
  lcd.clear();

  if (lastSwitchState == HIGH) {  // Mode: Menunggu Kartu (GET Mode)
    lcd.setCursor(0, 0);
    lcd.print("Check Ticket");
    if (cardPresent) {
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        unsigned long decimalUID = strtoul(lastUID.c_str(), NULL, 16);

        // GET Request: Fetch Ticket Data
        http.begin(url + "/cutting-ticket/" + WiFi.macAddress() + "/" + decimalUID);
        int httpResponseCode = http.GET();

        if (httpResponseCode >= 200 && httpResponseCode < 300) {
          Serial.print("HTTP Response code: ");
          Serial.println(httpResponseCode);
          String payload = http.getString();
          Serial.println(payload);

          DynamicJsonDocument doc(1024);
          DeserializationError error = deserializeJson(doc, payload);

          if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
          } else {
            // Extract ticket details
            const char* gl_number = doc["data"]["0"]["gl_number"];
            int table_number = doc["data"]["0"]["table_number"];
            int ticket_number = doc["data"]["0"]["ticket_number"];
            const char* color = doc["data"]["0"]["color"];
            const char* size = doc["data"]["0"]["size"];
            int layer = doc["data"]["0"]["layer"];

            Serial.print("Gl Number: ");
            Serial.println(gl_number);
            Serial.print("Table/Ticket: ");
            Serial.println(String(table_number) + "/" + String(ticket_number));

            // Display ticket details on the LCD
            lcd.setCursor(0, 0);
            lcd.print("Gl:");
            lcd.print(gl_number);

            lcd.setCursor(0, 1);
            lcd.print("Lot/Ticket:");
            lcd.print(table_number);
            lcd.print("/");
            lcd.print(ticket_number);

            lcd.setCursor(0, 2);
            lcd.print("Color:");
            lcd.print(color);

            lcd.setCursor(0, 3);
            lcd.print("Size/Layer:");
            lcd.print(size);
            lcd.print("/");
            lcd.print(layer);
          }
        } else {
          // Handle error: beep and show error message from response body
          handleError(httpResponseCode, http);
        }
        http.end();
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Wi-Fi Disconnected");
      }
    } 
  } else {  // Mode: Stock IN (POST Mode)
    if (WiFi.status() == WL_CONNECTED) {
      // Fetch and cache the description only once per mode switch
      if (!descriptionFetched) {
        HTTPClient http;
        http.begin(url + "/iot-device/detail/" + WiFi.macAddress());
        int httpResponseCode = http.GET();
        if (httpResponseCode >= 200 && httpResponseCode < 300) {
          Serial.print("HTTP Response code: ");
          Serial.println(httpResponseCode);
          String payload = http.getString();
          Serial.println(payload);

          DynamicJsonDocument doc(1024);
          DeserializationError error = deserializeJson(doc, payload);
          if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
          } else {
            const char* description = doc["data"]["data"]["deviceable"]["description"];
            cachedDescription = String(description);
            descriptionFetched = true;
            Serial.print("Fetched Description: ");
            Serial.println(cachedDescription);
          }
        } else {
          handleError(httpResponseCode, http);
        }
        http.end();
      }

      // Display the cached description on the LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(cachedDescription);

      // Only perform POST if a new card is scanned in LOW mode
      if (cardPresent) {
        HTTPClient http;
        http.begin("http://192.168.5.105/cutting-app/public/api/bundle-stocks/store-ticket-tag");  // POST endpoint

        DynamicJsonDocument postDoc(256);
        postDoc["mac_address"] = WiFi.macAddress();
        postDoc["uid_no_zero"] = strtoul(lastUID.c_str(), NULL, 16);

        String postPayload;
        serializeJson(postDoc, postPayload);
        http.addHeader("Content-Type", "application/json");

        int httpResponseCode = http.POST(postPayload);
        if (httpResponseCode >= 200 && httpResponseCode < 300) {
          Serial.print("HTTP POST Response code: ");
          Serial.println(httpResponseCode);
          String response = http.getString();
          Serial.println(response);

          DynamicJsonDocument responseDoc(256);
          DeserializationError error = deserializeJson(responseDoc, response);
          if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
          } else {
            const char* message = responseDoc["message"];
            Serial.print("Message: ");
            Serial.println(message);
            lcd.setCursor(0, 2);
            lcd.print(message);
          }
        } else {
          handleError(httpResponseCode, http);
        }
        http.end();
      }
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wi-Fi Disconnected");
    }
  }

  Serial.println(lastSwitchState == HIGH ? "Mode: Menunggu Kartu" : "Mode: Stock IN");
}
