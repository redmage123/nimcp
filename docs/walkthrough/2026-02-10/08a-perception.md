# Walkthrough 08a: Perception Module Review

**Date**: 2026-02-10
**Scope**: `src/perception/` and subdirectories (`cortical/`, `immune/`, `integration/`, `sleep/`)
**Files Reviewed**: 19

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| P1 | 3 | NULL deref, buffer overflow |
| P2 | 6 | False positive NIMCP_THROW_TO_IMMUNE, resource leak, wrong error code |
| P3 | 5 | Wrong error messages, inconsistent mutex API, magic numbers |

---

## P1 Findings

### P1-1: omni_sensory_bridge query functions use uninitialized `bridge->mutex` instead of `bridge->base.mutex`

**File**: `/home/bbrelin/nimcp/src/perception/nimcp_omni_sensory_bridge.c`
**Lines**: 701, 710, 722, 731

The struct `omni_sensory_bridge` (defined in `include/perception/nimcp_omni_sensory_bridge.h:251`) has both `bridge_base_t base` (with `base.mutex`) and a separate `void* mutex` field (line 283). The `create` function initializes only `bridge->base.mutex` via `bridge_base_init()` (line 173). The separate `bridge->mutex` field is never set and remains NULL from `nimcp_calloc`.

Four query functions pass this NULL pointer to `nimcp_mutex_lock()`:

```c
// Line 701 - omni_sensory_get_omni_effects
nimcp_mutex_lock(((omni_sensory_bridge_t*)bridge)->mutex);  // mutex is NULL!

// Line 710 - omni_sensory_get_sensory_effects
nimcp_mutex_lock(((omni_sensory_bridge_t*)bridge)->mutex);  // mutex is NULL!

// Line 722 - omni_sensory_get_modality_state
nimcp_mutex_lock(((omni_sensory_bridge_t*)bridge)->mutex);  // mutex is NULL!

// Line 731 - omni_sensory_get_stats
nimcp_mutex_lock(((omni_sensory_bridge_t*)bridge)->mutex);  // mutex is NULL!
```

Meanwhile, other functions like `omni_sensory_reset_stats` (line 739) and `omni_sensory_update` correctly use `bridge->base.mutex`.

**Fix**: Change all four query functions to use `bridge->base.mutex` instead of `bridge->mutex`. Also consider removing the duplicate `void* mutex` field from the struct to prevent future confusion.

---

### P1-2: `update_modality_state` sets `dim` before comparing it, causing buffer overflow on dimension change

**File**: `/home/bbrelin/nimcp/src/perception/nimcp_omni_sensory_bridge.c`
**Lines**: 110, 115

```c
static void update_modality_state(omni_modality_state_t* state,
                                   const float* features,
                                   uint32_t dim,
                                   float pe) {
    if (!state) return;
    state->prediction_error = pe;
    state->dim = dim;            // Line 110: dim is set HERE
    state->active = (features != NULL);

    if (features && dim > 0) {
        if (!state->features || state->dim != dim) {  // Line 115: state->dim == dim ALWAYS
```

At line 110, `state->dim` is assigned `dim`. Then at line 115, the condition `state->dim != dim` is always false because they were just made equal. This means the buffer is only reallocated when `state->features` is NULL (first call). On subsequent calls with a different dimension, `memcpy` at line 120 writes `dim * sizeof(float)` bytes into a buffer sized for the original dimension -- a heap buffer overflow if `dim` grew.

**Fix**: Move `state->dim = dim;` to after the reallocation block (after line 121), or compare against the old dim by swapping the order:

```c
if (features && dim > 0) {
    if (!state->features || state->dim != dim) {  // Compare BEFORE setting
        if (state->features) nimcp_free(state->features);
        state->features = nimcp_calloc(dim, sizeof(float));
    }
    state->dim = dim;  // Set AFTER comparison
```

---

### P1-3: Stack buffer overflow in speech_jepa_bridge encoding functions

**File**: `/home/bbrelin/nimcp/src/perception/nimcp_speech_jepa_bridge.c`
**Lines**: 414, 531, 672, 720

Four locations use a fixed-size stack buffer `float encoding[512]` or `float frame_encoding[512]` to hold encoder output. If the encoder's `output_dim` (which is the `latent_dim` configuration) exceeds 512, the encoder writes past the stack buffer.

```c
// Line 414
float frame_encoding[512];  /* Max latent dim */
rc = speech_encoder_forward(bridge->encoder, bridge->frame_buffer, frame_encoding);

// Line 531
float encoding[512];
rc = speech_encoder_forward(bridge->encoder, bridge->frame_buffer, encoding);

// Line 672
float encoding[512];
rc = speech_encoder_forward(bridge->encoder, bridge->frame_buffer, encoding);

// Line 720
float encoding[512];
rc = speech_encoder_forward(target_enc, bridge->frame_buffer, encoding);
```

**Fix**: Replace the hardcoded 512 with a dynamic allocation based on `bridge->config.latent_dim`, or add an assertion/clamp: `assert(bridge->config.latent_dim <= 512)` in the create function. Alternatively, use `alloca()` or heap allocation with proper size.

---

## P2 Findings

### P2-1: False positive NIMCP_THROW_TO_IMMUNE in lip_reading face detection paths

**File**: `/home/bbrelin/nimcp/src/perception/nimcp_lip_reading.c`
**Lines**: 597, 669, 1147

These are normal control-flow paths where face detection or feature extraction fails -- not programming errors:

```c
// Line 597 - not enough skin pixels for face detection
if (skin_count < (width * height) / 50) {
    system->last_error = LIP_READING_ERROR_NO_FACE_DETECTED;
    system->stats.face_detection_failures++;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
        "lip_reading_detect_face: validation failed");  // False positive
    return false;
}

// Line 669 - face not detected in result
if (!face_result->face_detected) {
    system->last_error = LIP_READING_ERROR_NO_FACE_DETECTED;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
        "lip_reading_extract_mouth_roi: face_result->face_detected is NULL");  // False positive
    return false;
}

// Line 1147 - feature extraction failure
if (!lip_reading_extract_features(system, mouth_roi, roi_width, roi_height,
                                   NULL, &features)) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
        "lip_reading_classify_viseme_from_roi: validation failed");  // False positive
    return false;
}
```

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` calls. The `return false` with `system->last_error` is sufficient for these expected failure paths. Additionally, line 669 incorrectly uses `NIMCP_ERROR_NULL_POINTER` when the issue is not a NULL pointer.

---

### P2-2: False positive NIMCP_THROW_TO_IMMUNE in speech_immune_bridge `is_illness_word`

**File**: `/home/bbrelin/nimcp/src/perception/immune/nimcp_speech_immune_bridge.c`
**Lines**: 100-108

```c
static bool is_illness_word(const char* word) {
    if (!word) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "is_illness_word: word is NULL");  // False positive - normal validation
        return false;
    }
```

A NULL word is a normal validation rejection. The function already returns `false`, which is the correct behavior. The `NIMCP_THROW_TO_IMMUNE` is unnecessary and fires on a normal code path.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call. Keep the `return false`.

---

### P2-3: False positive NIMCP_THROW_TO_IMMUNE in perception_bio_async `find_subscription`

**File**: `/home/bbrelin/nimcp/src/perception/integration/nimcp_perception_bio_async_bridge.c`
**Line**: 77

```c
static percept_bio_subscription_t* find_subscription(
    perception_bio_bridge_t* b,
    percept_modality_t modality,
    uint32_t module_id
) {
    if (modality >= PERCEPT_MODALITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW,
            "find_subscription: capacity exceeded");  // Misleading
        return NULL;
    }
```

While out-of-range modality is arguably an error, calling it `NIMCP_ERROR_BUFFER_OVERFLOW` is misleading. It should be `NIMCP_ERROR_INVALID_PARAM` or `NIMCP_ERROR_OUT_OF_RANGE`. The caller `perception_bio_async_subscribe_module` already handles `modality >= PERCEPT_MODALITY_COUNT` by recursing over all modalities, so this throw fires on a valid code path.

**Fix**: Change to `NIMCP_ERROR_OUT_OF_RANGE` with a more accurate message, or remove the throw since callers guard against this.

---

### P2-4: False positive NIMCP_THROW_TO_IMMUNE in `perception_bio_async_unsubscribe_module` for missing subscription

**File**: `/home/bbrelin/nimcp/src/perception/integration/nimcp_perception_bio_async_bridge.c`
**Lines**: 1054-1056

```c
percept_bio_subscription_t* sub = find_subscription(bridge, modality, module_id);
if (!sub) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
        "perception_bio_async_unsubscribe_module: sub is NULL");
    return -1;
}
```

Unsubscribing a non-existent subscription is a "not found" path, not a critical error. The error code `NIMCP_ERROR_NULL_POINTER` is also wrong -- the pointer is not NULL, the subscription simply does not exist.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`. Return a "not found" status code or silently succeed.

---

### P2-5: Resource leak in visual_jepa_bridge_encode_patches on error

**File**: `/home/bbrelin/nimcp/src/perception/nimcp_visual_jepa_bridge.c`
**Lines**: 503-529

In the loop that processes patches, if `NIMCP_CHECK_THROW` at line 504 fires (allocation failure) or `visual_jepa_bridge_encode` fails at line 528-529, the function returns immediately without freeing any previously-allocated `patch_latents[0..patch_idx-1]`. Those latents were allocated by the caller, but the encode results written into them are lost and no cleanup occurs.

```c
float* enc_input = nimcp_malloc(input_dim * sizeof(float));
NIMCP_CHECK_THROW(enc_input, NIMCP_ERROR_NO_MEMORY, ...);  // Returns, leaking state

// ...
if (result != NIMCP_SUCCESS) {
    return result;  // Returns without cleanup
}
```

**Fix**: Add a `goto cleanup` error path that frees any allocated intermediate state, or document that the caller owns the patch_latents and is responsible for cleanup.

---

### P2-6: Wrong error code in cortical bridge create validation

**Files**:
- `/home/bbrelin/nimcp/src/perception/cortical/nimcp_audio_cortical_bridge.c` line 186
- `/home/bbrelin/nimcp/src/perception/cortical/nimcp_visual_cortical_bridge.c` line 244
- `/home/bbrelin/nimcp/src/perception/cortical/nimcp_speech_cortical_bridge.c` line 271

All three cortical bridge create functions use `NIMCP_ERROR_NULL_POINTER` when the config validation fails (num_hypercolumns is 0 or too large):

```c
// audio_cortical_bridge.c line 186
if (config->num_hypercolumns == 0 || config->num_hypercolumns > AUDIO_CORTICAL_MAX_HYPERCOLUMNS) {
    NIMCP_LOGGING_ERROR("Invalid num_hypercolumns: %u", config->num_hypercolumns);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,  // Wrong! Not a NULL pointer
        "audio_cortical_bridge_create: config is NULL");  // Message is also wrong
    return NULL;
}
```

**Fix**: Change to `NIMCP_ERROR_INVALID_PARAM` with an accurate message like `"audio_cortical_bridge_create: invalid num_hypercolumns"`.

---

## P3 Findings

### P3-1: Wrong function name in error messages in omni_sensory_are_bound

**File**: `/home/bbrelin/nimcp/src/perception/nimcp_omni_sensory_bridge.c`
**Lines**: 568, 572

```c
bool omni_sensory_are_bound(const omni_sensory_bridge_t* bridge,
                             omni_modality_t m1, omni_modality_t m2) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "omni_sensory_apply_to_omni: bridge is NULL");  // Wrong: should be omni_sensory_are_bound
        return false;
    }
    if (m1 >= OMNI_MODALITY_COUNT || m2 >= OMNI_MODALITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW,
            "omni_sensory_apply_to_omni: capacity exceeded");  // Wrong: should be omni_sensory_are_bound
        return false;
    }
```

**Fix**: Change error messages to reference `omni_sensory_are_bound`.

---

### P3-2: Inconsistent mutex API in immune bridges (platform vs thread layer)

**Files**:
- `/home/bbrelin/nimcp/src/perception/immune/nimcp_audio_immune_bridge.c`
- `/home/bbrelin/nimcp/src/perception/immune/nimcp_speech_immune_bridge.c`
- `/home/bbrelin/nimcp/src/perception/immune/nimcp_visual_immune_bridge.c`

All three immune bridge files use `nimcp_platform_mutex_lock/unlock` (the platform layer) instead of `nimcp_mutex_lock/unlock` (the thread layer) used by all other perception bridges.

**Fix**: Change to `nimcp_mutex_lock/unlock` for consistency with the rest of the perception module.

---

### P3-3: perception_bio_async_bridge has no thread safety (no mutex)

**File**: `/home/bbrelin/nimcp/src/perception/integration/nimcp_perception_bio_async_bridge.c`

The `perception_bio_bridge_struct` has a `bridge_base_t base` field but never initializes the base mutex via `bridge_base_init()`. The create function (line 186) uses `nimcp_calloc` and copies config but never creates a mutex. All functions operate without any locking.

This is not necessarily a bug if the bridge is designed to be single-threaded, but it is inconsistent with the rest of the perception module which uses mutex protection.

**Fix**: Either add `bridge_base_init()` and mutex protection, or document that this bridge is not thread-safe.

---

### P3-4: Magic number 512 for encoder output buffer size

**File**: `/home/bbrelin/nimcp/src/perception/nimcp_speech_jepa_bridge.c`
**Lines**: 414, 531, 672, 720

The value 512 is used as a hardcoded maximum latent dimension in four locations. This should be a named constant:

```c
#define SPEECH_JEPA_MAX_LATENT_DIM 512
```

And validated at create time to ensure `config.latent_dim <= SPEECH_JEPA_MAX_LATENT_DIM`.

**Fix**: Define a named constant, validate in the create function, and use the constant in all four locations.

---

### P3-5: Unused `nimcp_mutex_t mutex` and `bool mutex_initialized` fields in cortical bridge structs

**Files**:
- `/home/bbrelin/nimcp/src/perception/cortical/nimcp_audio_cortical_bridge.c` lines 68-69
- `/home/bbrelin/nimcp/src/perception/cortical/nimcp_visual_cortical_bridge.c` lines 70-71
- `/home/bbrelin/nimcp/src/perception/cortical/nimcp_speech_cortical_bridge.c` lines 67-68

All three cortical bridge internal structs declare `nimcp_mutex_t mutex` and `bool mutex_initialized` fields that are never used. The bridges correctly use `bridge->base.mutex` via the `bridge_base_t` base struct. These dead fields waste memory and could cause confusion.

```c
struct audio_cortical_bridge {
    bridge_base_t base;  // Contains the actual mutex
    // ...
    nimcp_mutex_t mutex;         // UNUSED - dead field
    bool mutex_initialized;      // UNUSED - dead field
};
```

**Fix**: Remove the unused `mutex` and `mutex_initialized` fields from all three cortical bridge structs.

---

## Files Reviewed (No Issues Found)

The following files were reviewed and found to have no P1/P2/P3 issues:

| File | Notes |
|------|-------|
| `src/perception/nimcp_visual_jepa_fep_bridge.c` (601 lines) | Clean. Proper error handling, mutex usage, and cleanup. |
| `src/perception/nimcp_audio_cortex_fep_bridge.c` (487 lines) | Thread-unsafe const getters (known issue, not flagged). |
| `src/perception/nimcp_speech_cortex_fep_bridge.c` (532 lines) | Thread-unsafe const getters (known issue, not flagged). |
| `src/perception/nimcp_visual_cortex_fep_bridge.c` (508 lines) | Thread-unsafe const getters (known issue, not flagged). |
| `src/perception/sleep/nimcp_audio_cortex_sleep_bridge.c` (276 lines) | Clean. Proper callback registration/unregistration. |
| `src/perception/sleep/nimcp_retina_sleep_bridge.c` (279 lines) | Clean. |
| `src/perception/sleep/nimcp_speech_cortex_sleep_bridge.c` (267 lines) | Clean. |
| `src/perception/sleep/nimcp_visual_cortex_sleep_bridge.c` (276 lines) | Clean. |

---

## Known Issues (Not Flagged)

- **Thread-unsafe const getters** in FEP bridges and cortical bridges cast away `const` to lock/unlock mutexes. This is a known project-wide pattern.
- **Cortical bridges** use conditional mutex locking (`if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(...)`) which continues without thread safety if mutex init failed. This is intentional per the WARN log message in create functions.
