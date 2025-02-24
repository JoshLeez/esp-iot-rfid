#define RXD2 16
#define BUZZER_PIN 18  // Set the buzzer pin (change to your pin)
#define SWITCH_PIN 12  // Define the pin for the on/off switch

#include <LiquidCrystal_I2C.h> // Library for LCD

String lastScannedUID = "";  // Variable to store the last scanned UID
LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address 0x27, 20 column and 4 rows

unsigned long lastScanTime = 0; // Variable to track the time of last scan
unsigned long debounceDelay = 2000; // Delay before allowing another scan (2 seconds)
bool isCardPresent = false; // Flag to track if a card is present
int previousSwitchState = HIGH; // Variable to store the previous switch state

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, -1);
  pinMode(BUZZER_PIN, OUTPUT);  // Set the buzzer pin as output
  pinMode(SWITCH_PIN, INPUT);   // Set the switch pin as input
  Serial.println("Menunggu kartu RFID...");

  // Initialize LCD
  lcd.init();  // Initialize LCD
  lcd.backlight();  // Turn on backlight

  // Display initial message on LCD
  lcd.setCursor(0, 0);  // Top row
  lcd.print("Menunggu kartu...");
}

void loop() {
  int switchState = digitalRead(SWITCH_PIN);  // Read the state of the switch

  // Check if the switch state has changed
  if (switchState != previousSwitchState) {
    // Update the LCD only if the switch state has changed
    lcd.setCursor(0, 0);  // Top row
    if (switchState == HIGH) {
      Serial.println("Menunggu Kartu");
      lcd.print("Menunggu kartu...");
    } else {
      Serial.println("Stock in");
      lcd.print("Stock In         "); // Clear any leftover text with spaces
    }
    previousSwitchState = switchState; // Update the previous switch state
  }

  if (Serial2.available()) {
    char buffer[14];
    int index = 0;
    delay(50); // Small delay to allow the buffer to fill

    // Read the data from Serial2
    while (Serial2.available() && index < 14) {
      buffer[index] = Serial2.read();
      index++;
    }
    buffer[index] = '\0'; // Null-terminate the string

    // Check if the data is a valid UID packet
    if (buffer[0] == 0x02 && buffer[13] == 0x03) {
      String uidHex = String(buffer).substring(1, 11);

      // Ambil hanya 4 byte terakhir
      String uidHex4Byte = uidHex.substring(2, 10);

      // Konversi ke Decimal
      unsigned long decimalUID = strtoul(uidHex4Byte.c_str(), NULL, 16);

      // If the card is not already scanned and held, or card is removed for long enough
      if (uidHex4Byte != lastScannedUID || millis() - lastScanTime > debounceDelay) {
        // Play buzzer sound
        tone(BUZZER_PIN, 1000, 200);  // Play a 1kHz tone for 200ms

        // Print the UID to the serial monitor
        Serial.print("\nRFID UID HEX (4 Byte): ");
        Serial.println(uidHex4Byte);
        Serial.print("RFID UID Decimal (4 Byte): ");
        Serial.println(decimalUID);

        // Update the last scanned UID
        lastScannedUID = uidHex4Byte;

        // Mark the card as present
        isCardPresent = true;

        // Display message on LCD after scan
        lcd.setCursor(0, 1);  // Bottom row
        if (switchState == HIGH) {
          lcd.print("UID: ");
          lcd.print(decimalUID);  // Display the UID on LCD
        } else {
          lcd.print("Stock IN Success");
        }
        delay(2000);  // Display the message for 2 seconds

        // After 2 seconds, clear the bottom row
        lcd.setCursor(0, 1);
        lcd.print("                "); // Clear the bottom row

        // Set the time of the last scan to avoid multiple scans in quick succession
        lastScanTime = millis();
      }
    }

    // Clear the Serial2 buffer to avoid processing leftover data
    while (Serial2.available()) {
      Serial2.read();
    }
  }

  // Check if the card is removed (i.e., no new scans for debounceDelay)
  if (millis() - lastScanTime > debounceDelay) {
    isCardPresent = false;  // Allow scanning again after debounce period
  }
}