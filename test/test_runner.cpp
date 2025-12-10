/**
 * @file test_runner.cpp
 * @brief Main test runner for PersistentStorage library
 * 
 * This file provides a convenient way to run all tests for the PersistentStorage library.
 * It can be included in any ESP32 project to test the library functionality.
 */

#include <Arduino.h>
#include <unity.h>

// Forward declarations for test suites
void runPersistentStorageTests();
void runPersistentStorageMqttTests();

// Test configuration
#define RUN_BASIC_TESTS true
#define RUN_MQTT_TESTS true
#define TEST_DELAY_MS 2000

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    
    // Wait for serial connection
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime) < 5000) {
        delay(10);
    }
    
    delay(TEST_DELAY_MS);
    
    Serial.println("\n========================================");
    Serial.println("PersistentStorage Library Test Suite");
    Serial.println("========================================\n");
    
    // Run basic functionality tests
    if (RUN_BASIC_TESTS) {
        Serial.println("Running Basic Tests...");
        Serial.println("----------------------------------------");
        runPersistentStorageTests();
        Serial.println();
    }
    
    // Run MQTT integration tests
    if (RUN_MQTT_TESTS) {
        Serial.println("Running MQTT Integration Tests...");
        Serial.println("----------------------------------------");
        runPersistentStorageMqttTests();
        Serial.println();
    }
    
    Serial.println("========================================");
    Serial.println("All Tests Complete!");
    Serial.println("========================================");
}

void loop() {
    // Nothing to do in loop
    delay(1000);
}