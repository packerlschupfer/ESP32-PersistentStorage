/**
 * PersistentStorage Basic Usage Example
 * 
 * Demonstrates:
 * - Parameter registration
 * - Save/load operations
 * - Change callbacks
 * - MQTT integration
 */

#include <Arduino.h>
#include <PersistentStorage.h>
#include <WiFi.h>

// WiFi credentials
const char* WIFI_SSID = "your_ssid";
const char* WIFI_PASSWORD = "your_password";

// Create storage instance
PersistentStorage storage("example", "esp32/params");

// Example settings structure
struct Settings {
    // System settings
    bool systemEnabled = true;
    char deviceName[32] = "ESP32-Example";
    
    // Temperature control
    float targetTemperature = 22.0f;
    float temperatureHysteresis = 0.5f;
    
    // PID parameters
    float pidKp = 1.0f;
    float pidKi = 0.1f;
    float pidKd = 0.05f;
    
    // Timing
    int32_t sensorInterval = 5000;
    int32_t reportInterval = 60000;
} settings;

// Current readings (read-only parameters)
float currentTemperature = 20.0f;
float currentHumidity = 50.0f;
uint32_t uptime = 0;

void setupWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void registerParameters() {
    Serial.println("Registering parameters...");
    
    // System parameters
    storage.registerBool("system/enabled", &settings.systemEnabled,
                        "Enable system operation");
    
    storage.registerString("system/name", settings.deviceName, 
                          sizeof(settings.deviceName),
                          "Device friendly name");
    
    // Temperature control parameters
    storage.registerFloat("temp/target", &settings.targetTemperature,
                         10.0f, 30.0f,
                         "Target temperature in Celsius");
    
    storage.registerFloat("temp/hysteresis", &settings.temperatureHysteresis,
                         0.1f, 2.0f,
                         "Temperature control hysteresis");
    
    // PID parameters
    storage.registerFloat("pid/kp", &settings.pidKp, 0.0f, 10.0f,
                         "PID proportional gain");
    
    storage.registerFloat("pid/ki", &settings.pidKi, 0.0f, 5.0f,
                         "PID integral gain");
    
    storage.registerFloat("pid/kd", &settings.pidKd, 0.0f, 5.0f,
                         "PID derivative gain");
    
    // Timing parameters
    storage.registerInt("timing/sensorInterval", &settings.sensorInterval,
                       1000, 60000,
                       "Sensor reading interval (ms)");
    
    storage.registerInt("timing/reportInterval", &settings.reportInterval,
                       10000, 300000,
                       "Status report interval (ms)");
    
    // Read-only status parameters
    storage.registerFloat("status/temperature", &currentTemperature,
                         -50.0f, 100.0f,
                         "Current temperature",
                         ParameterInfo::ACCESS_READ_ONLY);
    
    storage.registerFloat("status/humidity", &currentHumidity,
                         0.0f, 100.0f,
                         "Current humidity",
                         ParameterInfo::ACCESS_READ_ONLY);
    
    storage.registerInt("status/uptime", (int32_t*)&uptime,
                       0, INT32_MAX,
                       "System uptime (seconds)",
                       ParameterInfo::ACCESS_READ_ONLY);
}

void setupCallbacks() {
    // Temperature target change
    storage.setOnChange("temp/target", [](const std::string& name, const void* value) {
        float newTemp = *(float*)value;
        Serial.printf("Target temperature changed to: %.1f°C\n", newTemp);
        // Here you would update your temperature controller
    });
    
    // PID parameter changes
    auto pidCallback = [](const std::string& name, const void* value) {
        Serial.printf("PID parameter %s changed to: %.3f\n", 
                     name.c_str(), *(float*)value);
        // Here you would update your PID controller
    };
    
    storage.setOnChange("pid/kp", pidCallback);
    storage.setOnChange("pid/ki", pidCallback);
    storage.setOnChange("pid/kd", pidCallback);
    
    // System enable/disable
    storage.setOnChange("system/enabled", [](const std::string& name, const void* value) {
        bool enabled = *(bool*)value;
        Serial.printf("System %s\n", enabled ? "ENABLED" : "DISABLED");
        // Here you would start/stop your control loops
    });
    
    // Custom validator for temperature
    storage.setValidator("temp/target", [](const void* value) {
        float temp = *(float*)value;
        // Ensure temperature is reasonable
        if (temp < 5.0f || temp > 35.0f) {
            Serial.println("Temperature out of safe range!");
            return false;
        }
        return true;
    });
}

void printAllParameters() {
    Serial.println("\n=== Current Parameters ===");
    
    StaticJsonDocument<2048> doc;
    storage.getAllJson(doc);
    
    serializeJsonPretty(doc, Serial);
    Serial.println("\n========================\n");
}

void simulateSensorReadings() {
    // Simulate temperature changes
    currentTemperature += random(-10, 11) / 10.0f;
    currentTemperature = constrain(currentTemperature, 15.0f, 25.0f);
    
    // Simulate humidity changes
    currentHumidity += random(-20, 21) / 10.0f;
    currentHumidity = constrain(currentHumidity, 30.0f, 70.0f);
    
    // Update uptime
    uptime = millis() / 1000;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("\n=== PersistentStorage Example ===\n");
    
    // Initialize WiFi (optional for MQTT)
    // setupWiFi();
    
    // Initialize storage
    if (!storage.begin()) {
        Serial.println("Failed to initialize storage!");
        while (1) delay(1000);
    }
    
    // Register all parameters
    registerParameters();
    
    // Setup callbacks
    setupCallbacks();
    
    // Load saved values
    Serial.println("Loading saved parameters...");
    storage.loadAll();
    
    // Print all parameters
    printAllParameters();
    
    // Get NVS statistics
    size_t used, free, total;
    storage.getNvsStats(used, free, total);
    Serial.printf("NVS Stats - Used: %d, Free: %d, Total: %d\n\n", 
                  used, free, total);
    
    // Example: Change a parameter programmatically
    Serial.println("Changing target temperature to 23.5°C...");
    settings.targetTemperature = 23.5f;
    storage.save("temp/target");
    
    // Example: Get parameter info
    auto info = storage.getInfo("temp/target");
    if (info) {
        Serial.printf("Parameter '%s': %s\n", 
                     info->name.c_str(), info->description.c_str());
    }
    
    // List all PID parameters
    Serial.println("\nPID Parameters:");
    auto pidParams = storage.listByPrefix("pid/");
    for (const auto& param : pidParams) {
        StaticJsonDocument<256> paramDoc;
        storage.getJson(param, paramDoc);
        Serial.print("  - ");
        serializeJson(paramDoc, Serial);
        Serial.println();
    }
}

void loop() {
    static uint32_t lastSensorUpdate = 0;
    static uint32_t lastReport = 0;
    static uint32_t lastSave = 0;
    
    uint32_t now = millis();
    
    // Update sensor readings
    if (now - lastSensorUpdate >= settings.sensorInterval) {
        lastSensorUpdate = now;
        simulateSensorReadings();
    }
    
    // Report status
    if (now - lastReport >= settings.reportInterval) {
        lastReport = now;
        
        Serial.printf("\n--- Status Report ---\n");
        Serial.printf("System: %s\n", settings.systemEnabled ? "ENABLED" : "DISABLED");
        Serial.printf("Temperature: %.1f°C (target: %.1f°C)\n", 
                     currentTemperature, settings.targetTemperature);
        Serial.printf("Humidity: %.1f%%\n", currentHumidity);
        Serial.printf("Uptime: %lu seconds\n", uptime);
        Serial.println("-------------------\n");
    }
    
    // Periodic save (every 5 minutes)
    if (now - lastSave >= 300000) {
        lastSave = now;
        Serial.println("Saving all parameters...");
        storage.saveAll();
    }
    
    // Handle Serial commands for testing
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd == "help") {
            Serial.println("\nCommands:");
            Serial.println("  list     - List all parameters");
            Serial.println("  save     - Save all parameters");
            Serial.println("  load     - Load all parameters");
            Serial.println("  reset    - Reset to defaults");
            Serial.println("  temp XX  - Set target temperature");
            Serial.println("  enable   - Enable system");
            Serial.println("  disable  - Disable system");
            Serial.println();
        }
        else if (cmd == "list") {
            printAllParameters();
        }
        else if (cmd == "save") {
            storage.saveAll();
            Serial.println("All parameters saved");
        }
        else if (cmd == "load") {
            storage.loadAll();
            Serial.println("All parameters loaded");
        }
        else if (cmd == "reset") {
            storage.resetAll();
            Serial.println("All parameters reset to defaults");
        }
        else if (cmd.startsWith("temp ")) {
            float temp = cmd.substring(5).toFloat();
            StaticJsonDocument<64> doc;
            doc["value"] = temp;
            
            auto result = storage.setJson("temp/target", doc);
            Serial.printf("Set temperature: %s\n", 
                         PersistentStorage::resultToString(result));
        }
        else if (cmd == "enable") {
            settings.systemEnabled = true;
            storage.save("system/enabled");
        }
        else if (cmd == "disable") {
            settings.systemEnabled = false;
            storage.save("system/enabled");
        }
    }
    
    delay(100);
}