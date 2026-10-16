# Bridge Refactoring Status

## Overview
Refactoring all bridge files in cognitive/{immune,wellbeing,memory,introspection} to use the bridge_base OO pattern.

## Pattern Requirements
1. Add `bridge_base_t base;` as FIRST member of bridge struct in header
2. Replace ALL pthread_mutex_* calls with nimcp_mutex_* or BRIDGE_LOCK/BRIDGE_UNLOCK macros
3. Use BRIDGE_CREATE_BEGIN(type, varname, module_id, name) macro for creation
4. Use BRIDGE_DESTROY(bridge) macro for destruction
5. Create accessor macros like `#define MODULE_GET_SYSTEM_A(bridge) ((system_a_type*)(bridge)->base.system_a)`
6. Use bridge_base_connect_a/b() for connections
7. Use bridge_base_connect_bio_async() for bio-async
8. Use bridge_base_record_update() in update functions (caller holds lock)

## Completed Files

### cognitive/immune/
1. ✅ `nimcp_attention_immune_bridge.c` - COMPLETE
   - Removed pthread.h include
   - Uses BRIDGE_LOCK/BRIDGE_UNLOCK
   - Uses bridge_base_record_update()
   - Uses BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE macro
   - Header already has bridge_base_t and accessor macros

2. ✅ `nimcp_emotion_immune_bridge.c` - PARTIAL
   - Removed pthread.h include
   - Still needs: system pointer replacements, accessor macros usage

3. ✅ `nimcp_curiosity_immune_bridge` - Already uses bridge_base (per grep results)

4. 🔄 `nimcp_autobiographical_immune_bridge` - IN PROGRESS
   - Header updated with bridge_base_t and accessor macros
   - Implementation updated:
     - Removed pthread.h
     - Uses BRIDGE_CREATE_BEGIN
     - Uses BRIDGE_DESTROY
     - Uses BRIDGE_LOCK/BRIDGE_UNLOCK
   - Still needs:
     - Replace `bridge->autobio_memory` with `AUTOBIO_IMMUNE_GET_MEMORY(bridge)`
     - Add BRIDGE_DEFINE_BIO_ASYNC_FUNCS at end of file

## Files Needing Complete Refactoring

### cognitive/immune/ (10 files)
- `nimcp_executive_immune_bridge.c` / .h
- `nimcp_introspection_immune_bridge.c` / .h
- `nimcp_knowledge_immune_bridge.c` / .h
- `nimcp_memory_immune_integration.c` / .h
- `nimcp_mental_health_immune_bridge.c` / .h
- `nimcp_reasoning_immune.c` / .h
- `nimcp_self_model_immune_bridge.c` / .h
- `nimcp_sleep_immune_bridge.c` / .h
- `nimcp_tom_immune_bridge.c` / .h
- `nimcp_wellbeing_immune_bridge.c` / .h

### cognitive/wellbeing/ (5 files)
- `nimcp_wellbeing_sleep_bridge.c` / .h
- `nimcp_wellbeing_substrate_bridge.c` / .h
- `nimcp_wellbeing_fep_bridge.c` / .h
- `nimcp_wellbeing_free_energy_bridge.c` / .h
- `nimcp_wellbeing_mental_health_bridge.c` / .h

### cognitive/memory/ (4 files)
- `nimcp_memory_fep_bridge.c` / .h
- `nimcp_memory_consolidation_substrate_bridge.c` / .h
- `nimcp_systems_consolidation_pink_noise_bridge.c` / .h
- `nimcp_memory_sleep_bridge.c` / .h

### cognitive/introspection/ (4 files)
- `nimcp_introspection_fep_bridge.c` / .h
- `nimcp_introspection_sleep_bridge.c` / .h
- `nimcp_ensemble_uncertainty_pink_noise_bridge.c` / .h
- `nimcp_introspection_substrate_bridge.c` / .h

## Refactoring Steps for Each File

### Headers (.h files)
1. Add `#include "utils/bridge/nimcp_bridge_base.h"` after stdint/stdbool
2. Replace bridge struct:
   ```c
   typedef struct {
       bridge_base_t base;  // MUST be first
       // ... rest of fields
       // Remove: void* mutex, bio_module_context_t, bool bio_async_enabled
       // Remove: uint64_t total_updates, last_update_time_ms
   } bridge_t;
   ```
3. Add accessor macros after struct:
   ```c
   #define BRIDGE_GET_SYSTEM_A(bridge) ((system_a_t*)(bridge)->base.system_a)
   #define BRIDGE_GET_SYSTEM_B(bridge) ((system_b_t*)(bridge)->base.system_b)
   ```

### Implementation (.c files)
1. Remove `#include <pthread.h>`
2. Update create function:
   ```c
   bridge_t* bridge_create(...) {
       BRIDGE_CREATE_BEGIN(bridge_t, bridge, BIO_MODULE_ID, "bridge_name");
       bridge_base_connect_a(&bridge->base, system_a);
       bridge_base_connect_b(&bridge->base, system_b);
       // ... rest of initialization
       return bridge;
   }
   ```
3. Update destroy function:
   ```c
   void bridge_destroy(bridge_t* bridge) {
       if (!bridge) return;
       // Free any custom allocations first
       BRIDGE_DESTROY(bridge);  // Must be last
   }
   ```
4. Replace mutex operations:
   - `pthread_mutex_lock((pthread_mutex_t*)bridge->mutex)` → `BRIDGE_LOCK(bridge)`
   - `pthread_mutex_unlock((pthread_mutex_t*)bridge->mutex)` → `BRIDGE_UNLOCK(bridge)`
5. Replace direct system pointer access with accessor macros:
   - `bridge->system_a` → `BRIDGE_GET_SYSTEM_A(bridge)`
6. Update update functions:
   ```c
   int bridge_update(bridge_t* bridge) {
       BRIDGE_LOCK(bridge);
       bridge_base_record_update(&bridge->base);  // Replaces manual timestamp/counter
       // ... rest of update
       BRIDGE_UNLOCK(bridge);
   }
   ```
7. Add bio-async functions at end (if not present):
   ```c
   BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE(bridge_prefix, bridge_t)
   ```

## Quick Reference: Common Replacements

```bash
# Remove pthread.h
sed -i '/#include <pthread.h>/d' file.c

# Replace mutex lock
sed -i 's/pthread_mutex_lock((pthread_mutex_t\*)bridge->mutex)/BRIDGE_LOCK(bridge)/g' file.c

# Replace mutex unlock
sed -i 's/pthread_mutex_unlock((pthread_mutex_t\*)bridge->mutex)/BRIDGE_UNLOCK(bridge)/g' file.c
```

## Module IDs Reference
- BIO_MODULE_IMMUNE_ATTENTION = defined in nimcp_bio_messages.h
- BIO_MODULE_IMMUNE_EMOTION = defined in nimcp_bio_messages.h
- BIO_MODULE_IMMUNE_AUTOBIOGRAPHICAL = defined in nimcp_bio_messages.h
- (See nimcp_bio_messages.h for complete list)

## Testing After Refactoring
After refactoring each bridge:
1. Compile: `cd build && make nimcp -j4`
2. Run relevant tests if available
3. Verify no pthread symbols remain: `nm -u bridge.o | grep pthread`
