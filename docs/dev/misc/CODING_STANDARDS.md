# NIMCP Coding Standards & API Compliance

**MANDATORY: All code contributions MUST follow these standards.**

This document ensures API compatibility and prevents compilation failures from incorrect API usage.

---

## Table of Contents
1. [Pre-Code Requirements](#pre-code-requirements)
2. [API Discovery Process](#api-discovery-process)
3. [Mandatory Validations](#mandatory-validations)
4. [Common Violations](#common-violations)
5. [Enforcement Mechanisms](#enforcement-mechanisms)

---

## Pre-Code Requirements

### BEFORE Writing ANY Code

1. **Read the Header File**
   ```bash
   # Find the correct header first
   find include/ -name "*memory*.h" | head -5

   # Then read it to understand the API
   cat include/utils/memory/nimcp_memory.h
   ```

2. **Check API Reference**
   ```bash
   cat docs/API_REFERENCE.md
   ```

3. **Find Existing Usage Patterns**
   ```bash
   # See how other code uses the API
   grep -rn "nimcp_calloc" src/ | head -10
   ```

4. **Never Guess API Names**
   - If you're unsure, search the codebase
   - If it doesn't exist, it's probably wrong
   - Check the memory graph: `mcp__memory__search_nodes "NIMCP_*_API"`

---

## API Discovery Process

### Step 1: Identify the Subsystem

| Feature | Header Location |
|---------|-----------------|
| Memory | `include/utils/memory/nimcp_memory.h` |
| Logging | `include/utils/logging/nimcp_logging.h` |
| Bio-Async | `include/async/nimcp_bio_*.h` |
| Threading | `include/utils/thread/nimcp_thread.h` |
| Time | `include/utils/platform/nimcp_platform_time.h` |
| NLP | `include/networking/nlp/nimcp_nlp.h` |

### Step 2: Read the Header

```bash
# Example: Before using memory functions
cat include/utils/memory/nimcp_memory.h | grep -A2 "^void\|^int\|^char"
```

### Step 3: Find Examples

```bash
# How is this function used elsewhere?
grep -rn "nimcp_mutex_init" src/ --include="*.c" | head -5
```

### Step 4: Verify with Validation Script

```bash
# After writing code, validate API usage
./scripts/validate_api_usage.sh src/your_new_file.c
```

---

## Mandatory Validations

### Pre-Commit Checklist

- [ ] Read relevant header files
- [ ] Consulted `docs/API_REFERENCE.md`
- [ ] Searched for existing usage patterns
- [ ] Ran `./scripts/validate_api_usage.sh`
- [ ] Code compiles without API errors

### Build Verification

```bash
# Always verify compilation after each file
cd build
cmake .. && make -j$(nproc) 2>&1 | head -50

# Check for undefined reference errors
make 2>&1 | grep "undefined reference"
```

### Test After Changes

```bash
# Run relevant tests
ctest -R your_module_name
```

---

## Common Violations

### Memory API

| WRONG | CORRECT |
|-------|---------|
| `nimcp_unified_calloc(n, size)` | `nimcp_calloc(n, size)` |
| `nimcp_unified_malloc(size)` | `nimcp_malloc(size)` |
| `nimcp_unified_free(ptr)` | `nimcp_free(ptr)` |
| `nimcp_unified_realloc(p, s)` | `nimcp_realloc(p, s)` |

### Logging API

| WRONG | CORRECT |
|-------|---------|
| `NIMCP_LOG_DEBUG(...)` | `LOG_DEBUG(...)` |
| `NIMCP_LOG_ERROR(...)` | `LOG_ERROR(...)` |
| `NIMCP_LOG_INFO(...)` | `LOG_INFO(...)` |
| `NIMCP_LOG_WARN(...)` | `LOG_WARN(...)` |

### Bio-Async API

| WRONG | CORRECT |
|-------|---------|
| `nimcp_bio_message_t` | `bio_message_header_t` |
| `NIMCP_BIO_MSG_SPIKE_FIRED` | `BIO_MSG_SPIKE_FIRED` |
| `nimcp_bio_async_inbox_t*` | `void*` |

### Time API

| WRONG | CORRECT |
|-------|---------|
| `nimcp_platform_time_ms()` | `nimcp_platform_time_monotonic_ms()` |
| `nimcp_time_ms()` | `nimcp_platform_time_monotonic_ms()` |
| `nimcp_get_time_ms()` | `nimcp_platform_time_monotonic_ms()` |

### NLP API

| WRONG | CORRECT |
|-------|---------|
| `nlp_node_send(...)` | `nlp_send(node, peer, type, data, len, prio)` |
| `nlp_node_broadcast(...)` | `nlp_broadcast(node, type, data, len, prio)` |
| `NLP_MSG_NEURAL_SPIKE_BATCH` | `NLP_MSG_SPIKE_BATCH` |
| `NLP_MSG_BROADCAST` | `NLP_MSG_STATE_SYNC` |

---

## Enforcement Mechanisms

### 1. Validation Script (Pre-Commit)

```bash
# Run before every commit
./scripts/validate_api_usage.sh

# Run on specific directory
./scripts/validate_api_usage.sh src/networking/nlp/
```

### 2. Memory Graph Queries

Agents can query canonical APIs:
```
mcp__memory__search_nodes "NIMCP_Memory_API"
mcp__memory__search_nodes "NIMCP_Logging_API"
mcp__memory__open_nodes ["NIMCP_API_Contract_System"]
```

### 3. CI Integration (Recommended)

Add to `.github/workflows/build.yml`:
```yaml
- name: Validate API Usage
  run: ./scripts/validate_api_usage.sh src/
```

### 4. Git Hook (Recommended)

Create `.git/hooks/pre-commit`:
```bash
#!/bin/bash
./scripts/validate_api_usage.sh src/
if [ $? -ne 0 ]; then
    echo "API validation failed. Fix errors before committing."
    exit 1
fi
```

---

## Quick Reference Commands

```bash
# Find correct API function
grep -rn "function_name" include/ --include="*.h"

# See usage examples
grep -rn "function_name" src/ --include="*.c" | head -10

# Validate all source
./scripts/validate_api_usage.sh src/

# Check specific file
./scripts/validate_api_usage.sh src/path/to/file.c

# Build and check for errors
cd build && make 2>&1 | grep -i error

# Full API reference
cat docs/API_REFERENCE.md
```

---

## Root Cause Prevention

The primary causes of API mismatches:

1. **Guessing API names** instead of reading headers
2. **Assuming naming patterns** (e.g., `nimcp_unified_*` doesn't exist)
3. **Not checking existing code** for usage examples
4. **Skipping validation** before committing

Prevention:
- Always read the header file first
- Always search for existing usage patterns
- Always run the validation script
- Always verify compilation works

---

*Last updated: 2025-12-08*
