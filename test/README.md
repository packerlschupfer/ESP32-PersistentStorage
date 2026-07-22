# PersistentStorage Unit Tests

This directory contains comprehensive unit tests for the PersistentStorage library.

## Test Structure

- **test_persistent_storage.cpp** - Core functionality tests
  - Parameter registration (bool, int, float, string, blob)
  - Get/Set operations with type safety
  - Validation and range checking
  - Callbacks and change notifications
  - NVS persistence (save/load)
  - JSON operations
  - Hierarchical parameter organization
  - Error handling

- **test_persistent_storage_mqtt.cpp** - MQTT integration tests
  - Remote parameter access via MQTT
  - Command processing (set, get, list, save)
  - Status publishing
  - Grouped/chunked publishing
  - Connection handling
  - Mock MQTT manager for testing

- **test_runner.cpp** - Main test runner
  - Runs all test suites
  - Provides serial output formatting

## Running Tests

### Option 1: In Your Project

1. Copy the test files to your project's `test/` directory
2. Include the test runner in your main.cpp:

```cpp
#ifdef UNIT_TEST
#include "test_runner.cpp"
#else
// Your normal application code
#endif
```

3. Run tests with PlatformIO:
```bash
pio test -e test_persistent_storage
```

### Option 2: Standalone Test Project

1. Create a new PlatformIO project
2. Copy all test files to the `test/` directory
3. Copy or merge the provided `platformio.ini`
4. Add PersistentStorage library to lib_deps
5. Run: `pio test`

### Option 3: Integration in Existing Project

Add to your existing test suite:

```cpp
#include "test_persistent_storage.cpp"

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    // Run your existing tests...
    
    // Add PersistentStorage tests
    runPersistentStorageTests();
    runPersistentStorageMqttTests();
}
```

## Test Configuration

### Testing with Custom Logger

To test with custom Logger support:

```ini
build_flags = 
    -D PSTORAGE_USE_CUSTOM_LOGGER
lib_deps = 
    Logger  ; Your Logger library
```

### Testing without Custom Logger

Default configuration uses ESP-IDF logging:

```ini
; No special flags needed
```

## Mock MQTT Manager

The MQTT tests include a MockMQTTManager class that simulates MQTT functionality without requiring a real broker connection. This allows testing of:

- Command processing
- Status publishing  
- Connection state handling
- Message callbacks

## Test Coverage

The tests cover:

- ✅ All parameter types (bool, int, float, string, blob)
- ✅ Registration with constraints
- ✅ Type safety and validation
- ✅ Read-only parameters
- ✅ Change callbacks
- ✅ Custom validators
- ✅ NVS persistence
- ✅ JSON serialization
- ✅ MQTT commands
- ✅ Error conditions
- ✅ Thread safety (basic)
- ✅ Memory management

## Assertions

Tests use Unity framework assertions:
- `TEST_ASSERT_TRUE/FALSE`
- `TEST_ASSERT_EQUAL`
- `TEST_ASSERT_FLOAT_WITHIN`
- `TEST_ASSERT_EQUAL_STRING`
- `TEST_ASSERT_EQUAL_UINT8_ARRAY`
- `TEST_ASSERT_NOT_NULL`

## Customization

To add custom tests:

1. Add test functions following the pattern:
```cpp
void test_my_feature() {
    // Setup
    
    // Test
    TEST_ASSERT_EQUAL(expected, actual);
    
    // Cleanup
}
```

2. Register in the test runner:
```cpp
RUN_TEST(test_my_feature);
```

## Troubleshooting

- **NVS Full**: Tests may fail if NVS is full. Erase flash: `pio run -t erase`
- **Timing Issues**: Increase delays in MQTT tests if needed
- **Memory**: Monitor heap usage, especially with many parameters
- **Serial Output**: Ensure monitor_speed matches Serial.begin()