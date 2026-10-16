//=============================================================================
// nimcp_flashbulb.c - Flashbulb Memory System Implementation
//=============================================================================
/**
 * @file nimcp_flashbulb.c
 * @brief Implementation of flashbulb memory system for enhanced emotional encoding
 *
 * This file implements the flashbulb memory system which handles:
 * - Detection of flashbulb-worthy events based on arousal/surprise/significance
 * - Enhanced encoding with arousal-modulated strength
 * - Contextual detail capture (location, informant, activity, etc.)
 * - Vividness assessment and accuracy verification
 * - Reconsolidation processing
 * - Trauma-specific handling (intrusions, avoidance, therapy)
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_flashbulb.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(flashbulb, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Constants
//=============================================================================

/** Magic number for validation */
#define FLASHBULB_MAGIC 0xFB5B00

/** Default initial capacity for flashbulb array */
#define DEFAULT_FLASHBULB_CAPACITY 128

/** Default initial capacity for trauma tracking */
#define DEFAULT_TRAUMA_CAPACITY 32

/** Base arousal decay rate per millisecond */
#define AROUSAL_DECAY_RATE_MS 0.0001f

/** Vividness decay rate per day */
#define VIVIDNESS_DECAY_PER_DAY 0.005f

/** Rehearsal boost factor */
#define REHEARSAL_BOOST_FACTOR 0.1f

/** Milliseconds per day */
#define MS_PER_DAY 86400000ULL

/** Default base encoding strength */
#define DEFAULT_BASE_STRENGTH 0.5f

/** Trauma intrusion decay per day */
#define INTRUSION_DECAY_PER_DAY 0.01f

/** Therapy effectiveness multiplier */
#define THERAPY_EFFECTIVENESS 0.8f

//=============================================================================
// Internal Helper Functions - Forward Declarations
//=============================================================================

static uint64_t get_current_time_ms(void);
static flashbulb_memory_t* find_flashbulb_by_id(
    flashbulb_system_t* system,
    uint64_t flashbulb_id
);
static flashbulb_error_t ensure_capacity(flashbulb_system_t* system);
static flashbulb_error_t ensure_trauma_capacity(flashbulb_system_t* system);
static void init_flashbulb_memory(flashbulb_memory_t* fb);
static float clamp_float(float value, float min_val, float max_val);
static void update_retrieval_stats(flashbulb_memory_t* fb, uint64_t current_time_ms);
static bool should_trigger_reconsolidation(
    const flashbulb_system_t* system,
    const flashbulb_memory_t* fb,
    uint64_t current_time_ms
);
static void close_reconsolidation_window(flashbulb_memory_t* fb);

//=============================================================================
// Configuration Functions
//=============================================================================

flashbulb_config_t flashbulb_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_config_default", 0.0f);


    flashbulb_config_t config = {
        .arousal_threshold = FLASHBULB_DEFAULT_AROUSAL_THRESHOLD,
        .arousal_boost = FLASHBULB_DEFAULT_AROUSAL_BOOST,
        .surprise_boost = FLASHBULB_DEFAULT_SURPRISE_BOOST,
        .significance_weight = 1.0f,
        .trauma_arousal_threshold = 0.9f,
        .min_vividness_threshold = FLASHBULB_MIN_VIVIDNESS,
        .max_flashbulb_memories = FLASHBULB_MAX_MEMORIES,
        .max_trauma_memories = FLASHBULB_MAX_TRAUMA,
        .enable_reconsolidation = true,
        .enable_trauma_handling = true,
        .reconsolidation_window_ms = FLASHBULB_RECONSOLIDATION_WINDOW_MS
    };
    return config;
}

bool flashbulb_config_validate(const flashbulb_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "flashbulb_config_validate: config is NULL");
        return false;
    }

    // Validate thresholds are in valid range
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_config_validate", 0.0f);


    if (config->arousal_threshold < 0.0f || config->arousal_threshold > 1.0f) {
        return false;
    }
    if (config->trauma_arousal_threshold < 0.0f || config->trauma_arousal_threshold > 1.0f) {
        return false;
    }
    if (config->min_vividness_threshold < 0.0f || config->min_vividness_threshold > 1.0f) {
        return false;
    }

    // Validate boost factors are positive
    if (config->arousal_boost < 0.0f) {
        return false;
    }
    if (config->surprise_boost < 0.0f) {
        return false;
    }
    if (config->significance_weight < 0.0f) {
        return false;
    }

    // Validate capacities
    if (config->max_flashbulb_memories == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "flashbulb_config_validate: config->max_flashbulb_memories is zero");
        return false;
    }

    return true;
}

//=============================================================================
// System Lifecycle Functions
//=============================================================================

flashbulb_system_t* flashbulb_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const flashbulb_config_t* config
) {
    // Validate node manager (required)
    if (!node_manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node_manager is NULL");

        return NULL;
    }

    // Use default config if not provided
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_create", 0.0f);


    flashbulb_config_t cfg = config ? *config : flashbulb_config_default();
    if (!flashbulb_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "flashbulb_create: flashbulb_config_validate is NULL");
        return NULL;
    }

    // Allocate system
    flashbulb_system_t* system = (flashbulb_system_t*)nimcp_calloc(1, sizeof(flashbulb_system_t));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;
    }

    // Initialize with config
    system->config = cfg;
    system->entanglement = entanglement;
    system->node_manager = node_manager;

    // Allocate flashbulb memory array
    size_t initial_capacity = (cfg.max_flashbulb_memories < DEFAULT_FLASHBULB_CAPACITY) ?
                              cfg.max_flashbulb_memories : DEFAULT_FLASHBULB_CAPACITY;

    system->memories = (flashbulb_memory_t*)nimcp_calloc(initial_capacity, sizeof(flashbulb_memory_t));
    if (!system->memories) {
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "flashbulb_create: system->memories is NULL");
        return NULL;
    }
    system->capacity = initial_capacity;
    system->num_memories = 0;

    // Allocate trauma tracking array if enabled
    if (cfg.enable_trauma_handling) {
        size_t trauma_capacity = (cfg.max_trauma_memories < DEFAULT_TRAUMA_CAPACITY) ?
                                 cfg.max_trauma_memories : DEFAULT_TRAUMA_CAPACITY;

        system->trauma_memories = (flashbulb_memory_t**)nimcp_calloc(trauma_capacity, sizeof(flashbulb_memory_t*));
        if (!system->trauma_memories) {
            nimcp_free(system->memories);
            nimcp_free(system);
            system = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "flashbulb_create: system->trauma_memories is NULL");
            return NULL;
        }
        system->trauma_capacity = trauma_capacity;
        system->num_trauma = 0;
    }

    // Initialize arousal state
    system->current_arousal = 0.5f;  // Neutral baseline
    system->baseline_arousal = 0.5f;
    system->arousal_decay_rate = AROUSAL_DECAY_RATE_MS;

    // Initialize ID counter
    system->next_flashbulb_id = 1;

    // Mark as initialized
    system->is_initialized = true;

    return system;
}

void flashbulb_destroy(flashbulb_system_t* system) {
    if (!system) {
        return;
    }

    // Note: We don't destroy the underlying PR memory nodes
    // Those are owned by the PR memory system

    // Free trauma tracking array
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_destroy", 0.0f);


    if (system->trauma_memories) {
        nimcp_free(system->trauma_memories);
        system->trauma_memories = NULL;
    }

    // Free flashbulb memory array
    if (system->memories) {
        nimcp_free(system->memories);
        system->memories = NULL;
    }

    // Clear and free system
    memset(system, 0, sizeof(flashbulb_system_t));
    nimcp_free(system);
    system = NULL;
}

flashbulb_error_t flashbulb_reset(flashbulb_system_t* system) {
    if (!system) {
        return FLASHBULB_ERROR_NULL_POINTER;
    }

    // Clear all flashbulb memories (but keep array allocated)
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_reset", 0.0f);


    if (system->memories) {
        memset(system->memories, 0, system->capacity * sizeof(flashbulb_memory_t));
    }
    system->num_memories = 0;

    // Clear trauma tracking
    if (system->trauma_memories) {
        memset(system->trauma_memories, 0, system->trauma_capacity * sizeof(flashbulb_memory_t*));
    }
    system->num_trauma = 0;

    // Reset arousal
    system->current_arousal = system->baseline_arousal;

    // Reset ID counter
    system->next_flashbulb_id = 1;

    // Reset statistics
    system->total_detections = 0;
    system->total_encodings = 0;
    system->total_retrievals = 0;
    system->total_intrusions = 0;

    return FLASHBULB_SUCCESS;
}

//=============================================================================
// Detection and Encoding Functions
//=============================================================================

bool flashbulb_detect(
    flashbulb_system_t* system,
    const flashbulb_event_t* event,
    flashbulb_type_t* detected_type
) {
    if (!system || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "flashbulb_detect: required parameter is NULL (system, event)");
        return false;
    }

    // Check if event has meaningful content
    if (!event->event_data || event->event_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "flashbulb_detect: event->event_data is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_detect", 0.0f);


    const emotional_intensity_t* intensity = &event->intensity;

    // Primary criterion: arousal must exceed threshold
    if (intensity->arousal < system->config.arousal_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "flashbulb_detect: validation failed");
        return false;
    }

    // Secondary criterion: combined emotional significance
    // Must have sufficient arousal * surprise * significance
    float combined = intensity->arousal *
                    (1.0f + intensity->surprise) *
                    (intensity->personal_significance + 0.1f);

    // Detection threshold (derived from config)
    float detection_threshold = system->config.arousal_threshold * 0.8f;
    if (combined < detection_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "flashbulb_detect: validation failed");
        return false;
    }

    // Event qualifies as flashbulb - classify type
    system->total_detections++;

    if (detected_type) {
        *detected_type = flashbulb_classify_type(system, intensity);
    }

    return true;
}

flashbulb_memory_t* flashbulb_encode(
    flashbulb_system_t* system,
    const flashbulb_event_t* event,
    flashbulb_type_t type
) {
    if (!system || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "flashbulb_encode: required parameter is NULL (system, event)");
        return NULL;
    }

    // Ensure capacity
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_encode", 0.0f);


    if (ensure_capacity(system) != FLASHBULB_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "flashbulb_encode: validation failed");
        return NULL;
    }

    // Get current time
    uint64_t current_time = event->timestamp_ms;
    if (current_time == 0) {
        current_time = get_current_time_ms();
    }

    // Get pointer to next available slot
    flashbulb_memory_t* fb = &system->memories[system->num_memories];
    init_flashbulb_memory(fb);

    // Assign ID
    fb->flashbulb_id = system->next_flashbulb_id++;
    fb->type = type;

    // Compute encoding strength with arousal boost
    float encoding_strength = flashbulb_compute_encoding_strength(
        system,
        &event->intensity,
        DEFAULT_BASE_STRENGTH
    );

    // Create PR memory node with enhanced encoding
    pr_node_config_t node_config = pr_node_config_emotional(event->intensity.valence);
    node_config.initial_strength = clamp_float(encoding_strength, 0.0f, 1.0f);

    fb->memory = pr_memory_node_create(
        system->node_manager,
        event->event_data,
        event->event_size,
        &node_config
    );

    if (!fb->memory) {
        // Failed to create PR node
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "flashbulb_encode: fb->memory is NULL");
        return NULL;
    }

    // Compute event signature
    prime_signature_t* sig = prime_sig_from_content(event->event_data, event->event_size);
    if (sig) {
        fb->event_signature = *sig;
        prime_sig_destroy(sig);
    }

    // Store quaternion state
    fb->event_quaternion = event->state;

    // Store emotional intensity
    fb->intensity = event->intensity;

    // Initialize vividness and confidence based on arousal
    fb->vividness = clamp_float(0.5f + event->intensity.arousal * 0.5f, 0.0f, 1.0f);
    fb->confidence = clamp_float(0.5f + event->intensity.arousal * 0.4f, 0.0f, 1.0f);

    // Store consolidation state
    fb->consolidation_strength = encoding_strength;
    fb->encoding_boost = encoding_strength / DEFAULT_BASE_STRENGTH;

    // Timestamps
    fb->event_time_ms = event->timestamp_ms;
    fb->encoding_time_ms = current_time;
    fb->last_retrieval_ms = current_time;

    // Update system arousal
    flashbulb_update_arousal(system, event->intensity.arousal, false);

    // Handle trauma if applicable
    if (type == FLASHBULB_TRAUMATIC && system->config.enable_trauma_handling) {
        fb->requires_trauma_handling = true;
        flashbulb_handle_trauma(system, fb);
    }

    // Increment counters
    system->num_memories++;
    system->total_encodings++;

    return fb;
}

flashbulb_error_t flashbulb_encode_context(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    const flashbulb_context_request_t* request
) {
    if (!system || !flashbulb || !request) {
        return FLASHBULB_ERROR_NULL_POINTER;
    }

    if (!request->context_data || request->context_size == 0) {
        return FLASHBULB_ERROR_INVALID_STATE;
    }

    // Check if context slot is available
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_encode_context", 0.0f);


    if (request->type >= FLASHBULB_CONTEXT_COUNT) {
        return FLASHBULB_ERROR_INVALID_STATE;
    }

    // Get context slot
    flashbulb_context_t* ctx = &flashbulb->contexts[request->type];

    // Set context type
    ctx->type = request->type;

    // Compute signature for context content
    prime_signature_t* sig = prime_sig_from_content(request->context_data, request->context_size);
    if (sig) {
        ctx->signature = *sig;
        prime_sig_destroy(sig);
    }

    // Set vividness and confidence
    ctx->vividness = clamp_float(request->vividness, 0.0f, 1.0f);
    ctx->confidence = clamp_float(request->confidence, 0.0f, 1.0f);
    ctx->is_verified = false;
    ctx->actual_accuracy = 0.0f;

    // Update context count
    if (request->type >= flashbulb->num_contexts) {
        flashbulb->num_contexts = request->type + 1;
    }

    // Create entanglement link if graph is available
    if (system->entanglement && flashbulb->memory) {
        // Create edge linking flashbulb to context
        entangle_edge_t edge = {
            .from_id = flashbulb->memory->node_id,
            .to_id = flashbulb->flashbulb_id + FLASHBULB_MAX_MEMORIES * (request->type + 1),
            .resonance_score = ctx->vividness,
            .type = ENTANGLE_EDGE_CONTEXTUAL,
            .weight = ctx->vividness,
            .bidirectional = true
        };
        entangle_add_edge(system->entanglement, &edge);
    }

    return FLASHBULB_SUCCESS;
}

//=============================================================================
// Arousal State Functions
//=============================================================================

float flashbulb_update_arousal(
    flashbulb_system_t* system,
    float arousal,
    bool apply_decay
) {
    if (!system) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_update_arousal", 0.0f);


    float previous = system->current_arousal;

    if (apply_decay) {
        // Blend new arousal with decayed previous
        float decayed = previous * (1.0f - system->arousal_decay_rate);
        system->current_arousal = clamp_float(
            (decayed + arousal) / 2.0f,
            0.0f, 1.0f
        );
    } else {
        // Direct update
        system->current_arousal = clamp_float(arousal, 0.0f, 1.0f);
    }

    return previous;
}

float flashbulb_compute_encoding_strength(
    const flashbulb_system_t* system,
    const emotional_intensity_t* intensity,
    float base_strength
) {
    if (!system || !intensity) {
        return base_strength;
    }

    // Encoding strength formula:
    // strength = base * (1 + arousal_boost * arousal)
    //          * (1 + surprise_boost * surprise)
    //          * max(significance, 0.1)

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_compute_encoding_str", 0.0f);


    float arousal_factor = 1.0f + system->config.arousal_boost * intensity->arousal;
    float surprise_factor = 1.0f + system->config.surprise_boost * intensity->surprise;
    float significance_factor = fmaxf(intensity->personal_significance, 0.1f);

    float strength = base_strength * arousal_factor * surprise_factor * significance_factor;

    // Apply significance weight
    strength *= system->config.significance_weight;

    return strength;
}

float flashbulb_decay_arousal(
    flashbulb_system_t* system,
    uint64_t elapsed_ms
) {
    if (!system) {
        return 0.0f;
    }

    // Exponential decay toward baseline
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_decay_arousal", 0.0f);


    float decay_amount = system->arousal_decay_rate * (float)elapsed_ms;
    float diff = system->current_arousal - system->baseline_arousal;

    system->current_arousal = system->baseline_arousal + diff * expf(-decay_amount);

    return system->current_arousal;
}

//=============================================================================
// Retrieval Functions
//=============================================================================

flashbulb_memory_t* flashbulb_retrieve(
    flashbulb_system_t* system,
    uint64_t flashbulb_id,
    flashbulb_retrieval_result_t* result
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_retrieve", 0.0f);


    flashbulb_memory_t* fb = find_flashbulb_by_id(system, flashbulb_id);
    if (!fb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fb is NULL");

        return NULL;
    }

    uint64_t current_time = get_current_time_ms();

    // Update retrieval statistics
    update_retrieval_stats(fb, current_time);

    // Check if this triggers reconsolidation
    bool triggered_reconsolidation = false;
    if (system->config.enable_reconsolidation) {
        triggered_reconsolidation = should_trigger_reconsolidation(system, fb, current_time);
        if (triggered_reconsolidation) {
            fb->is_reconsolidating = true;
            fb->reconsolidation_start_ms = current_time;
        }
    }

    // Increment rehearsal count
    fb->intensity.rehearsal_count += 1.0f;

    // Update system statistics
    system->total_retrievals++;

    // Fill result if requested
    if (result) {
        result->memory = fb;
        result->retrieval_vividness = fb->vividness;
        result->resonance_score = 1.0f;  // Perfect match by ID
        result->triggered_reconsolidation = triggered_reconsolidation;
    }

    return fb;
}

flashbulb_memory_t* flashbulb_retrieve_by_content(
    flashbulb_system_t* system,
    const prime_signature_t* query,
    float min_resonance,
    flashbulb_retrieval_result_t* result
) {
    if (!system || !query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "flashbulb_retrieve_by_content: required parameter is NULL (system, query)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_retrieve_by_content", 0.0f);


    flashbulb_memory_t* best_match = NULL;
    float best_score = 0.0f;

    // Search all flashbulb memories
    for (size_t i = 0; i < system->num_memories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_memories > 256) {
            flashbulb_heartbeat("flashbulb_loop",
                             (float)(i + 1) / (float)system->num_memories);
        }

        flashbulb_memory_t* fb = &system->memories[i];

        // Compute resonance using Jaccard similarity
        float score = prime_sig_jaccard(&fb->event_signature, query);

        if (score > best_score && score >= min_resonance) {
            best_score = score;
            best_match = fb;
        }
    }

    if (best_match) {
        uint64_t current_time = get_current_time_ms();
        update_retrieval_stats(best_match, current_time);
        best_match->intensity.rehearsal_count += 1.0f;
        system->total_retrievals++;

        // Check reconsolidation
        bool triggered = false;
        if (system->config.enable_reconsolidation) {
            triggered = should_trigger_reconsolidation(system, best_match, current_time);
            if (triggered) {
                best_match->is_reconsolidating = true;
                best_match->reconsolidation_start_ms = current_time;
            }
        }

        if (result) {
            result->memory = best_match;
            result->retrieval_vividness = best_match->vividness;
            result->resonance_score = best_score;
            result->triggered_reconsolidation = triggered;
        }
    }

    return best_match;
}

flashbulb_error_t flashbulb_retrieve_by_type(
    flashbulb_system_t* system,
    flashbulb_type_t type,
    flashbulb_memory_t** memories,
    size_t max_memories,
    size_t* count
) {
    if (!system || !memories || !count) {
        return FLASHBULB_ERROR_NULL_POINTER;
    }

    *count = 0;

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_retrieve_by_type", 0.0f);


    for (size_t i = 0; i < system->num_memories && *count < max_memories; i++) {
        if (system->memories[i].type == type) {
            memories[*count] = &system->memories[i];
            (*count)++;
        }
    }

    return FLASHBULB_SUCCESS;
}

flashbulb_error_t flashbulb_retrieve_most_vivid(
    flashbulb_system_t* system,
    size_t k,
    flashbulb_memory_t** memories,
    size_t* count
) {
    if (!system || !memories || !count) {
        return FLASHBULB_ERROR_NULL_POINTER;
    }

    *count = 0;

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_retrieve_most_vivid", 0.0f);


    if (k == 0 || system->num_memories == 0) {
        return FLASHBULB_SUCCESS;
    }

    /* Heap-based top-K selection: O(n log k) instead of O(nk) selection sort
     * Use min-heap of size k: smallest element is root
     * For each memory, if vividness > heap root, replace and re-heapify
     */
    size_t actual_k = (k < system->num_memories) ? k : system->num_memories;

    /* Heap element: store vividness (key) and index (value) */
    typedef struct {
        float vividness;
        size_t idx;
    } heap_elem_t;

    /* Allocate min-heap (1-indexed for simpler parent/child math) */
    heap_elem_t* heap = (heap_elem_t*)nimcp_calloc(actual_k + 1, sizeof(heap_elem_t));
    if (!heap) {
        return FLASHBULB_ERROR_NO_MEMORY;
    }

    size_t heap_size = 0;

    /* Helper: sift down for min-heap */
    #define HEAP_SIFT_DOWN(idx) do { \
        size_t _i = (idx); \
        while (2 * _i <= heap_size) { \
            size_t _child = 2 * _i; \
            if (_child + 1 <= heap_size && heap[_child + 1].vividness < heap[_child].vividness) { \
                _child++; \
            } \
            if (heap[_child].vividness < heap[_i].vividness) { \
                heap_elem_t _tmp = heap[_child]; heap[_child] = heap[_i]; heap[_i] = _tmp; \
                _i = _child; \
            } else break; \
        } \
    } while(0)

    /* Helper: sift up for min-heap */
    #define HEAP_SIFT_UP(idx) do { \
        size_t _i = (idx); \
        while (_i > 1 && heap[_i].vividness < heap[_i / 2].vividness) { \
            heap_elem_t _tmp = heap[_i]; heap[_i] = heap[_i / 2]; heap[_i / 2] = _tmp; \
            _i /= 2; \
        } \
    } while(0)

    /* Process all memories */
    for (size_t j = 0; j < system->num_memories; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && system->num_memories > 256) {
            flashbulb_heartbeat("flashbulb_loop",
                             (float)(j + 1) / (float)system->num_memories);
        }

        float vividness = system->memories[j].vividness;

        if (heap_size < actual_k) {
            /* Heap not full yet - insert */
            heap_size++;
            heap[heap_size].vividness = vividness;
            heap[heap_size].idx = j;
            HEAP_SIFT_UP(heap_size);
        } else if (vividness > heap[1].vividness) {
            /* This memory is more vivid than the least vivid in top-k - replace */
            heap[1].vividness = vividness;
            heap[1].idx = j;
            HEAP_SIFT_DOWN(1);
        }
    }

    #undef HEAP_SIFT_DOWN
    #undef HEAP_SIFT_UP

    /* Extract results from heap (will be in arbitrary order) */
    /* Sort by vividness descending for consistent output */
    for (size_t i = 0; i < heap_size; i++) {
        memories[i] = &system->memories[heap[i + 1].idx];
    }

    /* Simple insertion sort for final ordering (heap_size <= k, typically small) */
    for (size_t i = 1; i < heap_size; i++) {
        flashbulb_memory_t* key = memories[i];
        int64_t j = (int64_t)i - 1;
        while (j >= 0 && memories[j]->vividness < key->vividness) {
            memories[j + 1] = memories[j];
            j--;
        }
        memories[j + 1] = key;
    }

    nimcp_free(heap);
    heap = NULL;
    *count = heap_size;

    return FLASHBULB_SUCCESS;
}

//=============================================================================
// Vividness and Accuracy Functions
//=============================================================================

float flashbulb_assess_vividness(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    uint64_t current_time_ms
) {
    if (!system || !flashbulb) {
        return 0.0f;
    }

    // Calculate time since encoding in days
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_assess_vividness", 0.0f);


    uint64_t age_ms = (current_time_ms > flashbulb->encoding_time_ms) ?
                      (current_time_ms - flashbulb->encoding_time_ms) : 0;
    float age_days = (float)age_ms / (float)MS_PER_DAY;

    // Vividness formula:
    // vividness = base_vividness
    //           * (1 - decay_factor * time_since_encoding)
    //           * (1 + rehearsal_boost * sqrt(retrieval_count))
    //           * consolidation_strength

    float base_vividness = 0.5f + flashbulb->intensity.arousal * 0.5f;

    // Time decay (slow for flashbulb memories)
    float time_decay = expf(-VIVIDNESS_DECAY_PER_DAY * age_days);

    // Rehearsal boost (thinking about it strengthens vividness)
    float rehearsal_boost = 1.0f + REHEARSAL_BOOST_FACTOR *
                           sqrtf(flashbulb->intensity.rehearsal_count);

    // Consolidation factor
    float consolidation_factor = flashbulb->consolidation_strength;

    float vividness = base_vividness * time_decay * rehearsal_boost * consolidation_factor;

    // Clamp and update
    flashbulb->vividness = clamp_float(vividness, 0.0f, 1.0f);

    return flashbulb->vividness;
}

float flashbulb_verify_accuracy(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    const prime_signature_t* ground_truth_sig
) {
    if (!system || !flashbulb || !ground_truth_sig) {
        return 0.0f;
    }

    // Compute similarity between memory and ground truth
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_verify_accuracy", 0.0f);


    float accuracy = prime_sig_jaccard(&flashbulb->event_signature, ground_truth_sig);

    // Update flashbulb with verified accuracy
    flashbulb->accuracy_verified = true;
    flashbulb->actual_accuracy = accuracy;

    return accuracy;
}

float flashbulb_confidence_accuracy_gap(const flashbulb_memory_t* flashbulb) {
    if (!flashbulb || !flashbulb->accuracy_verified) {
        return 0.0f;
    }

    // Positive = overconfident, negative = underconfident
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_confidence_accuracy_", 0.0f);


    return flashbulb->confidence - flashbulb->actual_accuracy;
}

//=============================================================================
// Reconsolidation Functions
//=============================================================================

flashbulb_error_t flashbulb_reconsolidate(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    const void* new_information,
    size_t new_info_size,
    float blend_factor
) {
    if (!system || !flashbulb) {
        return FLASHBULB_ERROR_NULL_POINTER;
    }

    // Must be in reconsolidation window
    if (!flashbulb->is_reconsolidating) {
        return FLASHBULB_ERROR_INVALID_STATE;
    }

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_reconsolidate", 0.0f);


    blend_factor = clamp_float(blend_factor, 0.0f, 1.0f);

    // If new information provided, update memory content
    if (new_information && new_info_size > 0 && flashbulb->memory) {
        // Compute new signature from blended content
        prime_signature_t* new_sig = prime_sig_from_content(new_information, new_info_size);
        if (new_sig) {
            // Blend signatures
            // Simple approach: interpolate exponents
            for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
                    flashbulb_heartbeat("flashbulb_loop",
                                     (float)(i + 1) / (float)PRIME_SIG_DIM);
                }

                uint8_t old_exp = flashbulb->event_signature.exponents[i];
                uint8_t new_exp = new_sig->exponents[i];
                float blended = (1.0f - blend_factor) * old_exp + blend_factor * new_exp;
                flashbulb->event_signature.exponents[i] = (uint8_t)clamp_float(blended, 0.0f, 255.0f);
            }

            // Recompute hash
            flashbulb->event_signature.hash = prime_sig_hash(&flashbulb->event_signature);
            prime_sig_recount_factors(&flashbulb->event_signature);

            prime_sig_destroy(new_sig);
        }
    }

    // Reconsolidation may reduce confidence (memory becomes more malleable)
    flashbulb->confidence *= (1.0f - blend_factor * 0.2f);

    // Close reconsolidation window
    close_reconsolidation_window(flashbulb);

    return FLASHBULB_SUCCESS;
}

bool flashbulb_is_reconsolidating(
    const flashbulb_system_t* system,
    const flashbulb_memory_t* flashbulb,
    uint64_t current_time_ms
) {
    if (!system || !flashbulb) {
        return false;
    }

    if (!flashbulb->is_reconsolidating) {
        return false;
    }

    // Check if still within reconsolidation window
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_is_reconsolidating", 0.0f);


    uint64_t elapsed = current_time_ms - flashbulb->reconsolidation_start_ms;
    return elapsed < system->config.reconsolidation_window_ms;
}

flashbulb_error_t flashbulb_trigger_reconsolidation(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    uint64_t current_time_ms
) {
    if (!system || !flashbulb) {
        return FLASHBULB_ERROR_NULL_POINTER;
    }

    if (!system->config.enable_reconsolidation) {
        return FLASHBULB_ERROR_INVALID_STATE;
    }

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_trigger_reconsolidat", 0.0f);


    flashbulb->is_reconsolidating = true;
    flashbulb->reconsolidation_start_ms = current_time_ms;

    return FLASHBULB_SUCCESS;
}

//=============================================================================
// Trauma Handling Functions
//=============================================================================

flashbulb_error_t flashbulb_handle_trauma(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb
) {
    if (!system || !flashbulb) {
        return FLASHBULB_ERROR_NULL_POINTER;
    }

    if (!system->config.enable_trauma_handling) {
        return FLASHBULB_ERROR_INVALID_CONFIG;
    }

    // Mark as requiring trauma handling
    flashbulb->requires_trauma_handling = true;

    // Initialize trauma markers based on emotional intensity
    flashbulb->intrusion_frequency = clamp_float(
        flashbulb->intensity.arousal * 0.8f,
        0.0f, 1.0f
    );

    flashbulb->avoidance_level = clamp_float(
        fabsf(flashbulb->intensity.valence) * 0.6f,
        0.0f, 1.0f
    );

    // Fragmentation increases with extreme arousal
    flashbulb->fragmentation = clamp_float(
        (flashbulb->intensity.arousal > 0.9f) ?
        (flashbulb->intensity.arousal - 0.9f) * 5.0f : 0.0f,
        0.0f, 1.0f
    );

    // Hyperarousal mirrors initial arousal
    flashbulb->hyperarousal = flashbulb->intensity.arousal;

    // Add to trauma tracking array
    if (ensure_trauma_capacity(system) == FLASHBULB_SUCCESS) {
        system->trauma_memories[system->num_trauma++] = flashbulb;
    }

    return FLASHBULB_SUCCESS;
}

float flashbulb_process_intrusion(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    float intrusion_strength,
    uint64_t current_time_ms
) {
    if (!system || !flashbulb) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_process_intrusion", 0.0f);


    (void)current_time_ms;  // Could be used for temporal tracking

    // Update intrusion frequency
    // Each intrusion slightly increases frequency (sensitization)
    float increase = intrusion_strength * 0.05f;
    flashbulb->intrusion_frequency = clamp_float(
        flashbulb->intrusion_frequency + increase,
        0.0f, 1.0f
    );

    // Update statistics
    system->total_intrusions++;

    // Intrusion also increases arousal
    system->current_arousal = clamp_float(
        system->current_arousal + intrusion_strength * 0.2f,
        0.0f, 1.0f
    );

    return flashbulb->intrusion_frequency;
}

flashbulb_error_t flashbulb_apply_therapy(
    flashbulb_system_t* system,
    flashbulb_memory_t* flashbulb,
    const flashbulb_therapy_params_t* params
) {
    if (!system || !flashbulb || !params) {
        return FLASHBULB_ERROR_NULL_POINTER;
    }

    if (!flashbulb->requires_trauma_handling) {
        return FLASHBULB_ERROR_INVALID_STATE;
    }

    // Apply emotional dampening
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_apply_therapy", 0.0f);


    if (params->emotional_dampening > 0.0f) {
        float dampening = params->emotional_dampening * THERAPY_EFFECTIVENESS;

        // Reduce arousal component
        flashbulb->intensity.arousal *= (1.0f - dampening);

        // Move valence toward neutral
        flashbulb->intensity.valence *= (1.0f - dampening * 0.5f);

        // Reduce hyperarousal
        flashbulb->hyperarousal *= (1.0f - dampening);
    }

    // Apply intrusion reduction
    if (params->intrusion_reduction > 0.0f) {
        float reduction = params->intrusion_reduction * THERAPY_EFFECTIVENESS;
        flashbulb->intrusion_frequency *= (1.0f - reduction);
    }

    // Apply avoidance reduction
    if (params->avoidance_reduction > 0.0f) {
        float reduction = params->avoidance_reduction * THERAPY_EFFECTIVENESS;
        flashbulb->avoidance_level *= (1.0f - reduction);
    }

    // Trigger reconsolidation if requested
    if (params->trigger_reconsolidation && system->config.enable_reconsolidation) {
        flashbulb->is_reconsolidating = true;
        flashbulb->reconsolidation_start_ms = get_current_time_ms();
    }

    // Increment therapy session count
    flashbulb->therapy_sessions++;

    return FLASHBULB_SUCCESS;
}

flashbulb_error_t flashbulb_get_high_intrusion_memories(
    flashbulb_system_t* system,
    float min_intrusion_freq,
    flashbulb_memory_t** memories,
    size_t max_memories,
    size_t* count
) {
    if (!system || !memories || !count) {
        return FLASHBULB_ERROR_NULL_POINTER;
    }

    *count = 0;

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_get_high_intrusion_m", 0.0f);


    for (size_t i = 0; i < system->num_trauma && *count < max_memories; i++) {
        flashbulb_memory_t* fb = system->trauma_memories[i];
        if (fb && fb->intrusion_frequency >= min_intrusion_freq) {
            memories[*count] = fb;
            (*count)++;
        }
    }

    return FLASHBULB_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* flashbulb_type_name(flashbulb_type_t type) {
    static const char* names[] = {
        "POSITIVE",
        "NEGATIVE",
        "SURPRISING",
        "TRAUMATIC"
    };

    if (type < FLASHBULB_TYPE_COUNT) {
        return names[type];
    }
    return "UNKNOWN";
}

const char* flashbulb_context_type_name(flashbulb_context_type_t type) {
    static const char* names[] = {
        "LOCATION",
        "INFORMANT",
        "ACTIVITY",
        "OTHERS",
        "AFTERMATH",
        "EMOTIONAL",
        "SENSORY",
        "TEMPORAL"
    };

    if (type < FLASHBULB_CONTEXT_COUNT) {
        return names[type];
    }
    return "UNKNOWN";
}

const char* flashbulb_error_string(flashbulb_error_t error) {
    switch (error) {
        case FLASHBULB_SUCCESS:
            return "Success";
        case FLASHBULB_ERROR_NULL_POINTER:
            return "Null pointer argument";
        case FLASHBULB_ERROR_INVALID_CONFIG:
            return "Invalid configuration";
        case FLASHBULB_ERROR_NO_MEMORY:
            return "Memory allocation failed";
        case FLASHBULB_ERROR_NOT_FOUND:
            return "Flashbulb memory not found";
        case FLASHBULB_ERROR_CAPACITY:
            return "Maximum capacity reached";
        case FLASHBULB_ERROR_INVALID_STATE:
            return "Invalid state for operation";
        case FLASHBULB_ERROR_BELOW_THRESHOLD:
            return "Event below arousal threshold";
        case FLASHBULB_ERROR_RECONSOLIDATING:
            return "Memory is currently reconsolidating";
        default:
            return "Unknown error";
    }
}

flashbulb_error_t flashbulb_get_stats(
    const flashbulb_system_t* system,
    flashbulb_stats_t* stats
) {
    if (!system || !stats) {
        return FLASHBULB_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_get_stats", 0.0f);


    memset(stats, 0, sizeof(flashbulb_stats_t));

    stats->total_flashbulbs = system->num_memories;

    // Count by type and compute averages
    float vividness_sum = 0.0f;
    float confidence_sum = 0.0f;
    float accuracy_sum = 0.0f;
    float intrusion_sum = 0.0f;

    for (size_t i = 0; i < system->num_memories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_memories > 256) {
            flashbulb_heartbeat("flashbulb_loop",
                             (float)(i + 1) / (float)system->num_memories);
        }

        const flashbulb_memory_t* fb = &system->memories[i];

        // Count by type
        switch (fb->type) {
            case FLASHBULB_POSITIVE:
                stats->positive_count++;
                break;
            case FLASHBULB_NEGATIVE:
                stats->negative_count++;
                break;
            case FLASHBULB_SURPRISING:
                stats->surprising_count++;
                break;
            case FLASHBULB_TRAUMATIC:
                stats->traumatic_count++;
                break;
            default:
                break;
        }

        // Accumulate for averages
        vividness_sum += fb->vividness;
        confidence_sum += fb->confidence;

        if (fb->accuracy_verified) {
            accuracy_sum += fb->actual_accuracy;
            stats->verified_count++;
        }

        if (fb->requires_trauma_handling) {
            intrusion_sum += fb->intrusion_frequency;
        }
    }

    // Compute averages
    if (system->num_memories > 0) {
        stats->mean_vividness = vividness_sum / (float)system->num_memories;
        stats->mean_confidence = confidence_sum / (float)system->num_memories;
    }

    if (stats->verified_count > 0) {
        stats->mean_accuracy = accuracy_sum / (float)stats->verified_count;
    }

    if (stats->traumatic_count > 0) {
        stats->mean_intrusion_freq = intrusion_sum / (float)stats->traumatic_count;
    }

    stats->total_therapy_sessions = 0;
    for (size_t i = 0; i < system->num_trauma; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_trauma > 256) {
            flashbulb_heartbeat("flashbulb_loop",
                             (float)(i + 1) / (float)system->num_trauma);
        }

        if (system->trauma_memories[i]) {
            stats->total_therapy_sessions += system->trauma_memories[i]->therapy_sessions;
        }
    }

    return FLASHBULB_SUCCESS;
}

void flashbulb_print(const flashbulb_memory_t* flashbulb) {
    if (!flashbulb) {
        printf("Flashbulb: (null)\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_print", 0.0f);


    printf("Flashbulb Memory [ID: %lu]\n", (unsigned long)flashbulb->flashbulb_id);
    printf("  Type: %s\n", flashbulb_type_name(flashbulb->type));
    printf("  Emotional Intensity:\n");
    printf("    Arousal: %.3f\n", flashbulb->intensity.arousal);
    printf("    Valence: %.3f\n", flashbulb->intensity.valence);
    printf("    Surprise: %.3f\n", flashbulb->intensity.surprise);
    printf("    Significance: %.3f\n", flashbulb->intensity.personal_significance);
    printf("  Vividness: %.3f\n", flashbulb->vividness);
    printf("  Confidence: %.3f\n", flashbulb->confidence);

    if (flashbulb->accuracy_verified) {
        printf("  Actual Accuracy: %.3f (gap: %+.3f)\n",
               flashbulb->actual_accuracy,
               flashbulb_confidence_accuracy_gap(flashbulb));
    }

    printf("  Consolidation: %.3f (boost: %.2fx)\n",
           flashbulb->consolidation_strength,
           flashbulb->encoding_boost);

    printf("  Reconsolidating: %s\n", flashbulb->is_reconsolidating ? "yes" : "no");
    printf("  Retrieval Count: %u\n", flashbulb->retrieval_count);
    printf("  Contexts: %u/%d\n", flashbulb->num_contexts, FLASHBULB_CONTEXT_COUNT);

    if (flashbulb->requires_trauma_handling) {
        printf("  Trauma Markers:\n");
        printf("    Intrusion Freq: %.3f\n", flashbulb->intrusion_frequency);
        printf("    Avoidance Level: %.3f\n", flashbulb->avoidance_level);
        printf("    Fragmentation: %.3f\n", flashbulb->fragmentation);
        printf("    Hyperarousal: %.3f\n", flashbulb->hyperarousal);
        printf("    Therapy Sessions: %u\n", flashbulb->therapy_sessions);
    }
}

void flashbulb_system_print_summary(const flashbulb_system_t* system) {
    if (!system) {
        printf("Flashbulb System: (null)\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_system_print_summary", 0.0f);


    printf("Flashbulb Memory System Summary\n");
    printf("================================\n");
    printf("  Total Memories: %zu / %zu\n", system->num_memories, system->capacity);
    printf("  Trauma Memories: %zu / %zu\n", system->num_trauma, system->trauma_capacity);
    printf("  Current Arousal: %.3f (baseline: %.3f)\n",
           system->current_arousal, system->baseline_arousal);
    printf("  Config:\n");
    printf("    Arousal Threshold: %.3f\n", system->config.arousal_threshold);
    printf("    Arousal Boost: %.3f\n", system->config.arousal_boost);
    printf("    Surprise Boost: %.3f\n", system->config.surprise_boost);
    printf("  Statistics:\n");
    printf("    Total Detections: %lu\n", (unsigned long)system->total_detections);
    printf("    Total Encodings: %lu\n", (unsigned long)system->total_encodings);
    printf("    Total Retrievals: %lu\n", (unsigned long)system->total_retrievals);
    printf("    Total Intrusions: %lu\n", (unsigned long)system->total_intrusions);
}

uint64_t flashbulb_current_time_ms(void) {
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_current_time_ms", 0.0f);


    return get_current_time_ms();
}

void flashbulb_intensity_init(emotional_intensity_t* intensity) {
    if (!intensity) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_intensity_init", 0.0f);


    intensity->arousal = 0.5f;
    intensity->valence = 0.0f;
    intensity->surprise = 0.0f;
    intensity->personal_significance = 0.5f;
    intensity->rehearsal_count = 0.0f;
}

emotional_intensity_t flashbulb_intensity_create(
    float arousal,
    float valence,
    float surprise,
    float significance
) {
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_intensity_create", 0.0f);


    emotional_intensity_t intensity = {
        .arousal = clamp_float(arousal, 0.0f, 1.0f),
        .valence = clamp_float(valence, -1.0f, 1.0f),
        .surprise = clamp_float(surprise, 0.0f, 1.0f),
        .personal_significance = clamp_float(significance, 0.0f, 1.0f),
        .rehearsal_count = 0.0f
    };
    return intensity;
}

flashbulb_type_t flashbulb_classify_type(
    const flashbulb_system_t* system,
    const emotional_intensity_t* intensity
) {
    if (!system || !intensity) {
        return FLASHBULB_SURPRISING;  // Default
    }

    // Check for trauma first (highest arousal + negative valence)
    /* Phase 8: Heartbeat at operation start */
    flashbulb_heartbeat("flashbulb_classify_type", 0.0f);


    if (intensity->arousal >= system->config.trauma_arousal_threshold &&
        intensity->valence < -0.5f) {
        return FLASHBULB_TRAUMATIC;
    }

    // Classify by valence
    if (intensity->valence > 0.3f) {
        return FLASHBULB_POSITIVE;
    } else if (intensity->valence < -0.3f) {
        return FLASHBULB_NEGATIVE;
    }

    // Neutral valence but high surprise
    if (intensity->surprise > 0.7f) {
        return FLASHBULB_SURPRISING;
    }

    // Default based on valence direction
    return (intensity->valence >= 0.0f) ? FLASHBULB_POSITIVE : FLASHBULB_NEGATIVE;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }
    // Fallback
    return (uint64_t)time(NULL) * 1000ULL;
}

/**
 * @brief Find flashbulb memory by ID
 */
static flashbulb_memory_t* find_flashbulb_by_id(
    flashbulb_system_t* system,
    uint64_t flashbulb_id
) {
    for (size_t i = 0; i < system->num_memories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_memories > 256) {
            flashbulb_heartbeat("flashbulb_loop",
                             (float)(i + 1) / (float)system->num_memories);
        }

        if (system->memories[i].flashbulb_id == flashbulb_id) {
            return &system->memories[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_flashbulb_by_id: validation failed");
    return NULL;
}

/**
 * @brief Ensure capacity for new flashbulb memory
 */
static flashbulb_error_t ensure_capacity(flashbulb_system_t* system) {
    if (system->num_memories >= system->config.max_flashbulb_memories) {
        return FLASHBULB_ERROR_CAPACITY;
    }

    if (system->num_memories >= system->capacity) {
        // Double capacity
        size_t new_capacity = system->capacity * 2;
        if (new_capacity > system->config.max_flashbulb_memories) {
            new_capacity = system->config.max_flashbulb_memories;
        }

        flashbulb_memory_t* new_memories = (flashbulb_memory_t*)nimcp_realloc(
            system->memories,
            new_capacity * sizeof(flashbulb_memory_t)
        );

        if (!new_memories) {
            return FLASHBULB_ERROR_NO_MEMORY;
        }

        // Zero new memory
        memset(&new_memories[system->capacity], 0,
               (new_capacity - system->capacity) * sizeof(flashbulb_memory_t));

        system->memories = new_memories;
        system->capacity = new_capacity;
    }

    return FLASHBULB_SUCCESS;
}

/**
 * @brief Ensure capacity for trauma tracking
 */
static flashbulb_error_t ensure_trauma_capacity(flashbulb_system_t* system) {
    if (!system->trauma_memories) {
        return FLASHBULB_ERROR_INVALID_CONFIG;
    }

    if (system->num_trauma >= system->config.max_trauma_memories) {
        return FLASHBULB_ERROR_CAPACITY;
    }

    if (system->num_trauma >= system->trauma_capacity) {
        // Double capacity
        size_t new_capacity = system->trauma_capacity * 2;
        if (new_capacity > system->config.max_trauma_memories) {
            new_capacity = system->config.max_trauma_memories;
        }

        flashbulb_memory_t** new_trauma = (flashbulb_memory_t**)nimcp_realloc(
            system->trauma_memories,
            new_capacity * sizeof(flashbulb_memory_t*)
        );

        if (!new_trauma) {
            return FLASHBULB_ERROR_NO_MEMORY;
        }

        // Zero new slots
        memset(&new_trauma[system->trauma_capacity], 0,
               (new_capacity - system->trauma_capacity) * sizeof(flashbulb_memory_t*));

        system->trauma_memories = new_trauma;
        system->trauma_capacity = new_capacity;
    }

    return FLASHBULB_SUCCESS;
}

/**
 * @brief Initialize flashbulb memory structure
 */
static void init_flashbulb_memory(flashbulb_memory_t* fb) {
    memset(fb, 0, sizeof(flashbulb_memory_t));

    // Set defaults
    fb->vividness = 0.5f;
    fb->confidence = 0.5f;
    fb->consolidation_strength = DEFAULT_BASE_STRENGTH;
    fb->encoding_boost = 1.0f;

    // Initialize emotional intensity
    flashbulb_intensity_init(&fb->intensity);
}

/**
 * @brief Clamp float to range
 */
static float clamp_float(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/**
 * @brief Update retrieval statistics for flashbulb memory
 */
static void update_retrieval_stats(flashbulb_memory_t* fb, uint64_t current_time_ms) {
    fb->retrieval_count++;
    fb->last_retrieval_ms = current_time_ms;
}

/**
 * @brief Determine if retrieval should trigger reconsolidation
 */
static bool should_trigger_reconsolidation(
    const flashbulb_system_t* system,
    const flashbulb_memory_t* fb,
    uint64_t current_time_ms
) {
    (void)system;

    // Already reconsolidating
    if (fb->is_reconsolidating) {
        return false;
    }

    // Need some time since last retrieval for reconsolidation
    // (simplified model - in reality depends on many factors)
    uint64_t time_since_last = current_time_ms - fb->last_retrieval_ms;

    // More than 1 hour since last retrieval
    return time_since_last > 3600000ULL;
}

/**
 * @brief Close reconsolidation window
 */
static void close_reconsolidation_window(flashbulb_memory_t* fb) {
    fb->is_reconsolidating = false;
    fb->reconsolidation_start_ms = 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void flashbulb_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_flashbulb_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int flashbulb_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "flashbulb_training_begin: NULL argument");
        return -1;
    }
    flashbulb_heartbeat_instance(NULL, "flashbulb_training_begin", 0.0f);
    return 0;
}

int flashbulb_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "flashbulb_training_end: NULL argument");
        return -1;
    }
    flashbulb_heartbeat_instance(NULL, "flashbulb_training_end", 1.0f);
    return 0;
}

int flashbulb_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "flashbulb_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    flashbulb_heartbeat_instance(NULL, "flashbulb_training_step", progress);
    return 0;
}
