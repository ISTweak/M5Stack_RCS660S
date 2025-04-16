/*
 * RCS660S_ESP32 Library - Simple Example
 * This example demonstrates basic usage of the RCS660S_ESP32 library
 * for FeliCa card detection
 */

#include <Arduino.h>
#include "RCS660S_ESP32.h"

// For M5 Core2 PORT-A(Red)
#define UART_RX 33
#define UART_TX 32

// Create RCS660S instance using Serial1
RCS660S nfc;

void setup() {
  // Initialize Serial for debug output
  Serial.begin(115200);
  while (!Serial && millis() < 3000);  // シリアルポートの準備を待つ（最大3秒）

  Serial.println("\nRCS660S_ESP32 Simple Example");
  Serial.println("---------------------------");

  // Initialize Serial1 for RCS660S communication
  Serial1.begin(115200, SERIAL_8N1, UART_RX, UART_TX);

  // Initialize RCS660S
  Serial.println("Initializing NFC reader...");
  int result = nfc.initDevice();
  if (result != 1) {
    Serial.println("Failed to initialize RCS660S!");
  } else {
    Serial.println("RCS660S initialized successfully");
  }

  Serial.println("\nWaiting for FeliCa card...");
}

void loop() {
  // Poll for FeliCa card
  int result = nfc.polling();

  if (result == 1) {
    // Card found - print IDm and PMm
    Serial.println("\n----- Card Detected! -----");

    // Get and print IDm (Card ID)
    Serial.print("IDm: ");
    for (int i = 0; i < 8; i++) {
      if (nfc.idm[i] < 0x10) Serial.print('0');
      Serial.print(nfc.idm[i], HEX);
    }
    Serial.println();

    // Get and print PMm (Manufacturer info)
    Serial.print("PMm: ");
    for (int i = 0; i < 8; i++) {
      if (nfc.pmm[i] < 0x10) Serial.print('0');
      Serial.print(nfc.pmm[i], HEX);
    }
    Serial.println();

    // Wait a bit before next polling
    delay(1000);

    Serial.println("Remove card and place another...");
  } else {
    // No card or error
    delay(100);  // Short delay between polling attempts
  }
}
