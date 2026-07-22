/**
 * @file example_integration.cpp
 * @brief Example of integrating PersistentStorage tests into a main project
 * 
 * This shows how to include the tests in your main application for easy testing
 * during development.
 */

#include <Arduino.h>

// Include this to run tests
#define RUN_TESTS

#ifdef RUN_TESTS
    // Include test files directly
    #include "test_persistent_storage.cpp"
    #include "test_persistent_storage_mqtt.cpp"
    
    void setup() {
        Serial.begin(115200);
        delay(2000);
        
        Serial.println("\n=== Running PersistentStorage Tests ===\n");
        
        // Run basic tests
        runPersistentStorageTests();
        
        // Run MQTT tests
        runPersistentStorageMqttTests();
        
        Serial.println("\n=== Tests Complete ===\n");
    }
    
    void loop() {
        delay(1000);
    }

#else
    // Normal application code
    #include <PersistentStorage.h>
    #include <MQTTManager.h>
    
    PersistentStorage storage("app", "device/params");
    
    // Application parameters
    bool systemEnabled = true;
    float temperature = 20.0f;
    int32_t deviceId = 1;
    char deviceName[32] = "ESP32-Device";
    
    void setup() {
        Serial.begin(115200);
        
        // Initialize storage
        if (!storage.begin()) {
            Serial.println("Failed to initialize storage!");
            return;
        }
        
        // Register parameters
        storage.registerBool("system/enabled", &systemEnabled, "System enable state");
        storage.registerFloat("sensors/temperature", &temperature, -40.0f, 125.0f, "Temperature reading");
        storage.registerInt("device/id", &deviceId, 1, 9999, "Device ID");
        storage.registerString("device/name", deviceName, sizeof(deviceName), "Device name");
        
        // Set callbacks
        storage.setOnChange("system/enabled", [](const std::string& name, const void* value) {
            bool enabled = *(bool*)value;
            Serial.printf("System %s\n", enabled ? "enabled" : "disabled");
        });
        
        // Load saved values
        storage.loadAll();
        
        Serial.println("Application started");
    }
    
    void loop() {
        // Process MQTT commands
        storage.processCommands();
        
        // Your application logic here
        
        delay(100);
    }
#endif