#define RXD2 16
#define BUZZER_PIN 18  // Buzzer Pin
#define SWITCH_PIN 13  // Switch Pin

#include <LiquidCrystal_I2C.h>  // Library for LCD
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);  // I2C address 0x27, 20 columns and 4 rows

const char* ssid = "Production FTY A";   // Replace with your Wi-Fi SSID
const char* password = "Wifi.gla@2025";  // Replace with your Wi-Fi password

String url = "http://192.168.5.105/cutting-app/public/api";

bool cardPresent = false;
bool cardPreviouslyDetected = false;
String lastUID = "";
unsigned long lastDetectionTime = 0;
const unsigned long cardRemovalTimeout = 2000;

bool lastSwitchState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;  // 50ms debounce time

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, -1);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);  // Top row
  lcd.print("Connecting to Wi-Fi");

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(0, 1);  // Bottom row
    lcd.print("Connecting...");
  }

  // Once connected, display "Connected"
  lcd.clear();
  lcd.setCursor(0, 0);  // Top row
  lcd.print("Wi-Fi Connected");
  lcd.setCursor(0, 1);  // Bottom row
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
  const unsigned long debounceDelay = 50;    // Debounce time

  bool switchState = digitalRead(SWITCH_PIN);

  // Debounce switch input
  if (millis() - lastSwitchCheck > debounceDelay) {

    if (switchState != lastSwitchState) {
      lastSwitchState = switchState;
      updateLCD();
      Serial.println(switchState == HIGH ? "Mode: Menunggu Kartu" : "Mode: Stock IN");
    }
    lastSwitchCheck = millis();
  }

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

        // Convert UID to Decimal
        unsigned long decimalUID = strtoul(uidHex4Byte.c_str(), NULL, 16);

        // Beep the buzzer
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
    if (cardPreviouslyDetected && millis() - lastReadTime > cardRemovalTimeout) {
      cardPresent = false;
      cardPreviouslyDetected = false;
      lastUID = "";

      updateLCD();
    }
  }
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);

  if (lastSwitchState == HIGH) {  // Mode: Menunggu Kartu
    lcd.print("Check Ticket");

    if (cardPresent) {
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        unsigned long decimalUID = strtoul(lastUID.c_str(), NULL, 16);

        // First API Call: Fetch Ticket Data
        http.begin(url + "/cutting-ticket/" + WiFi.macAddress() + "/" + decimalUID);
        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
          Serial.print("HTTP Response code: ");
          Serial.println(httpResponseCode);
          String payload = http.getString();
          Serial.println(payload);

          DynamicJsonDocument doc(1024);  // Adjust the size according to your JSON response
          DeserializationError error = deserializeJson(doc, payload);

          if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
          } else {
            // Extract the gl_number field
            const char* gl_number = doc["data"]["0"]["gl_number"];
            int table_number = doc["data"]["0"]["table_number"];
            int ticket_number = doc["data"]["0"]["ticket_number"];
            Serial.print("Gl Number: ");
            Serial.println(gl_number);
            
            Serial.print("Table/Ticket:");
            Serial.println(table_number + "/" + ticket_number);

            // Display the gl_number on the LCD
            lcd.clear();

            lcd.setCursor(0, 0);
            lcd.print('Gl Number:');
            lcd.print(gl_number);

            lcd.setCursor(0, 1);
            lcd.print("No. Lot/Ticket:");
            lcd.print(table_number);
            lcd.print("/");
            lcd.print(ticket_number);
          }
        } else {
          Serial.print("Error code: ");
          Serial.println(httpResponseCode);
        }

        http.end();  // Free resources
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);  // Top row
        lcd.print("Wi-Fi Disconnected");
      }

      lcd.setCursor(0, 3);
      lcd.print("HEX: ");
      lcd.print(lastUID);
    }
  } else {  // Mode: Stock IN
    HTTPClient http;

    http.begin(url + "/iot-device/detail/" + WiFi.macAddress());
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);

      DynamicJsonDocument doc(1024);  // Adjust the size according to your JSON response
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      } else {
        // Extract the description field
        const char* description = doc["data"]["data"]["deviceable"]["description"];
        Serial.print("Description: ");
        Serial.println(description);

        // Display the description on the LCD
        lcd.setCursor(0, 0);
        lcd.print(description);
      }
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }

    http.end();  // Free resources

    if (cardPresent) {
      lcd.setCursor(0, 3);
      lcd.print("Stock IN Success!");

      // Perform HTTP POST request
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("http://192.168.5.105/cutting-app/public/api/bundle-stocks/store-ticket-tag");  // Replace with your POST endpoint

        // Create JSON payload
        DynamicJsonDocument postDoc(256);
        postDoc["mac_address"] = WiFi.macAddress();
        postDoc["uid_no_zero"] = strtoul(lastUID.c_str(), NULL, 16);  // Convert HEX UID to decimal
        // postDoc["transaction_type"] = "OUT";

        String postPayload;
        serializeJson(postDoc, postPayload);

        // Set content type header
        http.addHeader("Content-Type", "application/json");

        // Send POST request
        int httpResponseCode = http.POST(postPayload);

        if (httpResponseCode > 0) {
          Serial.print("HTTP POST Response code: ");
          Serial.println(httpResponseCode);
          String response = http.getString();
          Serial.println(response);

          DynamicJsonDocument responseDoc(256);  // Adjust size as needed
          DeserializationError error = deserializeJson(responseDoc, response);

          if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
          } else {
            // Extract the "message" field
            const char* message = responseDoc["message"];
            Serial.print("Message: ");
            Serial.println(message);

            // Display the message on the LCD
            lcd.setCursor(0, 2);  // Adjust the position as needed
            lcd.print(message);
          }
        } else {
          Serial.print("HTTP POST Error code: ");
          Serial.println(httpResponseCode);
        }

        http.end();  // Free resources
      } else {
        Serial.println("Wi-Fi Disconnected. Cannot send POST request.");
      }
    }
  }

  Serial.println(lastSwitchState == HIGH ? "Mode: Menunggu Kartu" : "Mode: Stock IN");
}