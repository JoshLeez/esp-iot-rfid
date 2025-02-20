#define RXD2 16
#define BUZZER_PIN 18  // Set the buzzer pin (change to your pin)
#include <LiquidCrystal_I2C.h> // Library for LCD

String lastScannedUID = "";  // Variable to store the last scanned UID
LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address 0x27, 20 column and 4 rows

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, -1);
  pinMode(BUZZER_PIN, OUTPUT);  // Set the buzzer pin as output
  Serial.println("Menunggu kartu RFID...");

  // Initialize LCD
  lcd.init();  // Initialize LCD
  lcd.backlight();  // Turn on backlight

  // Display initial message on LCD
  lcd.setCursor(0, 0);
  lcd.print("Menunggu kartu...");
}

void loop() {
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
      
      // Ambil hanya 4 byte terakhir
      String uidHex4Byte = uidHex.substring(2, 10);
      
      // Konversi ke Decimal
      unsigned long decimalUID = strtoul(uidHex4Byte.c_str(), NULL, 16);

      // Check if this UID has been scanned before
      if (uidHex4Byte != lastScannedUID) {
        // Play buzzer sound
        tone(BUZZER_PIN, 1000, 200);  // Play a 1kHz tone for 200ms
        
        // Print the UID to the serial monitor
        Serial.print("\nRFID UID  HEX (4 Byte): ");
        Serial.println(uidHex4Byte);
        Serial.print("RFID UID Decimal (4 Byte): ");
        Serial.println(decimalUID);

        // Update the last scanned UID
        lastScannedUID = uidHex4Byte;

        // Display message on LCD after scan
        lcd.clear();  // Clear the previous display
        lcd.setCursor(0, 0);  // Set cursor at the top-left corner
        lcd.setCursor(0, 0);  // Set cursor at the top-left corner
        lcd.print("UID Scanned:");
        lcd.setCursor(0, 1);  // Move to the second row
        lcd.print(decimalUID);  // Display the UID on LCD
        delay(2000);  // Display the message for 2 seconds

        // After 2 seconds, return to "Waiting for card..." message
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Menunggu kartu...");
      }
    }
  }
}
