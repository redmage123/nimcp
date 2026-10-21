# Bio-Async Swarm Integration TODO

## Status: PARTIALLY IMPLEMENTED (Needs Completion)

**Last Updated**: 2026-10-22

## What Was Done

1. **Brain Accessor Functions** ✅
   - Added `brain_get_bio_router()` to nimcp_brain.h
   - Added `brain_get_neuromodulator_system()` (already existed)
   - Implemented in nimcp_brain_part_accessors.c

2. **Brain Struct Updates** ✅
   - Added `bio_router` field to brain_struct (nimcp_brain_internal.h)
   - Added `bio_router_enabled` flag

3. **Type Definitions** ✅
   - Added `bio_router_t` forward declaration to nimcp_brain.h

## What Needs To Be Done

### 1. Bio-Router Initialization (HIGH PRIORITY)

The `bio_router` field in brain_struct is declared but NEVER INITIALIZED.

**Required Changes**:
- `src/core/brain/factory/nimcp_brain_factory.c`: Initialize bio_router during brain creation
- Call `bio_router_init()` (from nimcp_bio_router.h) in appropriate factory wave
- Set `brain->bio_router_enabled = true` when successful
- Add cleanup in brain destruction path

**Example**:
```c
// In brain factory init (wave TBD - after neuromodulator init)
brain->bio_router = bio_router_init(config);
if (brain->bio_router) {
    brain->bio_router_enabled = true;
}
```

### 2. Bio-Async API Fixes (HIGH PRIORITY)

The swarm quorum code uses OUTDATED bio-async message API.

**Current Issues**:
- Uses `.msg_type` field → should be `.type`
- Uses `.timestamp` → should be `.timestamp_us`
- Uses `.priority` field → doesn't exist in bio_message_header_t
- Uses `.source_module` → exists but may be set incorrectly
- Uses `BIO_PRIORITY_*` constants → don't exist

**Actual bio_message_header_t Structure**:
```c
typedef struct {
    bio_message_type_t type;        // NOT msg_type
    uint32_t sequence_id;
    uint32_t source_module;
    uint32_t target_module;
    uint64_t timestamp_us;          // NOT timestamp
    nimcp_bio_channel_type_t channel;
    uint32_t payload_size;
    uint32_t flags;                 // NO priority field
} bio_message_header_t;
```

**Required Changes** in `src/swarm/nimcp_swarm_quorum.c`:
- Replace all `.msg_type =` with `.type =`
- Replace all `.timestamp =` with `.timestamp_us =`
- Remove all `.priority =` lines
- Add `.sequence_id`, `.target_module`, `.channel`, `.payload_size`, `.flags` as needed
- Use `nimcp_time_get_monotonic_us()` instead of `_ns()` for timestamp

### 3. Swarm Struct Fixes (MEDIUM PRIORITY)

**Current Issues**:
- `nimcp_swarm_quorum_t` doesn't have `drone_id` field
- `nimcp_swarm_quorum_t` doesn't have `decision_strength` field

**Required Changes**:
- Add these fields to struct definition in `include/swarm/nimcp_swarm_quorum.h`
- OR use existing fields that serve the same purpose
- OR remove references to these fields from bio-async code

### 4. Bio-Router Send API (MEDIUM PRIORITY)

**Current Usage**:
```c
nimcp_bio_router_send(bio_router, &msg.header);
```

**Check**: Does this match actual API in `include/async/nimcp_bio_router.h`?
- Function signature may require additional parameters
- May need to specify message size, channel, etc.

### 5. Re-Enable in Build (FINAL STEP)

Once all above fixes are complete:
- Uncomment line in `src/lib/CMakeLists.txt`:
  ```cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_quorum.c
  ```
- Rebuild and verify no compilation errors
- Test swarm neuromodulator blending functionality

## Testing Checklist

- [ ] Brain initializes bio_router successfully
- [ ] `brain_get_bio_router()` returns non-NULL
- [ ] Swarm quorum code compiles without errors
- [ ] Neuromodulator messages send successfully
- [ ] Message handler receives and processes messages
- [ ] Neuromodulator blending (80% local + 20% swarm) works correctly
- [ ] No memory leaks in bio-router or message handling

## Files Modified

1. `include/core/brain/nimcp_brain.h` - Added bio_router accessor
2. `include/core/brain/nimcp_brain_internal.h` - Added bio_router field
3. `src/core/brain/nimcp_brain_part_accessors.c` - Implemented accessor
4. `src/swarm/nimcp_swarm_quorum.c` - Added bio-async integration (NEEDS API FIXES)
5. `src/lib/CMakeLists.txt` - Disabled swarm_quorum from build

## Estimated Effort

- Bio-router initialization: 2-4 hours
- API fixes in swarm_quorum: 1-2 hours
- Struct fixes: 1 hour
- Testing and debugging: 2-4 hours
- **Total**: 6-11 hours

## Notes

The bio-async swarm integration was originally attempted in commits:
- `67605b924d`: Initial implementation
- `8d03aeca1a`: API usage fixes
- `4bfce4e18f`: brain_t cast fixes

However, it was never completed because:
1. bio_router was never initialized in brain
2. Bio-async message API was misunderstood/outdated
3. Swarm struct didn't have required fields

This document captures what needs to be finished to make it fully functional.
