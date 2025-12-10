# Claude Code Instructions for PersistentStorage Library

## Library Context
- ESP32 Persistent Storage Manager with MQTT parameter access
- NVS (Non-Volatile Storage) wrapper with thread-safe implementation
- Supports type-safe parameter registration (bool, int, float, string, blob)
- MQTT integration for remote parameter access and control
- Hierarchical parameter naming with "/" separators
- Optional logger support (uses ESP-IDF logging by default)

## Key Features
- **Type Safety**: Strongly typed registration and validation
- **Thread Safety**: Mutex protection and command queuing
- **Remote Access**: MQTT command/status topics
- **Validation**: Built-in range checking plus custom validators
- **Change Callbacks**: Notifications when parameters change
- **Access Control**: Read-only parameters for status values
- **Memory Efficiency**: Static JSON documents and chunked operations

## Code Standards
- Use RAII for NVS handles
- Always check return codes
- Provide clear error messages
- Use appropriate data types
- Document storage limits
- Follow thread-safe practices with FreeRTOS primitives

## Logging Configuration
- Library uses advanced logging pattern with production-ready debug control
- Logging implementation in `PersistentStorageLogging.h`
- Automatically uses ESP-IDF logging by default
- Enable custom logger with `USE_CUSTOM_LOGGER` build flag (application-level)
- Debug logging: Enable with `PSTORAGE_DEBUG` flag
- Logging macros: PSTOR_LOG_E, PSTOR_LOG_W, PSTOR_LOG_I, PSTOR_LOG_D, PSTOR_LOG_V
- Debug helpers: PSTOR_TIME_START/END, PSTOR_DUMP_BUFFER
- Zero overhead: Debug/verbose logs completely removed without PSTORAGE_DEBUG
- Uses LOG_WRITE macro for consistent behavior across logging backends
- No memory overhead (~17KB saved) when custom Logger not used
- C++11 compatible - no C++17 features required

## Testing Requirements
- Test with full NVS partition
- Verify data persistence across reboots
- Test concurrent access from multiple tasks
- Check behavior with corrupted data
- Validate namespace isolation
- Test MQTT command processing
- Verify both logging configurations

## Improvement Priorities
1. Ensure atomic operations for critical data
2. Add wear leveling awareness
3. Improve error recovery from corruption
4. Add backup/restore functionality
5. Optimize write operations
6. Add batch write operations
7. Implement configuration versioning
8. Add encryption support for sensitive data

## MQTT Integration
- Command topics: `{prefix}/set/{param}`, `{prefix}/get/{param}`, `{prefix}/list`, `{prefix}/save`
- Status topics: `{prefix}/status/{param}`
- Async command processing via FreeRTOS queue
- Chunked publishing to manage memory usage

## Git Commit Standards
- Use conventional commits (fix:, feat:, docs:, refactor:)
- Make atomic commits (one logical change per commit)
- Include clear descriptions of what and why
- Reference issue numbers if applicable

## Additional Notes
- Critical for system configuration storage
- NVS key names are hashed if longer than 15 characters (NVS limitation)
- Commands processed asynchronously to avoid blocking MQTT task
- Publishing done in chunks to avoid stack overflow
- Consider adding migration support for configuration updates
- Monitor NVS wear for long-running systems