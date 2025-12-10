/**
 * @file test_persistent_storage.cpp
 * @brief Unit tests for PersistentStorage library
 * 
 * These tests can be run in any ESP32 project that includes the PersistentStorage library.
 * The tests use the Unity testing framework which is commonly used with PlatformIO.
 */

#include <unity.h>
#include <PersistentStorage.h>
#include <ArduinoJson.h>
#include <string.h>

// Test fixtures
static PersistentStorage* storage = nullptr;
static const char* TEST_NAMESPACE = "test_ps";
static const char* TEST_MQTT_PREFIX = "test/params";

// Test data
static bool testBool = false;
static int32_t testInt = 0;
static float testFloat = 0.0f;
static char testString[64] = "";
static uint8_t testBlob[32] = {0};

// Callback tracking
static int callbackCount = 0;
static std::string lastCallbackParam = "";

// Helper functions
void resetTestData() {
    testBool = false;
    testInt = 0;
    testFloat = 0.0f;
    memset(testString, 0, sizeof(testString));
    memset(testBlob, 0, sizeof(testBlob));
    callbackCount = 0;
    lastCallbackParam = "";
}

void testCallback(const std::string& name, const void* value) {
    callbackCount++;
    lastCallbackParam = name;
}

bool testValidator(const void* value) {
    // Example validator - accepts only positive integers
    int32_t val = *(int32_t*)value;
    return val > 0;
}

// Setup and teardown
void setUp() {
    resetTestData();
    storage = new PersistentStorage(TEST_NAMESPACE, TEST_MQTT_PREFIX);
    TEST_ASSERT_NOT_NULL(storage);
    TEST_ASSERT_TRUE(storage->begin());
}

void tearDown() {
    if (storage) {
        storage->end();
        delete storage;
        storage = nullptr;
    }
}

// Test cases

void test_initialization() {
    // Initialization is already tested in setUp
    TEST_ASSERT_NOT_NULL(storage);
}

void test_register_bool() {
    PersistentStorage::Result result = storage->registerBool("test/bool", &testBool, "Test boolean");
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    
    // Try to register same parameter again - should fail
    result = storage->registerBool("test/bool", &testBool);
    TEST_ASSERT_NOT_EQUAL(PersistentStorage::Result::SUCCESS, result);
}

void test_register_int() {
    PersistentStorage::Result result = storage->registerInt("test/int", &testInt, -100, 100, "Test integer");
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    
    // Test with invalid range (min > max)
    int32_t dummy;
    result = storage->registerInt("test/invalid", &dummy, 100, -100);
    TEST_ASSERT_NOT_EQUAL(PersistentStorage::Result::SUCCESS, result);
}

void test_register_float() {
    PersistentStorage::Result result = storage->registerFloat("test/float", &testFloat, -10.0f, 10.0f, "Test float");
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
}

void test_register_string() {
    PersistentStorage::Result result = storage->registerString("test/string", testString, sizeof(testString), "Test string");
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
}

void test_register_blob() {
    PersistentStorage::Result result = storage->registerBlob("test/blob", testBlob, sizeof(testBlob), "Test blob");
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
}

void test_set_get_bool() {
    storage->registerBool("test/bool", &testBool);
    
    // Set value
    bool newValue = true;
    PersistentStorage::Result result = storage->setBool("test/bool", newValue);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_TRUE(testBool);
    
    // Get value
    bool getValue = false;
    result = storage->getBool("test/bool", getValue);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_TRUE(getValue);
}

void test_set_get_int() {
    storage->registerInt("test/int", &testInt, -100, 100);
    
    // Set valid value
    int32_t newValue = 50;
    PersistentStorage::Result result = storage->setInt("test/int", newValue);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_EQUAL(50, testInt);
    
    // Try to set out of range value
    result = storage->setInt("test/int", 200);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::ERROR_VALIDATION_FAILED, result);
    TEST_ASSERT_EQUAL(50, testInt); // Value should not change
    
    // Get value
    int32_t getValue = 0;
    result = storage->getInt("test/int", getValue);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_EQUAL(50, getValue);
}

void test_set_get_float() {
    storage->registerFloat("test/float", &testFloat, -10.0f, 10.0f);
    
    // Set valid value
    float newValue = 5.5f;
    PersistentStorage::Result result = storage->setFloat("test/float", newValue);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.5f, testFloat);
    
    // Get value
    float getValue = 0.0f;
    result = storage->getFloat("test/float", getValue);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.5f, getValue);
}

void test_set_get_string() {
    storage->registerString("test/string", testString, sizeof(testString));
    
    // Set value
    const char* newValue = "Hello, World!";
    PersistentStorage::Result result = storage->setString("test/string", newValue);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING("Hello, World!", testString);
    
    // Get value
    char getValue[64] = "";
    result = storage->getString("test/string", getValue, sizeof(getValue));
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING("Hello, World!", getValue);
    
    // Try to set too long string
    char longString[128];
    memset(longString, 'A', sizeof(longString) - 1);
    longString[sizeof(longString) - 1] = '\0';
    result = storage->setString("test/string", longString);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::ERROR_TOO_LARGE, result);
}

void test_set_get_blob() {
    storage->registerBlob("test/blob", testBlob, sizeof(testBlob));
    
    // Set value
    uint8_t newValue[32];
    for (int i = 0; i < 32; i++) {
        newValue[i] = i;
    }
    
    PersistentStorage::Result result = storage->setBlob("test/blob", newValue, sizeof(newValue));
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(newValue, testBlob, sizeof(testBlob));
    
    // Get value
    uint8_t getValue[32] = {0};
    size_t size = sizeof(getValue);
    result = storage->getBlob("test/blob", getValue, size);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_EQUAL(sizeof(testBlob), size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(newValue, getValue, size);
}

void test_read_only_parameter() {
    // Register as read-only
    storage->registerInt("test/readonly", &testInt, -100, 100, "Read-only parameter", 
                        ParameterInfo::ACCESS_READ_ONLY);
    
    // Try to set value - should fail
    PersistentStorage::Result result = storage->setInt("test/readonly", 50);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::ERROR_ACCESS_DENIED, result);
    
    // Get should still work
    int32_t getValue = 0;
    result = storage->getInt("test/readonly", getValue);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
}

void test_callbacks() {
    storage->registerInt("test/int", &testInt, -100, 100);
    storage->setOnChange("test/int", testCallback);
    
    // Change value - should trigger callback
    storage->setInt("test/int", 50);
    TEST_ASSERT_EQUAL(1, callbackCount);
    TEST_ASSERT_EQUAL_STRING("test/int", lastCallbackParam.c_str());
    
    // Set same value - should not trigger callback
    storage->setInt("test/int", 50);
    TEST_ASSERT_EQUAL(1, callbackCount);
}

void test_validator() {
    storage->registerInt("test/validated", &testInt, -100, 100);
    storage->setValidator("test/validated", testValidator);
    
    // Set positive value - should succeed
    PersistentStorage::Result result = storage->setInt("test/validated", 10);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_EQUAL(10, testInt);
    
    // Set negative value - should fail validation
    result = storage->setInt("test/validated", -10);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::ERROR_VALIDATION_FAILED, result);
    TEST_ASSERT_EQUAL(10, testInt); // Value should not change
}

void test_save_load() {
    // Register and set values
    storage->registerBool("persist/bool", &testBool);
    storage->registerInt("persist/int", &testInt, -1000, 1000);
    storage->registerFloat("persist/float", &testFloat, -100.0f, 100.0f);
    storage->registerString("persist/string", testString, sizeof(testString));
    
    testBool = true;
    testInt = 42;
    testFloat = 3.14f;
    strcpy(testString, "Persistent");
    
    // Save all
    PersistentStorage::Result result = storage->saveAll();
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    
    // Reset values
    resetTestData();
    TEST_ASSERT_FALSE(testBool);
    TEST_ASSERT_EQUAL(0, testInt);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, testFloat);
    TEST_ASSERT_EQUAL_STRING("", testString);
    
    // Load all
    result = storage->loadAll();
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    
    // Verify loaded values
    TEST_ASSERT_TRUE(testBool);
    TEST_ASSERT_EQUAL(42, testInt);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.14f, testFloat);
    TEST_ASSERT_EQUAL_STRING("Persistent", testString);
}

void test_json_operations() {
    storage->registerInt("json/int", &testInt, -100, 100);
    storage->registerFloat("json/float", &testFloat, -10.0f, 10.0f);
    storage->registerString("json/string", testString, sizeof(testString));
    
    // Set via JSON
    StaticJsonDocument<256> setDoc;
    setDoc["value"] = 75;
    PersistentStorage::Result result = storage->setJson("json/int", setDoc);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_EQUAL(75, testInt);
    
    // Get as JSON
    StaticJsonDocument<256> getDoc;
    result = storage->getJson("json/int", getDoc);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_TRUE(getDoc.containsKey("name"));
    TEST_ASSERT_TRUE(getDoc.containsKey("value"));
    TEST_ASSERT_TRUE(getDoc.containsKey("type"));
    TEST_ASSERT_EQUAL(75, getDoc["value"].as<int>());
    
    // Set string via JSON
    setDoc.clear();
    setDoc["value"] = "JSON String";
    result = storage->setJson("json/string", setDoc);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING("JSON String", testString);
}

void test_list_parameters() {
    // Register multiple parameters
    storage->registerBool("list/bool", &testBool);
    storage->registerInt("list/int", &testInt, 0, 100);
    storage->registerFloat("list/float", &testFloat, 0.0f, 100.0f);
    
    std::vector<std::string> params = storage->listParameters();
    TEST_ASSERT_EQUAL(3, params.size());
    
    // Check that all parameters are in the list
    bool foundBool = false, foundInt = false, foundFloat = false;
    for (const auto& param : params) {
        if (param == "list/bool") foundBool = true;
        if (param == "list/int") foundInt = true;
        if (param == "list/float") foundFloat = true;
    }
    
    TEST_ASSERT_TRUE(foundBool);
    TEST_ASSERT_TRUE(foundInt);
    TEST_ASSERT_TRUE(foundFloat);
}

void test_hierarchical_names() {
    // Test hierarchical parameter organization
    bool heatingEnabled = false;
    float heatingTarget = 20.0f;
    float heatingCurrent = 18.0f;
    
    storage->registerBool("heating/enabled", &heatingEnabled);
    storage->registerFloat("heating/targetTemp", &heatingTarget, 10.0f, 30.0f);
    storage->registerFloat("heating/currentTemp", &heatingCurrent, -10.0f, 50.0f, 
                          "Current temperature", ParameterInfo::ACCESS_READ_ONLY);
    
    // Test setting values
    storage->setBool("heating/enabled", true);
    storage->setFloat("heating/targetTemp", 22.5f);
    
    TEST_ASSERT_TRUE(heatingEnabled);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.5f, heatingTarget);
    
    // Verify read-only parameter can't be set via API
    PersistentStorage::Result result = storage->setFloat("heating/currentTemp", 25.0f);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::ERROR_ACCESS_DENIED, result);
}

void test_invalid_operations() {
    // Test operations on non-existent parameter
    int32_t dummy;
    PersistentStorage::Result result = storage->getInt("nonexistent", dummy);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::ERROR_NOT_FOUND, result);
    
    // Test type mismatch
    storage->registerBool("typemismatch", &testBool);
    result = storage->getInt("typemismatch", dummy);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::ERROR_TYPE_MISMATCH, result);
    
    // Test invalid parameter name
    result = storage->registerBool("", &testBool);
    TEST_ASSERT_EQUAL(PersistentStorage::Result::ERROR_INVALID_NAME, result);
    
    // Test null pointer
    result = storage->registerBool("nullptr", nullptr);
    TEST_ASSERT_NOT_EQUAL(PersistentStorage::Result::SUCCESS, result);
}

// Test runner
void runPersistentStorageTests() {
    UNITY_BEGIN();
    
    // Basic tests
    RUN_TEST(test_initialization);
    RUN_TEST(test_register_bool);
    RUN_TEST(test_register_int);
    RUN_TEST(test_register_float);
    RUN_TEST(test_register_string);
    RUN_TEST(test_register_blob);
    
    // Get/Set tests
    RUN_TEST(test_set_get_bool);
    RUN_TEST(test_set_get_int);
    RUN_TEST(test_set_get_float);
    RUN_TEST(test_set_get_string);
    RUN_TEST(test_set_get_blob);
    
    // Feature tests
    RUN_TEST(test_read_only_parameter);
    RUN_TEST(test_callbacks);
    RUN_TEST(test_validator);
    RUN_TEST(test_save_load);
    RUN_TEST(test_json_operations);
    RUN_TEST(test_list_parameters);
    RUN_TEST(test_hierarchical_names);
    RUN_TEST(test_invalid_operations);
    
    UNITY_END();
}

// For use in Arduino setup()
#ifdef ARDUINO
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("Starting PersistentStorage Unit Tests...");
    runPersistentStorageTests();
}

void loop() {
    // Nothing to do
}
#endif