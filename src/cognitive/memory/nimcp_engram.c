/**
 * @file nimcp_engram.c
 * @brief Implementation of memory engram system
 *
 * WHAT: Memory traces stored as distributed synaptic patterns
 * WHY:  Enable realistic memory encoding, consolidation, recall
 * HOW:  Track engram cells, manage consolidation, pattern completion
 *
 * NIMCP STANDARDS:
 * - Guard clauses (no nested ifs)
 * - Functions < 50 lines
 * - WHAT-WHY-HOW documentation
 * - Single Responsibility Principle
 * - 100% test coverage
 *
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x0330 (BIO_MODULE_MEMORY)
 * - Publishes: memory encoding, recall, consolidation events
 * - Subscribes: None (passive memory store)
 *
 * @version Phase M1: Memory Engrams - Core Implementation with Bio-Async
 * @date 2025-11-28
 */

#define LOG_MODULE "engram"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(engram, MESH_ADAPTER_CATEGORY_MEMORY)



#include "cognitive/memory/nimcp_engram.h"
#include "cognitive/memory/nimcp_memory_kg_events.h"  /* W6: KG event emitters */
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "nimcp.h"  // For NIMCP_ERROR_* codes
#include "utils/exception/nimcp_exception_macros.h"  // Phase 7: Exception integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free

//=============================================================================
// BIO-ASYNC MODULE REGISTRATION
//=============================================================================

#define BIO_MODULE_MEMORY 0x0330

// Event types
#define ENGRAM_EVENT_ENCODED "engram.encoded"
#define ENGRAM_EVENT_RECALLED "engram.recalled"
#define ENGRAM_EVENT_CONSOLIDATED "engram.consolidated"
#define ENGRAM_EVENT_DECAYED "engram.decayed"

//=============================================================================
// BIO-ASYNC HANDLERS (Forward declarations)
//=============================================================================

static nimcp_error_t handle_memory_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static void bio_broadcast_engram_encoded(engram_system_t* system, uint32_t engram_id, float strength);
static void bio_broadcast_engram_recalled(engram_system_t* system, uint32_t engram_id, float confidence);

/*=============================================================================
 * KG-Driven Wiring Callback
 *============================================================================*/

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int engram_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO(LOG_MODULE, "engram_wiring_handler_callback: registering %u handlers from KG",
             message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            engram_heartbeat("engram_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_WORKING_MEMORY_RETRIEVE:
                bio_router_register_handler(ctx, message_types[i], handle_memory_query);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_WORKING_MEMORY_RETRIEVE");
                break;

            default:
                LOG_DEBUG(LOG_MODULE, "  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

//=============================================================================
// CONSTANTS
//=============================================================================

/* Initial capacity bumped from 512 -> 32K so typical training workloads
 * skip 5-6 expand_engram_array growth events. ~67 MB up-front allocation
 * which fits any normal brain config (we run at 15-16 GB). expand still
 * doubles past this if active_count survives below the soft cap. */
#define ENGRAM_INITIAL_CAPACITY 32768u
#define ENGRAM_GROWTH_FACTOR 2.0f

//=============================================================================
// RECALL ACCELERATION: BLOOM FILTER + INVERTED INDEX
//=============================================================================
//
// Replaces the original O(capacity * cue_count * engram_neurons) linear
// scan in engram_recall with a two-stage candidate-narrowing pipeline.
//
// Stage 1 (inverted index): for each cue neuron, look up the posting
//   list of engrams that contain it. Aggregate hit counts per candidate
//   in a small per-call working buffer. Avoids touching engrams that
//   share no neurons with the cue at all.
//
// Stage 2 (Bloom skip-test): for each surviving candidate, do an O(C*k)
//   Bloom-membership check on the cue against the engram's pre-built
//   filter. Cheap rejection step; if the approximate overlap is below
//   the completion threshold, skip the expensive exact overlap calc.
//
// Stage 3 (exact): on candidates that pass stages 1 + 2, run the original
//   calculate_overlap to get the bit-exact match score (no false negatives
//   thanks to Bloom's no-false-negatives guarantee on the membership
//   probe).
//
// The maintenance hooks (encode adds, extinction removes) are wired into
// the existing functions further down the file.

#define ENGRAM_BLOOM_BITS      256u
#define ENGRAM_BLOOM_K           4u
#define ENGRAM_INDEX_BUCKETS  4096u   /* must be power-of-2; ~64KB at 16B/entry */
#define ENGRAM_INDEX_BUCKET_MASK (ENGRAM_INDEX_BUCKETS - 1u)

/* Posting list — dynamic array of engram array-indices that contain a
 * given neuron_id. Capacity grows by doubling. count==0 + ids==NULL is
 * the empty-tombstone state. */
typedef struct {
    uint32_t  neuron_id;
    uint32_t* engram_indices;
    uint32_t  count;
    uint32_t  capacity;
} engram_posting_t;

/* Open-addressing hash table neuron_id -> posting. Linear probing,
 * power-of-2 size, empty marker is `engram_indices == NULL && capacity == 0`.
 * neuron_id 0 is treated as empty too (engram code never uses id 0). */
typedef struct {
    engram_posting_t* buckets;       /* [ENGRAM_INDEX_BUCKETS] */
    uint32_t          neuron_count;  /* distinct neurons in the index */
} engram_inverted_index_impl_t;

/* xorshift-based hash family for Bloom + index. Mixes the neuron_id
 * with a per-hash salt so all k bits land in different positions even
 * for tightly clustered ids. */
static inline uint32_t engram_hash(uint32_t x, uint32_t salt) {
    uint32_t h = x ^ salt;
    h ^= h >> 17;
    h *= 0xed5ad4bbu;
    h ^= h >> 11;
    h *= 0xac4c1b51u;
    h ^= h >> 15;
    h *= 0x31848babu;
    h ^= h >> 14;
    return h;
}

/* Bit position helpers for the 256-bit Bloom (uint64_t bloom[4]). */
static inline void engram_bloom_set(uint64_t* bloom, uint32_t neuron_id) {
    for (uint32_t k = 0; k < ENGRAM_BLOOM_K; k++) {
        uint32_t bit = engram_hash(neuron_id, 0x9e3779b9u + k * 0x6a09e667u)
                        & (ENGRAM_BLOOM_BITS - 1u);
        bloom[bit >> 6] |= ((uint64_t)1 << (bit & 63u));
    }
}

static inline bool engram_bloom_test(const uint64_t* bloom, uint32_t neuron_id) {
    for (uint32_t k = 0; k < ENGRAM_BLOOM_K; k++) {
        uint32_t bit = engram_hash(neuron_id, 0x9e3779b9u + k * 0x6a09e667u)
                        & (ENGRAM_BLOOM_BITS - 1u);
        if (((bloom[bit >> 6] >> (bit & 63u)) & 1u) == 0u) return false;
    }
    return true;
}

/* Build the Bloom filter for an engram from its full neuron list.
 * Idempotent — safe to call on already-built engrams (just re-OR's). */
static void engram_bloom_build(memory_engram_t* engram) {
    if (!engram) return;
    memset(engram->bloom, 0, sizeof(engram->bloom));
    for (uint32_t i = 0; i < engram->neuron_count; i++) {
        engram_bloom_set(engram->bloom, engram->neuron_ids[i]);
    }
    engram->bloom_built = true;
}

/* Approximate overlap via Bloom — count cue neurons whose Bloom test
 * hits. Always >= true overlap (no false negatives), may be > true
 * overlap (false positives). Used as a cheap pre-filter; an exact
 * calculate_overlap follows when the bloom score crosses threshold. */
static uint32_t engram_bloom_count_overlap(const uint64_t* bloom,
                                             const uint32_t* cue, uint32_t cue_count) {
    uint32_t hits = 0;
    for (uint32_t i = 0; i < cue_count; i++) {
        if (engram_bloom_test(bloom, cue[i])) hits++;
    }
    return hits;
}

/*-------------- inverted index --------------*/

static engram_inverted_index_impl_t* engram_index_create(void) {
    engram_inverted_index_impl_t* idx = (engram_inverted_index_impl_t*)
        nimcp_calloc(1, sizeof(*idx));
    if (!idx) return NULL;
    idx->buckets = (engram_posting_t*)
        nimcp_calloc(ENGRAM_INDEX_BUCKETS, sizeof(engram_posting_t));
    if (!idx->buckets) {
        nimcp_free(idx);
        return NULL;
    }
    return idx;
}

static void engram_index_destroy(engram_inverted_index_impl_t* idx) {
    if (!idx) return;
    if (idx->buckets) {
        for (uint32_t i = 0; i < ENGRAM_INDEX_BUCKETS; i++) {
            if (idx->buckets[i].engram_indices) {
                nimcp_free(idx->buckets[i].engram_indices);
            }
        }
        nimcp_free(idx->buckets);
    }
    nimcp_free(idx);
}

/* Locate the bucket for `neuron_id` (linear probing). If `create_if_missing`
 * is true and the slot is empty, claim it. Returns NULL if the table is
 * full (only happens at >75% load — not expected at 4K buckets / typical
 * vocab). */
static engram_posting_t* engram_index_find_bucket(engram_inverted_index_impl_t* idx,
                                                    uint32_t neuron_id,
                                                    bool create_if_missing) {
    if (!idx || neuron_id == 0) return NULL;
    uint32_t start = engram_hash(neuron_id, 0xcafebabeu) & ENGRAM_INDEX_BUCKET_MASK;
    for (uint32_t step = 0; step < ENGRAM_INDEX_BUCKETS; step++) {
        uint32_t i = (start + step) & ENGRAM_INDEX_BUCKET_MASK;
        engram_posting_t* b = &idx->buckets[i];
        if (b->capacity == 0 && b->engram_indices == NULL) {
            /* Empty slot. */
            if (!create_if_missing) return NULL;
            b->neuron_id = neuron_id;
            b->engram_indices = (uint32_t*)nimcp_calloc(8, sizeof(uint32_t));
            if (!b->engram_indices) return NULL;
            b->capacity = 8;
            b->count = 0;
            idx->neuron_count++;
            return b;
        }
        if (b->neuron_id == neuron_id) return b;
    }
    return NULL;
}

static void engram_index_add(engram_inverted_index_impl_t* idx,
                              uint32_t neuron_id, uint32_t engram_idx) {
    engram_posting_t* b = engram_index_find_bucket(idx, neuron_id, true);
    if (!b) return;
    /* Skip duplicate (caller is encode; same engram shouldn't be added
     * twice for the same neuron). Linear scan; postings are tiny. */
    for (uint32_t i = 0; i < b->count; i++) {
        if (b->engram_indices[i] == engram_idx) return;
    }
    if (b->count >= b->capacity) {
        uint32_t new_cap = b->capacity * 2;
        uint32_t* grown = (uint32_t*)nimcp_calloc(new_cap, sizeof(uint32_t));
        if (!grown) return;  /* leave posting alone; encode best-effort */
        memcpy(grown, b->engram_indices, b->count * sizeof(uint32_t));
        nimcp_free(b->engram_indices);
        b->engram_indices = grown;
        b->capacity = new_cap;
    }
    b->engram_indices[b->count++] = engram_idx;
}

static void engram_index_remove(engram_inverted_index_impl_t* idx,
                                  uint32_t neuron_id, uint32_t engram_idx) {
    engram_posting_t* b = engram_index_find_bucket(idx, neuron_id, false);
    if (!b) return;
    for (uint32_t i = 0; i < b->count; i++) {
        if (b->engram_indices[i] == engram_idx) {
            /* Swap-with-last (order-independent). */
            b->engram_indices[i] = b->engram_indices[b->count - 1];
            b->count--;
            return;
        }
    }
}

/*-------------- maintenance helpers --------------*/

/* Add an engram (already populated with neuron_ids) to the index +
 * compute its Bloom filter. Idempotent: removing first then re-adding is
 * the right way to handle a reused slot. */
static void engram_index_add_engram(engram_system_t* system,
                                      uint32_t engram_idx) {
    if (!system || !system->inverted_index) return;
    if (engram_idx >= system->capacity) return;
    memory_engram_t* e = &system->engrams[engram_idx];
    if (!e->active) return;
    engram_bloom_build(e);
    for (uint32_t i = 0; i < e->neuron_count; i++) {
        engram_index_add((engram_inverted_index_impl_t*)system->inverted_index,
                          e->neuron_ids[i], engram_idx);
    }
}

static void engram_index_remove_engram(engram_system_t* system,
                                         uint32_t engram_idx) {
    if (!system || !system->inverted_index) return;
    if (engram_idx >= system->capacity) return;
    memory_engram_t* e = &system->engrams[engram_idx];
    /* Use whatever neuron list the slot has, even if !active — we may
     * be removing as part of teardown. */
    for (uint32_t i = 0; i < e->neuron_count; i++) {
        engram_index_remove((engram_inverted_index_impl_t*)system->inverted_index,
                              e->neuron_ids[i], engram_idx);
    }
    e->bloom_built = false;
    memset(e->bloom, 0, sizeof(e->bloom));
}

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Calculate overlap between two neuron sets
 */
static uint32_t calculate_overlap(
    const uint32_t* set1, uint32_t count1,
    const uint32_t* set2, uint32_t count2) {

    // WHAT: Count neurons present in both sets
    // WHY:  Pattern matching for recall
    // HOW:  Nested loop comparison (optimizable with hash table)

    if (!set1 || !set2) return 0;

    uint32_t overlap = 0;
    for (uint32_t i = 0; i < count1; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count1 > 256) {
            engram_heartbeat("engram_loop",
                             (float)(i + 1) / (float)count1);
        }

        for (uint32_t j = 0; j < count2; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && count2 > 256) {
                engram_heartbeat("engram_loop",
                                 (float)(j + 1) / (float)count2);
            }

            if (set1[i] == set2[j]) {
                overlap++;
                break;
            }
        }
    }

    return overlap;
}

/**
 * @brief Expand engram array capacity
 */
static bool expand_engram_array(engram_system_t* system) {
    // WHAT: Double array capacity when full
    // WHY:  Dynamic growth as needed
    // HOW:  Allocate new array, copy, free old (handles both unified and direct memory)

    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "expand_engram_array: system is NULL");
        return false;
    }

    uint32_t new_capacity = (uint32_t)(system->capacity * ENGRAM_GROWTH_FACTOR);
    size_t old_size = system->capacity * sizeof(memory_engram_t);

    // Allocate new array (always use direct allocation for grown arrays)
    memory_engram_t* new_array = (memory_engram_t*)nimcp_calloc(new_capacity, sizeof(memory_engram_t));
    if (!new_array) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "expand_engram_array: new_array is NULL");
        return false;
    }

    // Copy existing engrams
    memcpy(new_array, system->engrams, old_size);

    // Free old array based on how it was allocated
    if (system->engrams_handle) {
        // Allocated via unified memory - free through unified memory
        unified_mem_free(system->engrams_handle);
        system->engrams_handle = NULL;  // No longer using unified memory for array
    } else {
        // Allocated via direct memory
        nimcp_free(system->engrams);
    }

    system->engrams = new_array;
    system->capacity = new_capacity;

    return true;
}

/* Eviction: pick the weakest active engram and deactivate it so its
 * slot can be reused. Selection priority:
 *   1) any DEGRADING engram (cheapest to lose)
 *   2) oldest LABILE engram by encoding_time_us (least invested in)
 *   3) lowest consolidation_strength among active engrams (last resort)
 * Never evicts CONSOLIDATED unless every other tier is empty.
 * Returns the array index of the evicted slot, or UINT32_MAX if no
 * candidate exists (shouldn't happen — system is genuinely full). */
static uint32_t engram_pick_eviction_victim(engram_system_t* system) {
    if (!system || !system->engrams) return UINT32_MAX;

    uint32_t best_degrading = UINT32_MAX;
    uint32_t best_labile = UINT32_MAX;
    uint64_t best_labile_age = UINT64_MAX;
    uint32_t best_other = UINT32_MAX;
    float    best_other_strength = 1.1f;

    for (uint32_t i = 0; i < system->capacity; i++) {
        memory_engram_t* e = &system->engrams[i];
        if (!e->active) continue;
        if (e->state == ENGRAM_STATE_DEGRADING) {
            best_degrading = i;
            break;  /* first DEGRADING wins; all are equally cheap to drop */
        }
        if (e->state == ENGRAM_STATE_LABILE) {
            if (e->encoding_time_us < best_labile_age) {
                best_labile = i;
                best_labile_age = e->encoding_time_us;
            }
        }
        if (e->consolidation_strength < best_other_strength) {
            best_other_strength = e->consolidation_strength;
            best_other = i;
        }
    }

    if (best_degrading != UINT32_MAX) return best_degrading;
    if (best_labile != UINT32_MAX) return best_labile;
    return best_other;
}

static void engram_evict_slot(engram_system_t* system, uint32_t slot) {
    if (!system || slot >= system->capacity) return;
    memory_engram_t* e = &system->engrams[slot];
    /* Drop from inverted index first while neuron_ids are still valid. */
    if (e->neuron_count > 0) {
        engram_index_remove_engram(system, slot);
    }
    /* Track which counters to decrement based on prior state. */
    if (e->active) {
        if (system->active_count > 0) system->active_count--;
        switch (e->state) {
            case ENGRAM_STATE_LABILE:
                if (system->labile_count > 0) system->labile_count--;
                break;
            case ENGRAM_STATE_CONSOLIDATING:
                if (system->consolidating_count > 0) system->consolidating_count--;
                break;
            case ENGRAM_STATE_CONSOLIDATED:
                if (system->consolidated_count > 0) system->consolidated_count--;
                break;
            default:
                break;
        }
    }
    /* Wipe slot — leaves neuron_count == 0 and bloom/active=false so
     * the slot looks pristine to the next encode. */
    memset(e, 0, sizeof(*e));
    system->total_evictions++;
}

/**
 * @brief Find free engram slot
 */
static memory_engram_t* find_free_slot(engram_system_t* system) {
    // WHAT: Locate inactive engram slot
    // WHY:  Reuse memory efficiently
    // HOW:  Linear search, expand if needed (or evict at the soft cap)

    if (!system) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");


        return NULL;


    }

    /* Soft-cap eviction: when we're at or above the configured cap,
     * pick a victim and reuse its slot rather than growing the array.
     * Cap == 0 disables the policy (legacy unbounded-grow behaviour). */
    if (system->max_active_engrams != 0 &&
        system->active_count >= system->max_active_engrams) {
        uint32_t victim = engram_pick_eviction_victim(system);
        if (victim != UINT32_MAX) {
            engram_evict_slot(system, victim);
            /* engram_evict_slot already decremented active_count and
             * cleared the slot, so it now looks inactive — fall through
             * to the inactive-slot scan below which finds it. */
        }
    }

    // Try to find inactive slot
    for (uint32_t i = 0; i < system->capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->capacity > 256) {
            engram_heartbeat("engram_loop",
                             (float)(i + 1) / (float)system->capacity);
        }

        if (!system->engrams[i].active) {
            /* Recall acceleration: if this slot was previously occupied,
             * its old neuron_ids may still be present and stale-indexed.
             * Scrub the inverted index entries before encode rebuilds. */
            if (system->engrams[i].neuron_count > 0) {
                engram_index_remove_engram(system, i);
            }
            return &system->engrams[i];
        }
    }

    /* All slots occupied AND no eviction freed one — try to expand.
     * Refuse to grow past the soft cap. */
    if (system->max_active_engrams != 0 &&
        system->capacity >= system->max_active_engrams) {
        return NULL;  /* at the cap */
    }

    // Need to expand
    uint32_t old_capacity = system->capacity;
    if (!expand_engram_array(system)) {
        return NULL;  /* All slots occupied is normal */
    }

    // Return first slot in new space
    return &system->engrams[old_capacity];
}

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

engram_system_t* engram_system_create(void) {
    // WHAT: Allocate and initialize engram system
    // WHY:  Required for memory trace tracking
    // HOW:  Allocate struct and array using unified memory with CoW support

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_system_create", 0.0f);


    LOG_INFO("Creating engram system");

    engram_system_t* system = (engram_system_t*)nimcp_calloc(1, sizeof(engram_system_t));
    if (!system) {
        LOG_ERROR("Failed to allocate engram system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "engram_system_create: system is NULL");
        return NULL;
    }

    // Initialize unified memory manager for CoW support
    unified_mem_config_t mem_config = unified_mem_default_config();
    mem_config.enable_cow = true;
    mem_config.enable_tracking = true;
    system->mem_manager = unified_mem_create(&mem_config);

    if (system->mem_manager) {
        // Allocate engram array via unified memory (enables O(1) brain cloning)
        unified_mem_request_t req = unified_mem_request(
            ENGRAM_INITIAL_CAPACITY * sizeof(memory_engram_t),
            NULL,  // Zero-initialized
            true   // Enable CoW
        );
        system->engrams_handle = unified_mem_alloc(system->mem_manager, &req);
        if (system->engrams_handle) {
            system->engrams = (memory_engram_t*)unified_mem_write(system->engrams_handle);
            LOG_DEBUG(LOG_MODULE, "Engram array allocated via unified memory with CoW support");
        }
    }

    // Fallback to direct allocation if unified memory unavailable
    if (!system->engrams) {
        system->engrams = (memory_engram_t*)nimcp_calloc(
            ENGRAM_INITIAL_CAPACITY, sizeof(memory_engram_t));
        if (!system->engrams) {
            if (system->mem_manager) unified_mem_destroy(system->mem_manager);
            nimcp_free(system);
            system = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engram_system_create: validation failed");
            return NULL;
        }
        LOG_DEBUG(LOG_MODULE, "Engram array allocated via direct memory (no CoW)");
    }

    // Set initial capacity
    system->capacity = ENGRAM_INITIAL_CAPACITY;
    system->active_count = 0;
    system->next_engram_id = 1;  // Start at 1 (0 = invalid)

    // Set defaults
    system->systems_consolidation_enabled = true;
    system->hippocampal_capacity = 1.0F;
    system->cortical_capacity = 1.0F;
    system->sleep_consolidation_rate = 2.0F;  // 2x faster during sleep
    system->baseline_decay_rate = ENGRAM_BASE_DECAY_RATE;
    system->use_interference = true;
    system->separation_threshold = 0.3F;
    system->completion_threshold = 0.4F;

    // Enable integrations
    system->integrate_with_sleep = true;
    system->integrate_with_emotion = true;
    system->integrate_with_consolidation = true;

    // Recall acceleration: build the inverted index up front. Failure
    // here is non-fatal — recall transparently falls back to the linear
    // scan if `inverted_index == NULL`.
    system->inverted_index = engram_index_create();

    // Soft cap on active engrams. Default = ENGRAM_MAX_COUNT (524288).
    // Eviction kicks in once active_count reaches this value. 0 means
    // unbounded (legacy behaviour); set via engram_system_set_max_active.
    system->max_active_engrams = ENGRAM_MAX_COUNT;
    system->total_evictions = 0;

    // Phase 1.5: Initialize memory pool for engram allocations
    memory_pool_config_t engram_pool_config = {
        .block_size = sizeof(memory_engram_t),
        .num_blocks = ENGRAM_INITIAL_CAPACITY,  // Match initial array capacity
        .alignment = 16,   // SIMD alignment
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    system->engram_pool = memory_pool_create(&engram_pool_config);

    
    // Bio-async registration
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_MEMORY,
            .module_name = "engram",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;

            // Try KG-driven wiring callback first
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_MEMORY,
                (void*)engram_wiring_handler_callback,
                system
            );

            if (wiring_result != NIMCP_SUCCESS) {
                // Fallback to legacy hardcoded registration
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(system->bio_ctx, BIO_MSG_WORKING_MEMORY_RETRIEVE,
                                                handle_memory_query)
                );
                LOG_INFO(LOG_MODULE, "Bio-async registered with legacy handlers (module_id=0x%04X)", BIO_MODULE_MEMORY);
            } else {
                LOG_INFO(LOG_MODULE, "Bio-async registered with KG wiring callback (module_id=0x%04X)", BIO_MODULE_MEMORY);
            }
        }
    }

    return system;
}

/*=============================================================================
 * BIO-ASYNC HANDLER IMPLEMENTATIONS
 *============================================================================*/

static nimcp_error_t handle_memory_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)msg_size;
    (void)response_promise;
    NIMCP_CHECK_THROW(msg && user_data, NIMCP_ERROR_NULL_ARG, "msg or user_data is NULL");
    engram_system_t* system = (engram_system_t*)user_data;
    LOG_DEBUG(LOG_MODULE, "Received memory query, active engrams=%u", system->active_count);
    return NIMCP_SUCCESS;
}

static void bio_broadcast_engram_encoded(engram_system_t* system, uint32_t engram_id, float strength) {
    if (!system || !system->bio_async_enabled || !system->bio_ctx) { return; }
    // Use salience response for engram encoded notification
    bio_msg_salience_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(system->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.stimulus_id = engram_id;
    msg.salience_score = strength;
    msg.attention_priority = strength;
    msg.requires_immediate_attention = (strength > 0.8F);
    bio_router_broadcast(system->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast engram encoded: id=%u, strength=%.2f", engram_id, strength);
}

static void bio_broadcast_engram_recalled(engram_system_t* system, uint32_t engram_id, float confidence) {
    if (!system || !system->bio_async_enabled || !system->bio_ctx) { return; }
    // Use salience response for engram recalled notification
    bio_msg_salience_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(system->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.stimulus_id = engram_id;
    msg.salience_score = confidence;
    msg.attention_priority = confidence;
    msg.requires_immediate_attention = false;
    bio_router_broadcast(system->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast engram recalled: id=%u, confidence=%.2f", engram_id, confidence);
}

void engram_system_destroy(engram_system_t* system) {
    // WHAT: Free all engram system resources
    // WHY:  Prevent memory leaks
    // HOW:  Free unified memory handles, then struct

    if (!system) return;

    // Free engram array (unified memory or direct)
    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_system_destroy", 0.0f);


    if (system->engrams_handle) {
        unified_mem_free(system->engrams_handle);
        system->engrams_handle = NULL;
        system->engrams = NULL;  // Was pointing into unified memory
    } else if (system->engrams) {
        nimcp_free(system->engrams);
        system->engrams = NULL;
    }

    // Destroy unified memory manager
    if (system->mem_manager) {
        unified_mem_destroy(system->mem_manager);
        system->mem_manager = NULL;
    }

    // Phase 1.5: Destroy memory pool
    if (system->engram_pool) {
        memory_pool_destroy(system->engram_pool);
    }

    // Recall acceleration: tear down the inverted index (frees every
    // posting list it owns).
    if (system->inverted_index) {
        engram_index_destroy((engram_inverted_index_impl_t*)system->inverted_index);
        system->inverted_index = NULL;
    }

    // Unregister from bio-router
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_enabled = false;
    }

    nimcp_free(system);
    system = NULL;
}

void engram_system_reset(engram_system_t* system) {
    // WHAT: Clear all engrams, preserve configuration
    // WHY:  Fresh start without reallocating
    // HOW:  Zero engram array, reset counters

    if (!system) return;

    // Save configuration
    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_system_reset", 0.0f);


    bool sys_consol = system->systems_consolidation_enabled;
    float sleep_rate = system->sleep_consolidation_rate;
    float decay = system->baseline_decay_rate;
    bool interference = system->use_interference;
    float sep_thresh = system->separation_threshold;
    float comp_thresh = system->completion_threshold;
    bool int_sleep = system->integrate_with_sleep;
    bool int_emotion = system->integrate_with_emotion;
    bool int_consol = system->integrate_with_consolidation;

    // Clear engrams
    memset(system->engrams, 0, system->capacity * sizeof(memory_engram_t));

    // Recall acceleration: rebuild the inverted index from empty. The
    // memset above wiped every Bloom field; the index would still point
    // at the now-empty slots without this reset.
    if (system->inverted_index) {
        engram_index_destroy((engram_inverted_index_impl_t*)system->inverted_index);
    }
    system->inverted_index = engram_index_create();

    // Reset counters
    system->active_count = 0;
    system->next_engram_id = 1;
    system->labile_count = 0;
    system->consolidating_count = 0;
    system->consolidated_count = 0;
    system->replays_during_sleep = 0;
    system->total_encodings = 0;
    system->total_recalls = 0;
    system->total_consolidations = 0;
    system->total_extinctions = 0;
    system->average_consolidation_time = 0.0F;

    // Restore configuration
    system->systems_consolidation_enabled = sys_consol;
    system->sleep_consolidation_rate = sleep_rate;
    system->baseline_decay_rate = decay;
    system->use_interference = interference;
    system->separation_threshold = sep_thresh;
    system->completion_threshold = comp_thresh;
    system->integrate_with_sleep = int_sleep;
    system->integrate_with_emotion = int_emotion;
    system->integrate_with_consolidation = int_consol;
}

//=============================================================================
// ENCODING FUNCTIONS
//=============================================================================

uint64_t engram_encode(
    engram_system_t* system,
    const uint32_t* neuron_ids,
    const float* activations,
    uint32_t count,
    memory_type_t memory_type,
    emotional_tag_t emotion) {

    // WHAT: Create new engram from neural activity
    // WHY:  Store experience as memory trace
    // HOW:  Tag neurons, record activations, initialize state

    // Guard clauses
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engram_encode: system is NULL");
        return 0;
    }
    if (!neuron_ids || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engram_encode: neuron_ids or activations is NULL");
        return 0;
    }
    if (count == 0) return 0;
    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_encode", 0.0f);


    if (count > ENGRAM_MAX_NEURONS) {
        count = ENGRAM_MAX_NEURONS;  // Truncate to max
    }

    // Find free slot
    memory_engram_t* engram = find_free_slot(system);
    if (!engram) return 0;

    // Initialize engram
    engram->active = true;
    engram->engram_id = system->next_engram_id++;
    engram->memory_type = memory_type;
    engram->neuron_count = count;

    // Copy neurons and activations
    memcpy(engram->neuron_ids, neuron_ids, count * sizeof(uint32_t));
    memcpy(engram->neuron_activation, activations, count * sizeof(float));

    // Set initial state
    engram->state = ENGRAM_STATE_ENCODING;
    engram->consolidation_strength = 0.0F;
    engram->primary_location = ENGRAM_LOCATION_HIPPOCAMPUS;  // Always starts here
    engram->secondary_location = ENGRAM_LOCATION_CORTEX;     // Target for systems consolidation

    // Temporal info (get from system time - placeholder for now)
    engram->encoding_time_us = 0;  // Will be set by caller
    engram->last_reactivation_us = 0;
    engram->reactivation_count = 0;
    engram->decay_rate = system->baseline_decay_rate;

    // IEG tagging - strength modulated by arousal
    engram->is_tagged = (emotion.arousal > 0.5F);  // Tag if arousal above threshold
    // Tag strength scales with arousal: higher arousal = stronger tagging
    engram->tag_strength = 0.5F + (emotion.arousal * 0.5F);  // Range: 0.5 to 1.0
    engram->tag_onset_time_us = 0;  // Will be set by caller

    // Emotional context
    engram->emotion = emotion;
    engram->vividness = 1.0F;  // Starts vivid
    engram->confidence = 1.0F;

    // Emotional enhancement
    if (system->integrate_with_emotion && emotion.arousal > 0.6F) {
        engram->decay_rate *= 0.5F;  // Emotional memories resist forgetting
        engram->vividness *= 1.3F;   // More vivid
    }

    // Reconsolidation
    engram->is_reconsolidating = false;
    engram->reconsolidation_start_us = 0;

    // Statistics
    engram->recall_latency_ms = 0.0F;
    engram->successful_recalls = 0;

    // Update system counters
    system->active_count++;
    system->labile_count++;
    system->total_encodings++;

    /* W6: emit KG event for engram formation. Null-safe if no brain
     * registered (e.g. unit-test mode without full brain init). */
    memory_kg_emit_engram_form(
        memory_kg_events_get_registered_brain(),
        engram->engram_id, engram->vividness);

    /* Recall acceleration: build the engram's Bloom filter and add its
     * array-index to the inverted index for every neuron it contains.
     * This is the on-write maintenance that lets engram_recall query
     * the index in O(C) instead of scanning every slot. Cost is
     * O(neuron_count * k) ≈ O(1024) hash ops per encode — small. */
    {
        ptrdiff_t slot = engram - system->engrams;
        if (slot >= 0 && (uint32_t)slot < system->capacity) {
            engram_index_add_engram(system, (uint32_t)slot);
        }
    }

    return engram->engram_id;
}

//=============================================================================
// RECALL FUNCTIONS
//=============================================================================

uint64_t engram_recall(
    engram_system_t* system,
    const uint32_t* cue_neurons,
    uint32_t cue_count,
    uint32_t* activation_out,
    float* activations_out,
    uint32_t max_activation_count,
    float* confidence_out) {

    // WHAT: Reactivate engram from partial cue
    // WHY:  Retrieve stored memory
    // HOW:  Pattern completion via overlap matching

    // Guard clauses
    if (!system) return 0;
    if (!cue_neurons) return 0;
    if (cue_count == 0) return 0;
    if (!activation_out || !activations_out) return 0;

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_recall", 0.0f);


    float best_match = 0.0F;
    uint64_t best_engram_id = 0;
    memory_engram_t* best_engram = NULL;

    /* Recall acceleration: prefer the inverted-index pipeline when it's
     * available + populated. Falls back to the legacy linear scan when
     * the index is missing (e.g. allocation failed at create time) or
     * empty (no engrams encoded yet — scan is trivially cheap). */
    engram_inverted_index_impl_t* idx =
        (engram_inverted_index_impl_t*)system->inverted_index;
    bool use_index_path = (idx != NULL && idx->neuron_count > 0);

    if (use_index_path) {
        /* Stage 1 — candidate selection via inverted index.
         *
         * For every cue neuron, look up the posting list of engrams
         * containing it; bump a per-candidate hit counter. Candidates
         * with zero hits never get touched at all (the whole point of
         * the index — skip engrams that share no neurons with the cue).
         *
         * Working buffer: hits_count[engram_idx] keyed by array index,
         * sized to system->capacity. A 100K-engram brain costs 400 KB
         * here — small enough to stack-alloca for typical sizes but
         * we heap-alloc to be safe at any scale. */
        uint16_t* hits = (uint16_t*)nimcp_calloc(system->capacity, sizeof(uint16_t));
        if (!hits) {
            /* OOM on the working buffer — fall through to the linear
             * scan below rather than fail recall outright. */
            use_index_path = false;
        } else {
            for (uint32_t ci = 0; ci < cue_count; ci++) {
                engram_posting_t* post = engram_index_find_bucket(
                    idx, cue_neurons[ci], false);
                if (!post) continue;
                for (uint32_t pi = 0; pi < post->count; pi++) {
                    uint32_t eid = post->engram_indices[pi];
                    if (eid < system->capacity) {
                        if (hits[eid] < UINT16_MAX) hits[eid]++;
                    }
                }
            }

            /* Stage 2 + 3 — Bloom skip-test then exact overlap on
             * candidates with at least one hit. */
            for (uint32_t i = 0; i < system->capacity; i++) {
                if (hits[i] == 0) continue;
                memory_engram_t* engram = &system->engrams[i];
                if (!engram->active) continue;
                if (engram->state == ENGRAM_STATE_DEGRADING) continue;
                if (engram->neuron_count == 0) continue;

                /* Bloom skip-test: if the Bloom-approximate overlap
                 * can't possibly clear the threshold, skip the exact
                 * calc. The approximate is an upper bound on the true
                 * overlap (Bloom has no false negatives but may produce
                 * false positives — overcounting only). So if the
                 * upper-bound match is below threshold, the real one
                 * definitely is too. */
                if (engram->bloom_built) {
                    uint32_t bloom_hits = engram_bloom_count_overlap(
                        engram->bloom, cue_neurons, cue_count);
                    float bloom_match = (float)bloom_hits / (float)engram->neuron_count;
                    bloom_match *= engram->consolidation_strength;
                    if (bloom_match <= system->completion_threshold) continue;
                    if (bloom_match <= best_match) continue;
                }

                /* Exact overlap calc — same as the legacy path. */
                uint32_t overlap = calculate_overlap(
                    cue_neurons, cue_count,
                    engram->neuron_ids, engram->neuron_count);
                float match = (float)overlap / engram->neuron_count;
                match *= engram->consolidation_strength;
                if (match > system->completion_threshold && match > best_match) {
                    best_match = match;
                    best_engram_id = engram->engram_id;
                    best_engram = engram;
                }
            }
            nimcp_free(hits);
        }
    }

    if (!use_index_path) {
        /* Legacy linear-scan path — runs only when the index isn't
         * available. Identical to the original implementation; kept
         * verbatim for behavioural compatibility with engram-system
         * configurations that disable the index. */
        for (uint32_t i = 0; i < system->capacity; i++) {
            if ((i & 0xFF) == 0 && system->capacity > 256) {
                engram_heartbeat("engram_loop",
                                 (float)(i + 1) / (float)system->capacity);
            }

            memory_engram_t* engram = &system->engrams[i];
            if (!engram->active) continue;
            if (engram->state == ENGRAM_STATE_DEGRADING) continue;

            uint32_t overlap = calculate_overlap(
                cue_neurons, cue_count,
                engram->neuron_ids, engram->neuron_count);
            float match = (float)overlap / engram->neuron_count;
            match *= engram->consolidation_strength;
            if (match > system->completion_threshold && match > best_match) {
                best_match = match;
                best_engram_id = engram->engram_id;
                best_engram = engram;
            }
        }
    }

    // No match found
    if (best_engram_id == 0) {
        if (confidence_out) *confidence_out = 0.0F;
        return 0;
    }

    // Reactivate engram
    uint32_t output_count = best_engram->neuron_count;
    if (output_count > max_activation_count) {
        output_count = max_activation_count;
    }

    memcpy(activation_out, best_engram->neuron_ids,
           output_count * sizeof(uint32_t));
    memcpy(activations_out, best_engram->neuron_activation,
           output_count * sizeof(float));

    // Update engram statistics
    best_engram->reactivation_count++;
    best_engram->successful_recalls++;
    best_engram->last_reactivation_us = 0;  // Will be set by caller

    // Trigger reconsolidation if consolidated
    if (best_engram->state == ENGRAM_STATE_CONSOLIDATED) {
        engram_trigger_reconsolidation(system, best_engram_id);
    }

    // Update system statistics
    system->total_recalls++;

    // Output confidence
    if (confidence_out) {
        *confidence_out = best_match * best_engram->confidence;
    }

    /* W6: emit KG event for engram recall */
    memory_kg_emit_engram_recall(
        memory_kg_events_get_registered_brain(),
        best_engram_id, best_match * best_engram->confidence);

    return best_engram_id;
}

bool engram_recognize(
    engram_system_t* system,
    const uint32_t* pattern,
    uint32_t count,
    float* familiarity_out) {

    // WHAT: Test if pattern is familiar
    // WHY:  Recognition faster than recall
    // HOW:  Check overlap without full reactivation

    // Guard clauses
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return false;
    }
    if (!pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: pattern is NULL");
        return false;
    }
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: count is zero");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_recognize", 0.0f);


    float max_familiarity = 0.0F;

    // Check all engrams
    for (uint32_t i = 0; i < system->capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->capacity > 256) {
            engram_heartbeat("engram_loop",
                             (float)(i + 1) / (float)system->capacity);
        }

        memory_engram_t* engram = &system->engrams[i];

        if (!engram->active) continue;

        // Calculate overlap
        uint32_t overlap = calculate_overlap(
            pattern, count,
            engram->neuron_ids, engram->neuron_count);

        float familiarity = (float)overlap / (count > 0 ? count : 1);

        // Weight by consolidation
        familiarity *= engram->consolidation_strength;

        if (familiarity > max_familiarity) {
            max_familiarity = familiarity;
        }
    }

    if (familiarity_out) {
        *familiarity_out = max_familiarity;
    }

    return (max_familiarity >= ENGRAM_RECOGNITION_THRESHOLD);
}

//=============================================================================
// CONSOLIDATION FUNCTIONS
//=============================================================================

void engram_consolidate_update(
    engram_system_t* system,
    float dt,
    bool is_sleeping) {
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_consolidate_update", 0.0f);


    if (system && system->bio_async_enabled && system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 5);
    }


    // WHAT: Advance consolidation for all engrams
    // WHY:  Labile → stable transition
    // HOW:  Time-dependent strengthening, sleep boost

    if (!system) return;
    if (dt <= 0.0F) return;

    float consolidation_rate = is_sleeping ?
        (dt / ENGRAM_SYNAPTIC_CONSOLIDATION_TIME) * system->sleep_consolidation_rate :
        (dt / ENGRAM_SYNAPTIC_CONSOLIDATION_TIME);

    uint32_t labile = 0;
    uint32_t consolidating = 0;
    uint32_t consolidated = 0;

    for (uint32_t i = 0; i < system->capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->capacity > 256) {
            engram_heartbeat("engram_loop",
                             (float)(i + 1) / (float)system->capacity);
        }

        memory_engram_t* engram = &system->engrams[i];

        if (!engram->active) continue;

        // Update based on state
        if (engram->state == ENGRAM_STATE_ENCODING) {
            // Transition to labile and start consolidating
            engram->state = ENGRAM_STATE_LABILE;
            engram->consolidation_strength += consolidation_rate;

            if (engram->consolidation_strength >= 1.0F) {
                engram->consolidation_strength = 1.0F;
                engram->state = ENGRAM_STATE_CONSOLIDATED;
                system->total_consolidations++;
                consolidated++;
            } else if (engram->consolidation_strength > 0.0F) {
                engram->state = ENGRAM_STATE_CONSOLIDATING;
                consolidating++;
            } else {
                labile++;
            }
        }
        else if (engram->state == ENGRAM_STATE_LABILE ||
                 engram->state == ENGRAM_STATE_CONSOLIDATING) {

            // Increase consolidation strength
            engram->consolidation_strength += consolidation_rate;

            if (engram->consolidation_strength >= 1.0F) {
                engram->consolidation_strength = 1.0F;
                engram->state = ENGRAM_STATE_CONSOLIDATED;
                system->total_consolidations++;
                consolidated++;
            } else {
                engram->state = ENGRAM_STATE_CONSOLIDATING;
                consolidating++;
            }
        }
        else if (engram->state == ENGRAM_STATE_CONSOLIDATED) {
            consolidated++;
        }
        else if (engram->state == ENGRAM_STATE_RECONSOLIDATING) {
            // Check if reconsolidation window expired
            // (placeholder - needs time tracking)
            consolidating++;
        }
    }

    // Update counts
    system->labile_count = labile;
    system->consolidating_count = consolidating;
    system->consolidated_count = consolidated;
}

void engram_sleep_replay(
    engram_system_t* system,
    uint32_t replay_count) {

    // WHAT: Reactivate engrams during sleep
    // WHY:  Sleep strengthens memories
    // HOW:  Strengthen recently encoded engrams

    if (!system) return;
    if (replay_count == 0) return;

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_sleep_replay", 0.0f);


    uint32_t replayed = 0;

    // Prioritize tagged, labile and consolidating engrams
    for (uint32_t i = 0; i < system->capacity && replayed < replay_count; i++) {
        memory_engram_t* engram = &system->engrams[i];

        if (!engram->active) continue;

        // Replay tagged engrams (high arousal) or those in consolidation states
        if (engram->is_tagged ||
            engram->state == ENGRAM_STATE_LABILE ||
            engram->state == ENGRAM_STATE_CONSOLIDATING) {

            // Replay strengthens
            engram->consolidation_strength += 0.01F;
            if (engram->consolidation_strength > 1.0F) {
                engram->consolidation_strength = 1.0F;
            }

            engram->reactivation_count++;
            replayed++;
        }
    }

    system->replays_during_sleep += replayed;
}

//=============================================================================
// RECONSOLIDATION FUNCTIONS
//=============================================================================

void engram_trigger_reconsolidation(
    engram_system_t* system,
    uint64_t engram_id) {

    // WHAT: Make engram temporarily labile
    // WHY:  Recalled memories can be updated
    // HOW:  Set reconsolidation flag and timer

    if (!system) return;
    if (engram_id == 0) return;

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_trigger_reconsolidat", 0.0f);


    memory_engram_t* engram = engram_get_by_id(system, engram_id);
    if (!engram) return;

    // Only consolidated engrams can reconsolidate
    if (engram->state != ENGRAM_STATE_CONSOLIDATED) return;

    engram->is_reconsolidating = true;
    engram->state = ENGRAM_STATE_RECONSOLIDATING;
    engram->reconsolidation_start_us = 0;  // Will be set by caller
}

bool engram_block_reconsolidation(
    engram_system_t* system,
    uint64_t engram_id) {

    // WHAT: Prevent engram restabilization
    // WHY:  Therapeutic weakening of maladaptive memories
    // HOW:  Set to degrading state

    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return false;
    }
    if (engram_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: engram_id is zero");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_block_reconsolidatio", 0.0f);


    memory_engram_t* engram = engram_get_by_id(system, engram_id);
    if (!engram) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: engram is NULL");
        return false;
    }

    if (!engram->is_reconsolidating) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: engram->is_reconsolidating is NULL");
        return false;
    }

    // Block = put in degrading state
    engram->state = ENGRAM_STATE_DEGRADING;
    engram->is_reconsolidating = false;
    engram->consolidation_strength *= 0.5F;  // Weaken significantly

    return true;
}

//=============================================================================
// FORGETTING AND EXTINCTION
//=============================================================================

void engram_apply_decay(
    engram_system_t* system,
    float dt) {

    // WHAT: Natural forgetting over time
    // WHY:  Realistic memory decay
    // HOW:  Exponential decay of unused engrams

    if (!system) return;
    if (dt <= 0.0F) return;

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_apply_decay", 0.0f);


    for (uint32_t i = 0; i < system->capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->capacity > 256) {
            engram_heartbeat("engram_loop",
                             (float)(i + 1) / (float)system->capacity);
        }

        memory_engram_t* engram = &system->engrams[i];

        if (!engram->active) continue;

        // Only decay consolidated engrams
        if (engram->state != ENGRAM_STATE_CONSOLIDATED) continue;

        // Apply decay
        float decay = engram->decay_rate * dt;
        engram->consolidation_strength -= decay;
        engram->vividness -= decay * 0.5F;

        // Clamp
        if (engram->consolidation_strength < 0.0F) {
            engram->consolidation_strength = 0.0F;
        }
        if (engram->vividness < 0.0F) {
            engram->vividness = 0.0F;
        }

        // Mark as degrading if very weak
        if (engram->consolidation_strength < 0.1F) {
            bool was_already_degrading = (engram->state == ENGRAM_STATE_DEGRADING);
            engram->state = ENGRAM_STATE_DEGRADING;
            /* Recall acceleration: drop newly-degraded engrams from the
             * inverted index. Skip if it was already DEGRADING (avoids
             * repeated remove on every decay tick). */
            if (!was_already_degrading) {
                engram_index_remove_engram(system, i);
            }
        }

        // Mark as forgotten if extremely weak (but keep active for tracking)
        // Very old/forgotten memories remain in system as degraded traces
        if (engram->consolidation_strength < 0.01F) {
            engram->state = ENGRAM_STATE_DEGRADING;
            // Don't deactivate - allow tracking of degraded memories
        }
    }
}

void engram_extinction(
    engram_system_t* system,
    uint64_t engram_id,
    float extinction_strength) {

    // WHAT: Active weakening through unreinforced recall
    // WHY:  Model extinction learning
    // HOW:  Direct reduction of consolidation strength

    if (!system) return;
    if (engram_id == 0) return;
    if (extinction_strength <= 0.0F) return;

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_extinction", 0.0f);


    memory_engram_t* engram = engram_get_by_id(system, engram_id);
    if (!engram) return;

    // Weaken engram - affects consolidation, confidence, and vividness
    engram->consolidation_strength -= extinction_strength;
    if (engram->consolidation_strength < 0.0F) {
        engram->consolidation_strength = 0.0F;
    }

    // Extinction also reduces confidence and vividness of the memory
    engram->confidence -= extinction_strength * 0.5F;  // Confidence drops with extinction
    if (engram->confidence < 0.0F) {
        engram->confidence = 0.0F;
    }

    engram->vividness -= extinction_strength * 0.3F;  // Memory becomes less vivid
    if (engram->vividness < 0.0F) {
        engram->vividness = 0.0F;
    }

    // Update state
    if (engram->consolidation_strength < 0.1F) {
        engram->state = ENGRAM_STATE_DEGRADING;
        system->total_extinctions++;
        /* Recall acceleration: degraded engrams are skipped by recall,
         * so drop them from the index to avoid wasted candidate scoring.
         * Bloom is left zeroed by remove. */
        ptrdiff_t slot = engram - system->engrams;
        if (slot >= 0 && (uint32_t)slot < system->capacity) {
            engram_index_remove_engram(system, (uint32_t)slot);
        }
    }
}

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

memory_engram_t* engram_get_by_id(
    engram_system_t* system,
    uint64_t engram_id) {

    // WHAT: Find engram by ID
    // WHY:  Access for inspection/modification
    // HOW:  Linear search

    if (!system) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");


        return NULL;


    }
    if (engram_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: engram_id is zero");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_get_by_id", 0.0f);


    for (uint32_t i = 0; i < system->capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->capacity > 256) {
            engram_heartbeat("engram_loop",
                             (float)(i + 1) / (float)system->capacity);
        }

        if (system->engrams[i].active &&
            system->engrams[i].engram_id == engram_id) {
            return &system->engrams[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: operation failed");
    return NULL;
}

engram_state_t engram_get_state(
    const engram_system_t* system,
    uint64_t engram_id) {

    if (!system || engram_id == 0) return ENGRAM_STATE_DEGRADING;

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_get_state", 0.0f);


    for (uint32_t i = 0; i < system->capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->capacity > 256) {
            engram_heartbeat("engram_loop",
                             (float)(i + 1) / (float)system->capacity);
        }

        if (system->engrams[i].active &&
            system->engrams[i].engram_id == engram_id) {
            return system->engrams[i].state;
        }
    }

    return ENGRAM_STATE_DEGRADING;
}

float engram_get_consolidation_strength(
    const engram_system_t* system,
    uint64_t engram_id) {

    if (!system || engram_id == 0) return 0.0F;

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_get_consolidation_st", 0.0f);


    for (uint32_t i = 0; i < system->capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->capacity > 256) {
            engram_heartbeat("engram_loop",
                             (float)(i + 1) / (float)system->capacity);
        }

        if (system->engrams[i].active &&
            system->engrams[i].engram_id == engram_id) {
            return system->engrams[i].consolidation_strength;
        }
    }

    return 0.0F;
}

bool engram_is_reconsolidating(
    const engram_system_t* system,
    uint64_t engram_id) {

    if (!system || engram_id == 0) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_is_reconsolidating", 0.0f);


    for (uint32_t i = 0; i < system->capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->capacity > 256) {
            engram_heartbeat("engram_loop",
                             (float)(i + 1) / (float)system->capacity);
        }

        if (system->engrams[i].active &&
            system->engrams[i].engram_id == engram_id) {
            return system->engrams[i].is_reconsolidating;
        }
    }

    return false;
}

float engram_get_age_seconds(
    const engram_system_t* system,
    uint64_t engram_id,
    uint64_t current_time_us) {

    if (!system || engram_id == 0) return 0.0F;

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_get_age_seconds", 0.0f);


    for (uint32_t i = 0; i < system->capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->capacity > 256) {
            engram_heartbeat("engram_loop",
                             (float)(i + 1) / (float)system->capacity);
        }

        if (system->engrams[i].active &&
            system->engrams[i].engram_id == engram_id) {
            uint64_t age_us = current_time_us - system->engrams[i].encoding_time_us;
            return (float)age_us / 1000000.0F;
        }
    }

    return 0.0F;
}

uint32_t engram_get_active_count(const engram_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_get_active_count", 0.0f);


    return system ? system->active_count : 0;
}

void engram_system_set_max_active(engram_system_t* system, uint32_t cap) {
    if (!system) return;
    system->max_active_engrams = cap;
}

uint64_t engram_get_total_evictions(const engram_system_t* system) {
    return system ? system->total_evictions : 0;
}

void engram_get_statistics(
    const engram_system_t* system,
    uint64_t* total_encodings_out,
    uint64_t* total_recalls_out,
    uint32_t* active_count_out) {

    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_get_statistics", 0.0f);


    if (total_encodings_out) {
        *total_encodings_out = system->total_encodings;
    }
    if (total_recalls_out) {
        *total_recalls_out = system->total_recalls;
    }
    if (active_count_out) {
        *active_count_out = system->active_count;
    }
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query self-knowledge from knowledge graph
 * WHAT: Retrieve module's self-awareness information from KG
 * WHY:  Enable introspection about module capabilities and connections
 * HOW:  Query KG reader for entity and relations
 */
int engram_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    engram_heartbeat("engram_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Engram_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                engram_heartbeat("engram_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Engram self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Engram_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Engram_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void engram_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_engram_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int engram_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "engram_training_begin: NULL argument");
        return -1;
    }
    engram_heartbeat_instance(NULL, "engram_training_begin", 0.0f);
    return 0;
}

int engram_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "engram_training_end: NULL argument");
        return -1;
    }
    engram_heartbeat_instance(NULL, "engram_training_end", 1.0f);
    return 0;
}

int engram_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "engram_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    engram_heartbeat_instance(NULL, "engram_training_step", progress);
    return 0;
}
