/*
 * RCS660S_ESP32 Library Basic Example
 * This example demonstrates basic usage of the RCS660S_ESP32 library
 */

#include <Arduino.h>
#include "RCS660S_ESP32.h"

// Create RCS660S instance using Serial1
RCS660S nfc;

void setup() {
    // Initialize Serial for debug output
    Serial.begin(115200);
    Serial.println("RCS660S_ESP32 Basic Example");

    // Initialize Serial1 for RCS660S communication
    Serial1.begin(115200);

    // Enable debug output
    nfc.setDebug(true);

    // Initialize RCS660S
    RCS660Error result = nfc.initDevice();
    if (result != RCS660Error::SUCCESS) {
        Serial.print("Failed to initialize RCS660S: ");
        Serial.println(nfc.getLastErrorMessage());
        return;
    }
    Serial.println("RCS660S initialized successfully");
}

void loop() {
    // Poll for FeliCa card
    RCS660Error result = nfc.polling();

    if (result == RCS660Error::SUCCESS) {
        // Card found - print IDm and PMm
        const uint8_t* idm = nfc.getIDM();
        const uint8_t* pmm = nfc.getPMM();

        Serial.print("Card detected! IDm: ");
        for (int i = 0; i < 8; i++) {
            if (idm[i] < 0x10) Serial.print('0');
            Serial.print(idm[i], HEX);
        }
        Serial.println();

        Serial.print("PMm: ");
        for (int i = 0; i < 8; i++) {
            if (pmm[i] < 0x10) Serial.print('0');
            Serial.print(pmm[i], HEX);
        }
        Serial.println();

        // Wait a bit before next polling
        delay(1000);
    }
    else if (result == RCS660Error::NO_CARD) {
        // No card found - this is normal
        delay(100);
    }
    else {
        // Other error occurred
        Serial.print("Error during polling: ");
        Serial.println(nfc.getLastErrorMessage());
        delay(1000);
    }
}
