#include <SoftwareSerial.h>

const byte rxPin = 16;  // RX pin for RDM6300
const byte txPin = 17;  // TX pin for RDM6300
const byte buzzerPin = 18;  // Buzzer pin

SoftwareSerial rfidSerial(rxPin, txPin);  // Create the SoftwareSerial object

void setup() {
  Serial.begin(115200);  // Initialize the Serial Monitor for debugging
  rfidSerial.begin(9600);  // Baud rate of RDM6300
  pinMode(buzzerPin, OUTPUT);  // Set the buzzer pin as an output
  
  Serial.println("Ready to read RFID tags!");
}

void loop() {
  static byte rfidData[12];  // Buffer to hold incoming RFID data
  static int i = 0;  // Index for reading into the buffer
  static bool reading = false;  // Flag to track reading state

  // Check if there's data available to read
  if (rfidSerial.available()) {
    byte incomingByte = rfidSerial.read();

    // Start of a new frame: Check if we found the start byte (0x02)
    if (incomingByte == 0x02) {
      i = 0;  // Reset buffer index
      rfidData[i++] = incomingByte;  // Store the start byte
      reading = true;  // Start reading data
    }

    // If we're in the middle of reading a frame, keep reading
    if (reading) {
      rfidData[i++] = incomingByte;

      // Check if we've reached the end of the frame (0x03)
      if (incomingByte == 0x03 && i >= 10) {
        // Print the raw data
        Serial.print("Raw Data: ");
        for (int j = 0; j < i; j++) {
          Serial.print(rfidData[j], HEX);
          Serial.print(" ");
        }
        Serial.println();

        // Extract UID if the frame is valid
        String uid = "";
        if (rfidData[0] == 0x02 && rfidData[i - 1] == 0x03) {
          for (int j = 3; j < 7; j++) {  // UID typically starts at index 3 and is 4 bytes
            // Convert byte to hexadecimal string and append
            uid += String(rfidData[j], HEX);
          }

          // Convert the hexadecimal UID to a decimal format similar to your USB reader output
          String decimalUID = "";
          for (int j = 3; j < 7; j++) {
            decimalUID += String(rfidData[j], DEC);
          }

          // Print both the raw hex UID and the decimal formatted UID
          Serial.println("RFID UID (Hex): " + uid);
          Serial.println("RFID UID (Decimal): " + decimalUID);

          // Activate the buzzer (turn it on for 100 milliseconds)
          digitalWrite(buzzerPin, HIGH);  // Turn on the buzzer
          delay(500);  // Buzzer on for 100 milliseconds
          digitalWrite(buzzerPin, LOW);  // Turn off the buzzer
        }

        // Reset for the next frame
        //0016246565
      
        reading = false;
        i = 0;
      }
    }
  }

  // Small delay to avoid flooding the serial monitor too much
  delay(100);
}
