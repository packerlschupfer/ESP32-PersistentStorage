/**
 * @file test_persistent_storage_mqtt.cpp
 * @brief MQTT integration tests for PersistentStorage library
 * 
 * These tests require a mock or real MQTT connection to test the remote
 * parameter access functionality.
 */

#include <unity.h>
#include <PersistentStorage.h>
#include <MQTTManager.h>
#include <ArduinoJson.h>
#include <string.h>

// Test fixtures
static PersistentStorage* storage = nullptr;
static const char* TEST_NAMESPACE = "test_mqtt";
static const char* TEST_MQTT_PREFIX = "test/device/params";

// Mock MQTT Manager for testing
class MockMQTTManager : public MQTTManager {
private:
    bool connected_ = true;
    std::function<void(const char*, const char*)> messageHandler_;
    std::vector<std::pair<std::string, std::string>> publishedMessages_;

public:
    MockMQTTManager() : MQTTManager("mock_client", "mock_server", 1883) {}
    
    bool isConnected() const override { return connected_; }
    
    void setConnected(bool state) { connected_ = state; }
    
    bool publish(const char* topic, const char* payload, bool retain = false) override {
        if (!connected_) return false;
        publishedMessages_.push_back({topic, payload});
        return true;
    }
    
    void onMessage(std::function<void(const char*, const char*)> handler) override {
        messageHandler_ = handler;
    }
    
    void simulateMessage(const char* topic, const char* payload) {
        if (messageHandler_) {
            messageHandler_(topic, payload);
        }
    }
    
    void clearPublished() {
        publishedMessages_.clear();
    }
    
    const std::vector<std::pair<std::string, std::string>>& getPublished() const {
        return publishedMessages_;
    }
    
    bool wasPublished(const std::string& topic) const {
        for (const auto& msg : publishedMessages_) {
            if (msg.first == topic) return true;
        }
        return false;
    }
    
    std::string getPublishedPayload(const std::string& topic) const {
        for (const auto& msg : publishedMessages_) {
            if (msg.first == topic) return msg.second;
        }
        return "";
    }
};

static MockMQTTManager* mockMqtt = nullptr;

// Test data
static bool testBool = false;
static int32_t testInt = 0;
static float testFloat = 0.0f;
static char testString[64] = "";

// Helper to format topic
std::string formatTopic(const std::string& suffix) {
    return std::string(TEST_MQTT_PREFIX) + "/" + suffix;
}

// Setup and teardown
void setUp() {
    testBool = false;
    testInt = 0;
    testFloat = 0.0f;
    memset(testString, 0, sizeof(testString));
    
    storage = new PersistentStorage(TEST_NAMESPACE, TEST_MQTT_PREFIX);
    mockMqtt = new MockMQTTManager();
    
    TEST_ASSERT_NOT_NULL(storage);
    TEST_ASSERT_NOT_NULL(mockMqtt);
    TEST_ASSERT_TRUE(storage->begin());
    
    // Register test parameters
    storage->registerBool("mqtt/bool", &testBool, "Test boolean");
    storage->registerInt("mqtt/int", &testInt, -100, 100, "Test integer");
    storage->registerFloat("mqtt/float", &testFloat, -10.0f, 10.0f, "Test float");
    storage->registerString("mqtt/string", testString, sizeof(testString), "Test string");
    
    // Set MQTT manager
    storage->setMqttManager(mockMqtt);
    mockMqtt->clearPublished();
}

void tearDown() {
    if (storage) {
        storage->end();
        delete storage;
        storage = nullptr;
    }
    if (mockMqtt) {
        delete mockMqtt;
        mockMqtt = nullptr;
    }
}

// Test cases

void test_mqtt_set_command() {
    // Simulate MQTT set command for boolean
    std::string setTopic = formatTopic("set/mqtt/bool");
    mockMqtt->simulateMessage(setTopic.c_str(), "true");
    
    // Allow command processing
    delay(100);
    storage->processCommands();
    
    TEST_ASSERT_TRUE(testBool);
    
    // Verify status was published
    std::string statusTopic = formatTopic("status/mqtt/bool");
    TEST_ASSERT_TRUE(mockMqtt->wasPublished(statusTopic));
}

void test_mqtt_set_json() {
    // Set integer via JSON
    std::string setTopic = formatTopic("set/mqtt/int");
    mockMqtt->simulateMessage(setTopic.c_str(), "{\"value\": 42}");
    
    delay(100);
    storage->processCommands();
    
    TEST_ASSERT_EQUAL(42, testInt);
}

void test_mqtt_get_command() {
    // Set a value first
    testFloat = 3.14f;
    
    // Simulate MQTT get command
    std::string getTopic = formatTopic("get/mqtt/float");
    mockMqtt->simulateMessage(getTopic.c_str(), "");
    
    delay(100);
    storage->processCommands();
    
    // Verify parameter was published
    std::string statusTopic = formatTopic("status/mqtt/float");
    TEST_ASSERT_TRUE(mockMqtt->wasPublished(statusTopic));
    
    // Verify published data contains correct value
    std::string payload = mockMqtt->getPublishedPayload(statusTopic);
    StaticJsonDocument<256> doc;
    deserializeJson(doc, payload);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.14f, doc["value"].as<float>());
}

void test_mqtt_list_command() {
    // Simulate MQTT list command
    std::string listTopic = formatTopic("list");
    mockMqtt->simulateMessage(listTopic.c_str(), "");
    
    delay(100);
    storage->processCommands();
    
    // Verify parameters were published
    TEST_ASSERT_TRUE(mockMqtt->wasPublished(formatTopic("status/mqtt/bool")));
    TEST_ASSERT_TRUE(mockMqtt->wasPublished(formatTopic("status/mqtt/int")));
    TEST_ASSERT_TRUE(mockMqtt->wasPublished(formatTopic("status/mqtt/float")));
    TEST_ASSERT_TRUE(mockMqtt->wasPublished(formatTopic("status/mqtt/string")));
}

void test_mqtt_save_command() {
    // Set some values
    testBool = true;
    testInt = 99;
    
    // Simulate MQTT save command
    std::string saveTopic = formatTopic("save");
    mockMqtt->simulateMessage(saveTopic.c_str(), "");
    
    delay(100);
    storage->processCommands();
    
    // Reset values
    testBool = false;
    testInt = 0;
    
    // Load from NVS
    storage->loadAll();
    
    // Verify values were saved
    TEST_ASSERT_TRUE(testBool);
    TEST_ASSERT_EQUAL(99, testInt);
}

void test_mqtt_publish_all() {
    // Set some values
    testBool = true;
    testInt = 50;
    testFloat = 2.5f;
    strcpy(testString, "MQTT Test");
    
    // Clear previous publishes
    mockMqtt->clearPublished();
    
    // Publish all parameters
    storage->publishAll();
    
    // Verify all parameters were published
    TEST_ASSERT_TRUE(mockMqtt->wasPublished(formatTopic("status/mqtt/bool")));
    TEST_ASSERT_TRUE(mockMqtt->wasPublished(formatTopic("status/mqtt/int")));
    TEST_ASSERT_TRUE(mockMqtt->wasPublished(formatTopic("status/mqtt/float")));
    TEST_ASSERT_TRUE(mockMqtt->wasPublished(formatTopic("status/mqtt/string")));
    
    // Verify correct values in published data
    std::string boolPayload = mockMqtt->getPublishedPayload(formatTopic("status/mqtt/bool"));
    StaticJsonDocument<256> doc;
    deserializeJson(doc, boolPayload);
    TEST_ASSERT_TRUE(doc["value"].as<bool>());
}

void test_mqtt_disconnected() {
    // Disconnect MQTT
    mockMqtt->setConnected(false);
    mockMqtt->clearPublished();
    
    // Try to publish - should handle gracefully
    storage->publishAll();
    
    // Verify nothing was published
    TEST_ASSERT_EQUAL(0, mockMqtt->getPublished().size());
}

void test_mqtt_callback_publish() {
    // Test publishing via callback instead of MQTTManager
    storage->setMqttManager(nullptr);
    
    bool callbackCalled = false;
    std::string lastTopic, lastPayload;
    
    storage->setPublishCallback([&](const char* topic, const char* payload) -> bool {
        callbackCalled = true;
        lastTopic = topic;
        lastPayload = payload;
        return true;
    });
    
    // Set and trigger publish
    testInt = 123;
    storage->publishParameter("mqtt/int");
    
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_EQUAL_STRING(formatTopic("status/mqtt/int").c_str(), lastTopic.c_str());
    
    // Verify payload contains correct value
    StaticJsonDocument<256> doc;
    deserializeJson(doc, lastPayload);
    TEST_ASSERT_EQUAL(123, doc["value"].as<int>());
}

void test_mqtt_invalid_commands() {
    mockMqtt->clearPublished();
    
    // Invalid topic format
    mockMqtt->simulateMessage("invalid/topic", "data");
    delay(100);
    storage->processCommands();
    
    // Non-existent parameter
    std::string setTopic = formatTopic("set/nonexistent");
    mockMqtt->simulateMessage(setTopic.c_str(), "value");
    delay(100);
    storage->processCommands();
    
    // Invalid JSON
    setTopic = formatTopic("set/mqtt/int");
    mockMqtt->simulateMessage(setTopic.c_str(), "{invalid json}");
    delay(100);
    storage->processCommands();
    
    // Verify error handling didn't crash and original values unchanged
    TEST_ASSERT_FALSE(testBool);
    TEST_ASSERT_EQUAL(0, testInt);
}

void test_mqtt_grouped_publish() {
    // Register many parameters to test chunked publishing
    for (int i = 0; i < 20; i++) {
        char name[32];
        snprintf(name, sizeof(name), "group/param%d", i);
        storage->registerInt(name, &testInt, 0, 100);
    }
    
    mockMqtt->clearPublished();
    
    // Publish all grouped
    storage->publishAllGrouped();
    
    // Should have multiple group publishes
    int groupCount = 0;
    for (const auto& msg : mockMqtt->getPublished()) {
        if (msg.first.find("/status/group") != std::string::npos) {
            groupCount++;
        }
    }
    
    TEST_ASSERT_GREATER_THAN(0, groupCount);
}

// Test runner for MQTT tests
void runPersistentStorageMqttTests() {
    UNITY_BEGIN();
    
    RUN_TEST(test_mqtt_set_command);
    RUN_TEST(test_mqtt_set_json);
    RUN_TEST(test_mqtt_get_command);
    RUN_TEST(test_mqtt_list_command);
    RUN_TEST(test_mqtt_save_command);
    RUN_TEST(test_mqtt_publish_all);
    RUN_TEST(test_mqtt_disconnected);
    RUN_TEST(test_mqtt_callback_publish);
    RUN_TEST(test_mqtt_invalid_commands);
    RUN_TEST(test_mqtt_grouped_publish);
    
    UNITY_END();
}

// For use in Arduino setup() if running MQTT tests
#ifdef ARDUINO
#ifdef RUN_MQTT_TESTS
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("Starting PersistentStorage MQTT Tests...");
    runPersistentStorageMqttTests();
}

void loop() {
    // Nothing to do
}
#endif
#endif