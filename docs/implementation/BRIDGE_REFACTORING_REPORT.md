# Bridge Refactoring Report: bridge_base OO Pattern Migration

## Summary
This document tracks the refactoring of ALL bridge files in `/home/bbrelin/nimcp/src/security/` and `/home/bbrelin/nimcp/src/portia/` to use the bridge_base OO pattern.

**Total Bridges to Refactor**: 20+ files
**Completed**: 2 files
**Remaining**: 18+ files

---

## ✅ Completed Refactorings

### 1. `/home/bbrelin/nimcp/src/security/sleep/nimcp_anomaly_detector_sleep_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/sleep/nimcp_anomaly_detector_sleep_bridge.h`

**Changes Made**:
- ✅ Added `bridge_base_t base` as FIRST member in `anomaly_detector_sleep_bridge_t` struct
- ✅ Changed from opaque pointer to full struct definition in header
- ✅ Replaced `pthread_mutex_lock/unlock` with `BRIDGE_LOCK/BRIDGE_UNLOCK` macros
- ✅ Used `BRIDGE_CREATE_BEGIN` macro in create function
- ✅ Used `BRIDGE_DESTROY` macro in destroy function
- ✅ Used `bridge_base_connect_a()` for sleep system connection (outside lock)
- ✅ Used `bridge_base_record_update()` in update function (inside lock)
- ✅ Created accessor macro: `ANOMALY_DETECTOR_SLEEP_GET_SLEEP(bridge)`
- ✅ Updated all function signatures to use `*` pointer types
- ✅ Removed manual mutex allocation/destruction
- ✅ Module ID: 0x0D70

### 2. `/home/bbrelin/nimcp/src/security/sleep/nimcp_bbb_sleep_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/sleep/nimcp_bbb_sleep_bridge.h`

**Changes Made**:
- ✅ Added `bridge_base_t base` as FIRST member in `bbb_sleep_bridge_t` struct
- ✅ Changed from opaque pointer to full struct definition in header
- ✅ Replaced `pthread_mutex_lock/unlock` with `BRIDGE_LOCK/BRIDGE_UNLOCK` macros
- ✅ Used `BRIDGE_CREATE_BEGIN` macro in create function
- ✅ Used `BRIDGE_DESTROY` macro in destroy function
- ✅ Used `bridge_base_connect_a()` for sleep system connection
- ✅ Used `bridge_base_record_update()` in update function
- ✅ Created accessor macro: `BBB_SLEEP_GET_SLEEP(bridge)`
- ✅ Updated all function signatures to use `*` pointer types
- ✅ Removed manual mutex allocation/destruction
- ✅ Module ID: 0x0D71

---

## ⏳ Pending Refactorings

### Security/Sleep Bridges (2 remaining)

#### 3. `/home/bbrelin/nimcp/src/security/sleep/nimcp_pattern_db_sleep_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/sleep/nimcp_pattern_db_sleep_bridge.h`
**Module ID Suggestion**: 0x0D72
**Accessor Macro**: `PATTERN_DB_SLEEP_GET_SLEEP(bridge)`

#### 4. `/home/bbrelin/nimcp/src/security/sleep/nimcp_rate_limiter_sleep_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/sleep/nimcp_rate_limiter_sleep_bridge.h`
**Module ID Suggestion**: 0x0D73
**Accessor Macro**: `RATE_LIMITER_SLEEP_GET_SLEEP(bridge)`

### Security/Immune Bridges (3 files)

#### 5. `/home/bbrelin/nimcp/src/security/immune/nimcp_anomaly_immune_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/immune/nimcp_anomaly_immune_bridge.h`
**Module ID Suggestion**: 0x0D80
**Accessor Macros**:
- `ANOMALY_IMMUNE_GET_ANOMALY(bridge)` → system_a
- `ANOMALY_IMMUNE_GET_IMMUNE(bridge)` → system_b

#### 6. `/home/bbrelin/nimcp/src/security/immune/nimcp_pattern_db_immune_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/immune/nimcp_pattern_db_immune_bridge.h`
**Module ID Suggestion**: 0x0D81
**Accessor Macros**:
- `PATTERN_DB_IMMUNE_GET_PATTERN_DB(bridge)` → system_a
- `PATTERN_DB_IMMUNE_GET_IMMUNE(bridge)` → system_b

#### 7. `/home/bbrelin/nimcp/src/security/immune/nimcp_rate_limiter_immune_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/immune/nimcp_rate_limiter_immune_bridge.h`
**Module ID Suggestion**: 0x0D82
**Accessor Macros**:
- `RATE_LIMITER_IMMUNE_GET_RATE_LIMITER(bridge)` → system_a
- `RATE_LIMITER_IMMUNE_GET_IMMUNE(bridge)` → system_b

### Security FEP Bridges (7 files)

#### 8. `/home/bbrelin/nimcp/src/security/nimcp_anomaly_detector_fep_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/nimcp_anomaly_detector_fep_bridge.h`
**Module ID Suggestion**: 0x0D90
**Accessor Macros**:
- `ANOMALY_FEP_GET_ANOMALY(bridge)` → system_a
- `ANOMALY_FEP_GET_FEP(bridge)` → system_b

#### 9. `/home/bbrelin/nimcp/src/security/nimcp_blood_brain_barrier_fep_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/nimcp_blood_brain_barrier_fep_bridge.h`
**Module ID Suggestion**: 0x0D91
**Accessor Macros**:
- `BBB_FEP_GET_BBB(bridge)` → system_a
- `BBB_FEP_GET_FEP(bridge)` → system_b

#### 10. `/home/bbrelin/nimcp/src/security/nimcp_pattern_db_fep_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/nimcp_pattern_db_fep_bridge.h`
**Module ID Suggestion**: 0x0D92
**Accessor Macros**:
- `PATTERN_DB_FEP_GET_PATTERN_DB(bridge)` → system_a
- `PATTERN_DB_FEP_GET_FEP(bridge)` → system_b

#### 11. `/home/bbrelin/nimcp/src/security/nimcp_rate_limiter_fep_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/nimcp_rate_limiter_fep_bridge.h`
**Module ID Suggestion**: 0x0D93
**Accessor Macros**:
- `RATE_LIMITER_FEP_GET_RATE_LIMITER(bridge)` → system_a
- `RATE_LIMITER_FEP_GET_FEP(bridge)` → system_b

#### 12. `/home/bbrelin/nimcp/src/security/nimcp_security_fep_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/nimcp_security_fep_bridge.h`
**Module ID Suggestion**: 0x0D94
**Accessor Macros**:
- `SECURITY_FEP_GET_SECURITY(bridge)` → system_a
- `SECURITY_FEP_GET_FEP(bridge)` → system_b

#### 13. `/home/bbrelin/nimcp/src/security/nimcp_security_perception_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/nimcp_security_perception_bridge.h`
**Module ID Suggestion**: 0x0D95
**Accessor Macros**:
- `SECURITY_PERCEPTION_GET_SECURITY(bridge)` → system_a
- `SECURITY_PERCEPTION_GET_PERCEPTION(bridge)` → system_b

#### 14. `/home/bbrelin/nimcp/src/security/nimcp_security_recovery_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/security/nimcp_security_recovery_bridge.h`
**Module ID Suggestion**: 0x0D96
**Accessor Macros**:
- `SECURITY_RECOVERY_GET_SECURITY(bridge)` → system_a
- `SECURITY_RECOVERY_GET_RECOVERY(bridge)` → system_b

### Portia/Immune Bridges (3 files)

#### 15. `/home/bbrelin/nimcp/src/portia/immune/nimcp_portia_attention_immune_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/portia/immune/nimcp_portia_attention_immune_bridge.h`
**Module ID Suggestion**: 0x0DA0
**Accessor Macros**:
- `PORTIA_ATTENTION_IMMUNE_GET_ATTENTION(bridge)` → system_a
- `PORTIA_ATTENTION_IMMUNE_GET_IMMUNE(bridge)` → system_b

#### 16. `/home/bbrelin/nimcp/src/portia/immune/nimcp_portia_learning_immune_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/portia/immune/nimcp_portia_learning_immune_bridge.h`
**Module ID Suggestion**: 0x0DA1
**Accessor Macros**:
- `PORTIA_LEARNING_IMMUNE_GET_LEARNING(bridge)` → system_a
- `PORTIA_LEARNING_IMMUNE_GET_IMMUNE(bridge)` → system_b

#### 17. `/home/bbrelin/nimcp/src/portia/immune/nimcp_portia_sensor_fusion_immune_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/portia/immune/nimcp_portia_sensor_fusion_immune_bridge.h`
**Module ID Suggestion**: 0x0DA2
**Accessor Macros**:
- `PORTIA_SENSOR_FUSION_IMMUNE_GET_SENSOR_FUSION(bridge)` → system_a
- `PORTIA_SENSOR_FUSION_IMMUNE_GET_IMMUNE(bridge)` → system_b

### Portia Main Bridges (3 files)

#### 18. `/home/bbrelin/nimcp/src/portia/nimcp_portia_logic_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/portia/nimcp_portia_logic_bridge.h`
**Module ID Suggestion**: 0x0DB0
**Accessor Macros**:
- `PORTIA_LOGIC_GET_PORTIA(bridge)` → system_a
- `PORTIA_LOGIC_GET_LOGIC(bridge)` → system_b

#### 19. `/home/bbrelin/nimcp/src/portia/nimcp_portia_swarm_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/portia/nimcp_portia_swarm_bridge.h`
**Module ID Suggestion**: 0x0DB1
**Accessor Macros**:
- `PORTIA_SWARM_GET_PORTIA(bridge)` → system_a
- `PORTIA_SWARM_GET_SWARM(bridge)` → system_b

#### 20. `/home/bbrelin/nimcp/src/portia/nimcp_portia_swarm_logic_bridge.c`
**Header**: `/home/bbrelin/nimcp/include/portia/nimcp_portia_swarm_logic_bridge.h`
**Module ID Suggestion**: 0x0DB2
**Special Note**: This is a complex 3-way bridge with multiple connections
**Accessor Macros**:
- Multiple systems - requires custom accessor pattern
- Has `pthread_mutex_t*` that needs conversion to `base.mutex`

---

## Refactoring Checklist (Apply to Each File)

### Header File (.h) Changes:
1. ✅ Replace `#include "utils/thread/nimcp_thread.h"` with `#include "utils/bridge/nimcp_bridge_base.h"`
2. ✅ Convert opaque pointer typedef to full struct definition:
   ```c
   // OLD:
   typedef struct my_bridge_struct* my_bridge_t;

   // NEW:
   typedef struct {
       bridge_base_t base;        /* MUST be first */
       my_config_t config;
       my_effects_t effects;
       /* other fields */
   } my_bridge_t;
   ```
3. ✅ Create accessor macros for system pointers:
   ```c
   #define MY_BRIDGE_GET_SYSTEM_A(bridge) ((system_a_t)(bridge)->base.system_a)
   #define MY_BRIDGE_GET_SYSTEM_B(bridge) ((system_b_t)(bridge)->base.system_b)
   ```
4. ✅ Update all function signatures to use `*` pointer types:
   ```c
   // OLD: my_bridge_t bridge
   // NEW: my_bridge_t* bridge
   ```

### Implementation File (.c) Changes:
1. ✅ Remove internal struct definition (now in header)
2. ✅ Replace mutex operations:
   ```c
   // OLD:
   nimcp_mutex_lock(bridge->mutex);
   nimcp_mutex_unlock(bridge->mutex);
   pthread_mutex_lock(bridge->mutex);
   pthread_mutex_unlock(bridge->mutex);

   // NEW:
   BRIDGE_LOCK(bridge);
   BRIDGE_UNLOCK(bridge);
   ```
3. ✅ Update `create()` function:
   ```c
   my_bridge_t* my_bridge_create(const my_config_t* config, system_a_t sys_a, system_b_t sys_b)
   {
       if (!sys_a || !sys_b) return NULL;

       /* Use bridge_base abstraction for creation */
       BRIDGE_CREATE_BEGIN(my_bridge_t, bridge, MODULE_ID, "module_name");

       /* Initialize configuration */
       if (config) {
           memcpy(&bridge->config, config, sizeof(my_config_t));
       } else {
           my_bridge_default_config(&bridge->config);
       }

       /* Connect systems using bridge_base (OUTSIDE lock) */
       bridge_base_connect_a(&bridge->base, sys_a);
       bridge_base_connect_b(&bridge->base, sys_b);

       /* Initialize effects/state */
       memset(&bridge->effects, 0, sizeof(my_effects_t));

       /* Register callbacks if needed */

       return bridge;
   }
   ```
4. ✅ Update `destroy()` function:
   ```c
   void my_bridge_destroy(my_bridge_t* bridge)
   {
       if (!bridge) return;

       /* Unregister callbacks if needed */
       system_a_t sys_a = MY_BRIDGE_GET_SYSTEM_A(bridge);
       if (sys_a && bridge->callback_registered) {
           unregister_callback(sys_a, callback_func, bridge);
       }

       /* Use bridge_base macro for cleanup */
       BRIDGE_DESTROY(bridge);
   }
   ```
5. ✅ Update `update()` function:
   ```c
   int my_bridge_update(my_bridge_t* bridge)
   {
       BRIDGE_NULL_CHECK(bridge);

       system_a_t sys_a = MY_BRIDGE_GET_SYSTEM_A(bridge);
       system_b_t sys_b = MY_BRIDGE_GET_SYSTEM_B(bridge);

       BRIDGE_LOCK(bridge);

       /* Update logic here */

       /* Record update in base bridge (INSIDE lock) */
       bridge_base_record_update(&bridge->base);

       BRIDGE_UNLOCK(bridge);

       return 0;
   }
   ```
6. ✅ Update all getter functions to use `BRIDGE_LOCK/BRIDGE_UNLOCK`
7. ✅ Update callback functions to use `BRIDGE_LOCK/BRIDGE_UNLOCK`
8. ✅ Use accessor macros instead of direct field access:
   ```c
   // OLD: bridge->system_a
   // NEW: MY_BRIDGE_GET_SYSTEM_A(bridge)
   ```

---

## Module ID Allocation

| Range | Purpose |
|-------|---------|
| 0x0D70-0x0D7F | Security/Sleep Bridges |
| 0x0D80-0x0D8F | Security/Immune Bridges |
| 0x0D90-0x0D9F | Security FEP Bridges |
| 0x0DA0-0x0DAF | Portia/Immune Bridges |
| 0x0DB0-0x0DBF | Portia Main Bridges |

---

## Testing After Refactoring

After refactoring each file, verify:
1. ✅ File compiles without errors
2. ✅ All mutex operations use BRIDGE_LOCK/BRIDGE_UNLOCK
3. ✅ Create/destroy use BRIDGE_CREATE_BEGIN/BRIDGE_DESTROY
4. ✅ Update function calls bridge_base_record_update()
5. ✅ Accessor macros used for system pointer access
6. ✅ All function signatures updated to pointer types
7. ✅ No manual mutex allocation/destruction remains

---

## Reference Implementation

See `/home/bbrelin/nimcp/src/cognitive/working_memory/nimcp_working_memory_substrate_bridge.c` for the canonical implementation of the bridge_base pattern.

---

## Notes

- **Thread Safety**: BRIDGE_LOCK/BRIDGE_UNLOCK handle mutex operations automatically
- **Connection Order**: Always call `bridge_base_connect_*()` OUTSIDE of any locks
- **Update Recording**: Always call `bridge_base_record_update()` INSIDE the update lock
- **Accessor Macros**: Use type-safe casts for system pointers
- **Module IDs**: Ensure unique IDs within the allocated ranges

---

## Status Summary

| Category | Total | Completed | Remaining |
|----------|-------|-----------|-----------|
| Security/Sleep | 4 | 2 | 2 |
| Security/Immune | 3 | 0 | 3 |
| Security FEP | 7 | 0 | 7 |
| Portia/Immune | 3 | 0 | 3 |
| Portia Main | 3 | 0 | 3 |
| **TOTAL** | **20** | **2** | **18** |

**Completion**: 10% (2/20 files)

---

## Next Steps

1. Apply refactoring checklist to remaining 18 files
2. Test each file after refactoring
3. Update this document as files are completed
4. Verify all bridges compile and link correctly
5. Run integration tests for bridge functionality

---

**Generated**: 2025-12-22
**Last Updated**: 2025-12-22
