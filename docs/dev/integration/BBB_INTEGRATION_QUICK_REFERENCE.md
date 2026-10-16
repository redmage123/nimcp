# BBB Security Integration - Quick Reference Guide

**For**: NIMCP Developers
**Purpose**: Quick copy-paste patterns for BBB security integration
**Date**: 2025-12-08

---

## Include Headers

```c
#include "security/nimcp_bbb_helpers.h"
#include "security/nimcp_security.h"
```

---

## Module Registration (in create/init function)

```c
module_t* module_create(const config_t* config) {
    // Register module FIRST THING
    bbb_register_module("module_name", BBB_MODULE_TYPE_SWARM);  // or _CORE, _PLATFORM, etc.

    // Continue with normal initialization...
}
```

---

## Input Validation Patterns

### Pointer Validation
```c
if (!bbb_validate_pointer(ptr, "function_name")) {
    return ERROR_CODE;  // or NULL, or false
}
```

### String Validation
```c
if (!bbb_validate_string(str, MAX_LEN, "function_name")) {
    return ERROR_CODE;
}
```

### Numeric Range Validation
```c
// Signed integers
if (!bbb_validate_range(value, MIN, MAX, "function_name")) {
    return ERROR_CODE;
}

// Unsigned integers
if (!bbb_validate_range_u(value, MIN, MAX, "function_name")) {
    return ERROR_CODE;
}
```

### Network Data Validation
```c
if (!bbb_validate_network_data(buffer, length, "function_name")) {
    bbb_audit_log(BBB_AUDIT_WARNING, "module", "invalid_data", "len=%zu", length);
    return ERROR_CODE;
}
```

### Buffer Access Validation
```c
if (!bbb_validate_buffer_access(buffer, offset, access_size, buffer_size, "function_name")) {
    return ERROR_CODE;
}
```

---

## Threat Detection

### Scan Data for Threats
```c
bbb_threat_type_t threat = bbb_detect_threat(data, length);
if (threat != BBB_THREAT_NONE) {
    bbb_audit_log(BBB_AUDIT_CRITICAL, "module", "threat_detected",
                  "threat=%d src=%u", threat, source_id);
    return ERROR_CODE;
}
```

### Verify Message Integrity
```c
if (!bbb_verify_message_integrity(message, message_len)) {
    bbb_audit_log(BBB_AUDIT_WARNING, "module", "integrity_check_failed", "");
    return ERROR_CODE;
}
```

---

## Audit Logging Patterns

### Info Level (Normal Operations)
```c
bbb_audit_log(BBB_AUDIT_INFO, "module_name", "event_name",
              "param1=%d param2=%s", value1, value2);
```

### Warning Level (Suspicious Activity)
```c
bbb_audit_log(BBB_AUDIT_WARNING, "module_name", "suspicious_event",
              "reason=%s value=%d", reason, value);
```

### Error Level (Operation Failed)
```c
bbb_audit_log(BBB_AUDIT_ERROR, "module_name", "operation_failed",
              "error=%s code=%d", error_msg, error_code);
```

### Critical Level (Security Violation)
```c
bbb_audit_log(BBB_AUDIT_CRITICAL, "module_name", "security_violation",
              "attack=%s source=%u", attack_type, source_id);
```

---

## Complete Function Template

```c
/**
 * @brief Function description
 */
return_type function_name(param1_t* param1, param2_t param2, const char* str) {
    // 1. Validate pointer parameters
    if (!bbb_validate_pointer(param1, "function_name")) {
        return ERROR_CODE;
    }

    // 2. Validate string parameters
    if (!bbb_validate_string(str, MAX_STRING_LEN, "function_name")) {
        return ERROR_CODE;
    }

    // 3. Validate numeric ranges
    if (!bbb_validate_range_u(param2, MIN_VALUE, MAX_VALUE, "function_name")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "module", "invalid_range",
                      "param2=%u", param2);
        return ERROR_CODE;
    }

    // 4. Perform operation
    result_type result = do_operation(param1, param2, str);

    // 5. Log significant events
    if (result_is_significant(result)) {
        bbb_audit_log(BBB_AUDIT_INFO, "module", "operation_complete",
                      "result=%d param2=%u", result, param2);
    }

    return result;
}
```

---

## Network Send Function Template

```c
bool module_send(module_t* module, const uint8_t* data, size_t len, uint32_t dest) {
    // Validate inputs
    if (!bbb_validate_pointer(module, "module_send")) return false;
    if (!bbb_validate_pointer(data, "module_send")) return false;
    if (!bbb_validate_range_u(len, 1, MAX_PACKET_SIZE, "module_send")) return false;

    // Validate network data for injection attacks
    if (!bbb_validate_network_data(data, len, "module_send")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "module", "invalid_send_data",
                      "len=%zu dest=%u", len, dest);
        module->stats.packets_dropped++;
        return false;
    }

    // Perform send
    bool success = send_impl(module, data, len, dest);

    // Log result
    if (success) {
        bbb_audit_log(BBB_AUDIT_DEBUG, "module", "packet_sent",
                      "dest=%u len=%zu", dest, len);
    } else {
        bbb_audit_log(BBB_AUDIT_WARNING, "module", "send_failed",
                      "dest=%u len=%zu", dest, len);
    }

    return success;
}
```

---

## Network Receive Function Template

```c
bool module_receive(module_t* module, uint8_t* buffer, size_t buffer_size,
                    size_t* received_len, uint32_t* source_id) {
    // Validate inputs
    if (!bbb_validate_pointer(module, "module_receive")) return false;
    if (!bbb_validate_pointer(buffer, "module_receive")) return false;
    if (!bbb_validate_pointer(received_len, "module_receive")) return false;
    if (!bbb_validate_range_u(buffer_size, 1, MAX_BUFFER_SIZE, "module_receive")) return false;

    // Perform receive
    bool received = receive_impl(module, buffer, buffer_size, received_len, source_id);
    if (!received) {
        return false;
    }

    // Scan for threats
    bbb_threat_type_t threat = bbb_detect_threat(buffer, *received_len);
    if (threat != BBB_THREAT_NONE) {
        bbb_audit_log(BBB_AUDIT_CRITICAL, "module", "threat_detected",
                      "threat=%d src=%u len=%zu", threat,
                      source_id ? *source_id : 0, *received_len);
        return false;  // Block malicious packet
    }

    // Log successful receive
    bbb_audit_log(BBB_AUDIT_DEBUG, "module", "packet_received",
                  "src=%u len=%zu", source_id ? *source_id : 0, *received_len);

    return true;
}
```

---

## Create Function Template

```c
module_t* module_create(const module_config_t* config) {
    // Validate config pointer
    if (!bbb_validate_pointer(config, "module_create")) {
        bbb_audit_log(BBB_AUDIT_ERROR, "module", "create_failed", "config=NULL");
        return NULL;
    }

    // Register module with BBB
    bbb_register_module("module_name", BBB_MODULE_TYPE_SWARM);

    // Validate config parameters
    if (!bbb_validate_range_u(config->param1, MIN_VAL, MAX_VAL, "module_create")) {
        bbb_audit_log(BBB_AUDIT_ERROR, "module", "invalid_config",
                      "param1=%u", config->param1);
        return NULL;
    }

    // Allocate module
    module_t* module = malloc(sizeof(*module));
    if (!bbb_validate_pointer(module, "module_create")) {
        bbb_audit_log(BBB_AUDIT_ERROR, "module", "alloc_failed",
                      "size=%zu", sizeof(*module));
        return NULL;
    }

    // Initialize module
    memset(module, 0, sizeof(*module));
    module->config = *config;

    // Log successful creation
    bbb_audit_log(BBB_AUDIT_INFO, "module", "created",
                  "param1=%u param2=%u", config->param1, config->param2);

    return module;
}
```

---

## Destroy Function Template

```c
void module_destroy(module_t* module) {
    if (!bbb_validate_pointer(module, "module_destroy")) {
        return;
    }

    // Log destruction
    bbb_audit_log(BBB_AUDIT_INFO, "module", "destroyed",
                  "id=%u", module->id);

    // Free resources
    free_resources(module);
    free(module);
}
```

---

## Configuration Change Template

```c
int module_set_config(module_t* module, config_param_t value) {
    if (!bbb_validate_pointer(module, "module_set_config")) {
        return -1;
    }

    if (!bbb_validate_range_u(value, MIN_VAL, MAX_VAL, "module_set_config")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "module", "invalid_config_value",
                      "value=%u", value);
        return -1;
    }

    // Apply config change
    module->config_param = value;

    // Log configuration change (security-relevant)
    bbb_audit_log(BBB_AUDIT_INFO, "module", "config_changed",
                  "param=%u old=%u new=%u", PARAM_ID, module->old_value, value);

    return 0;
}
```

---

## Privilege Checking Template

```c
bool module_privileged_operation(module_t* module, operation_t* op) {
    if (!bbb_validate_pointer(module, "module_privileged_operation")) return false;
    if (!bbb_validate_pointer(op, "module_privileged_operation")) return false;

    // Check authorization
    if (!bbb_validate_privileged_operation(op, BBB_PRIV_ADMIN)) {
        bbb_audit_log(BBB_AUDIT_WARNING, "module", "unauthorized_operation",
                      "op=%d user=%u", op->type, op->user_id);
        return false;
    }

    // Perform privileged operation
    bool success = perform_operation(module, op);

    // Log privileged operation
    bbb_audit_log(BBB_AUDIT_INFO, "module", "privileged_op_executed",
                  "op=%d user=%u success=%d", op->type, op->user_id, success);

    return success;
}
```

---

## State Validation Template

```c
bool module_validate_state(const module_state_t* state) {
    if (!bbb_validate_pointer(state, "module_validate_state")) {
        return false;
    }

    // Validate ranges
    if (!bbb_validate_range_u(state->count, 0, MAX_COUNT, "module_validate_state")) {
        bbb_audit_log(BBB_AUDIT_ERROR, "module", "invalid_count",
                      "count=%u", state->count);
        return false;
    }

    // Validate float ranges
    if (state->ratio < 0.0f || state->ratio > 1.0f) {
        bbb_audit_log(BBB_AUDIT_ERROR, "module", "invalid_ratio",
                      "ratio=%f", state->ratio);
        return false;
    }

    // Validate consistency
    if (state->part_count > state->total_count) {
        bbb_audit_log(BBB_AUDIT_ERROR, "module", "inconsistent_state",
                      "part=%u total=%u", state->part_count, state->total_count);
        return false;
    }

    return true;
}
```

---

## File Path Validation Template

```c
int module_read_file(const char* path, void* buffer, size_t size) {
    if (!bbb_validate_string(path, PATH_MAX, "module_read_file")) return -1;
    if (!bbb_validate_pointer(buffer, "module_read_file")) return -1;
    if (!bbb_validate_range_u(size, 1, MAX_FILE_SIZE, "module_read_file")) return -1;

    // Check for path traversal attacks
    if (strstr(path, "..") || strstr(path, "~") || path[0] != '/') {
        bbb_audit_log(BBB_AUDIT_CRITICAL, "module", "path_traversal_attempt",
                      "path=%s", path);
        return -1;
    }

    // Perform file read
    int bytes_read = read_file_impl(path, buffer, size);

    if (bytes_read > 0) {
        bbb_audit_log(BBB_AUDIT_INFO, "module", "file_read",
                      "path=%s bytes=%d", path, bytes_read);
    } else {
        bbb_audit_log(BBB_AUDIT_ERROR, "module", "file_read_failed",
                      "path=%s error=%d", path, errno);
    }

    return bytes_read;
}
```

---

## Consensus/Voting Template (Byzantine Resistance)

```c
bool module_process_vote(module_t* module, const vote_t* vote) {
    if (!bbb_validate_pointer(module, "module_process_vote")) return false;
    if (!bbb_validate_pointer(vote, "module_process_vote")) return false;

    // Validate vote structure
    if (!bbb_validate_range_u(vote->voter_id, 0, MAX_VOTERS, "module_process_vote")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "module", "invalid_voter_id",
                      "voter_id=%u", vote->voter_id);
        return false;
    }

    // Verify message integrity (prevent Byzantine attacks)
    if (!bbb_verify_message_integrity(vote, sizeof(*vote))) {
        bbb_audit_log(BBB_AUDIT_CRITICAL, "module", "vote_integrity_failed",
                      "voter=%u", vote->voter_id);
        return false;
    }

    // Check for duplicate votes
    if (already_voted(module, vote->voter_id)) {
        bbb_audit_log(BBB_AUDIT_WARNING, "module", "duplicate_vote",
                      "voter=%u", vote->voter_id);
        return false;
    }

    // Process vote
    bool accepted = process_vote_impl(module, vote);

    bbb_audit_log(BBB_AUDIT_INFO, "module", "vote_processed",
                  "voter=%u choice=%u accepted=%d", vote->voter_id, vote->choice, accepted);

    return accepted;
}
```

---

## Weight/Parameter Update Template (NaN/Inf Protection)

```c
bool module_update_weight(module_t* module, size_t index, float weight) {
    if (!bbb_validate_pointer(module, "module_update_weight")) return false;
    if (!bbb_validate_range_u(index, 0, module->weight_count - 1, "module_update_weight")) return false;

    // Detect weight poisoning attacks (NaN, Inf)
    if (isnan(weight) || isinf(weight)) {
        bbb_audit_log(BBB_AUDIT_CRITICAL, "module", "invalid_weight",
                      "weight=%f index=%zu", weight, index);
        return false;
    }

    // Optionally validate weight range
    if (!bbb_validate_range(weight, MIN_WEIGHT, MAX_WEIGHT, "module_update_weight")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "module", "weight_out_of_range",
                      "weight=%f index=%zu", weight, index);
        return false;
    }

    // Update weight
    module->weights[index] = weight;

    return true;
}
```

---

## Common Error Patterns to Avoid

### ❌ WRONG - No validation
```c
void function(module_t* module) {
    module->value = 42;  // Crash if module is NULL!
}
```

### ✅ CORRECT - With validation
```c
void function(module_t* module) {
    if (!bbb_validate_pointer(module, "function")) return;
    module->value = 42;
}
```

### ❌ WRONG - No logging
```c
bool send_packet(const uint8_t* data, size_t len) {
    return send_impl(data, len);  // No audit trail!
}
```

### ✅ CORRECT - With logging
```c
bool send_packet(const uint8_t* data, size_t len) {
    if (!bbb_validate_network_data(data, len, "send_packet")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "network", "invalid_data", "len=%zu", len);
        return false;
    }

    bool success = send_impl(data, len);

    bbb_audit_log(BBB_AUDIT_DEBUG, "network", "packet_sent",
                  "len=%zu success=%d", len, success);

    return success;
}
```

### ❌ WRONG - Unchecked buffer access
```c
void copy_data(uint8_t* dest, const uint8_t* src, size_t offset, size_t len) {
    memcpy(&dest[offset], src, len);  // Buffer overflow risk!
}
```

### ✅ CORRECT - Bounds checking
```c
bool copy_data(uint8_t* dest, size_t dest_size, const uint8_t* src,
               size_t offset, size_t len) {
    if (!bbb_validate_buffer_access(dest, offset, len, dest_size, "copy_data")) {
        return false;
    }
    memcpy(&dest[offset], src, len);
    return true;
}
```

---

## BBB Module Types

```c
typedef enum {
    BBB_MODULE_TYPE_COGNITIVE = 0,    // Cognitive/reasoning modules
    BBB_MODULE_TYPE_CORE,             // Core neural network
    BBB_MODULE_TYPE_SWARM,            // Swarm coordination
    BBB_MODULE_TYPE_PLATFORM,         // Platform abstraction
    BBB_MODULE_TYPE_NETWORK,          // Network communication
    BBB_MODULE_TYPE_PLASTICITY,       // Learning/plasticity
    BBB_MODULE_TYPE_MIDDLEWARE        // Middleware layer
} bbb_module_type_t;
```

---

## BBB Audit Levels

```c
typedef enum {
    BBB_AUDIT_DEBUG = 0,    // Detailed operation info
    BBB_AUDIT_INFO,         // Normal operations
    BBB_AUDIT_WARNING,      // Suspicious activity
    BBB_AUDIT_ERROR,        // Operation failures
    BBB_AUDIT_CRITICAL      // Security violations
} bbb_audit_level_t;
```

---

## BBB Threat Types

```c
typedef enum {
    BBB_THREAT_NONE = 0,              // No threat
    BBB_THREAT_BUFFER_OVERFLOW,       // Buffer overflow attempt
    BBB_THREAT_FORMAT_STRING,         // Format string attack
    BBB_THREAT_SQL_INJECTION,         // SQL injection
    BBB_THREAT_CODE_INJECTION,        // Code injection
    BBB_THREAT_SHELLCODE,             // Shellcode detected
    BBB_THREAT_INVALID_SIGNATURE,     // Invalid signature
    BBB_THREAT_MEMORY_VIOLATION,      // Memory violation
    BBB_THREAT_UNAUTHORIZED_ACCESS,   // Access violation
    BBB_THREAT_NETWORK_INJECTION,     // Network injection
    BBB_THREAT_BYZANTINE_ATTACK,      // Byzantine attack
    BBB_THREAT_UNKNOWN                // Unknown threat
} bbb_threat_type_t;
```

---

## Quick Checklist for New Functions

- [ ] Include BBB headers at top of file
- [ ] Register module in create/init function
- [ ] Validate all pointer parameters
- [ ] Validate all string parameters
- [ ] Validate all numeric ranges
- [ ] Validate network/external data
- [ ] Scan for threats in received data
- [ ] Log significant operations
- [ ] Log security violations
- [ ] Log configuration changes
- [ ] Check privileged operations
- [ ] Handle errors gracefully
- [ ] Return appropriate error codes

---

## Summary

**Remember**: Security is not optional. Every public function must:
1. Validate inputs
2. Detect threats
3. Log events
4. Fail safely

Copy the templates above and customize for your module. When in doubt, add more validation, not less.

For questions or clarifications, refer to:
- `/home/bbrelin/nimcp/docs/BBB_SWARM_PLATFORM_INTEGRATION.md` - Detailed integration guide
- `/home/bbrelin/nimcp/docs/BBB_INTEGRATION_COMPLETE_SUMMARY.md` - Implementation summary
- `/home/bbrelin/nimcp/include/security/nimcp_bbb_helpers.h` - API reference
