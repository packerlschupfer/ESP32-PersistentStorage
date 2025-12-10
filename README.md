# PersistentStorage Library

ESP32 Persistent Storage Manager with MQTT parameter access support.

## Features

- **Type-safe parameter storage** with NVS (Non-Volatile Storage) backend
- **MQTT integration** for remote parameter read/write
- **Parameter validation** with min/max ranges and custom validators
- **Change notifications** via callbacks
- **Hierarchical parameter names** (e.g., "heating/targetTemp")
- **Access control** (read-only, read-write)
- **JSON serialization** for complex data structures
- **Automatic save/load** on startup
- **Parameter grouping** by prefix

## Installation

Add to your `platformio.ini`:

```ini
lib_deps = 
    PersistentStorage
```

## Basic Usage

```cpp
#include <PersistentStorage.h>

// Create storage instance
PersistentStorage storage("myapp", "mydevice/params");

// System settings struct
struct Settings {
    float targetTemp = 22.0;
    float pidKp = 1.0;
    bool heatingEnabled = true;
    char deviceName[32] = "Boiler1";
} settings;

void setup() {
    // Initialize storage
    storage.begin();
    
    // Register parameters
    storage.registerFloat("heating/targetTemp", &settings.targetTemp, 15.0, 30.0,
                         "Target room temperature");
    
    storage.registerFloat("pid/kp", &settings.pidKp, 0.0, 10.0,
                         "PID proportional gain");
    
    storage.registerBool("heating/enabled", &settings.heatingEnabled,
                        "Enable heating system");
    
    storage.registerString("device/name", settings.deviceName, 32,
                          "Device friendly name");
    
    // Set change callback
    storage.setOnChange("heating/targetTemp", [](const std::string& name, const void* value) {
        float temp = *(float*)value;
        Serial.printf("Target temperature changed to: %.1f°C\n", temp);
    });
    
    // Load all parameters from NVS
    storage.loadAll();
}

void loop() {
    // Parameters are automatically saved when changed via MQTT
}
```

## MQTT Integration

### Topics

The library uses the following MQTT topic structure:

- **Set parameter**: `{prefix}/set/{parameter_name}`
  - Payload: JSON with "value" field
  - Example: `mydevice/params/set/heating/targetTemp` with payload `{"value": 23.5}`

- **Get parameter**: `{prefix}/get/{parameter_name}`
  - Response published to: `{prefix}/status/{parameter_name}`
  - Request all: `{prefix}/get/all`

- **List parameters**: `{prefix}/list`
  - Response: JSON array of parameter names

- **Save all**: `{prefix}/save`
  - Saves all parameters to NVS

### Integration Example

```cpp
#include <MQTTManager.h>
#include <PersistentStorage.h>

MQTTManager mqttManager;
PersistentStorage storage;

void onMqttMessage(const char* topic, const char* payload) {
    // Let storage handle parameter commands
    if (storage.handleMqttCommand(topic, payload)) {
        return;  // Command was handled
    }
    
    // Handle other MQTT messages
}

void setup() {
    // Initialize MQTT
    mqttManager.begin();
    mqttManager.setCallback(onMqttMessage);
    
    // Initialize storage with MQTT
    storage.begin();
    storage.setMqttManager(&mqttManager);
    
    // Subscribe to parameter topics
    mqttManager.subscribe("mydevice/params/set/+");
    mqttManager.subscribe("mydevice/params/get/+");
    mqttManager.subscribe("mydevice/params/list");
    mqttManager.subscribe("mydevice/params/save");
}
```

## Parameter Types

### Boolean
```cpp
bool enabled = true;
storage.registerBool("system/enabled", &enabled);
```

### Integer (with range validation)
```cpp
int32_t interval = 5000;
storage.registerInt("sensor/interval", &interval, 1000, 60000);
```

### Float (with range validation)
```cpp
float temperature = 22.5;
storage.registerFloat("heating/setpoint", &temperature, 10.0, 30.0);
```

### String (with max length)
```cpp
char name[64] = "My Device";
storage.registerString("device/name", name, sizeof(name));
```

### Binary Blob
```cpp
uint8_t calibData[256];
storage.registerBlob("sensor/calibration", calibData, sizeof(calibData));
```

## Advanced Features

### Custom Validators

```cpp
storage.setValidator("heating/targetTemp", [](const void* value) {
    float temp = *(float*)value;
    // Custom validation logic
    return temp >= 15.0 && temp <= 30.0;
});
```

### Change Notifications

```cpp
storage.setOnChange("pid/kp", [](const std::string& name, const void* value) {
    float kp = *(float*)value;
    // Update PID controller
    pidController.setKp(kp);
});
```

### Parameter Grouping

```cpp
// List all heating parameters
auto heatingParams = storage.listByPrefix("heating/");
for (const auto& param : heatingParams) {
    Serial.println(param.c_str());
}
```

### Access Control

```cpp
// Read-only parameter
storage.registerFloat("sensor/temperature", &currentTemp, -50.0, 100.0,
                     "Current temperature", ParameterInfo::ACCESS_READ_ONLY);
```

## JSON Format

Parameters are serialized to JSON with metadata:

```json
{
  "name": "heating/targetTemp",
  "description": "Target room temperature",
  "type": "float",
  "value": 22.5,
  "min": 15.0,
  "max": 30.0,
  "access": "rw"
}
```

## NVS Considerations

- Parameter names longer than 15 characters are hashed for NVS storage
- Each parameter uses one NVS entry
- Maximum NVS value size is 4000 bytes (blob type)
- NVS wear leveling is handled automatically

## Thread Safety

The library is not thread-safe by default. If accessing parameters from multiple tasks:

```cpp
// Use external mutex
SemaphoreHandle_t paramMutex = xSemaphoreCreateMutex();

// In task 1
if (xSemaphoreTake(paramMutex, portMAX_DELAY)) {
    settings.targetTemp = 23.5;
    storage.save("heating/targetTemp");
    xSemaphoreGive(paramMutex);
}

// In task 2
if (xSemaphoreTake(paramMutex, portMAX_DELAY)) {
    float temp = settings.targetTemp;
    xSemaphoreGive(paramMutex);
}
```

## Error Handling

```cpp
auto result = storage.save("heating/targetTemp");
if (result != PersistentStorage::Result::SUCCESS) {
    Serial.printf("Save failed: %s\n", PersistentStorage::resultToString(result));
}
```

## Logging Configuration

This library supports flexible logging configuration with production-ready debug control.

### Using ESP-IDF Logging (Default)
No configuration needed. The library will use ESP-IDF logging.

### Using Custom Logger
Define `USE_CUSTOM_LOGGER` in your build flags:
```ini
build_flags = -DUSE_CUSTOM_LOGGER
lib_deps = 
    Logger  ; Must include Logger library when using custom logger
    PersistentStorage
```

In your application:
```cpp
#ifdef USE_CUSTOM_LOGGER
#include <Logger.h>
#include <LogInterfaceImpl.h>  // Include ONCE in main.cpp
#endif
#include <PersistentStorage.h>

void setup() {
#ifdef USE_CUSTOM_LOGGER
    // Initialize Logger
    Logger::getInstance().init(1024);
#endif
    
    PersistentStorage storage;
    storage.begin();
}
```

### Debug Logging
To enable debug/verbose logging for this library:
```ini
build_flags = -DPSTORAGE_DEBUG
```

**Important**: Without `PSTORAGE_DEBUG`, all debug and verbose logs are completely removed at compile time (zero overhead).

### Complete Example
```ini
[env:production]
; Production build - minimal logging
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = PersistentStorage

[env:debug]
; Debug build - all logging enabled
platform = espressif32
board = esp32dev
framework = arduino
build_flags = 
    -DUSE_CUSTOM_LOGGER  ; Use custom logger
    -DPSTORAGE_DEBUG     ; Enable debug for this library
lib_deps = 
    Logger
    PersistentStorage
```

### Debug Features

When `PSTORAGE_DEBUG` is enabled, additional debugging tools are available:

```cpp
// Performance timing
PSTOR_TIME_START();
// ... code to measure ...
PSTOR_TIME_END("operation name");

// Buffer dumps
uint8_t data[16] = {0x01, 0x02, 0x03, 0x04};
PSTOR_DUMP_BUFFER("Config data", data, sizeof(data));
```

### Log Levels

- **Error/Warning/Info**: Always available (production ready)
- **Debug/Verbose**: Only with `PSTORAGE_DEBUG` flag (zero overhead when disabled)

## License

GPL-3 License