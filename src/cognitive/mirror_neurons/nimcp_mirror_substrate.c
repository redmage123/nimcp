//=============================================================================
// nimcp_mirror_substrate.c - Mirror Neuron Substrate Integration
//=============================================================================
/**
 * @file nimcp_mirror_substrate.c
 * @brief Implementation of substrate-level integration for mirror neurons
 *
 * WHAT: Bridges cognitive mirror neurons with biological substrate
 * WHY:  Enable biologically-realistic mirror neuron behavior
 * HOW:  Integrates axon, dendrite, myelin sheath, and glial systems
 *
 * NIMCP STANDARDS:
 * - Functions < 50 lines
 * - Guard clauses for NULL checks
 * - WHAT/WHY/HOW documentation
 * - nimcp_malloc/nimcp_free for memory
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#include "cognitive/mirror_neurons/nimcp_mirror_substrate.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "core/axon/nimcp_axon.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(mirror_substrate, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Logging Macros
//=============================================================================

#define SUBSTRATE_LOG_ERROR(...) NIMCP_LOGGING_ERROR(__VA_ARGS__)
#define SUBSTRATE_LOG_WARN(...)  NIMCP_LOGGING_WARN(__VA_ARGS__)
#define SUBSTRATE_LOG_INFO(...)  NIMCP_LOGGING_INFO(__VA_ARGS__)
#define SUBSTRATE_LOG_DEBUG(...) NIMCP_LOGGING_DEBUG(__VA_ARGS__)

//=============================================================================
// Internal Constants
//=============================================================================

/** Activity EMA decay factor per second */
static const float ACTIVITY_DECAY_PER_SEC = 0.9F;

/** Coactivation time window (microseconds) */
static const uint64_t COACTIVATION_WINDOW_US = 100000;  /* 100ms */

/** Spine weight change rate per second */
static const float SPINE_WEIGHT_DELTA_PER_SEC = 0.1F;

/** Spine state transition thresholds */
static const float SPINE_THIN_TO_STUBBY_THRESHOLD = 0.4F;
static const float SPINE_STUBBY_TO_MUSHROOM_THRESHOLD = 0.7F;
static const float SPINE_PRUNE_ACTIVITY_THRESHOLD = 0.05F;

/** Microglia surveillance decay rate */
static const float SURVEILLANCE_DECAY_PER_SEC = 0.95F;

/** Astrocyte modulation update rate */
static const float ASTROCYTE_MOD_RATE = 0.1F;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Find first set bit in 64-bit word
 */
static inline int find_first_set_bit64(uint64_t word)
{
    if (word == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_first_set_bit64: word is zero");
        return -1;
    }
#ifdef __GNUC__
    return __builtin_ctzll(word);
#else
    int pos = 0;
    while ((word & 1) == 0) {
        word >>= 1;
        pos++;
    }
    return pos;
#endif
}

/**
 * @brief Calculate myelination speedup factor
 *
 * WHAT: Compute conduction speedup from myelination level
 * WHY:  Higher myelination = faster conduction
 * HOW:  Linear interpolation from base to max velocity
 */
static float calculate_myelin_speedup(float myelination_level)
{
    /* Guard: Clamp to valid range */
    float level = nimcp_clampf(myelination_level, 0.0F, 1.0F);

    /* Linear speedup: 1x at 0, NIMCP_MIRROR_MYELIN_SPEEDUP_MAX at 1.0 */
    return 1.0F + (NIMCP_MIRROR_MYELIN_SPEEDUP_MAX - 1.0F) * level;
}

/**
 * @brief Check if times are within coactivation window
 */
static bool is_coactivated(uint64_t time1, uint64_t time2)
{
    if (time1 == 0 || time2 == 0) {
        return false;
    }
    uint64_t diff = (time1 > time2) ? (time1 - time2) : (time2 - time1);
    return diff <= COACTIVATION_WINDOW_US;
}

//=============================================================================
// Pool Management Implementation
//=============================================================================

mirror_substrate_pool_t* mirror_substrate_pool_create(uint32_t capacity)
{
    /* Guard: Validate capacity */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_pool_create", 0.0f);


    if (capacity == 0) {
        SUBSTRATE_LOG_ERROR("Mirror substrate pool: capacity cannot be zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_substrate_pool_create: capacity is zero");
        return NULL;
    }

    /* Round capacity up to multiple of 64 for bitmap efficiency */
    uint32_t aligned_capacity = ((capacity + 63) / 64) * 64;
    uint32_t num_words = aligned_capacity / 64;

    /* Allocate pool structure */
    mirror_substrate_pool_t* pool = (mirror_substrate_pool_t*)nimcp_calloc(
        1, sizeof(mirror_substrate_pool_t));
    if (!pool) {
        SUBSTRATE_LOG_ERROR("Mirror substrate pool: failed to allocate pool structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_substrate_pool_create: pool is NULL");
        return NULL;
    }

    /* Allocate backing buffer */
    pool->buffer = (mirror_substrate_backing_t*)nimcp_calloc(
        aligned_capacity, sizeof(mirror_substrate_backing_t));
    if (!pool->buffer) {
        SUBSTRATE_LOG_ERROR("Mirror substrate pool: failed to allocate buffer");
        nimcp_free(pool);
        pool = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_substrate_pool_create: pool->buffer is NULL");
        return NULL;
    }

    /* Allocate bitmap (1 = free) */
    pool->bitmap = (uint64_t*)nimcp_calloc(num_words, sizeof(uint64_t));
    if (!pool->bitmap) {
        SUBSTRATE_LOG_ERROR("Mirror substrate pool: failed to allocate bitmap");
        nimcp_free(pool->buffer);
        nimcp_free(pool);
        pool = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_substrate_pool_create: pool->bitmap is NULL");
        return NULL;
    }

    /* Initialize bitmap - all slots free (all 1s) */
    memset(pool->bitmap, 0xFF, num_words * sizeof(uint64_t));

    /* Handle partial last word if original capacity wasn't aligned */
    if (capacity < aligned_capacity) {
        uint32_t extra_bits = aligned_capacity - capacity;
        uint64_t mask = ((uint64_t)1 << (64 - extra_bits)) - 1;
        pool->bitmap[num_words - 1] &= mask;
    }

    pool->capacity = aligned_capacity;
    pool->num_bitmap_words = num_words;
    pool->allocated_count = 0;
    pool->next_search_pos = 0;
    pool->high_water_mark = 0;

    nimcp_spinlock_init(&pool->lock);

    SUBSTRATE_LOG_INFO("Mirror substrate pool: created with capacity %u", aligned_capacity);
    return pool;
}

void mirror_substrate_pool_destroy(mirror_substrate_pool_t* pool)
{
    if (!pool) return;

    /* Free buffer and bitmap */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_pool_destroy", 0.0f);


    if (pool->buffer) nimcp_free(pool->buffer);
    if (pool->bitmap) nimcp_free(pool->bitmap);

    nimcp_free(pool);
    pool = NULL;
}

mirror_substrate_backing_t* mirror_substrate_pool_alloc(mirror_substrate_pool_t* pool)
{
    if (!pool) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_pool_alloc", 0.0f);


    nimcp_spinlock_lock(&pool->lock);

    /* Check if pool is full */
    if (pool->allocated_count >= pool->capacity) {
        nimcp_spinlock_unlock(&pool->lock);
        SUBSTRATE_LOG_WARN("Mirror substrate pool: pool exhausted");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_substrate_pool_alloc: capacity exceeded");
        return NULL;
    }

    /* Search for free slot starting from next_search_pos */
    uint32_t start_word = pool->next_search_pos / 64;
    uint32_t found_slot = UINT32_MAX;

    for (uint32_t i = 0; i < pool->num_bitmap_words; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_bitmap_words > 256) {
            mirror_substrate_heartbeat("mirror_subst_loop",
                             (float)(i + 1) / (float)pool->num_bitmap_words);
        }

        uint32_t word_idx = (start_word + i) % pool->num_bitmap_words;

        if (pool->bitmap[word_idx] != 0) {
            /* Found word with free slot */
            int bit_pos = find_first_set_bit64(pool->bitmap[word_idx]);
            if (bit_pos >= 0) {
                found_slot = word_idx * 64 + bit_pos;
                /* Mark as allocated (clear bit) */
                pool->bitmap[word_idx] &= ~((uint64_t)1 << bit_pos);
                break;
            }
        }
    }

    if (found_slot == UINT32_MAX || found_slot >= pool->capacity) {
        nimcp_spinlock_unlock(&pool->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mirror_substrate_pool_alloc: capacity exceeded");
        return NULL;
    }

    /* Update statistics */
    pool->allocated_count++;
    if (pool->allocated_count > pool->high_water_mark) {
        pool->high_water_mark = pool->allocated_count;
    }
    pool->next_search_pos = found_slot + 1;

    nimcp_spinlock_unlock(&pool->lock);

    /* Zero-initialize the backing */
    mirror_substrate_backing_t* backing = &pool->buffer[found_slot];
    memset(backing, 0, sizeof(mirror_substrate_backing_t));
    backing->substrate_id = found_slot;

    return backing;
}

void mirror_substrate_pool_free(mirror_substrate_pool_t* pool,
                                 mirror_substrate_backing_t* backing)
{
    if (!pool || !backing) return;

    /* Calculate slot index */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_pool_free", 0.0f);


    ptrdiff_t offset = backing - pool->buffer;
    if (offset < 0 || (uint32_t)offset >= pool->capacity) {
        SUBSTRATE_LOG_ERROR("Mirror substrate pool: invalid backing pointer");
        return;
    }

    uint32_t slot = (uint32_t)offset;
    uint32_t word_idx = slot / 64;
    uint32_t bit_pos = slot % 64;

    nimcp_spinlock_lock(&pool->lock);

    /* Check if already free */
    if (pool->bitmap[word_idx] & ((uint64_t)1 << bit_pos)) {
        nimcp_spinlock_unlock(&pool->lock);
        SUBSTRATE_LOG_WARN("Mirror substrate pool: double free detected");
        return;
    }

    /* Mark as free (set bit) */
    pool->bitmap[word_idx] |= ((uint64_t)1 << bit_pos);
    pool->allocated_count--;

    /* Update search position for locality */
    if (slot < pool->next_search_pos) {
        pool->next_search_pos = slot;
    }

    nimcp_spinlock_unlock(&pool->lock);

    /* Clear the backing data */
    memset(backing, 0, sizeof(mirror_substrate_backing_t));
}

void mirror_substrate_pool_stats(const mirror_substrate_pool_t* pool,
                                  uint32_t* allocated,
                                  uint32_t* capacity,
                                  uint32_t* high_water)
{
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_pool_stats: pool is NULL");
        if (allocated) *allocated = 0;
        if (capacity) *capacity = 0;
        if (high_water) *high_water = 0;
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_pool_stats", 0.0f);


    if (allocated) *allocated = pool->allocated_count;
    if (capacity) *capacity = pool->capacity;
    if (high_water) *high_water = pool->high_water_mark;
}

//=============================================================================
// Substrate Backing Lifecycle Implementation
//=============================================================================

mirror_substrate_backing_t* mirror_substrate_backing_create(
    uint32_t mirror_unit_id,
    const mirror_substrate_config_t* config,
    mirror_substrate_pool_t* pool)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_backing_create", 0.0f);


    mirror_substrate_backing_t* backing = NULL;

    /* Allocate from pool or heap */
    if (pool && config && config->enable_memory_pool) {
        backing = mirror_substrate_pool_alloc(pool);
    } else {
        backing = (mirror_substrate_backing_t*)nimcp_calloc(
            1, sizeof(mirror_substrate_backing_t));
    }

    if (!backing) {
        SUBSTRATE_LOG_ERROR("Mirror substrate: failed to allocate backing for unit %u",
                           mirror_unit_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_substrate_backing_create: backing is NULL");
        return NULL;
    }

    /* Initialize with config */
    mirror_substrate_backing_init(backing, config);
    backing->mirror_unit_id = mirror_unit_id;

    /* Initialize CoW */
    backing->cow_ref_count = 1;
    backing->cow_modified = false;
    backing->cow_original = NULL;

    /* Initialize spinlock */
    nimcp_spinlock_init(&backing->lock);

    return backing;
}

void mirror_substrate_backing_destroy(mirror_substrate_backing_t* backing,
                                       mirror_substrate_pool_t* pool)
{
    if (!backing) return;

    /* Handle CoW: decrement ref count */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_backing_destroy", 0.0f);


    if (backing->cow_ref_count > 1) {
        backing->cow_ref_count--;
        return;
    }

    /* Return to pool or free to heap */
    if (pool) {
        mirror_substrate_pool_free(pool, backing);
    } else {
        nimcp_free(backing);
        backing = NULL;
    }
}

void mirror_substrate_backing_init(mirror_substrate_backing_t* backing,
                                    const mirror_substrate_config_t* config)
{
    if (!backing) return;

    /* Zero-init structure */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_backing_init", 0.0f);


    memset(backing, 0, sizeof(mirror_substrate_backing_t));

    /* Set biological defaults */
    backing->myelination_level = 0.0F;  /* Start unmyelinated */
    backing->conduction_velocity_ms = NIMCP_MIRROR_BASE_DELAY_MS;

    backing->astrocyte_modulation = 1.0F;  /* Neutral modulation */
    backing->lactate_received = 0.0F;
    backing->surveillance_score = 0.5F;  /* Neutral surveillance */
    backing->marked_for_pruning = false;

    /* Set delays based on config */
    if (config) {
        backing->observation_delay_ms = config->base_recognition_delay_ms;
        backing->execution_delay_ms = config->base_recognition_delay_ms;
        backing->brain_region_id = config->primary_region_id;
    } else {
        backing->observation_delay_ms = NIMCP_MIRROR_BASE_DELAY_MS;
        backing->execution_delay_ms = NIMCP_MIRROR_BASE_DELAY_MS;
        backing->brain_region_id = NIMCP_MIRROR_REGION_F5_PREMOTOR;
    }

    /* CoW defaults */
    backing->cow_ref_count = 1;
    backing->cow_modified = false;
    backing->cow_original = NULL;
}

//=============================================================================
// Copy-on-Write Implementation
//=============================================================================

mirror_substrate_backing_t* mirror_substrate_cow_copy(
    mirror_substrate_backing_t* backing,
    mirror_substrate_pool_t* pool)
{
    if (!backing) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "backing is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_cow_copy", 0.0f);


    nimcp_spinlock_lock(&backing->lock);

    /* Increment reference count on original */
    backing->cow_ref_count++;

    nimcp_spinlock_unlock(&backing->lock);

    /* Allocate new backing structure */
    mirror_substrate_backing_t* copy = NULL;
    if (pool) {
        copy = mirror_substrate_pool_alloc(pool);
    } else {
        copy = (mirror_substrate_backing_t*)nimcp_calloc(
            1, sizeof(mirror_substrate_backing_t));
    }

    if (!copy) {
        /* Revert ref count increment */
        nimcp_spinlock_lock(&backing->lock);
        backing->cow_ref_count--;
        nimcp_spinlock_unlock(&backing->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_substrate_cow_copy: copy is NULL");
        return NULL;
    }

    /* Shallow copy all fields */
    memcpy(copy, backing, sizeof(mirror_substrate_backing_t));

    /* Set CoW metadata */
    copy->cow_ref_count = 1;
    copy->cow_modified = false;
    copy->cow_original = backing;

    /* Re-initialize spinlock for copy */
    nimcp_spinlock_init(&copy->lock);

    return copy;
}

nimcp_result_t mirror_substrate_cow_prepare_write(mirror_substrate_backing_t* backing)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_cow_prepare_write", 0.0f);


    NIMCP_CHECK_THROW(backing, NIMCP_ERROR_INVALID_PARAM, "backing is NULL");

    nimcp_spinlock_lock(&backing->lock);

    /* If sole owner or already modified, ready to write */
    if (backing->cow_ref_count <= 1 || backing->cow_modified) {
        backing->cow_modified = true;
        nimcp_spinlock_unlock(&backing->lock);
        return NIMCP_SUCCESS;
    }

    nimcp_spinlock_unlock(&backing->lock);

    /* Need to make deep copy - for now, just mark as modified */
    /* Note: Deep copy of substrate references (axon, dendrite, etc.) */
    /* would require cloning those structures, which is complex */
    /* For now, we just detach from sharing by marking modified */

    nimcp_spinlock_lock(&backing->lock);
    backing->cow_modified = true;

    /* Decrement ref count on original if we have one */
    if (backing->cow_original) {
        mirror_substrate_backing_t* orig = backing->cow_original;
        nimcp_spinlock_lock(&orig->lock);
        if (orig->cow_ref_count > 0) {
            orig->cow_ref_count--;
        }
        nimcp_spinlock_unlock(&orig->lock);
        backing->cow_original = NULL;
    }

    nimcp_spinlock_unlock(&backing->lock);

    return NIMCP_SUCCESS;
}

void mirror_substrate_cow_release(mirror_substrate_backing_t* backing,
                                   mirror_substrate_pool_t* pool)
{
    if (!backing) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_cow_release", 0.0f);


    nimcp_spinlock_lock(&backing->lock);

    if (backing->cow_ref_count > 1) {
        backing->cow_ref_count--;
        nimcp_spinlock_unlock(&backing->lock);
        return;
    }

    /* Last reference - also release original if we have one */
    mirror_substrate_backing_t* orig = backing->cow_original;

    nimcp_spinlock_unlock(&backing->lock);

    /* Release original's ref count */
    if (orig) {
        mirror_substrate_cow_release(orig, pool);
    }

    /* Destroy backing */
    mirror_substrate_backing_destroy(backing, pool);
}

bool mirror_substrate_is_cow_copy(const mirror_substrate_backing_t* backing)
{
    if (!backing) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_is_cow_copy", 0.0f);


    return backing->cow_original != NULL;
}

//=============================================================================
// Axon Integration Implementation
//=============================================================================

nimcp_result_t mirror_substrate_bind_observation_axon(
    mirror_substrate_backing_t* backing,
    void* axon_ptr)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_bind_observation_axo", 0.0f);


    NIMCP_CHECK_THROW(backing, NIMCP_ERROR_INVALID_PARAM, "backing is NULL");

    backing->observation_axon = axon_ptr;
    axon_t* axon = (axon_t*)axon_ptr;

    if (axon) {
        backing->observation_axon_id = axon->id;
        /* Get propagation delay from axon */
        backing->observation_delay_ms = axon_get_propagation_delay(axon);
    } else {
        backing->observation_axon_id = 0;
        backing->observation_delay_ms = NIMCP_MIRROR_BASE_DELAY_MS;
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t mirror_substrate_bind_execution_axon(
    mirror_substrate_backing_t* backing,
    void* axon_ptr)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_bind_execution_axon", 0.0f);


    NIMCP_CHECK_THROW(backing, NIMCP_ERROR_INVALID_PARAM, "backing is NULL");

    backing->execution_axon = axon_ptr;
    axon_t* axon = (axon_t*)axon_ptr;

    if (axon) {
        backing->execution_axon_id = axon->id;
        backing->execution_delay_ms = axon_get_propagation_delay(axon);
    } else {
        backing->execution_axon_id = 0;
        backing->execution_delay_ms = NIMCP_MIRROR_BASE_DELAY_MS;
    }

    return NIMCP_SUCCESS;
}

float mirror_substrate_get_observation_delay(const mirror_substrate_backing_t* backing)
{
    if (!backing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_get_observation_delay: backing is NULL");
        return NIMCP_MIRROR_BASE_DELAY_MS;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_get_observation_dela", 0.0f);


    float base_delay = backing->observation_delay_ms;

    /* Apply myelination speedup */
    float speedup = calculate_myelin_speedup(backing->myelination_level);
    float adjusted_delay = base_delay / speedup;

    /* Add dendritic integration time */
    adjusted_delay += NIMCP_MIRROR_DENDRITE_INTEGRATION_MS;

    return adjusted_delay;
}

float mirror_substrate_get_execution_delay(const mirror_substrate_backing_t* backing)
{
    if (!backing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_get_execution_delay: backing is NULL");
        return NIMCP_MIRROR_BASE_DELAY_MS;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_get_execution_delay", 0.0f);


    float base_delay = backing->execution_delay_ms;

    /* Apply myelination speedup */
    float speedup = calculate_myelin_speedup(backing->myelination_level);

    return base_delay / speedup;
}

//=============================================================================
// Myelin Sheath Integration Implementation
//=============================================================================

nimcp_result_t mirror_substrate_bind_myelin_sheath(
    mirror_substrate_backing_t* backing,
    void* sheath_ptr)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_bind_myelin_sheath", 0.0f);


    NIMCP_CHECK_THROW(backing, NIMCP_ERROR_INVALID_PARAM, "backing is NULL");

    backing->myelin_sheath = sheath_ptr;
    myelin_sheath_t* sheath = (myelin_sheath_t*)sheath_ptr;

    if (sheath) {
        backing->myelin_sheath_id = sheath->id;
        /* Sync myelination level from sheath */
        backing->myelination_level = sheath->coverage_fraction;
        backing->conduction_velocity_ms = myelin_sheath_get_velocity_ratio(sheath);
    } else {
        backing->myelin_sheath_id = 0;
        backing->myelination_level = 0.0F;
        backing->conduction_velocity_ms = NIMCP_MYELIN_BASE_VELOCITY_MS;
    }

    return NIMCP_SUCCESS;
}

float mirror_substrate_update_myelination(mirror_substrate_backing_t* backing)
{
    if (!backing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_update_myelination: backing is NULL");
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_update_myelination", 0.0f);


    if (backing->myelin_sheath) {
        myelin_sheath_t* sheath = (myelin_sheath_t*)backing->myelin_sheath;
        backing->myelination_level = sheath->coverage_fraction;
        backing->conduction_velocity_ms = myelin_sheath_get_velocity_ratio(sheath);
    }

    return backing->myelination_level;
}

void mirror_substrate_apply_activity_to_myelin(
    mirror_substrate_backing_t* backing,
    float activity_level,
    float dt_seconds)
{
    if (!backing || !backing->myelin_sheath) {
        if (!backing) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_apply_activity_to_myelin: backing is NULL");
            return;
        }
        return;
    }

    /* Forward activity to myelin sheath for activity-dependent plasticity */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_apply_activity_to_my", 0.0f);


    myelin_sheath_t* sheath = (myelin_sheath_t*)backing->myelin_sheath;

    /* High activity promotes myelination */
    if (activity_level > 0.5F) {
        myelin_sheath_myelinate(sheath, activity_level * 0.1F, dt_seconds);
    }

    /* Update cached values */
    mirror_substrate_update_myelination(backing);
}

//=============================================================================
// Dendrite Integration Implementation
//=============================================================================

nimcp_result_t mirror_substrate_bind_dendrite(
    mirror_substrate_backing_t* backing,
    void* dendrite_ptr)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_bind_dendrite", 0.0f);


    NIMCP_CHECK_THROW(backing, NIMCP_ERROR_INVALID_PARAM, "backing is NULL");

    backing->dendrite = dendrite_ptr;
    dendrite_t* dendrite = (dendrite_t*)dendrite_ptr;

    if (dendrite) {
        backing->dendrite_id = dendrite->id;
    } else {
        backing->dendrite_id = 0;
    }

    return NIMCP_SUCCESS;
}

int32_t mirror_substrate_add_spine(
    mirror_substrate_backing_t* backing,
    void* spine_ptr,
    float initial_weight)
{
    if (!backing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_add_spine: backing is NULL");
        return -1;
    }

    /* Check capacity */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_add_spine", 0.0f);


    if (backing->num_spines >= NIMCP_MIRROR_SUBSTRATE_MAX_SPINES) {
        SUBSTRATE_LOG_WARN("Mirror substrate: max spines reached for unit %u",
                          backing->mirror_unit_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mirror_substrate_add_spine: capacity exceeded");
        return -1;
    }

    /* Add spine at next available slot */
    uint32_t idx = backing->num_spines;
    dendritic_spine_t* spine = (dendritic_spine_t*)spine_ptr;

    backing->spines[idx] = spine_ptr;
    backing->spine_ids[idx] = spine ? spine->id : 0;
    backing->spine_states[idx] = MIRROR_SPINE_STATE_THIN;  /* Start as learning spine */
    backing->spine_weights[idx] = nimcp_clampf(initial_weight, 0.0F, 1.0F);

    backing->num_spines++;

    return (int32_t)idx;
}

void mirror_substrate_update_spine_plasticity(
    mirror_substrate_backing_t* backing,
    bool observation_active,
    bool execution_active,
    float dt_seconds)
{
    if (!backing || backing->num_spines == 0) {
        if (!backing) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_update_spine_plasticity: backing is NULL");
            return;
        }
        return;
    }

    /* Get astrocyte modulation factor */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_update_spine_plastic", 0.0f);


    float mod_factor = backing->astrocyte_modulation;

    /* Update each spine */
    for (uint32_t i = 0; i < backing->num_spines; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && backing->num_spines > 256) {
            mirror_substrate_heartbeat("mirror_subst_loop",
                             (float)(i + 1) / (float)backing->num_spines);
        }

        /* Skip pruned spines */
        if (backing->spine_states[i] == MIRROR_SPINE_STATE_PRUNED) continue;

        float weight = backing->spine_weights[i];
        float delta = 0.0F;

        /* Hebbian-like plasticity: coactivation strengthens */
        if (observation_active && execution_active) {
            /* LTP: strengthen association */
            delta = SPINE_WEIGHT_DELTA_PER_SEC * dt_seconds * mod_factor;
        } else if (!observation_active && !execution_active) {
            /* Weak LTD: slow decay when inactive */
            delta = -SPINE_WEIGHT_DELTA_PER_SEC * 0.1F * dt_seconds;
        } else {
            /* Single pathway active: slight weakening */
            delta = -SPINE_WEIGHT_DELTA_PER_SEC * 0.05F * dt_seconds;
        }

        /* Update weight */
        weight = nimcp_clampf(weight + delta, 0.0F, 1.0F);
        backing->spine_weights[i] = weight;

        /* Update spine state based on weight */
        mirror_spine_state_t new_state = backing->spine_states[i];

        if (weight >= SPINE_STUBBY_TO_MUSHROOM_THRESHOLD) {
            new_state = MIRROR_SPINE_STATE_MUSHROOM;
        } else if (weight >= SPINE_THIN_TO_STUBBY_THRESHOLD) {
            new_state = MIRROR_SPINE_STATE_STUBBY;
        } else if (weight > SPINE_PRUNE_ACTIVITY_THRESHOLD) {
            new_state = MIRROR_SPINE_STATE_THIN;
        } else {
            /* Mark for pruning if weight very low */
            new_state = MIRROR_SPINE_STATE_PRUNED;
        }

        backing->spine_states[i] = new_state;
    }
}

float mirror_substrate_get_total_spine_weight(const mirror_substrate_backing_t* backing)
{
    if (!backing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_get_total_spine_weight: backing is NULL");
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_get_total_spine_weig", 0.0f);


    float total = 0.0F;
    for (uint32_t i = 0; i < backing->num_spines; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && backing->num_spines > 256) {
            mirror_substrate_heartbeat("mirror_subst_loop",
                             (float)(i + 1) / (float)backing->num_spines);
        }

        if (backing->spine_states[i] != MIRROR_SPINE_STATE_PRUNED) {
            total += backing->spine_weights[i];
        }
    }

    return total;
}

//=============================================================================
// Glial Cell Integration Implementation
//=============================================================================

nimcp_result_t mirror_substrate_bind_astrocyte(
    mirror_substrate_backing_t* backing,
    void* astrocyte)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_bind_astrocyte", 0.0f);


    NIMCP_CHECK_THROW(backing, NIMCP_ERROR_INVALID_PARAM, "backing is NULL");

    backing->astrocyte = astrocyte;
    backing->astrocyte_id = astrocyte ? 1 : 0;  /* Simplified ID */
    backing->astrocyte_modulation = 1.0F;  /* Neutral to start */

    return NIMCP_SUCCESS;
}

nimcp_result_t mirror_substrate_bind_oligodendrocyte(
    mirror_substrate_backing_t* backing,
    void* oligo)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_bind_oligodendrocyte", 0.0f);


    NIMCP_CHECK_THROW(backing, NIMCP_ERROR_INVALID_PARAM, "backing is NULL");

    backing->oligodendrocyte = oligo;
    backing->oligodendrocyte_id = oligo ? 1 : 0;
    backing->lactate_received = 0.0F;

    return NIMCP_SUCCESS;
}

nimcp_result_t mirror_substrate_bind_microglia(
    mirror_substrate_backing_t* backing,
    void* microglia)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_bind_microglia", 0.0f);


    NIMCP_CHECK_THROW(backing, NIMCP_ERROR_INVALID_PARAM, "backing is NULL");

    backing->microglia = microglia;
    backing->microglia_id = microglia ? 1 : 0;
    backing->surveillance_score = 0.5F;  /* Neutral surveillance */
    backing->marked_for_pruning = false;

    return NIMCP_SUCCESS;
}

float mirror_substrate_get_astrocyte_modulation(const mirror_substrate_backing_t* backing)
{
    if (!backing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_get_astrocyte_modulation: backing is NULL");
        return 1.0F;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_get_astrocyte_modula", 0.0f);


    return backing->astrocyte_modulation;
}

void mirror_substrate_update_glial(
    mirror_substrate_backing_t* backing,
    float activity_level,
    float dt_seconds)
{
    if (!backing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_update_glial: backing is NULL");
        return;
    }

    /* Update astrocyte modulation based on activity */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_update_glial", 0.0f);


    if (backing->astrocyte_id > 0) {
        /* Higher activity increases Ca2+ which boosts modulation */
        float target_mod = NIMCP_MIRROR_ASTROCYTE_MOD_MIN +
            (NIMCP_MIRROR_ASTROCYTE_MOD_MAX - NIMCP_MIRROR_ASTROCYTE_MOD_MIN) * activity_level;

        /* Smooth transition to target */
        backing->astrocyte_modulation += (target_mod - backing->astrocyte_modulation) *
            ASTROCYTE_MOD_RATE * dt_seconds;
        backing->astrocyte_modulation = nimcp_clampf(backing->astrocyte_modulation,
            NIMCP_MIRROR_ASTROCYTE_MOD_MIN, NIMCP_MIRROR_ASTROCYTE_MOD_MAX);
    }

    /* Update oligodendrocyte lactate support */
    if (backing->oligodendrocyte_id > 0) {
        /* Lactate provision proportional to activity */
        float lactate_rate = 0.1F * activity_level;
        backing->lactate_received += lactate_rate * dt_seconds;

        /* Decay over time */
        backing->lactate_received *= powf(0.99F, dt_seconds);
        backing->lactate_received = nimcp_clampf(backing->lactate_received, 0.0F, 1.0F);
    }

    /* Update microglia surveillance */
    if (backing->microglia_id > 0) {
        /* Activity keeps surveillance score up */
        backing->surveillance_score = backing->surveillance_score *
            powf(SURVEILLANCE_DECAY_PER_SEC, dt_seconds);
        backing->surveillance_score += activity_level * dt_seconds * 0.1F;
        backing->surveillance_score = nimcp_clampf(backing->surveillance_score, 0.0F, 1.0F);

        /* Mark for pruning if surveillance score drops too low */
        backing->marked_for_pruning = (backing->surveillance_score <
            NIMCP_MIRROR_MICROGLIA_PRUNE_THRESHOLD);
    }
}

bool mirror_substrate_is_marked_for_pruning(const mirror_substrate_backing_t* backing)
{
    if (!backing) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_is_marked_for_prunin", 0.0f);


    return backing->marked_for_pruning;
}

//=============================================================================
// Simulation Step Implementation
//=============================================================================

void mirror_substrate_step(
    mirror_substrate_backing_t* backing,
    uint64_t current_time,
    float dt_seconds)
{
    if (!backing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_step: backing is NULL");
        return;
    }

    /* Calculate combined activity level */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_step", 0.0f);


    float activity = (backing->observation_activity_ema + backing->execution_activity_ema) * 0.5F;

    /* Decay activity EMAs */
    float decay = powf(ACTIVITY_DECAY_PER_SEC, dt_seconds);
    backing->observation_activity_ema *= decay;
    backing->execution_activity_ema *= decay;

    /* Update myelination if sheath bound */
    if (backing->myelin_sheath) {
        mirror_substrate_apply_activity_to_myelin(backing, activity, dt_seconds);
    }

    /* Update spine plasticity */
    bool obs_active = backing->observation_activity_ema > 0.1F;
    bool exec_active = backing->execution_activity_ema > 0.1F;
    mirror_substrate_update_spine_plasticity(backing, obs_active, exec_active, dt_seconds);

    /* Update glial cells */
    mirror_substrate_update_glial(backing, activity, dt_seconds);

    /* Update coactivation score */
    if (is_coactivated(backing->last_observation_time, backing->last_execution_time)) {
        backing->coactivation_score = nimcp_clampf(
            backing->coactivation_score + 0.1F * dt_seconds, 0.0F, 1.0F);
        backing->last_coactivation_time = current_time;
    } else {
        backing->coactivation_score *= decay;
    }
}

void mirror_substrate_record_observation(
    mirror_substrate_backing_t* backing,
    float strength,
    uint64_t timestamp)
{
    if (!backing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_record_observation: backing is NULL");
        return;
    }

    /* Update EMA */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_record_observation", 0.0f);


    backing->observation_activity_ema = nimcp_clampf(
        backing->observation_activity_ema + strength, 0.0F, 1.0F);

    backing->last_observation_time = timestamp;
}

void mirror_substrate_record_execution(
    mirror_substrate_backing_t* backing,
    float strength,
    uint64_t timestamp)
{
    if (!backing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_substrate_record_execution: backing is NULL");
        return;
    }

    /* Update EMA */
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_record_execution", 0.0f);


    backing->execution_activity_ema = nimcp_clampf(
        backing->execution_activity_ema + strength, 0.0F, 1.0F);

    backing->last_execution_time = timestamp;
}

//=============================================================================
// Statistics Implementation
//=============================================================================

nimcp_result_t mirror_substrate_get_stats(
    mirror_neurons_t mirror,
    mirror_substrate_stats_t* stats)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_get_stats", 0.0f);


    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    /* Zero initialize */
    memset(stats, 0, sizeof(mirror_substrate_stats_t));

    /* Note: Full implementation would iterate through mirror neuron system */
    /* and aggregate stats from all substrate backings. */
    /* This requires access to internal mirror_neurons_system structure. */

    /* Set defaults for now */
    stats->avg_recognition_delay_ms = NIMCP_MIRROR_BASE_DELAY_MS;
    stats->min_recognition_delay_ms = NIMCP_MIRROR_BASE_DELAY_MS / NIMCP_MIRROR_MYELIN_SPEEDUP_MAX;
    stats->max_recognition_delay_ms = NIMCP_MIRROR_BASE_DELAY_MS;
    stats->avg_astrocyte_modulation = 1.0F;

    return NIMCP_SUCCESS;
}

mirror_substrate_config_t mirror_substrate_get_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_get_default_config", 0.0f);


    mirror_substrate_config_t config = {
        /* Mode selection */
        .mode = MIRROR_SUBSTRATE_MODE_PARTIAL,

        /* Feature flags */
        .enable_myelination = true,
        .enable_dendrites = true,
        .enable_axons = true,
        .enable_astrocytes = true,
        .enable_oligodendrocytes = true,
        .enable_microglia = true,
        .enable_memory_pool = true,
        .enable_cow = true,

        /* Timing parameters */
        .base_recognition_delay_ms = NIMCP_MIRROR_BASE_DELAY_MS,
        .myelin_speedup_factor = NIMCP_MIRROR_MYELIN_SPEEDUP_MAX,
        .dendrite_integration_time_ms = NIMCP_MIRROR_DENDRITE_INTEGRATION_MS,

        /* Plasticity parameters */
        .spine_ltp_threshold = NIMCP_MIRROR_SPINE_LTP_THRESHOLD,
        .spine_ltd_threshold = NIMCP_MIRROR_SPINE_LTD_THRESHOLD,
        .spine_maturation_rate = 0.01F,
        .pruning_activity_threshold = NIMCP_MIRROR_MICROGLIA_PRUNE_THRESHOLD,

        /* Brain region assignment */
        .primary_region_id = NIMCP_MIRROR_REGION_F5_PREMOTOR,
        .secondary_region_id = NIMCP_MIRROR_REGION_PARIETAL_IPL,

        /* Pool sizing */
        .pool_capacity = NIMCP_MIRROR_SUBSTRATE_POOL_SIZE
    };

    return config;
}

//=============================================================================
// System Integration Implementation
//=============================================================================

nimcp_result_t mirror_substrate_integrate_system(
    mirror_neurons_t mirror,
    const mirror_substrate_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_integrate_system", 0.0f);


    NIMCP_CHECK_THROW(mirror, NIMCP_ERROR_INVALID_PARAM, "mirror is NULL");

    /* Get default config if none provided */
    mirror_substrate_config_t effective_config;
    if (config) {
        effective_config = *config;
    } else {
        effective_config = mirror_substrate_get_default_config();
    }

    SUBSTRATE_LOG_INFO("Mirror substrate: integrating with mode %d", effective_config.mode);

    /* Note: Full integration requires modifying mirror_neurons_system internals */
    /* This would be done by adding substrate fields and updating the mirror neuron */
    /* implementation. The integration patterns are defined in this module. */

    return NIMCP_SUCCESS;
}

nimcp_result_t mirror_substrate_connect_glial_integration(
    mirror_neurons_t mirror,
    void* glial_integration)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_connect_glial_integr", 0.0f);


    NIMCP_CHECK_THROW(mirror, NIMCP_ERROR_INVALID_PARAM, "mirror is NULL");

    /* Wire to existing glial integration via mirror_neurons_integrate_glial */
    glial_integration_t* glial = (glial_integration_t*)glial_integration;
    bool result = mirror_neurons_integrate_glial(mirror, glial);

    if (result) {
        SUBSTRATE_LOG_INFO("Mirror substrate: connected to glial integration layer");
        return NIMCP_SUCCESS;
    }

    return NIMCP_ERROR_INVALID_PARAM;
}

nimcp_result_t mirror_substrate_connect_myelin_network(
    mirror_neurons_t mirror,
    void* myelin_network)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_connect_myelin_netwo", 0.0f);


    NIMCP_CHECK_THROW(mirror && myelin_network, NIMCP_ERROR_INVALID_PARAM, "mirror or myelin_network is NULL");

    SUBSTRATE_LOG_INFO("Mirror substrate: connected to myelin sheath network");

    /* Note: Would iterate through mirror neurons and create myelin sheaths */
    /* for active pathways based on configuration */

    return NIMCP_SUCCESS;
}

nimcp_result_t mirror_substrate_connect_axon_network(
    mirror_neurons_t mirror,
    void* axon_network)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_connect_axon_network", 0.0f);


    NIMCP_CHECK_THROW(mirror && axon_network, NIMCP_ERROR_INVALID_PARAM, "mirror or axon_network is NULL");

    SUBSTRATE_LOG_INFO("Mirror substrate: connected to axon network");

    return NIMCP_SUCCESS;
}

nimcp_result_t mirror_substrate_connect_dendrite_network(
    mirror_neurons_t mirror,
    void* dendrite_network)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_connect_dendrite_net", 0.0f);


    NIMCP_CHECK_THROW(mirror && dendrite_network, NIMCP_ERROR_INVALID_PARAM, "mirror or dendrite_network is NULL");

    SUBSTRATE_LOG_INFO("Mirror substrate: connected to dendrite network");

    return NIMCP_SUCCESS;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int mirror_substrate_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    mirror_substrate_heartbeat("mirror_subst_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mirror_Substrate");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mirror_substrate_heartbeat("mirror_subst_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            SUBSTRATE_LOG_DEBUG("Mirror substrate self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mirror_Substrate");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mirror_Substrate");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void mirror_substrate_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_substrate_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int mirror_substrate_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_substrate_training_begin: NULL argument");
        return -1;
    }
    mirror_substrate_heartbeat_instance(NULL, "mirror_substrate_training_begin", 0.0f);
    return 0;
}

int mirror_substrate_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_substrate_training_end: NULL argument");
        return -1;
    }
    mirror_substrate_heartbeat_instance(NULL, "mirror_substrate_training_end", 1.0f);
    return 0;
}

int mirror_substrate_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_substrate_training_step: NULL argument");
        return -1;
    }
    mirror_substrate_heartbeat_instance(NULL, "mirror_substrate_training_step", progress);
    return 0;
}
