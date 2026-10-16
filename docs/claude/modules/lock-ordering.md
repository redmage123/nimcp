# Lock Ordering Hierarchy

**Version**: 2.6.3
**Last Updated**: 2026-03-11

## Overview

NIMCP uses a layered mutex hierarchy to prevent deadlocks. The fundamental rule is:
**Always acquire higher-level locks before lower-level locks.** Never invert this ordering.

The project provides two mutex APIs:
- **Platform layer**: `nimcp_platform_mutex_*()` -- raw POSIX/Windows wrapper (used in core infrastructure)
- **Thread layer**: `nimcp_mutex_*()` -- higher-level wrapper with error handling, types (normal/recursive/errorcheck), and named resource locks

A dedicated deadlock detector (`include/utils/thread/nimcp_deadlock_detector.h`) provides runtime
enforcement via `tracked_mutex_t` with timeout, lock ordering numbers, and dependency cycle detection.

---

## Lock Levels

### Level 0 -- Platform Primitives (lowest, acquired last)

| Mutex | Location | Type | Purpose |
|-------|----------|------|---------|
| `nimcp_platform_mutex_t` | `include/utils/platform/nimcp_platform_mutex.h` | Platform | Raw cross-platform mutex (pthread_mutex / CRITICAL_SECTION) |
| `nimcp_platform_cond_t` | `include/utils/platform/nimcp_platform_cond.h` | Platform | Condition variable (paired with platform mutex) |

These are the lowest-level synchronization primitives. They wrap pthreads on POSIX and
CRITICAL_SECTION on Windows. All higher layers build on top of these.

### Level 1 -- Memory and Exception System Mutexes

| Mutex | Location | Type | Purpose |
|-------|----------|------|---------|
| `g_memory_state.lock` | `src/utils/memory/nimcp_memory.c` | Thread (nimcp_mutex_t) | Protects global memory allocator state |
| `g_immune_mutex` | `src/utils/exception/nimcp_exception_immune.c` | Platform | Guards immune exception handler chain |
| `g_circuit_mutex` | `src/utils/exception/nimcp_exception_circuit.c` | Platform | Guards circuit breaker state machine |
| `manager->mutex` (COW) | `src/utils/memory/nimcp_cow_manager.c` | Platform | Protects COW reference counting and page tables |
| `pool->mutex` (buffer pool) | `src/utils/memory/nimcp_buffer_pool.c` | Platform | Guards buffer pool free-list |

**Critical rules for this level:**
- Files in `src/utils/exception/` must NEVER use `NIMCP_THROW_TO_IMMUNE` (infinite recursion risk).
- Files in `src/utils/memory/` must use raw `malloc/calloc/free/realloc` only.
- COW manager mutex must never be held while calling into brain-level code.

### Level 2 -- Core Brain Mutexes

| Mutex | Location | Type | Purpose |
|-------|----------|------|---------|
| `brain->cache_mutex` | `include/core/brain/nimcp_brain_internal.h` | Platform | Thread-safe decision cache access |
| Atomic refcount | `nimcp_brain_internal.h` | Lock-free | COW reference counting (replaced former `refcount_mutex`) |
| `g_registry_mutex` | `src/core/brain/nimcp_distributed_cow.c` | Platform | Distributed COW brain registry |
| `state->fetch_mutex` | `src/core/brain/nimcp_distributed_cow.c` | Platform | COW segment fetch serialization |

**Critical rules for this level:**
- `cache_mutex` is the primary brain-level lock. Acquired during predict, learn, cache clear, and factory validation.
- Functions like `nimcp_brain_factory_cache_decision()` assert that `cache_mutex` is held by caller.
- Reference counting uses lock-free atomics (not a mutex) to avoid use-after-free races.
- `mutex_lock_with_timeout()` is used in `nimcp_brain_part_core.c` to avoid indefinite blocking.

### Level 3 -- Module-Level Mutexes

| Mutex | Location | Type | Purpose |
|-------|----------|------|---------|
| `wm->mutex` | `src/cognitive/working_memory/` | Platform | Working memory item store and decay |
| `queue->mutex` (event bus) | `src/core/events/nimcp_event_bus.c` | Thread | Event queue push/pop serialization |
| `bus->subscriber_mutex` | `src/core/events/nimcp_event_bus.c` | Thread | Event subscriber registration |
| `bus->stats_mutex` | `src/core/events/nimcp_event_bus.c` | Thread | Event bus statistics counters |
| `fsc->lock` (security fractal) | `src/security/nimcp_security_fractal.c` | Thread | Fractal security check state |
| `system->mutex` (corrigibility) | `src/security/nimcp_corrigibility_part_processing.c` | Thread | Corrigibility decision processing |
| Oscillation mutexes | `src/core/brain_oscillations/` | Thread/Platform | Phase tracking and coupling state |
| `queue->mutex` (middleware) | `src/middleware/events/nimcp_event_queue.c` | Platform | Middleware event queue |
| `manager->mutex` (subscribers) | `src/middleware/events/nimcp_event_subscriber.c` | Platform | Middleware subscriber management |

**Critical rules for this level:**
- Working memory mutex protects the salience-sorted item buffer; never call brain-level predict/learn while holding it.
- Event bus has three separate mutexes (queue, subscriber, stats) to reduce contention. Always acquire in this order: subscriber -> queue -> stats.
- Security mutexes must never be held while calling back into cognitive modules.

### Level 4 -- Bridge Mutexes

| Mutex | Location | Type | Purpose |
|-------|----------|------|---------|
| `bridge->base.mutex` | `include/utils/bridge/nimcp_bridge_base.h` | Thread (nimcp_mutex_t*) | Per-bridge instance lock (~1065 bridge files) |
| FEP bridge mutexes | `src/cognitive/free_energy/` | Thread | Free Energy Principle bridge state |
| Immune bridge mutexes | `src/cognitive/immune/` | Thread/Platform | Immune system bridge state |
| Oscillation bridge mutexes | `src/core/brain_oscillations/` | Thread | Oscillation integration bridges |
| Sleep bridge mutexes | `src/cognitive/*/sleep/` | Thread | Sleep-related bridge state |

**Critical rules for this level:**
- All bridges share a common `bridge_base_t` with a `mutex` field. The `BRIDGE_BOILERPLATE` macro handles initialization.
- Bridge mutexes are per-instance: different bridge instances can be locked concurrently.
- Some bridges use recursive mutexes to handle re-entrant callback patterns.
- FEP bridges return `0`/`-1` (not NIMCP error codes).
- Callbacks should be invoked after releasing the bridge mutex to prevent deadlock.

### Level 5 -- Application-Level Mutexes (highest, acquired first)

| Mutex | Location | Type | Purpose |
|-------|----------|------|---------|
| Named resource locks | `nimcp_thread.h` / `nimcp_thread.c` | Thread | String-keyed, reference-counted application locks |
| `global_mutex` (resource table) | `nimcp_thread.h` | Thread | Guards the resource lock hash table itself |
| `bucket_mutex` (resource table) | `nimcp_thread.h` | Thread | Per-bucket lock in resource lock hash table |
| User application mutexes | Application code | Any | User-created mutexes via `nimcp_mutex_create()` |

**Critical rules for this level:**
- Named resource locks are reference-counted; always pair `nimcp_get_resource_lock()` with `nimcp_release_resource_lock()`.
- The resource lock table uses a two-level locking scheme: global_mutex -> bucket_mutex -> resource entry.
- Application code should never hold application-level locks while calling into brain internals.

---

## Deadlock Prevention Rules

### Rule 1: Strict Level Ordering
```
Level 5 (application) -> Level 4 (bridges) -> Level 3 (modules) -> Level 2 (brain) -> Level 1 (memory/exception) -> Level 0 (platform)
```
Always acquire locks from higher levels first. Never acquire a higher-level lock while holding a lower-level lock.

### Rule 2: No Cross-Module Calls Under Lock
When holding a module's mutex, do NOT call functions in other modules that may also acquire locks. Instead:
1. Copy needed data under lock.
2. Release the lock.
3. Call the cross-module function with the copied data.
4. Re-acquire lock if needed (check for stale state).

### Rule 3: Callback Invocation After Unlock
Bridge code follows a pattern:
```c
nimcp_mutex_lock(bridge->base.mutex);
// ... compute result ...
nimcp_mutex_unlock(bridge->base.mutex);
// invoke callback OUTSIDE the lock
if (callback) callback(result, user_data);
```

### Rule 4: Timeout-Based Locking
Use `mutex_lock_with_timeout()` or `tracked_mutex_lock()` for locks that may contend. This prevents indefinite hangs and converts potential deadlocks into detectable timeout errors.

### Rule 5: Lock-Free Where Possible
The project uses atomic operations for:
- COW reference counting (`brain->network_refcount` uses atomics, not mutex)
- Lock-free metrics collection (`include/utils/fault_tolerance/nimcp_lockfree_metrics.h`)
- `nimcp_once_t` for one-time initialization

### Rule 6: Recursive Mutexes Only When Necessary
Recursive mutexes are used only in specific bridge contexts where re-entrant callbacks are unavoidable. Prefer normal mutexes for all other cases.

---

## Common Deadlock Scenarios and Mitigations

### Scenario 1: Brain Cache + Bridge Mutex
**Problem**: Thread A holds `cache_mutex`, calls bridge function that locks `bridge->base.mutex`. Thread B holds `bridge->base.mutex`, calls brain function that locks `cache_mutex`.
**Mitigation**: Brain code copies data from cache under lock, then calls bridge code after releasing `cache_mutex`.

### Scenario 2: Event Bus + Working Memory
**Problem**: Event handler holds `subscriber_mutex`, processes event that modifies working memory (acquires `wm->mutex`). Meanwhile working memory code fires event (acquires `subscriber_mutex`).
**Mitigation**: Event handlers should queue work items instead of performing inline mutations.

### Scenario 3: COW Manager + Brain Cache
**Problem**: `nimcp_brain_clone_cow()` needs both COW manager mutex and brain cache mutex.
**Mitigation**: Brain cache mutex (Level 2) is acquired before COW manager mutex (Level 1), following the level ordering rule.

### Scenario 4: Memory Allocator Recursion
**Problem**: Exception handler allocates memory, memory allocator throws exception, exception handler allocates memory again.
**Mitigation**: Exception system files use `MEMORY_SAFE_THROW()` / `UMM_SAFE_THROW()` and never call `NIMCP_THROW_TO_IMMUNE`.

---

## Diagnostic Tools

### Deadlock Detector (`nimcp_deadlock_detector.h`)
```c
deadlock_detector_config_t config = deadlock_detector_default_config();
config.enable_lock_ordering = true;
config.enable_timeout = true;
config.default_timeout_ms = 5000;
deadlock_detector_init(&config);

tracked_mutex_t mutex_brain, mutex_bridge;
tracked_mutex_init(&mutex_brain, "brain_cache", 5000);
tracked_mutex_init(&mutex_bridge, "bridge_base", 5000);

tracked_mutex_lock(&mutex_brain);   // order 0 first -- OK
tracked_mutex_lock(&mutex_bridge);  // order 1 second -- OK
tracked_mutex_unlock(&mutex_bridge);
tracked_mutex_unlock(&mutex_brain);

uint32_t deadlocks = deadlock_detector_check();
deadlock_detector_report();
deadlock_detector_print_dependencies();
```

### ThreadSanitizer
```bash
cmake -DNIMCP_TSAN=ON ..
make -j4
ctest
```

---

## File Reference

| File | Role |
|------|------|
| `include/utils/platform/nimcp_platform_mutex.h` | Platform mutex API (Level 0) |
| `include/utils/thread/nimcp_thread.h` | Thread-layer mutex/rwlock/cond API |
| `include/utils/thread/nimcp_deadlock_detector.h` | Lock ordering enforcement and cycle detection |
| `include/utils/bridge/nimcp_bridge_base.h` | Bridge base struct with per-instance mutex |
| `include/utils/bridge/nimcp_bridge_boilerplate.h` | BRIDGE_BOILERPLATE macros (FULL/MESH_ONLY/MINIMAL) |
| `include/core/brain/nimcp_brain_internal.h` | Brain internal struct with `cache_mutex` |
| `src/utils/memory/nimcp_cow_manager.c` | COW manager mutex |
| `src/core/events/nimcp_event_bus.c` | Event bus triple-mutex scheme |
| `include/utils/fault_tolerance/nimcp_health_agent.h` | Health agent deadlock detection integration |
| `include/utils/fault_tolerance/nimcp_lockfree_metrics.h` | Lock-free metrics collection |
