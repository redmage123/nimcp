# NIMCP Utils Test Coverage Plan
## Objective: 100% Test Coverage for All Utils Modules

## Utils Modules Inventory

### 1. **Containers** (Priority: HIGH)
- `nimcp_vector.c` - Dynamic array
- `nimcp_queue.c` - Queue data structure
- `nimcp_hash_table.c` - Hash table
- `nimcp_btree.c` - B-tree
- `nimcp_graph.c` - Graph data structure
- `nimcp_min_heap.c` - Min heap priority queue

**Test Strategy**: Unit tests for each operation (create, insert, delete, search), integration tests for combined usage, regression tests for edge cases

### 2. **Memory Management** (Priority: CRITICAL)
- `nimcp_memory.c` - Memory allocation/tracking
- `nimcp_memory_guards.c` - Memory corruption detection

**Test Strategy**: Unit tests for alloc/free, integration tests with containers, regression tests for leaks/corruption

### 3. **Threading** (Priority: HIGH)
- `nimcp_thread.c` - Thread primitives
- `nimcp_thread_pool.c` - Thread pool
- `nimcp_deadlock_detector.c` - Deadlock detection

**Test Strategy**: Unit tests for thread operations, integration tests for concurrent scenarios, regression tests for race conditions/deadlocks

### 4. **Platform Abstraction** (Priority: HIGH)
- `nimcp_platform_mutex.c` - Mutex primitives
- `nimcp_platform_cond.c` - Condition variables
- `nimcp_platform_rwlock.c` - Read-write locks
- `nimcp_platform_thread.c` - Thread creation
- `nimcp_platform_time.c` - Time functions
- `nimcp_platform_once.c` - One-time initialization
- `nimcp_platform.c` - General platform utilities

**Test Strategy**: Unit tests for each primitive, integration tests for synchronization patterns, regression tests for platform-specific issues

### 5. **Cache** (Priority: HIGH)
- `nimcp_cache.c` - Caching system

**Test Strategy**: Unit tests for cache operations, integration tests with COW, regression tests for cache coherency

### 6. **Configuration** (Priority: MEDIUM)
- `nimcp_config.c` - Configuration management
- `nimcp_dynamic_config.c` - Runtime config updates

**Test Strategy**: Unit tests for config loading/saving, integration tests with brain initialization, regression tests for config validation

### 7. **Specialized Modules** (Priority: MEDIUM)
- `nimcp_json.c` - JSON parsing/generation
- `nimcp_fft.c` - Fast Fourier Transform
- `nimcp_quantum_walk.c` - Quantum walk simulation
- `nimcp_mps.c` - Matrix Product States

**Test Strategy**: Unit tests for core algorithms, integration tests with using modules, regression tests for numerical accuracy

### 8. **Support Modules** (Priority: MEDIUM)
- `nimcp_logging.c` - Logging system
- `nimcp_metrics.c` - Metrics collection
- `nimcp_error_codes.c` - Error handling
- `nimcp_validate.c` - Input validation
- `nimcp_signal_handler.c` - Signal handling
- `nimcp_queue_manager.c` - Queue management
- `nimcp_time.c` - Time utilities
- `nimcp_integration.c` - Numerical integration
- `nimcp_hyperbolic.c` - Hyperbolic geometry

**Test Strategy**: Unit tests for each function, integration tests with consumers, regression tests for error conditions

## Test Implementation Order

### Phase 1: Critical Infrastructure (Estimated: 40 tests)
1. **Memory Management** (15 tests)
   - Unit: alloc, free, realloc, alignment, tracking
   - Integration: with containers
   - Regression: leak detection, double-free, use-after-free

2. **Threading Primitives** (15 tests)
   - Unit: mutex, cond, rwlock, thread create/join
   - Integration: producer-consumer, reader-writer
   - Regression: deadlock scenarios, race conditions

3. **Containers - Vector & Queue** (10 tests)
   - Unit: CRUD operations, resize, capacity
   - Integration: multi-threaded access
   - Regression: boundary conditions, memory leaks

### Phase 2: Core Utilities (Estimated: 50 tests)
4. **Containers - Hash Table & B-Tree** (15 tests)
5. **Cache System** (10 tests)
6. **Thread Pool** (10 tests)
7. **Platform Time & Mutex** (10 tests)
8. **Configuration** (5 tests)

### Phase 3: Specialized & Support (Estimated: 40 tests)
9. **JSON Parsing** (10 tests)
10. **FFT** (5 tests)
11. **Quantum Walk** (5 tests)
12. **Logging & Metrics** (10 tests)
13. **Error Handling & Validation** (10 tests)

## Target: 130+ tests for utils modules

## Test File Structure

```
test/unit/
├── test_utils_memory.cpp           (memory management)
├── test_utils_vector.cpp           (vector container)
├── test_utils_queue.cpp            (queue - enhance existing)
├── test_utils_hash_table.cpp       (hash table)
├── test_utils_btree.cpp            (b-tree)
├── test_utils_graph.cpp            (graph)
├── test_utils_min_heap.cpp         (min heap)
├── test_utils_thread.cpp           (threads - enhance existing)
├── test_utils_thread_pool.cpp      (thread pool)
├── test_utils_mutex.cpp            (mutex primitives)
├── test_utils_cache.cpp            (cache system)
├── test_utils_json.cpp             (JSON parsing)
├── test_utils_fft.cpp              (FFT)
├── test_utils_config.cpp           (configuration)
└── test_utils_comprehensive.cpp    (combined scenarios)

test/integration/
├── test_utils_memory_containers.cpp    (memory + containers)
├── test_utils_thread_sync.cpp          (thread synchronization)
├── test_utils_cache_cow.cpp            (cache + COW)
└── test_utils_platform_integration.cpp (platform abstractions)

test/regression/
├── test_utils_memory_leaks.cpp         (memory leak scenarios)
├── test_utils_thread_safety.cpp        (thread safety)
└── test_utils_backward_compat.cpp      (API compatibility)
```

## Success Criteria

- ✅ 100% line coverage of all utils modules
- ✅ All edge cases covered (NULL, boundary, error conditions)
- ✅ Thread safety verified for concurrent modules
- ✅ Memory leak detection passing
- ✅ Performance regression tests passing
- ✅ API backward compatibility verified
