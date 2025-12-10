#ifndef PERSISTENT_STORAGE_LOGGING_H
#define PERSISTENT_STORAGE_LOGGING_H

#define PSTOR_LOG_TAG "PStore"

// Define log levels based on debug flag
#ifdef PSTORAGE_DEBUG
    // Debug mode: Show all levels
    #define PSTOR_LOG_LEVEL_E ESP_LOG_ERROR
    #define PSTOR_LOG_LEVEL_W ESP_LOG_WARN
    #define PSTOR_LOG_LEVEL_I ESP_LOG_INFO
    #define PSTOR_LOG_LEVEL_D ESP_LOG_DEBUG
    #define PSTOR_LOG_LEVEL_V ESP_LOG_VERBOSE
#else
    // Release mode: Only Error, Warn, Info
    #define PSTOR_LOG_LEVEL_E ESP_LOG_ERROR
    #define PSTOR_LOG_LEVEL_W ESP_LOG_WARN
    #define PSTOR_LOG_LEVEL_I ESP_LOG_INFO
    #define PSTOR_LOG_LEVEL_D ESP_LOG_NONE  // Suppress
    #define PSTOR_LOG_LEVEL_V ESP_LOG_NONE  // Suppress
#endif

// Route to custom logger or ESP-IDF
#ifdef USE_CUSTOM_LOGGER
    #include <LogInterface.h>
    #define PSTOR_LOG_E(...) LOG_WRITE(PSTOR_LOG_LEVEL_E, PSTOR_LOG_TAG, __VA_ARGS__)
    #define PSTOR_LOG_W(...) LOG_WRITE(PSTOR_LOG_LEVEL_W, PSTOR_LOG_TAG, __VA_ARGS__)
    #define PSTOR_LOG_I(...) LOG_WRITE(PSTOR_LOG_LEVEL_I, PSTOR_LOG_TAG, __VA_ARGS__)
    // Compile-time suppression for debug/verbose when not in debug mode
    #ifdef PSTORAGE_DEBUG
        #define PSTOR_LOG_D(...) LOG_WRITE(PSTOR_LOG_LEVEL_D, PSTOR_LOG_TAG, __VA_ARGS__)
        #define PSTOR_LOG_V(...) LOG_WRITE(PSTOR_LOG_LEVEL_V, PSTOR_LOG_TAG, __VA_ARGS__)
    #else
        #define PSTOR_LOG_D(...) ((void)0)
        #define PSTOR_LOG_V(...) ((void)0)
    #endif
#else
    // ESP-IDF logging with compile-time suppression
    #include <esp_log.h>
    #define PSTOR_LOG_E(...) ESP_LOGE(PSTOR_LOG_TAG, __VA_ARGS__)
    #define PSTOR_LOG_W(...) ESP_LOGW(PSTOR_LOG_TAG, __VA_ARGS__)
    #define PSTOR_LOG_I(...) ESP_LOGI(PSTOR_LOG_TAG, __VA_ARGS__)
    #ifdef PSTORAGE_DEBUG
        #define PSTOR_LOG_D(...) ESP_LOGD(PSTOR_LOG_TAG, __VA_ARGS__)
        #define PSTOR_LOG_V(...) ESP_LOGV(PSTOR_LOG_TAG, __VA_ARGS__)
    #else
        #define PSTOR_LOG_D(...) ((void)0)
        #define PSTOR_LOG_V(...) ((void)0)
    #endif
#endif

// Additional debug helpers
#ifdef PSTORAGE_DEBUG
    // Hex buffer dump for debugging
    #define PSTOR_DUMP_BUFFER(msg, buf, len) do { \
        PSTOR_LOG_D("%s (%d bytes):", msg, len); \
        for (int i = 0; i < len; i += 16) { \
            char hex[50] = {0}; \
            char ascii[20] = {0}; \
            int bytes = (len - i) > 16 ? 16 : (len - i); \
            for (int j = 0; j < bytes; j++) { \
                sprintf(hex + (j * 3), "%02X ", ((uint8_t*)buf)[i + j]); \
                ascii[j] = isprint(((char*)buf)[i + j]) ? ((char*)buf)[i + j] : '.'; \
            } \
            PSTOR_LOG_D("  [%04X] %-48s %s", i, hex, ascii); \
        } \
    } while(0)
    
    // Performance timing
    #define PSTOR_TIME_START() unsigned long _pstor_start = millis()
    #define PSTOR_TIME_END(msg) PSTOR_LOG_D("Timing: %s took %lu ms", msg, millis() - _pstor_start)
#else
    #define PSTOR_DUMP_BUFFER(msg, buf, len) ((void)0)
    #define PSTOR_TIME_START() ((void)0)
    #define PSTOR_TIME_END(msg) ((void)0)
#endif

#endif // PERSISTENT_STORAGE_LOGGING_H