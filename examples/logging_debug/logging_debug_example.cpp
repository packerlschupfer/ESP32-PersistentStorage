/**
 * Example demonstrating PersistentStorage logging and debug features
 * 
 * Compile with different flags to see the difference:
 * 
 * 1. Default (ESP-IDF, no debug):
 *    platformio run
 * 
 * 2. With debug logging:
 *    platformio run -e debug
 * 
 * 3. With custom logger:
 *    platformio run -e custom_logger
 * 
 * 4. With both debug and custom logger:
 *    platformio run -e debug_custom
 */

#include <Arduino.h>
#include <PersistentStorage.h>

// Example platformio.ini environments:
/*
[env:default]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = PersistentStorage

[env:debug]
platform = espressif32
board = esp32dev
framework = arduino
build_flags = -DPSTORAGE_DEBUG
lib_deps = PersistentStorage

[env:custom_logger]
platform = espressif32
board = esp32dev
framework = arduino
build_flags = -DUSE_CUSTOM_LOGGER
lib_deps = 
    Logger
    PersistentStorage

[env:debug_custom]
platform = espressif32
board = esp32dev
framework = arduino
build_flags = 
    -DUSE_CUSTOM_LOGGER
    -DPSTORAGE_DEBUG
lib_deps = 
    Logger
    PersistentStorage
*/

// Only needed when USE_CUSTOM_LOGGER is defined
#ifdef USE_CUSTOM_LOGGER
#include <Logger.h>
#include <LogInterfaceImpl.h>
#endif

PersistentStorage storage("example", "device/params");

// Example settings
struct Settings {
    float temperature = 22.0;
    int counter = 0;
    char name[32] = "TestDevice";
    uint8_t config[16] = {0x01, 0x02, 0x03, 0x04};
} settings;

void demonstrateLogging() {
    Serial.println("\n=== Demonstrating Logging Levels ===");
    
    // These always show (Error, Warn, Info)
    PSTOR_LOG_E("This is an ERROR message - always visible");
    PSTOR_LOG_W("This is a WARNING message - always visible");
    PSTOR_LOG_I("This is an INFO message - always visible");
    
    // These only show with PSTORAGE_DEBUG
    PSTOR_LOG_D("This is a DEBUG message - only with PSTORAGE_DEBUG");
    PSTOR_LOG_V("This is a VERBOSE message - only with PSTORAGE_DEBUG");
}

void demonstrateDebugFeatures() {
    Serial.println("\n=== Demonstrating Debug Features ===");
    
    // Performance timing (only with PSTORAGE_DEBUG)
    PSTOR_TIME_START();
    
    // Simulate some work
    delay(100);
    storage.save("device/name");
    
    PSTOR_TIME_END("save operation");
    
    // Buffer dump (only with PSTORAGE_DEBUG)
    PSTOR_DUMP_BUFFER("Config data", settings.config, sizeof(settings.config));
}

void demonstrateParameterOperations() {
    Serial.println("\n=== Parameter Operations ===");
    
    // This demonstrates how debug logs help track operations
    PSTOR_LOG_I("Registering parameters...");
    
    storage.registerFloat("temperature", &settings.temperature, 0.0, 100.0, 
                         "Current temperature");
    
    storage.registerInt("counter", &settings.counter, 0, 1000,
                       "Operation counter");
    
    storage.registerString("device/name", settings.name, sizeof(settings.name),
                          "Device friendly name");
    
    storage.registerBlob("config", settings.config, sizeof(settings.config),
                        "Device configuration");
    
    // With debug enabled, you'll see detailed registration info
    PSTOR_LOG_D("Parameter registration complete");
    
    // Save and load operations
    auto result = storage.saveAll();
    if (result == PersistentStorage::Result::SUCCESS) {
        PSTOR_LOG_I("All parameters saved successfully");
    } else {
        PSTOR_LOG_E("Failed to save parameters");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=== PersistentStorage Logging Example ===");
    
#ifdef USE_CUSTOM_LOGGER
    Serial.println("Using CUSTOM LOGGER");
    Logger::getInstance().init(1024);
    Logger::getInstance().setLogLevel(ESP_LOG_VERBOSE);
    Logger::getInstance().enableLogging(true);
#else
    Serial.println("Using ESP-IDF logging");
#endif

#ifdef PSTORAGE_DEBUG
    Serial.println("DEBUG logging ENABLED");
#else
    Serial.println("DEBUG logging DISABLED");
#endif
    
    // Initialize storage
    if (!storage.begin()) {
        PSTOR_LOG_E("Failed to initialize storage!");
        return;
    }
    
    // Demonstrate different features
    demonstrateLogging();
    demonstrateParameterOperations();
    demonstrateDebugFeatures();
    
    Serial.println("\n=== Example Complete ===");
}

void loop() {
    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate > 5000) {
        lastUpdate = millis();
        
        // Increment counter
        settings.counter++;
        PSTOR_LOG_D("Counter incremented to %d", settings.counter);
        
        // Save periodically
        if (settings.counter % 10 == 0) {
            PSTOR_TIME_START();
            storage.save("counter");
            PSTOR_TIME_END("counter save");
        }
    }
}