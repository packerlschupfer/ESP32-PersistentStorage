# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- `COMMAND_QUEUE_SIZE` raised from 5 to 16 (~116 B per slot, ~1.9 kB total). A single
  client burst could overrun the queue: `handleMqttCommand()` enqueues with
  `xQueueSend(..., 0)` and drops the NEWEST command on overflow, notifying neither the
  caller nor the broker, so the commands simply never happened. Measured on a live
  device: a `get/all` followed immediately by 8 single `get/<param>` reads lost exactly
  the last 4 reads. `processCommandQueue()` drains only 5 commands per call, and a
  pipelined client delivers a whole burst before the consuming task is scheduled, so
  queue capacity - not drain rate - has to absorb it.

## [0.1.0] - 2025-12-04

### Added
- Initial public release
- Type-safe parameter registration (bool, int, float, string, blob)
- NVS (Non-Volatile Storage) wrapper with thread-safe operations
- MQTT integration for remote parameter access and control
- Hierarchical parameter naming with "/" separators
- Built-in range validation and custom validators
- Change callbacks for parameter update notifications
- Read-only parameters for status values
- Namespace isolation for multi-component systems
- eraseNamespace() method for NVS corruption recovery
- Async command processing via FreeRTOS queue
- Chunked MQTT publishing to manage memory usage
- Static JSON documents for efficient serialization
- MQTT topics: set, get, list, save operations

Platform: ESP32 (Arduino/ESP-IDF)
License: GPL-3
Dependencies: ArduinoJson

### Notes
- Production-tested for configuration management
- Previous internal versions (v1.x) not publicly released
- Reset to v0.1.0 for clean public release start
