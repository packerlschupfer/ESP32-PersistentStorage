#include "PersistentStorage.h"
#include <algorithm>
#include <cstring>
#include <MQTTManager.h>
#include <esp_task_wdt.h>
#include <nvs.h>

// Constructor
PersistentStorage::PersistentStorage(const char* namespaceName, const char* mqttPrefix) 
    : namespaceName_(namespaceName)
    , mqttPrefix_(mqttPrefix)
    , initialized_(false)
    , mqttManager_(nullptr)
    , commandQueue_(nullptr)
    , isPublishing_(false)
    , nextParamIndex_(0)
    , totalParams_(0)
    , publishMutex_(nullptr) {
    
    // Create command queue
    commandQueue_ = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(ParameterCommand));
    if (!commandQueue_) {
        PSTOR_LOG_E( "Failed to create command queue");
    }
    
    // Create mutex for thread safety
    publishMutex_ = xSemaphoreCreateMutex();
    if (!publishMutex_) {
        PSTOR_LOG_E( "Failed to create publish mutex");
    }
}

// Destructor
PersistentStorage::~PersistentStorage() {
    if (initialized_) {
        end();
    }
    
    // Delete command queue
    if (commandQueue_) {
        vQueueDelete(commandQueue_);
        commandQueue_ = nullptr;
    }
    
    // Delete mutex
    if (publishMutex_) {
        vSemaphoreDelete(publishMutex_);
        publishMutex_ = nullptr;
    }
}

// Initialize the storage system
bool PersistentStorage::begin() {
    if (initialized_) {
        PSTOR_LOG_W( "Already initialized");
        return true;
    }
    
    // Open NVS in read-write mode
    if (!preferences_.begin(namespaceName_.c_str(), false)) {
        PSTOR_LOG_E( "Failed to open NVS namespace: %s", 
                                 namespaceName_.c_str());
        return false;
    }
    
    initialized_ = true;
    PSTOR_LOG_I( "Initialized with namespace: %s", 
                             namespaceName_.c_str());
    
    // Load all registered parameters
    loadAll();
    
    return true;
}

// End storage system and free resources
void PersistentStorage::end() {
    if (!initialized_) {
        return;
    }
    
    // Save all parameters before closing
    saveAll();
    
    preferences_.end();
    initialized_ = false;
    
    PSTOR_LOG_I( "Storage system closed");
}

// Register a boolean parameter
PersistentStorage::Result PersistentStorage::registerBool(
    const std::string& name, bool* dataPtr, 
    const std::string& description,
    ParameterInfo::Access access) {
    
    if (!validateParameterName(name)) {
        return Result::ERROR_INVALID_NAME;
    }
    
    ParameterInfo info;
    info.name = name;
    info.description = description;
    info.type = ParameterInfo::TYPE_BOOL;
    info.access = access;
    info.dataPtr = dataPtr;
    info.size = sizeof(bool);
    
    parameters_[name] = info;
    
    PSTOR_LOG_D( "Registered bool parameter: %s", name.c_str());
    
    // Load value if storage is already initialized
    if (initialized_) {
        load(name);
    }
    
    return Result::SUCCESS;
}

// Register an integer parameter
PersistentStorage::Result PersistentStorage::registerInt(
    const std::string& name, int32_t* dataPtr,
    int32_t minVal, int32_t maxVal,
    const std::string& description,
    ParameterInfo::Access access) {
    
    if (!validateParameterName(name)) {
        return Result::ERROR_INVALID_NAME;
    }
    
    ParameterInfo info;
    info.name = name;
    info.description = description;
    info.type = ParameterInfo::TYPE_INT;
    info.access = access;
    info.dataPtr = dataPtr;
    info.size = sizeof(int32_t);
    info.constraints.intRange.min = minVal;
    info.constraints.intRange.max = maxVal;
    
    parameters_[name] = info;
    
    PSTOR_LOG_D( "Registered int parameter: %s [%d-%d]", 
                             name.c_str(), minVal, maxVal);
    
    if (initialized_) {
        load(name);
    }
    
    return Result::SUCCESS;
}

// Register a float parameter
PersistentStorage::Result PersistentStorage::registerFloat(
    const std::string& name, float* dataPtr,
    float minVal, float maxVal,
    const std::string& description,
    ParameterInfo::Access access) {
    
    if (!validateParameterName(name)) {
        return Result::ERROR_INVALID_NAME;
    }
    
    ParameterInfo info;
    info.name = name;
    info.description = description;
    info.type = ParameterInfo::TYPE_FLOAT;
    info.access = access;
    info.dataPtr = dataPtr;
    info.size = sizeof(float);
    info.constraints.floatRange.min = minVal;
    info.constraints.floatRange.max = maxVal;
    
    parameters_[name] = info;
    
    PSTOR_LOG_D( "Registered float parameter: %s [%.2f-%.2f]", 
                             name.c_str(), minVal, maxVal);
    
    if (initialized_) {
        load(name);
    }
    
    return Result::SUCCESS;
}

// Register a string parameter
PersistentStorage::Result PersistentStorage::registerString(
    const std::string& name, char* dataPtr, size_t maxLen,
    const std::string& description,
    ParameterInfo::Access access) {
    
    if (!validateParameterName(name)) {
        return Result::ERROR_INVALID_NAME;
    }
    
    ParameterInfo info;
    info.name = name;
    info.description = description;
    info.type = ParameterInfo::TYPE_STRING;
    info.access = access;
    info.dataPtr = dataPtr;
    info.size = maxLen;
    info.constraints.stringMax.maxLen = maxLen;
    
    parameters_[name] = info;
    
    PSTOR_LOG_D( "Registered string parameter: %s (max %d)", 
                             name.c_str(), maxLen);
    
    if (initialized_) {
        load(name);
    }
    
    return Result::SUCCESS;
}

// Register a binary blob parameter
PersistentStorage::Result PersistentStorage::registerBlob(
    const std::string& name, void* dataPtr, size_t size,
    const std::string& description,
    ParameterInfo::Access access) {
    
    if (!validateParameterName(name)) {
        return Result::ERROR_INVALID_NAME;
    }
    
    ParameterInfo info;
    info.name = name;
    info.description = description;
    info.type = ParameterInfo::TYPE_BLOB;
    info.access = access;
    info.dataPtr = dataPtr;
    info.size = size;
    
    parameters_[name] = info;
    
    PSTOR_LOG_D( "Registered blob parameter: %s (size %d)", 
                             name.c_str(), size);
    
    if (initialized_) {
        load(name);
    }
    
    return Result::SUCCESS;
}

// Set change callback for a parameter
PersistentStorage::Result PersistentStorage::setOnChange(const std::string& name, 
                      std::function<void(const std::string&, const void*)> callback) {
    auto it = parameters_.find(name);
    if (it == parameters_.end()) {
        return Result::ERROR_NOT_FOUND;
    }
    
    it->second.onChange = callback;
    return Result::SUCCESS;
}

// Set validator callback for a parameter
PersistentStorage::Result PersistentStorage::setValidator(const std::string& name,
                       std::function<bool(const void*)> validator) {
    auto it = parameters_.find(name);
    if (it == parameters_.end()) {
        return Result::ERROR_NOT_FOUND;
    }
    
    it->second.validator = validator;
    return Result::SUCCESS;
}

// Get parameter info
const ParameterInfo* PersistentStorage::getInfo(const std::string& name) const {
    auto it = parameters_.find(name);
    if (it == parameters_.end()) {
        return nullptr;
    }
    return &it->second;
}

// List parameters by prefix
std::vector<std::string> PersistentStorage::listByPrefix(const std::string& prefix) const {
    std::vector<std::string> result;
    for (const auto& pair : parameters_) {
        if (pair.first.find(prefix) == 0) {
            result.push_back(pair.first);
        }
    }
    return result;
}

// Reset a parameter to default value
PersistentStorage::Result PersistentStorage::reset(const std::string& name) {
    auto it = parameters_.find(name);
    if (it == parameters_.end()) {
        return Result::ERROR_NOT_FOUND;
    }
    
    // Remove from NVS
    std::string key = sanitizeNvsKey(name);
    preferences_.remove(key.c_str());
    
    return Result::SUCCESS;
}

// Reset all parameters to defaults
PersistentStorage::Result PersistentStorage::resetAll() {
    preferences_.clear();
    return Result::SUCCESS;
}

// Erase the entire NVS namespace
bool PersistentStorage::eraseNamespace() {
    // Close current handle if open
    if (initialized_) {
        preferences_.end();
        initialized_ = false;
    }

    // Open in read-write mode and clear
    if (!preferences_.begin(namespaceName_.c_str(), false)) {
        PSTOR_LOG_E("Failed to open NVS for erase: %s", namespaceName_.c_str());
        return false;
    }

    // Clear all keys in namespace
    bool success = preferences_.clear();

    if (success) {
        PSTOR_LOG_W("NVS namespace '%s' erased", namespaceName_.c_str());
    } else {
        PSTOR_LOG_E("Failed to erase NVS namespace: %s", namespaceName_.c_str());
    }

    // Close after erase - caller should call begin() to reinitialize
    preferences_.end();

    return success;
}

// Save a single parameter to NVS
PersistentStorage::Result PersistentStorage::save(const std::string& name) {
    if (!initialized_) {
        PSTOR_LOG_E( "Not initialized");
        return Result::ERROR_NVS_FAIL;
    }
    
    auto it = parameters_.find(name);
    if (it == parameters_.end()) {
        return Result::ERROR_NOT_FOUND;
    }
    
    return saveParameter(it->second);
}

// Save all parameters to NVS
PersistentStorage::Result PersistentStorage::saveAll() {
    if (!initialized_) {
        return Result::ERROR_NVS_FAIL;
    }
    
    Result lastResult = Result::SUCCESS;
    size_t savedCount = 0;
    
    for (auto& pair : parameters_) {
        Result res = saveParameter(pair.second);
        if (res == Result::SUCCESS) {
            savedCount++;
        } else {
            lastResult = res;
        }
    }
    
    PSTOR_LOG_I( "Saved %d/%d parameters", 
                             savedCount, parameters_.size());
    
    return lastResult;
}

// Load a single parameter from NVS
PersistentStorage::Result PersistentStorage::load(const std::string& name) {
    if (!initialized_) {
        return Result::ERROR_NVS_FAIL;
    }
    
    auto it = parameters_.find(name);
    if (it == parameters_.end()) {
        return Result::ERROR_NOT_FOUND;
    }
    
    return loadParameter(it->second);
}

// Load all parameters from NVS
PersistentStorage::Result PersistentStorage::loadAll(bool autoSaveDefaults) {
    if (!initialized_) {
        return Result::ERROR_NVS_FAIL;
    }

    Result lastResult = Result::SUCCESS;
    size_t loadedCount = 0;

    for (auto& pair : parameters_) {
        Result res = loadParameter(pair.second);
        if (res == Result::SUCCESS) {
            loadedCount++;
        } else {
            lastResult = res;
        }
    }

    PSTOR_LOG_I("Loaded %d/%d parameters", loadedCount, parameters_.size());

    // Auto-save defaults on first boot (when no parameters exist in NVS)
    if (autoSaveDefaults && loadedCount == 0 && !parameters_.empty()) {
        PSTOR_LOG_I("First boot detected - saving default parameters to NVS...");
        saveAll();
        return Result::SUCCESS;  // Defaults saved successfully
    }

    return lastResult;
}

// Get parameter value as JSON
PersistentStorage::Result PersistentStorage::getJson(const std::string& name, JsonDocument& doc) {
    auto it = parameters_.find(name);
    if (it == parameters_.end()) {
        return Result::ERROR_NOT_FOUND;
    }
    
    parameterToJson(it->second, doc);
    return Result::SUCCESS;
}

// Set parameter value from JSON
PersistentStorage::Result PersistentStorage::setJson(const std::string& name, const JsonDocument& doc) {
    auto it = parameters_.find(name);
    if (it == parameters_.end()) {
        return Result::ERROR_NOT_FOUND;
    }
    
    if (it->second.access == ParameterInfo::ACCESS_READ_ONLY) {
        return Result::ERROR_ACCESS_DENIED;
    }
    
    Result res = jsonToParameter(it->second, doc);
    if (res == Result::SUCCESS) {
        // Save to NVS
        saveParameter(it->second);
        
        // Notify change
        notifyChange(name, it->second.dataPtr);
        
        // Publish via MQTT if available
        if (mqttManager_) {
            publishUpdate(name);
        }
    }
    
    return res;
}

// Get all parameters as JSON
void PersistentStorage::getAllJson(JsonDocument& doc) {
    doc.clear();
    JsonObject root = doc.to<JsonObject>();
    
    // Add a summary instead of all parameters to avoid stack overflow
    root["parameterCount"] = parameters_.size();
    root["message"] = "Use individual parameter queries to avoid memory issues";
    root["timestamp"] = millis();
    
    // Add just parameter names as an array
    JsonArray names = root["parameters"].to<JsonArray>();
    for (const auto& pair : parameters_) {
        names.add(pair.first);
    }
}

// List all parameter names
std::vector<std::string> PersistentStorage::listParameters() const {
    std::vector<std::string> names;
    names.reserve(parameters_.size());
    
    for (const auto& pair : parameters_) {
        names.push_back(pair.first);
    }
    
    return names;
}

// Set MQTT manager for remote access
void PersistentStorage::setMqttManager(MQTTManager* mqtt) {
    mqttManager_ = mqtt;
    
    if (mqtt) {
        PSTOR_LOG_I( "MQTT manager set, remote access enabled");
        
        // Subscribe to parameter topics
        // This would be done in the main application to avoid circular dependencies
    }
}

// Set MQTT publish callback for thread-safe publishing
void PersistentStorage::setMqttPublishCallback(std::function<bool(const char*, const char*, int, bool)> callback) {
    mqttPublishCallback_ = callback;
    PSTOR_LOG_I( "MQTT publish callback set");
}

// Handle MQTT command
bool PersistentStorage::handleMqttCommand(const std::string& topic, const std::string& payload) {
    PSTOR_LOG_I( "handleMqttCommand called - topic: %s, payload: %s", 
                             topic.c_str(), payload.c_str());
    
    // Queue command for async processing to avoid blocking MQTT task
    if (!commandQueue_) {
        PSTOR_LOG_E( "Command queue not initialized");
        return false;
    }
    
    // Extract command from topic
    if (topic.find(mqttPrefix_) != 0) {
        PSTOR_LOG_W( "Topic doesn't match prefix. Topic: %s, Prefix: %s", 
                                 topic.c_str(), mqttPrefix_.c_str());
        return false;  // Not our topic
    }
    
    std::string subTopic = topic.substr(mqttPrefix_.length() + 1);  // Skip prefix and '/'
    
    ParameterCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    
    if (subTopic.find("set/") == 0) {
        cmd.type = ParameterCommand::SET;
        std::string paramName = subTopic.substr(4);  // Skip "set/"
        strncpy(cmd.paramName, paramName.c_str(), sizeof(cmd.paramName) - 1);
        strncpy(cmd.payload, payload.c_str(), sizeof(cmd.payload) - 1);
    } else if (subTopic == "get/all") {
        cmd.type = ParameterCommand::GET_ALL;
        strcpy(cmd.paramName, "all");
    } else if (subTopic.find("get/") == 0) {
        cmd.type = ParameterCommand::GET;
        std::string paramName = subTopic.substr(4);  // Skip "get/"
        strncpy(cmd.paramName, paramName.c_str(), sizeof(cmd.paramName) - 1);
    } else if (subTopic == "list") {
        cmd.type = ParameterCommand::LIST;
    } else if (subTopic == "save") {
        cmd.type = ParameterCommand::SAVE;
    } else {
        return false;  // Unknown command
    }
    
    // Queue the command - don't wait if queue is full
    if (xQueueSend(commandQueue_, &cmd, 0) != pdTRUE) {
        PSTOR_LOG_W( "Command queue full, dropping command");
        return true;  // Still return true as we handled the topic
    }
    
    PSTOR_LOG_D( "Queued command type %d for %s", 
                             cmd.type, cmd.paramName);
    return true;
}

// Private helper methods

bool PersistentStorage::validateParameterName(const std::string& name) const {
    if (name.empty() || name.length() > 64) {
        return false;
    }
    
    // Check for valid characters (alphanumeric, underscore, slash)
    for (char c : name) {
        if (!isalnum(c) && c != '_' && c != '/') {
            return false;
        }
    }
    
    return true;
}

std::string PersistentStorage::sanitizeNvsKey(const std::string& name) const {
    // NVS keys have max 15 chars, so we need to hash longer names
    if (name.length() <= 15) {
        return name;
    }
    
    // Simple hash for demo - in production use proper hashing
    uint32_t hash = 0;
    for (char c : name) {
        hash = hash * 31 + c;
    }
    
    char buf[16];
    snprintf(buf, sizeof(buf), "p%lu", (unsigned long)hash);
    return std::string(buf);
}

PersistentStorage::Result PersistentStorage::loadParameter(ParameterInfo& param) {
    std::string key = sanitizeNvsKey(param.name);
    
    switch (param.type) {
        case ParameterInfo::TYPE_BOOL: {
            bool defaultVal = *(bool*)param.dataPtr;
            *(bool*)param.dataPtr = preferences_.getBool(key.c_str(), defaultVal);
            break;
        }
        
        case ParameterInfo::TYPE_INT: {
            int32_t defaultVal = *(int32_t*)param.dataPtr;
            *(int32_t*)param.dataPtr = preferences_.getInt(key.c_str(), defaultVal);
            break;
        }
        
        case ParameterInfo::TYPE_FLOAT: {
            float defaultVal = *(float*)param.dataPtr;
            *(float*)param.dataPtr = preferences_.getFloat(key.c_str(), defaultVal);
            break;
        }
        
        case ParameterInfo::TYPE_STRING: {
            preferences_.getString(key.c_str(), (char*)param.dataPtr, param.size);
            break;
        }
        
        case ParameterInfo::TYPE_BLOB: {
            size_t len = preferences_.getBytesLength(key.c_str());
            if (len > 0 && len <= param.size) {
                preferences_.getBytes(key.c_str(), param.dataPtr, param.size);
            }
            break;
        }
    }
    
    return Result::SUCCESS;
}

PersistentStorage::Result PersistentStorage::saveParameter(const ParameterInfo& param) {
    std::string key = sanitizeNvsKey(param.name);
    bool success = false;
    
    switch (param.type) {
        case ParameterInfo::TYPE_BOOL:
            success = preferences_.putBool(key.c_str(), *(bool*)param.dataPtr);
            break;
            
        case ParameterInfo::TYPE_INT:
            success = preferences_.putInt(key.c_str(), *(int32_t*)param.dataPtr);
            break;
            
        case ParameterInfo::TYPE_FLOAT:
            success = preferences_.putFloat(key.c_str(), *(float*)param.dataPtr);
            break;
            
        case ParameterInfo::TYPE_STRING:
            success = preferences_.putString(key.c_str(), (const char*)param.dataPtr);
            break;
            
        case ParameterInfo::TYPE_BLOB:
            success = preferences_.putBytes(key.c_str(), param.dataPtr, param.size);
            break;
    }
    
    return success ? Result::SUCCESS : Result::ERROR_NVS_FAIL;
}

void PersistentStorage::parameterToJson(const ParameterInfo& param, JsonDocument& doc) {
    doc.clear();
    JsonObject root = doc.to<JsonObject>();
    
    root["name"] = param.name;
    root["description"] = param.description;
    root["access"] = (param.access == ParameterInfo::ACCESS_READ_ONLY) ? "ro" : "rw";
    
    switch (param.type) {
        case ParameterInfo::TYPE_BOOL:
            root["type"] = "bool";
            root["value"] = *(bool*)param.dataPtr;
            break;
            
        case ParameterInfo::TYPE_INT:
            root["type"] = "int";
            root["value"] = *(int32_t*)param.dataPtr;
            root["min"] = param.constraints.intRange.min;
            root["max"] = param.constraints.intRange.max;
            break;
            
        case ParameterInfo::TYPE_FLOAT:
            root["type"] = "float";
            root["value"] = *(float*)param.dataPtr;
            root["min"] = param.constraints.floatRange.min;
            root["max"] = param.constraints.floatRange.max;
            break;
            
        case ParameterInfo::TYPE_STRING:
            root["type"] = "string";
            root["value"] = (const char*)param.dataPtr;
            root["maxLen"] = param.constraints.stringMax.maxLen;
            break;
            
        case ParameterInfo::TYPE_BLOB:
            root["type"] = "blob";
            root["size"] = param.size;
            // Don't include blob data in JSON
            break;
    }
}

PersistentStorage::Result PersistentStorage::jsonToParameter(ParameterInfo& param, const JsonDocument& doc) {
    // Use isNull() instead of containsKey() for ArduinoJson v7
    if (doc["value"].isNull()) {
        return Result::ERROR_VALIDATION_FAILED;
    }
    
    // Store old value for validation
    void* oldValue = nullptr;
    switch (param.type) {
        case ParameterInfo::TYPE_BOOL: {
            bool newVal = doc["value"].as<bool>();
            oldValue = malloc(sizeof(bool));
            *(bool*)oldValue = *(bool*)param.dataPtr;
            *(bool*)param.dataPtr = newVal;
            break;
        }
        
        case ParameterInfo::TYPE_INT: {
            int32_t newVal = doc["value"].as<int32_t>();
            if (newVal < param.constraints.intRange.min || newVal > param.constraints.intRange.max) {
                return Result::ERROR_VALIDATION_FAILED;
            }
            oldValue = malloc(sizeof(int32_t));
            *(int32_t*)oldValue = *(int32_t*)param.dataPtr;
            *(int32_t*)param.dataPtr = newVal;
            break;
        }
        
        case ParameterInfo::TYPE_FLOAT: {
            float newVal = doc["value"].as<float>();
            if (newVal < param.constraints.floatRange.min || newVal > param.constraints.floatRange.max) {
                return Result::ERROR_VALIDATION_FAILED;
            }
            oldValue = malloc(sizeof(float));
            *(float*)oldValue = *(float*)param.dataPtr;
            *(float*)param.dataPtr = newVal;
            break;
        }
        
        case ParameterInfo::TYPE_STRING: {
            const char* newVal = doc["value"].as<const char*>();
            if (!newVal || strlen(newVal) >= param.constraints.stringMax.maxLen) {
                return Result::ERROR_VALIDATION_FAILED;
            }
            oldValue = malloc(param.size);
            strcpy((char*)oldValue, (char*)param.dataPtr);
            strcpy((char*)param.dataPtr, newVal);
            break;
        }
        
        default:
            return Result::ERROR_TYPE_MISMATCH;
    }
    
    // Run custom validator if set
    if (param.validator && !param.validator(param.dataPtr)) {
        // Restore old value
        switch (param.type) {
            case ParameterInfo::TYPE_BOOL:
                *(bool*)param.dataPtr = *(bool*)oldValue;
                break;
            case ParameterInfo::TYPE_INT:
                *(int32_t*)param.dataPtr = *(int32_t*)oldValue;
                break;
            case ParameterInfo::TYPE_FLOAT:
                *(float*)param.dataPtr = *(float*)oldValue;
                break;
            case ParameterInfo::TYPE_STRING:
                strcpy((char*)param.dataPtr, (char*)oldValue);
                break;
            case ParameterInfo::TYPE_BLOB:
                // BLOB types cannot be changed via JSON
                break;
        }
        free(oldValue);
        return Result::ERROR_VALIDATION_FAILED;
    }
    
    free(oldValue);
    return Result::SUCCESS;
}

void PersistentStorage::notifyChange(const std::string& name, const void* newValue) {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second.onChange) {
        it->second.onChange(name, newValue);
    }
}

const char* PersistentStorage::resultToString(Result result) {
    switch (result) {
        case Result::SUCCESS: return "Success";
        case Result::ERROR_NOT_FOUND: return "Parameter not found";
        case Result::ERROR_TYPE_MISMATCH: return "Type mismatch";
        case Result::ERROR_ACCESS_DENIED: return "Access denied";
        case Result::ERROR_VALIDATION_FAILED: return "Validation failed";
        case Result::ERROR_NVS_FAIL: return "NVS operation failed";
        case Result::ERROR_INVALID_NAME: return "Invalid parameter name";
        case Result::ERROR_TOO_LARGE: return "Value too large";
        default: return "Unknown error";
    }
}

void PersistentStorage::publishUpdate(const std::string& name) {
    // Only check connection if not using callback
    if (!mqttPublishCallback_) {
        if (!mqttManager_) return;

        // Check connection before publishing
        if (!mqttManager_->isConnected()) {
            PSTOR_LOG_D( "MQTT not connected, skipping publish of %s",
                                     name.c_str());
            return;
        }
    }
    
    auto it = parameters_.find(name);
    if (it == parameters_.end()) return;

    JsonDocument doc;  // ArduinoJson v7
    parameterToJson(it->second, doc);
    
    std::string topic = mqttPrefix_ + "/status/" + name;
    char buffer[256];
    serializeJson(doc, buffer, sizeof(buffer));
    
    // Use callback if available, otherwise direct publish
    if (mqttPublishCallback_) {
        if (!mqttPublishCallback_(topic.c_str(), buffer, 0, false)) {
            PSTOR_LOG_W( "Failed to publish parameter %s via callback", name.c_str());
        }
    } else {
        auto result = mqttManager_->publish(topic.c_str(), buffer, 0, false);
        if (!result.isOk()) {
            PSTOR_LOG_W( "Failed to publish parameter %s: %s", 
                                     name.c_str(), 
                                     result.error() == MQTTError::CONNECTION_FAILED ? "Not connected" : "Publish failed");
        }
    }
}

void PersistentStorage::publishAll() {
    // This is now just a wrapper for async publishing
    publishAllAsync();
}

void PersistentStorage::publishAllGrouped() {
    PSTOR_LOG_I( "publishAllGrouped called");

    // Only check connection if not using callback
    if (!mqttPublishCallback_) {
        if (!mqttManager_) {
            PSTOR_LOG_W( "MQTT manager not set");
            return;
        }

        // Check MQTT connection
        if (!mqttManager_->isConnected()) {
            PSTOR_LOG_W( "MQTT not connected");
            return;
        }
    }

    // Auto-discover all unique group prefixes from registered parameters
    std::vector<std::string> groups;
    for (const auto& pair : parameters_) {
        size_t slashPos = pair.first.find('/');
        if (slashPos != std::string::npos) {
            std::string group = pair.first.substr(0, slashPos);
            // Check if group already in list
            bool found = false;
            for (const auto& g : groups) {
                if (g == group) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                groups.push_back(group);
            }
        }
    }

    // Process each discovered group
    for (const auto& group : groups) {
        publishGroupedCategory(group);
    }

    // Send completion message
    JsonDocument completeDoc;  // ArduinoJson v7
    completeDoc["status"] = "complete";
    completeDoc["timestamp"] = millis();
    completeDoc["groupsPublished"] = groups.size();
    
    char buffer[256];
    serializeJson(completeDoc, buffer, sizeof(buffer));
    std::string completeTopic = mqttPrefix_ + "/status/complete";
    
    if (mqttPublishCallback_) {
        mqttPublishCallback_(completeTopic.c_str(), buffer, 0, false);
    } else {
        (void)mqttManager_->publish(completeTopic.c_str(), buffer, 0, false);
    }
    
    PSTOR_LOG_I( "Grouped publishing complete");
}

void PersistentStorage::publishGroupedCategory(const std::string& category) {
    // Use JSON doc (ArduinoJson v7)
    JsonDocument doc;
    JsonObject rootObj = doc.to<JsonObject>();

    // For PID, create sub-objects
    JsonObject spaceHeating, waterHeater;
    if (category == "pid") {
        spaceHeating = rootObj["spaceHeating"].to<JsonObject>();
        waterHeater = rootObj["waterHeater"].to<JsonObject>();
    }

    // Iterate through all parameters for this category
    for (const auto& pair : parameters_) {
        const std::string& fullName = pair.first;
        const ParameterInfo& param = pair.second;

        // Skip read-only parameters in get/all
        if (param.access == ParameterInfo::ACCESS_READ_ONLY) {
            continue;
        }

        // Find the first '/' to determine the group
        size_t slashPos = fullName.find('/');
        if (slashPos == std::string::npos) {
            continue;  // Skip parameters without a group
        }

        // Compare group without allocating string
        if (fullName.compare(0, slashPos, category) != 0) {
            continue;  // Not our category
        }

        // Get parameter name (after first slash)
        const char* nameStart = fullName.c_str() + slashPos + 1;
        JsonObject targetObj = rootObj;

        // Handle PID parameters specially - find second slash
        const char* secondSlash = nullptr;
        if (category == "pid") {
            secondSlash = strchr(nameStart, '/');
            if (secondSlash) {
                size_t pidGroupLen = secondSlash - nameStart;
                if (pidGroupLen == 12 && strncmp(nameStart, "spaceHeating", 12) == 0) {
                    targetObj = spaceHeating;
                    nameStart = secondSlash + 1;
                } else if (pidGroupLen == 11 && strncmp(nameStart, "waterHeater", 11) == 0) {
                    targetObj = waterHeater;
                    nameStart = secondSlash + 1;
                }
            }
        }

        // Add parameter value using C-string name
        if (!targetObj.isNull()) {
            switch (param.type) {
                case ParameterInfo::TYPE_BOOL:
                    targetObj[nameStart] = *(bool*)param.dataPtr;
                    break;
                case ParameterInfo::TYPE_INT:
                    targetObj[nameStart] = *(int32_t*)param.dataPtr;
                    break;
                case ParameterInfo::TYPE_FLOAT:
                    targetObj[nameStart] = *(float*)param.dataPtr;
                    break;
                case ParameterInfo::TYPE_STRING:
                    targetObj[nameStart] = (const char*)param.dataPtr;
                    break;
                default:
                    break;
            }
        }
    }

    // Only publish if we have content
    if (doc.size() > 0) {
        // Use static buffer to avoid stack allocation each call
        static char buffer[256];
        char topicBuf[64];
        snprintf(topicBuf, sizeof(topicBuf), "%s/status/%s", mqttPrefix_.c_str(), category.c_str());
        serializeJson(doc, buffer, sizeof(buffer));

        bool success = false;
        if (mqttPublishCallback_) {
            success = mqttPublishCallback_(topicBuf, buffer, 0, false);
        } else {
            auto result = mqttManager_->publish(topicBuf, buffer, 0, false);
            success = result.isOk();
        }

        if (success) {
            PSTOR_LOG_I( "Published %s group", category.c_str());
        } else {
            PSTOR_LOG_E( "Failed to publish %s group", category.c_str());
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void PersistentStorage::publishAllAsync() {
    PSTOR_LOG_I( "publishAllAsync called");

    // Only check connection if not using callback
    if (!mqttPublishCallback_) {
        if (!mqttManager_) {
            PSTOR_LOG_W( "MQTT manager not set");
            return;
        }

        // Check MQTT connection
        if (!mqttManager_->isConnected()) {
            PSTOR_LOG_W( "MQTT not connected, deferring publish");
            return;
        }
    }
    
    // Take mutex to protect shared state
    if (!publishMutex_ || xSemaphoreTake(publishMutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        PSTOR_LOG_W( "Failed to acquire publish mutex");
        return;
    }
    
    if (isPublishing_) {
        PSTOR_LOG_I( "Already publishing parameters");
        xSemaphoreGive(publishMutex_);
        return;
    }
    
    // Check if we have parameters to publish
    if (parameters_.empty()) {
        PSTOR_LOG_W( "No parameters registered to publish");
        xSemaphoreGive(publishMutex_);
        return;
    }
    
    PSTOR_LOG_I( "Starting async parameter publish, %d parameters...", parameters_.size());
    
    // Initialize async publishing state
    isPublishing_ = true;
    nextParamIndex_ = 0;
    totalParams_ = parameters_.size();
    
    // First, send a summary with parameter count
    JsonDocument summaryDoc;  // ArduinoJson v7
    summaryDoc["parameterCount"] = parameters_.size();
    summaryDoc["timestamp"] = millis();
    summaryDoc["message"] = "Publishing parameters asynchronously";
    
    std::string summaryTopic = mqttPrefix_ + "/status/summary";
    char summaryBuffer[256];
    serializeJson(summaryDoc, summaryBuffer, sizeof(summaryBuffer));
    
    // Publish summary with error handling
    bool publishSuccess = false;
    if (mqttPublishCallback_) {
        publishSuccess = mqttPublishCallback_(summaryTopic.c_str(), summaryBuffer, 0, false);
    } else {
        auto summaryResult = mqttManager_->publish(summaryTopic.c_str(), summaryBuffer, 0, false);
        publishSuccess = summaryResult.isOk();
    }
    
    if (!publishSuccess) {
        PSTOR_LOG_W( "Failed to publish summary");
        isPublishing_ = false;
        nextParamIndex_ = 0;
        totalParams_ = 0;
        xSemaphoreGive(publishMutex_);
        return;
    }
    
    // The actual publishing will be done in continueAsyncPublish()
    PSTOR_LOG_I( "Async publish initiated, %d parameters to send", 
                             totalParams_);
    
    // Release mutex
    xSemaphoreGive(publishMutex_);
}

void PersistentStorage::continueAsyncPublish() {
    // Take mutex to check state
    if (!publishMutex_ || xSemaphoreTake(publishMutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    if (!isPublishing_) {
        xSemaphoreGive(publishMutex_);
        return;
    }

    // Only check connection if not using callback
    if (!mqttPublishCallback_) {
        if (!mqttManager_) {
            xSemaphoreGive(publishMutex_);
            return;
        }

        // Verify MQTT is still connected
        if (!mqttManager_->isConnected()) {
            PSTOR_LOG_W( "MQTT disconnected during publish");
            isPublishing_ = false;
            nextParamIndex_ = 0;
            totalParams_ = 0;
            xSemaphoreGive(publishMutex_);
            return;
        }
    }
    
    // Check if we're done
    if (nextParamIndex_ >= totalParams_) {
        PSTOR_LOG_I( "Finished publishing all %d parameters", totalParams_);
        isPublishing_ = false;
        nextParamIndex_ = 0;
        totalParams_ = 0;
        xSemaphoreGive(publishMutex_);
        return;
    }
    
    // Calculate how many to publish in this chunk
    size_t toPublish = std::min(PARAMS_PER_CHUNK, totalParams_ - nextParamIndex_);
    size_t startIndex = nextParamIndex_;
    
    // Update index for next iteration
    nextParamIndex_ += toPublish;
    
    // Release mutex before publishing
    xSemaphoreGive(publishMutex_);

    // Note: Removed esp_task_wdt_reset() - caller task may not be registered
    
    // Publish parameters by iterating through the map
    size_t currentIndex = 0;
    size_t published = 0;
    
    for (const auto& pair : parameters_) {
        // Skip to the start index
        if (currentIndex < startIndex) {
            currentIndex++;
            continue;
        }
        
        // Check if we've published enough
        if (published >= toPublish) {
            break;
        }
        
        // Publish this parameter
        JsonDocument paramDoc;  // ArduinoJson v7
        parameterToJson(pair.second, paramDoc);
        
        // Use static buffers to avoid dynamic allocation
        char topicBuffer[128];
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/status/%s", 
                 mqttPrefix_.c_str(), pair.first.c_str());
        
        char paramBuffer[512];
        serializeJson(paramDoc, paramBuffer, sizeof(paramBuffer));
        
        bool success = false;
        if (mqttPublishCallback_) {
            success = mqttPublishCallback_(topicBuffer, paramBuffer, 0, false);
        } else {
            auto result = mqttManager_->publish(topicBuffer, paramBuffer, 0, false);
            success = result.isOk();
            if (!success && result.error() == MQTTError::CONNECTION_FAILED) {
                PSTOR_LOG_W( "MQTT connection lost, stopping publish");
                // Reset publishing state
                if (xSemaphoreTake(publishMutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
                    isPublishing_ = false;
                    nextParamIndex_ = 0;
                    totalParams_ = 0;
                    xSemaphoreGive(publishMutex_);
                }
                return;
            }
        }
        
        if (!success) {
            PSTOR_LOG_W( "Failed to publish parameter: %s", pair.first.c_str());
        }
        
        published++;
        currentIndex++;
        
        // Small delay between parameters
        vTaskDelay(pdMS_TO_TICKS(50));

        // Note: Removed esp_task_wdt_reset() - caller task may not be registered
    }
    
    PSTOR_LOG_D( "Published %d parameters, %d remaining", 
                             published, totalParams_ - nextParamIndex_);
}

void PersistentStorage::processCommandQueue() {
    if (!commandQueue_) {
        return;
    }
    
    ParameterCommand cmd;
    // Process up to 5 commands per call to avoid blocking
    for (int i = 0; i < 5; i++) {
        if (xQueueReceive(commandQueue_, &cmd, 0) != pdTRUE) {
            break;  // No more commands
        }
        
        // Minimal logging to save stack space
        PSTOR_LOG_D( "Cmd type: %d", cmd.type);

        // Note: Removed esp_task_wdt_reset() - caller task may not be registered
        // with ESP-IDF task watchdog, causing crash. Caller should handle WDT if needed.
        
        switch (cmd.type) {
            case ParameterCommand::SET: {
                JsonDocument doc;  // ArduinoJson v7
                DeserializationError error = deserializeJson(doc, cmd.payload);

                // If JSON parsing failed or no "value" key, wrap plain value
                if (error || doc["value"].isNull()) {
                    doc.clear();
                    // Try to detect type and set appropriately
                    char* endptr;
                    double numVal = strtod(cmd.payload, &endptr);
                    if (*endptr == '\0' && endptr != cmd.payload) {
                        // It's a number
                        doc["value"] = numVal;
                    } else if (strcmp(cmd.payload, "true") == 0) {
                        doc["value"] = true;
                    } else if (strcmp(cmd.payload, "false") == 0) {
                        doc["value"] = false;
                    } else {
                        // Treat as string
                        doc["value"] = cmd.payload;
                    }
                    PSTOR_LOG_D("Wrapped plain value: %s", cmd.payload);
                }

                Result res = setJson(cmd.paramName, doc);
                if (res == Result::SUCCESS) {
                    PSTOR_LOG_I("Set %s: %s", cmd.paramName, resultToString(res));
                } else {
                    PSTOR_LOG_E("Set %s: %s", cmd.paramName, resultToString(res));
                }
                break;
            }

            case ParameterCommand::GET: {
                // Check if this is a category/group query (no slash = group name)
                std::string paramName(cmd.paramName);
                if (paramName.find('/') == std::string::npos) {
                    // No slash - might be a group name like "heating", "wheater", "pid", "sensor", "system"
                    if (paramName == "heating" || paramName == "wheater" ||
                        paramName == "pid" || paramName == "sensor" || paramName == "system") {
                        PSTOR_LOG_I("GET group: %s", paramName.c_str());
                        publishGroupedCategory(paramName);
                    } else {
                        // Unknown group, try as exact parameter name
                        publishUpdate(paramName);
                    }
                } else {
                    // Has slash - exact parameter name like "heating/targetTemp"
                    publishUpdate(paramName);
                }
                break;
            }

            case ParameterCommand::GET_ALL:
                publishAllGrouped();
                break;

            case ParameterCommand::LIST: {
                // Use JSON doc (ArduinoJson v7)
                JsonDocument doc;
                JsonArray array = doc.to<JsonArray>();

                for (const auto& name : listParameters()) {
                    array.add(name);
                }
                
                std::string listTopic = mqttPrefix_ + "/list/response";
                char listBuffer[1024];
                serializeJson(doc, listBuffer, sizeof(listBuffer));
                if (mqttPublishCallback_) {
                    mqttPublishCallback_(listTopic.c_str(), listBuffer, 0, false);
                } else {
                    (void)mqttManager_->publish(listTopic.c_str(), listBuffer, 0, false);
                }
                break;
            }
            
            case ParameterCommand::SAVE:
                saveAll();
                PSTOR_LOG_I( "Parameters saved to NVS");
                break;
        }
        
        // Small delay between commands
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void PersistentStorage::getNvsStats(size_t& usedEntries, size_t& freeEntries, size_t& totalEntries) {
    nvs_stats_t nvs_stats;
    esp_err_t err = nvs_get_stats(NULL, &nvs_stats);

    if (err == ESP_OK) {
        usedEntries = nvs_stats.used_entries;
        freeEntries = nvs_stats.free_entries;
        totalEntries = nvs_stats.total_entries;
        PSTOR_LOG_D("NVS stats: used=%zu, free=%zu, total=%zu",
                    usedEntries, freeEntries, totalEntries);
    } else {
        usedEntries = 0;
        freeEntries = 0;
        totalEntries = 0;
        PSTOR_LOG_W("Failed to get NVS stats: %s", esp_err_to_name(err));
    }
}