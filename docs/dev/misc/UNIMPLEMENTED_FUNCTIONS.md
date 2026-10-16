# NIMCP Unimplemented Functions & TODOs

This document tracks placeholder implementations, TODOs, and unimplemented functions that need to be completed for 100% code coverage.

## Priority 1: Core Functionality (High Impact)

### 1. Network Serialization Checksum Verification
- **File**: `src/io/serialization/nimcp_network_serialization.c:483`
- **TODO**: Recalculate and verify checksum
- **Impact**: Data integrity for saved networks
- **Priority**: HIGH

### 2. CSV Data Loader - num_features
- **File**: `src/io/dataio/nimcp_dataio.c:302`
- **Status**: ✅ FIXED (commit 0ce4a57)
- **Solution**: Added num_features to csv_context_t, populated from config

## Priority 2: Backend Implementations (Medium Impact)

### 3. PostgreSQL Data Backend
- **File**: `src/io/dataio/nimcp_dataio.c:397-432`
- **Status**: Placeholder implementation
- **TODOs**:
  - Line 406: Implement with libpq
  - Line 415: Implement PQfinish, PQclear
  - Line 420: Fetch next N rows from result
  - Line 426: Re-execute query
  - Line 432: PQntuples
- **Impact**: Database-backed training data
- **Priority**: MEDIUM

### 4. Alternative Data Backends
- **File**: `src/io/dataio/nimcp_dataio.c:512-530`
- **TODOs**:
  - Line 512: Implement JSON backend
  - Line 516: Implement Parquet backend
  - Line 530: Implement SQLite backend
- **Impact**: Support for additional data formats
- **Priority**: MEDIUM

### 5. Redis/PostgreSQL Replication Strategies
- **File**: `src/networking/replication/nimcp_replication.c:651-661`
- **TODOs**:
  - Line 651: Implement Redis strategy
  - Line 661: Implement PostgreSQL strategy
- **Impact**: Distributed brain replication
- **Priority**: MEDIUM

## Priority 3: API Enhancements (Lower Impact)

### 6. Node.js Binding - Network Output Size
- **File**: `src/bindings/nodejs/binding.c:150`
- **TODO**: Get actual output size from network
- **Current**: Assumes num_outputs = num_inputs
- **Impact**: Node.js API accuracy
- **Priority**: LOW (assumption is reasonable for SNNs)

### 7. Brain Learning Example Integration
- **File**: `src/io/dataio/nimcp_dataio.c:898`
- **TODO**: Replace with actual brain_learn_example call when API available
- **Impact**: Training loop integration
- **Priority**: MEDIUM

### 8. Brain Decision/Prediction Export
- **File**: `src/io/dataio/nimcp_dataio.c:1121-1128`
- **TODOs**:
  - Line 1121: Get brain decision
  - Line 1126: Write features
  - Line 1128: Write prediction and confidence
- **Impact**: Model inference export
- **Priority**: MEDIUM

### 9. Brain COW Snapshot Implementation
- **File**: `src/api/nimcp.c:649`
- **TODO**: Implement actual COW snapshot using nimcp_cache_reference
- **Impact**: Copy-on-write snapshots
- **Priority**: LOW (current impl functional)

### 10. P2P Network Metrics
- **File**: `src/networking/p2p/nimcp_p2pnode.c:991-992`
- **TODOs**:
  - Line 991: Calculate coordinates from network metrics
  - Line 992: Exchange capabilities in handshake
- **Impact**: P2P network topology
- **Priority**: LOW

## Priority 4: Configuration & Tuning (Lowest Impact)

### 11. Brain Configuration Apply
- **File**: `src/api/nimcp.c:513`
- **TODO**: Apply plasticity settings, ethics config, etc.
- **Impact**: Additional configuration options
- **Priority**: LOW

### 12. Distributed Cognition Stats
- **File**: `src/networking/distributed/nimcp_distributed_cognition.c:321`
- **Note**: Increment stats as placeholder
- **Impact**: Monitoring/metrics
- **Priority**: LOW

### 13. NLP Coherence Analysis
- **File**: `src/nlp/nimcp_spike_nlp.c:380`
- **Note**: Placeholder - real coherence would use variance analysis
- **Impact**: NLP quality metrics
- **Priority**: LOW

## Testing Requirements for 100% Coverage

To achieve 100% code coverage, we need:

### Unit Tests
- [ ] CSV data loader with various feature dimensions
- [ ] Network serialization with checksum verification
- [ ] Backend strategy pattern (mock implementations)
- [ ] Node.js binding edge cases
- [ ] COW snapshot creation and restoration

### Integration Tests
- [ ] End-to-end data loading and training
- [ ] Network save/load with validation
- [ ] Multi-backend data streaming
- [ ] P2P network formation and communication
- [ ] Distributed cognition with replication

### Regression Tests
- [ ] Backward compatibility for serialized networks
- [ ] Data format compatibility (CSV, JSON, etc.)
- [ ] API stability for bindings
- [ ] Configuration changes don't break existing code

## Implementation Strategy

1. **Phase 1**: Complete core functionality (Priority 1)
   - Network checksum verification
   - ✅ CSV num_features fix

2. **Phase 2**: Implement critical backends (Priority 2)
   - PostgreSQL backend
   - JSON/Parquet/SQLite backends
   - Replication strategies

3. **Phase 3**: API enhancements (Priority 3)
   - Brain learning integration
   - Decision/prediction export
   - P2P metrics

4. **Phase 4**: Polish and optimization (Priority 4)
   - Configuration apply
   - Stats and monitoring
   - NLP coherence

5. **Phase 5**: Comprehensive testing
   - Write unit tests for all implemented functions
   - Create integration tests for workflows
   - Add regression tests for backward compatibility

## Progress Tracking

### Core NIMCP Features (Completed)
- [x] CSV num_features fix (0ce4a57)
- [x] Network checksum verification (107a446)
- [x] Brain learning integration (de5d7e7)
- [x] Brain decision/prediction export (de5d7e7)

### Application-Level Features (Deferred - Not Core Library)
- [ ] PostgreSQL backend (application concern)
- [ ] JSON/Parquet/SQLite backends (application concern)
- [ ] Redis/PostgreSQL replication (application concern)

### Testing & Coverage (In Progress)
- [ ] Unit tests for new features
- [ ] Integration tests
- [ ] Regression tests
- [ ] Verify coverage improvements

### Optimizations (Future Work)

#### COW (Copy-on-Write) Snapshot Optimization

**Current Implementation** (`src/api/nimcp.c:651-704`):
- Uses save/load to temporary file for snapshot creation
- Guarantees complete independence between snapshot and original
- Performance: ~100-500ms for typical brain (file I/O overhead)
- Memory: 2x brain size temporarily during snapshot
- **Status**: ✅ Functionally correct, not performance-optimized

**True COW Requirements**:
1. **Shared Memory Phase**:
   - Initial snapshot shares all memory pages with original brain
   - Mark shared pages/structures as read-only or use reference counting
   - Zero-copy creation: <1ms snapshot time
   - Memory: ~48 bytes overhead (just metadata)

2. **Copy-on-Write Phase**:
   - Intercept write operations to shared structures
   - Copy specific page/structure only when modified
   - Update pointers in writing brain to new copy
   - Other brain continues using shared copy

3. **Required Infrastructure**:
   - Modify `brain_t` structure to support shared pointers
   - Add copy-on-write flags to all major data structures:
     * Neural network weights/synapses
     * Cognitive module states
     * Memory buffers (working memory, engrams, etc.)
     * Neuromodulator states
   - Implement write interceptors for each structure type
   - Reference counting mechanism (can use `nimcp_cache_reference`)
   - Atomic operations for thread-safe COW

4. **Implementation Phases**:
   - **Phase 1**: Reference-counted structures (LOW hanging fruit)
     * Use `nimcp_cache_reference()` for heap-allocated buffers
     * Implement copy-on-write for neural network weights
     * Estimated effort: 2-3 days
     * Performance gain: 50% snapshot time reduction

   - **Phase 2**: Page-level COW (MEDIUM complexity)
     * Use `mprotect()` for memory page protection
     * Trap SIGSEGV on write to protected pages
     * Copy page and remap on write
     * Estimated effort: 1-2 weeks
     * Performance gain: 90% snapshot time reduction

   - **Phase 3**: Full structural COW (HIGH complexity)
     * Copy-on-write for all brain data structures
     * Thread-safe snapshot operations
     * Garbage collection for unreferenced copies
     * Estimated effort: 3-4 weeks
     * Performance gain: 95%+ snapshot time reduction

**Recommendation**:
Current implementation is sufficient for most use cases. Defer optimization until:
1. Profiling shows snapshot time is a bottleneck (>10% of total runtime)
2. Snapshots are created frequently (>10/second)
3. Brain size exceeds 1GB (where file I/O becomes significant)

For now, the save/load approach provides:
- ✅ Correctness (perfect state preservation)
- ✅ Simplicity (no complex COW logic to debug)
- ✅ Thread safety (no shared state race conditions)
- ✅ Portability (works on all platforms)

**Status**: Documented as low-priority optimization

---

#### Other Optimizations

- [ ] P2P network metrics calculation
- [ ] NLP coherence variance analysis

Last Updated: 2025-11-15
