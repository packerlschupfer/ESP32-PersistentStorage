#ifndef PERSISTENT_STORAGE_H
#define PERSISTENT_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <functional>
#include <map>
#include <vector>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Include the logging configuration
#include "PersistentStorageLogging.h"

// Forward declaration for MQTT integration
class MQTTManager;

/**
 * @brief Parameter metadata for registration
 */
struct ParameterInfo {
    enum Type {
        TYPE_BOOL,
        TYPE_INT,
        TYPE_FLOAT,
        TYPE_STRING,
        TYPE_BLOB
    };
    
    enum Access {
        ACCESS_READ_ONLY,
        ACCESS_READ_WRITE
    };
    
    std::string name;           // Parameter name (e.g., "heating/targetTemp")
    std::string description;    // Human-readable description
    Type type;                  // Data type
    Access access;              // Access level
    void* dataPtr;              // Pointer to actual data
    size_t size;                // Size for blob types
    
    // Constraints
    union {
        struct { int32_t min, max; } intRange;
        struct { float min, max; } floatRange;
        struct { size_t maxLen; } stringMax;
    } constraints;
    
    // Callbacks
    std::function<void(const std::string&, const void*)> onChange;
    std::function<bool(const void*)> validator;
};

/**
 * @brief Persistent Storage Manager with MQTT integration
 * 
 * Provides type-safe parameter storage with NVS backend and remote access via MQTT
 */
class PersistentStorage {
public:
    // Result type for operations
    enum class Result {
        SUCCESS,
        ERROR_NOT_FOUND,
        ERROR_TYPE_MISMATCH,
        ERROR_ACCESS_DENIED,
        ERROR_VALIDATION_FAILED,
        ERROR_NVS_FAIL,
        ERROR_INVALID_NAME,
        ERROR_TOO_LARGE
    };
    
    /**
     * @brief Constructor
     * @param namespaceName NVS namespace to use (max 15 chars)
     * @param mqttPrefix MQTT topic prefix (e.g., "esplan/params")
     */
    PersistentStorage(const char* namespaceName = "params", 
                      const char* mqttPrefix = "esplan/params");
    
    ~PersistentStorage();
    
    /**
     * @brief Initialize the storage system
     * @return true on success
     */
    bool begin();
    
    /**
     * @brief End storage system and free resources
     */
    void end();
    
    // Parameter registration methods
    
    /**
     * @brief Register a boolean parameter
     */
    Result registerBool(const std::string& name, bool* dataPtr, 
                       const std::string& description = "",
                       ParameterInfo::Access access = ParameterInfo::ACCESS_READ_WRITE);
    
    /**
     * @brief Register an integer parameter with optional range
     */
    Result registerInt(const std::string& name, int32_t* dataPtr,
                      int32_t minVal, int32_t maxVal,
                      const std::string& description = "",
                      ParameterInfo::Access access = ParameterInfo::ACCESS_READ_WRITE);
    
    /**
     * @brief Register a float parameter with optional range
     */
    Result registerFloat(const std::string& name, float* dataPtr,
                        float minVal, float maxVal,
                        const std::string& description = "",
                        ParameterInfo::Access access = ParameterInfo::ACCESS_READ_WRITE);
    
    /**
     * @brief Register a string parameter
     */
    Result registerString(const std::string& name, char* dataPtr, size_t maxLen,
                         const std::string& description = "",
                         ParameterInfo::Access access = ParameterInfo::ACCESS_READ_WRITE);
    
    /**
     * @brief Register a binary blob parameter
     */
    Result registerBlob(const std::string& name, void* dataPtr, size_t size,
                       const std::string& description = "",
                       ParameterInfo::Access access = ParameterInfo::ACCESS_READ_WRITE);
    
    /**
     * @brief Set change callback for a parameter
     */
    Result setOnChange(const std::string& name, 
                      std::function<void(const std::string&, const void*)> callback);
    
    /**
     * @brief Set validator callback for a parameter
     */
    Result setValidator(const std::string& name,
                       std::function<bool(const void*)> validator);
    
    // Storage operations
    
    /**
     * @brief Save a single parameter to NVS
     */
    Result save(const std::string& name);
    
    /**
     * @brief Save all registered parameters to NVS
     */
    Result saveAll();
    
    /**
     * @brief Load a single parameter from NVS
     */
    Result load(const std::string& name);
    
    /**
     * @brief Load all registered parameters from NVS
     * @param autoSaveDefaults If true and no parameters found in NVS (first boot),
     *                         automatically save default values to initialize NVS
     */
    Result loadAll(bool autoSaveDefaults = false);
    
    /**
     * @brief Reset a parameter to default value
     */
    Result reset(const std::string& name);
    
    /**
     * @brief Reset all parameters to defaults
     */
    Result resetAll();

    /**
     * @brief Erase the entire NVS namespace
     *
     * This removes all stored values from NVS. Use for recovery from corruption.
     * After calling this, loadAll() will return defaults and saveAll() can
     * repopulate the namespace with current values.
     *
     * @return true on success, false if NVS operation failed
     */
    bool eraseNamespace();
    
    // Value access methods
    
    /**
     * @brief Get parameter value as JSON
     */
    Result getJson(const std::string& name, JsonDocument& doc);
    
    /**
     * @brief Set parameter value from JSON
     */
    Result setJson(const std::string& name, const JsonDocument& doc);
    
    /**
     * @brief Get all parameters as JSON
     */
    void getAllJson(JsonDocument& doc);
    
    /**
     * @brief Get parameter info
     */
    const ParameterInfo* getInfo(const std::string& name) const;
    
    /**
     * @brief List all parameter names
     */
    std::vector<std::string> listParameters() const;
    
    /**
     * @brief List parameters by prefix (e.g., "heating/")
     */
    std::vector<std::string> listByPrefix(const std::string& prefix) const;
    
    // MQTT integration
    
    /**
     * @brief Set MQTT manager for remote access
     */
    void setMqttManager(MQTTManager* mqtt);
    
    /**
     * @brief Set MQTT publish callback for thread-safe publishing
     */
    void setMqttPublishCallback(std::function<bool(const char*, const char*, int, bool)> callback);
    
    /**
     * @brief Handle MQTT command
     * @return true if command was handled
     */
    bool handleMqttCommand(const std::string& topic, const std::string& payload);
    
    /**
     * @brief Publish parameter update via MQTT
     */
    void publishUpdate(const std::string& name);
    
    /**
     * @brief Publish all parameters via MQTT
     */
    void publishAll();
    
    /**
     * @brief Publish all parameters via MQTT grouped by category
     */
    void publishAllGrouped();
    void publishGroupedCategory(const std::string& category);
    
    /**
     * @brief Process queued commands (call from task loop)
     */
    void processCommandQueue();
    
    /**
     * @brief Continue async publishing if in progress
     */
    void continueAsyncPublish();
    
    // Utility methods
    
    /**
     * @brief Get error string for result code
     */
    static const char* resultToString(Result result);
    
    /**
     * @brief Check if storage is initialized
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * @brief Get number of registered parameters
     */
    size_t getParameterCount() const { return parameters_.size(); }
    
    /**
     * @brief Get NVS statistics
     */
    void getNvsStats(size_t& usedEntries, size_t& freeEntries, size_t& totalEntries);

private:
    // Command queue for async processing
    struct ParameterCommand {
        enum Type { GET, SET, LIST, SAVE, GET_ALL };
        Type type;
        char paramName[48];  // Reduced from 64
        char payload[64];    // Reduced from 128 to save stack
    };
    
    // Constants
    static constexpr size_t COMMAND_QUEUE_SIZE = 5;  // Reduced from 10
    static constexpr size_t PARAMS_PER_CHUNK = 5;
    
    // NVS namespace and preferences
    Preferences preferences_;
    std::string namespaceName_;
    std::string mqttPrefix_;
    bool initialized_;
    
    // Parameter registry
    std::map<std::string, ParameterInfo> parameters_;
    
    // MQTT manager reference
    MQTTManager* mqttManager_;
    
    // MQTT publish callback
    std::function<bool(const char*, const char*, int, bool)> mqttPublishCallback_;
    
    // Async publishing state
    QueueHandle_t commandQueue_;
    volatile bool isPublishing_;
    volatile size_t nextParamIndex_;
    volatile size_t totalParams_;
    
    // Thread safety
    SemaphoreHandle_t publishMutex_;
    
    // Helper methods
    bool validateParameterName(const std::string& name) const;
    std::string sanitizeNvsKey(const std::string& name) const;
    Result loadParameter(ParameterInfo& param);
    Result saveParameter(const ParameterInfo& param);
    void notifyChange(const std::string& name, const void* newValue);
    
    // JSON conversion helpers
    void parameterToJson(const ParameterInfo& param, JsonDocument& doc);
    Result jsonToParameter(ParameterInfo& param, const JsonDocument& doc);
    
    // Async publishing helper
    void publishAllAsync();
};

#endif // PERSISTENT_STORAGE_H