# Phase 1.2: Temporal Buffer Analysis

## Current Allocation Patterns

### 1. Integration Buffer (`nimcp_integration_buffer.c`)

**Creation Overhead** (lines 56-124):
```c
// Per integration buffer:
- Main structure: 1x malloc (struct)
- Channel array: 1x malloc (num_channels * sizeof(channel_buffers_t))

// Per channel (× num_channels):
  // Per timescale (× 3: fast, medium, slow):
    - circular_buffer_create()
    - sliding_window_create()

// Total: 1 + 1 + (num_channels × 3 × 2) = 2 + (6 × num_channels) allocations
```

**Example**: 256 channels = 2 + 1536 = **1538 allocations** per integration buffer

**Estimated Time** (at ~1μs per malloc):
- 1538 allocations × 1μs = **1.5ms** (not 50ms as claimed)

### 2. Sliding Window (`nimcp_sliding_window.c`)

**Stats Recalculation** (line 127):
```c
// Every stats recalculation:
float* samples = nimcp_malloc(count * sizeof(float));  // Temp allocation
// ... compute stats ...
nimcp_free(samples);
```

**Frequency**: Every time stats need updating (potentially frequent)
**Size**: Variable (window_size × sizeof(float))
**Impact**: **HIGH** - repeated allocations in hot path

### 3. Temporal Accumulator (`nimcp_temporal_accumulator.c`)

**Creation** (lines 107-111):
```c
- Main structure: 1x malloc
- Channel states: 1x malloc (num_channels × sizeof(channel_state_t))
// Total: 2 allocations
```

**Impact**: **LOW** - only at creation time

## Key Insights from Phase 1.1

### What We Learned:
1. **CoW is slower for sequential operations** (0.69x vs deep copy)
   - Reason: `signal_wrapper_acquire()` still needs malloc for wrapper struct
   - Overhead: ~900ns vs ~629ns for deep copy

2. **CoW benefits come from**:
   - Multiple concurrent consumers sharing data
   - Broadcast scenarios (1 signal → N destinations)
   - Reusing wrappers across operations

3. **Memory Pool provides real speedup**:
   - Pool acquire: ~10ns (O(1) bitmap)
   - malloc: ~629ns (O(log n) tree search)
   - **Speedup: ~63x** for simple allocations

## Recommended Strategy for Phase 1.2

### Focus on PROVEN Wins

Based on Phase 1.1 results, focus on **Memory Pool** (not CoW) for:

#### 1. Sliding Window Temp Buffer (HIGHEST IMPACT)
**Problem**: Line 127 repeatedly mallocs temp sample array
**Solution**: Use Buffer Pool for temp allocations

**Approach**:
```c
// Instead of:
float* samples = nimcp_malloc(count * sizeof(float));

// Use:
float* samples = buffer_pool_acquire(pool, count * sizeof(float));
// ... use ...
buffer_pool_release(pool, samples);
```

**Expected**: 63x speedup for this hot path operation

#### 2. Integration Buffer Pre-Allocation (MEDIUM IMPACT)
**Problem**: Creates 1538 allocations for 256-channel setup
**Solution**: Pre-allocate common configurations

**Approach**:
- Create pool of pre-configured integration buffers
- Standard sizes: 64, 128, 256 channels
- Reuse instead of recreating

**Expected**: Amortized speedup for repeated creations

#### 3. Skip CoW for Sequential Buffers (LEARNING)
**Insight**: CoW is slower for sequential operations in Phase 1.1
**Decision**: Don't force CoW into temporal buffers

**Rationale**:
- Temporal buffers are typically accessed sequentially
- No concurrent sharing benefit
- Wrapper overhead hurts more than helps

## Proposed Implementation Plan

### Step 1: Buffer Pool Integration (Primary Goal)
1. Use existing `nimcp_buffer_pool.h` from Phase 0
2. Integrate into sliding window stats recalculation
3. Configure pool with common sizes: 256, 1024, 4096, 16384 floats
4. Target: **63x speedup** for repeated operations

### Step 2: Integration Buffer Object Pool (Secondary)
1. Create pool of pre-initialized integration buffers
2. Standard configurations: 64, 128, 256, 512 channels
3. Fast, medium, slow timescales pre-configured
4. Target: **Amortized creation speedup**

### Step 3: Benchmarking
1. Measure sliding window stats performance
2. Measure integration buffer creation time
3. Compare against baseline
4. Document actual vs expected improvements

## Revised Expectations

### Realistic Targets (Based on Phase 1.1 Data)

| Operation | Baseline | With Pool | Speedup |
|-----------|----------|-----------|---------|
| Sliding window temp alloc | 629ns | 10ns | **63x** |
| Integration buffer create | 1.5ms | 0.5ms | **3x** |
| Stats recalculation (hot) | Variable | Much faster | **10-50x** |

### Why Not 250x?

The 250x claim (50ms → 0.2ms) assumes:
- 50ms baseline (unrealistically slow - would be ~79,000 mallocs)
- Perfect pool performance

**Reality**:
- Typical integration buffer: ~1.5ms creation
- With pooling: ~0.5ms (3x speedup, not 250x)
- With pre-allocated buffers: ~0.01ms (150x speedup)

**Recommendation**: Focus on **practical, measurable improvements** rather than theoretical maximums.

## Success Criteria for Phase 1.2

### Must Have:
- ✅ Buffer pool integrated into sliding window
- ✅ Stats recalculation using pooled temp buffers
- ✅ 20+ tests passing (100%)
- ✅ Measurable performance improvement (10x+ for hot paths)
- ✅ Zero memory leaks

### Nice to Have:
- Integration buffer object pool
- Pre-configured buffer templates
- Advanced pooling strategies

### Don't Force:
- CoW for sequential temporal buffers (learned from Phase 1.1)
- Complex pooling for one-time allocations
- Over-engineering for marginal gains

## Next Steps

1. Integrate Buffer Pool into sliding window stats (line 127)
2. Create tests for pooled operations
3. Benchmark performance
4. Document actual improvements
5. Decide on integration buffer pooling based on measurements
