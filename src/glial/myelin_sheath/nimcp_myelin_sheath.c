#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_myelin_sheath.c - Myelin Sheath Implementation
//=============================================================================
/**
 * @file nimcp_myelin_sheath.c
 * @brief Full implementation of myelin sheath module
 *
 * WHAT: Implements myelin sheath structural modeling and dynamics
 * WHY:  Separates myelin structure concerns from oligodendrocyte cell biology
 * HOW:  Models lamellae, compaction, integrity, and conduction properties
 *
 * IMPLEMENTATION DETAILS:
 * - Bitmap-based memory pools for O(1) allocation
 * - Copy-on-Write support for efficient cloning
 * - Thread-safe operations with spinlocks
 * - Biological accuracy with Hursh's/Rushton's laws
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#include "glial/myelin_sheath/nimcp_myelin_sheath.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "MYELIN"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(myelin_sheath)

//=============================================================================
// Internal Macros
//=============================================================================

/** Clamp value between min and max */
#define MYELIN_CLAMP(val, min, max) \
    ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

#include "constants/nimcp_constants.h"
/** Minimum float for division safety */
#define MYELIN_EPSILON NIMCP_EPSILON_NUMERICAL

/** Get current time in microseconds */
static inline uint64_t myelin_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Configuration Functions
//=============================================================================

myelin_network_config_t myelin_network_default_config(void)
{
    myelin_network_config_t config = {
        .max_sheaths = NIMCP_MYELIN_SHEATH_POOL_SIZE,
        .max_segments_per_sheath = 32,
        .target_g_ratio = NIMCP_MYELIN_G_RATIO_OPTIMAL,
        .myelination_threshold = 0.5F,
        .damage_threshold = NIMCP_MYELIN_INTEGRITY_DAMAGED,
        .repair_rate_multiplier = 1.0F,
        .enable_paranodes = true,
        .enable_metabolic_coupling = true,
        .enable_activity_dependence = true,
        .use_memory_pools = true,
        .enable_bio_async = false
    };
    return config;
}

//=============================================================================
// Memory Pool Functions - Sheath Pool
//=============================================================================

myelin_sheath_pool_t* myelin_sheath_pool_create(uint32_t capacity)
{
    if (capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "myelin_sheath_pool_create: capacity is zero");
        return NULL;
    }

    // Round up to 64-bit alignment
    uint32_t aligned_capacity = ((capacity + 63) / 64) * 64;

    myelin_sheath_pool_t* pool = nimcp_calloc(1, sizeof(myelin_sheath_pool_t));
    NIMCP_API_CHECK_ALLOC(pool, "myelin_sheath_pool_create: Failed to allocate pool");

    pool->capacity = aligned_capacity;
    pool->num_bitmap_words = aligned_capacity / 64;
    pool->allocated_count = 0;

    // Allocate buffer
    pool->buffer = nimcp_calloc(aligned_capacity, sizeof(myelin_sheath_t));
    if (!pool->buffer) {
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "myelin_sheath_pool_create: pool->buffer is NULL");
        return NULL;
    }

    // Allocate bitmap - all bits set to 1 (free)
    pool->bitmap = nimcp_malloc(pool->num_bitmap_words * sizeof(uint64_t));
    if (!pool->bitmap) {
        nimcp_free(pool->buffer);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "myelin_sheath_pool_create: pool->bitmap is NULL");
        return NULL;
    }
    memset(pool->bitmap, 0xFF, pool->num_bitmap_words * sizeof(uint64_t));

    nimcp_spinlock_init(&pool->lock);
    return pool;
}

void myelin_sheath_pool_destroy(myelin_sheath_pool_t* pool)
{
    if (!pool) return;

    if (pool->buffer) {
        nimcp_free(pool->buffer);
    }
    if (pool->bitmap) {
        nimcp_free(pool->bitmap);
    }
    nimcp_free(pool);
}

myelin_sheath_t* myelin_sheath_pool_alloc(myelin_sheath_pool_t* pool)
{
    if (!pool) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool is NULL");

        return NULL;

    }

    nimcp_spinlock_lock(&pool->lock);

    // Find first free slot using bitmap
    for (uint32_t i = 0; i < pool->num_bitmap_words; i++) {
        if (pool->bitmap[i] != 0) {
            // Found word with free bit
            int bit = __builtin_ctzll(pool->bitmap[i]);
            uint32_t index = i * 64 + bit;

            if (index < pool->capacity) {
                // Mark as allocated (clear bit)
                pool->bitmap[i] &= ~(1ULL << bit);
                pool->allocated_count++;

                myelin_sheath_t* sheath = &pool->buffer[index];
                memset(sheath, 0, sizeof(myelin_sheath_t));

                nimcp_spinlock_unlock(&pool->lock);
                return sheath;
            }
        }
    }

    nimcp_spinlock_unlock(&pool->lock);
    return NULL;  // Pool exhausted
}

void myelin_sheath_pool_free(myelin_sheath_pool_t* pool, myelin_sheath_t* sheath)
{
    if (!pool || !sheath) return;

    // Validate sheath is from this pool
    ptrdiff_t offset = sheath - pool->buffer;
    if (offset < 0 || (uint32_t)offset >= pool->capacity) {
        return;  // Not from this pool
    }

    nimcp_spinlock_lock(&pool->lock);

    uint32_t index = (uint32_t)offset;
    uint32_t word = index / 64;
    uint32_t bit = index % 64;

    // Mark as free (set bit)
    pool->bitmap[word] |= (1ULL << bit);
    pool->allocated_count--;

    nimcp_spinlock_unlock(&pool->lock);
}

//=============================================================================
// Memory Pool Functions - Segment Pool
//=============================================================================

myelin_segment_pool_t* myelin_segment_pool_create(uint32_t capacity)
{
    if (capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "myelin_segment_pool_create: capacity is zero");
        return NULL;
    }

    uint32_t aligned_capacity = ((capacity + 63) / 64) * 64;

    myelin_segment_pool_t* pool = nimcp_calloc(1, sizeof(myelin_segment_pool_t));
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pool is NULL");

        return NULL;
    }

    pool->capacity = aligned_capacity;
    pool->num_bitmap_words = aligned_capacity / 64;
    pool->allocated_count = 0;

    pool->buffer = nimcp_calloc(aligned_capacity, sizeof(myelin_segment_t));
    if (!pool->buffer) {
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "myelin_segment_pool_create: pool->buffer is NULL");
        return NULL;
    }

    pool->bitmap = nimcp_malloc(pool->num_bitmap_words * sizeof(uint64_t));
    if (!pool->bitmap) {
        nimcp_free(pool->buffer);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "myelin_segment_pool_create: pool->bitmap is NULL");
        return NULL;
    }
    memset(pool->bitmap, 0xFF, pool->num_bitmap_words * sizeof(uint64_t));

    nimcp_spinlock_init(&pool->lock);
    return pool;
}

void myelin_segment_pool_destroy(myelin_segment_pool_t* pool)
{
    if (!pool) return;

    if (pool->buffer) {
        nimcp_free(pool->buffer);
    }
    if (pool->bitmap) {
        nimcp_free(pool->bitmap);
    }
    nimcp_free(pool);
}

myelin_segment_t* myelin_segment_pool_alloc(myelin_segment_pool_t* pool)
{
    if (!pool) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool is NULL");

        return NULL;

    }

    nimcp_spinlock_lock(&pool->lock);

    for (uint32_t i = 0; i < pool->num_bitmap_words; i++) {
        if (pool->bitmap[i] != 0) {
            int bit = __builtin_ctzll(pool->bitmap[i]);
            uint32_t index = i * 64 + bit;

            if (index < pool->capacity) {
                pool->bitmap[i] &= ~(1ULL << bit);
                pool->allocated_count++;

                myelin_segment_t* segment = &pool->buffer[index];
                memset(segment, 0, sizeof(myelin_segment_t));

                nimcp_spinlock_unlock(&pool->lock);
                return segment;
            }
        }
    }

    nimcp_spinlock_unlock(&pool->lock);
    return NULL;  // Pool exhausted
}

void myelin_segment_pool_free(myelin_segment_pool_t* pool, myelin_segment_t* segment)
{
    if (!pool || !segment) return;

    ptrdiff_t offset = segment - pool->buffer;
    if (offset < 0 || (uint32_t)offset >= pool->capacity) {
        return;
    }

    nimcp_spinlock_lock(&pool->lock);

    uint32_t index = (uint32_t)offset;
    uint32_t word = index / 64;
    uint32_t bit = index % 64;

    pool->bitmap[word] |= (1ULL << bit);
    pool->allocated_count--;

    nimcp_spinlock_unlock(&pool->lock);
}

//=============================================================================
// Sheath Creation and Destruction
//=============================================================================

myelin_sheath_t* myelin_sheath_create(uint32_t id, uint32_t axon_id,
                                       uint32_t oligo_id, uint32_t max_segments)
{
    if (max_segments == 0) {
        max_segments = 16;  // Default
    }

    myelin_sheath_t* sheath = nimcp_calloc(1, sizeof(myelin_sheath_t));
    NIMCP_API_CHECK_ALLOC(sheath, "myelin_sheath_create: Failed to allocate sheath");

    sheath->id = id;
    sheath->axon_id = axon_id;
    sheath->oligodendrocyte_id = oligo_id;
    sheath->num_segments = 0;
    sheath->max_segments = max_segments;

    // Allocate segment pointer array
    sheath->segments = nimcp_calloc(max_segments, sizeof(myelin_segment_t*));
    if (!sheath->segments) {
        nimcp_free(sheath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "myelin_sheath_create: sheath->segments is NULL");
        return NULL;
    }

    // Initialize aggregate properties
    sheath->total_length_um = 0.0F;
    sheath->mean_g_ratio = NIMCP_MYELIN_G_RATIO_OPTIMAL;
    sheath->mean_lamellae = 0.0F;
    sheath->coverage_fraction = 0.0F;

    // Health defaults
    sheath->overall_health = MYELIN_HEALTH_INTACT;
    sheath->mean_integrity = 1.0F;
    sheath->min_integrity = 1.0F;
    sheath->damaged_segment_count = 0;

    // Conduction defaults
    sheath->effective_velocity_ms = NIMCP_MYELIN_BASE_VELOCITY_MS;
    sheath->total_delay_ms = 0.0F;
    sheath->velocity_ratio = 1.0F;

    // Metabolic defaults
    sheath->total_atp_consumption = 0.0F;
    sheath->mean_trophic_support = 1.0F;

    // Dynamics
    sheath->myelination_rate = 0.0F;
    sheath->demyelination_rate = 0.0F;
    sheath->repair_rate = NIMCP_MYELIN_REPAIR_RATE_BASE;

    // State tracking
    sheath->creation_time = myelin_get_time_us();
    sheath->last_update_time = sheath->creation_time;
    sheath->maturation_time = 0;
    sheath->is_mature = false;

    // CoW defaults
    sheath->cow_ref_count = 1;
    sheath->cow_modified = false;
    sheath->cow_original = NULL;

    nimcp_spinlock_init(&sheath->lock);

    return sheath;
}

myelin_sheath_t* myelin_sheath_create_for_axon(uint32_t id, uint32_t axon_id,
                                                uint32_t oligo_id,
                                                float axon_length,
                                                float axon_diameter,
                                                uint32_t max_segments)
{
    // Calculate optimal internode length
    float optimal_internode = axon_diameter * NIMCP_MYELIN_INTERNODE_RATIO;
    optimal_internode = MYELIN_CLAMP(optimal_internode,
                                      NIMCP_MYELIN_MIN_INTERNODE_UM,
                                      NIMCP_MYELIN_MAX_INTERNODE_UM);

    // Calculate number of segments needed
    uint32_t num_segments = (uint32_t)(axon_length / optimal_internode);
    if (num_segments < 1) num_segments = 1;
    if (max_segments > 0 && num_segments > max_segments) {
        num_segments = max_segments;
    }

    myelin_sheath_t* sheath = myelin_sheath_create(id, axon_id, oligo_id, num_segments);
    if (!sheath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sheath is NULL");

        return NULL;
    }

    // Pre-create segments with optimal spacing
    float segment_length = axon_length / (float)num_segments;
    float position = 0.0F;

    for (uint32_t i = 0; i < num_segments; i++) {
        myelin_segment_t* segment = myelin_sheath_add_segment(
            sheath, position, segment_length, axon_diameter);
        if (segment) {
            position += segment_length + NIMCP_MYELIN_NODE_LENGTH_UM;
        }
    }

    return sheath;
}

void myelin_sheath_destroy(myelin_sheath_t* sheath)
{
    if (!sheath) return;

    // Handle CoW copy - if this is an unmodified copy, it shares segments with original
    // Only free the segments array, not the segments themselves
    if (sheath->cow_original && !sheath->cow_modified) {
        nimcp_free(sheath->segments);
        nimcp_free(sheath);
        return;
    }

    // Free all segments (for original sheaths or modified CoW copies that own their segments)
    if (sheath->segments) {
        for (uint32_t i = 0; i < sheath->num_segments; i++) {
            if (sheath->segments[i]) {
                nimcp_free(sheath->segments[i]);
            }
        }
        nimcp_free(sheath->segments);
    }

    nimcp_free(sheath);
}

//=============================================================================
// Segment Management
//=============================================================================

myelin_segment_t* myelin_sheath_add_segment(myelin_sheath_t* sheath,
                                             float start_position_um,
                                             float length_um,
                                             float axon_diameter)
{
    if (!sheath) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sheath is NULL");

        return NULL;

    }
    if (sheath->num_segments >= sheath->max_segments) return NULL;

    myelin_segment_t* segment = nimcp_calloc(1, sizeof(myelin_segment_t));
    if (!segment) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "segment is NULL");

        return NULL;

    }

    // Assign ID
    segment->id = sheath->num_segments;
    segment->sheath_id = sheath->id;
    segment->axon_id = sheath->axon_id;

    // Position
    segment->start_position_um = start_position_um;
    segment->length_um = length_um;
    segment->position[0] = start_position_um + length_um / 2.0F;
    segment->position[1] = 0.0F;
    segment->position[2] = 0.0F;

    // Structural properties - start with minimal myelination
    segment->num_lamellae = NIMCP_MYELIN_MIN_LAMELLAE;
    segment->inner_diameter_um = axon_diameter;
    segment->thickness_um = myelin_compute_thickness(segment->num_lamellae);
    segment->outer_diameter_um = axon_diameter + 2.0F * segment->thickness_um;
    segment->g_ratio = myelin_compute_g_ratio(axon_diameter, segment->num_lamellae);

    // Compaction - starts as partial
    segment->compaction = MYELIN_COMPACT_PARTIAL;
    segment->compaction_score = 0.5F;
    segment->mdl_formation = 0.5F;
    segment->ipl_formation = 0.5F;

    // Paranodes - forming
    segment->proximal_paranode = PARANODE_FORMING;
    segment->distal_paranode = PARANODE_FORMING;
    segment->paranode_integrity = 0.5F;

    // Health - starts intact
    segment->health = MYELIN_HEALTH_INTACT;
    segment->integrity = 1.0F;
    segment->damage_accumulated = 0.0F;
    segment->damage_onset_time = 0;

    // Conduction
    segment->local_velocity_ms = myelin_segment_compute_velocity(segment);
    segment->propagation_delay_ms = myelin_segment_compute_delay(segment);

    // Metabolic
    segment->atp_level = 1.0F;
    segment->lactate_received = 0.0F;
    segment->trophic_support = 1.0F;

    // Timestamps
    segment->creation_time = myelin_get_time_us();
    segment->last_update_time = segment->creation_time;

    // Add to sheath
    sheath->segments[sheath->num_segments] = segment;
    sheath->num_segments++;

    // Update aggregate properties
    sheath->total_length_um += length_um;
    myelin_sheath_update_conduction(sheath);
    myelin_sheath_update_health(sheath);

    return segment;
}

nimcp_result_t myelin_sheath_remove_segment(myelin_sheath_t* sheath,
                                             uint32_t segment_id)
{
    NIMCP_CHECK_THROW(sheath, NIMCP_ERROR_NULL_POINTER, "sheath is NULL");

    // Find segment
    int32_t index = -1;
    for (uint32_t i = 0; i < sheath->num_segments; i++) {
        if (sheath->segments[i] && sheath->segments[i]->id == segment_id) {
            index = (int32_t)i;
            break;
        }
    }

    if (index < 0) return NIMCP_NOT_FOUND;

    // Update totals before removal
    sheath->total_length_um -= sheath->segments[index]->length_um;

    // Free segment
    nimcp_free(sheath->segments[index]);

    // Shift remaining segments
    for (uint32_t i = (uint32_t)index; i < sheath->num_segments - 1; i++) {
        sheath->segments[i] = sheath->segments[i + 1];
    }
    sheath->num_segments--;
    sheath->segments[sheath->num_segments] = NULL;

    // Update aggregates
    myelin_sheath_update_conduction(sheath);
    myelin_sheath_update_health(sheath);

    return NIMCP_SUCCESS;
}

myelin_segment_t* myelin_sheath_get_segment(myelin_sheath_t* sheath, uint32_t index)
{
    if (!sheath || index >= sheath->num_segments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "myelin_sheath_get_segment: sheath is NULL");
        return NULL;
    }
    return sheath->segments[index];
}

myelin_segment_t* myelin_sheath_find_segment_at(myelin_sheath_t* sheath,
                                                 float position_um)
{
    if (!sheath) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sheath is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < sheath->num_segments; i++) {
        myelin_segment_t* seg = sheath->segments[i];
        if (seg) {
            float start = seg->start_position_um;
            float end = start + seg->length_um;
            if (position_um >= start && position_um < end) {
                return seg;
            }
        }
    }
    return NULL;  /* Not found - no segment at this position */
}

//=============================================================================
// Structural Properties
//=============================================================================

nimcp_result_t myelin_segment_set_lamellae(myelin_segment_t* segment,
                                            uint32_t num_lamellae)
{
    NIMCP_CHECK_THROW(segment, NIMCP_ERROR_NULL_POINTER, "segment is NULL");

    // Clamp to valid range
    num_lamellae = (num_lamellae < NIMCP_MYELIN_MIN_LAMELLAE) ?
                   NIMCP_MYELIN_MIN_LAMELLAE : num_lamellae;
    num_lamellae = (num_lamellae > NIMCP_MYELIN_MAX_LAMELLAE) ?
                   NIMCP_MYELIN_MAX_LAMELLAE : num_lamellae;

    segment->num_lamellae = num_lamellae;
    segment->thickness_um = myelin_compute_thickness(num_lamellae);
    segment->outer_diameter_um = segment->inner_diameter_um + 2.0F * segment->thickness_um;
    segment->g_ratio = myelin_compute_g_ratio(segment->inner_diameter_um, num_lamellae);

    // Update conduction velocity
    segment->local_velocity_ms = myelin_segment_compute_velocity(segment);
    segment->propagation_delay_ms = myelin_segment_compute_delay(segment);

    segment->last_update_time = myelin_get_time_us();

    return NIMCP_SUCCESS;
}

nimcp_result_t myelin_segment_set_compaction(myelin_segment_t* segment,
                                              myelin_compaction_t compaction,
                                              float score)
{
    NIMCP_CHECK_THROW(segment, NIMCP_ERROR_NULL_POINTER, "segment is NULL");

    segment->compaction = compaction;
    segment->compaction_score = MYELIN_CLAMP(score, 0.0F, 1.0F);

    // Update MDL/IPL based on compaction
    if (compaction == MYELIN_COMPACT_FULL) {
        segment->mdl_formation = 1.0F;
        segment->ipl_formation = 1.0F;
    } else if (compaction == MYELIN_COMPACT_PARTIAL) {
        segment->mdl_formation = score * 0.8F;
        segment->ipl_formation = score * 0.7F;
    } else {
        segment->mdl_formation = 0.0F;
        segment->ipl_formation = 0.0F;
    }

    segment->last_update_time = myelin_get_time_us();
    return NIMCP_SUCCESS;
}

uint32_t myelin_compute_optimal_lamellae(float axon_diameter, float target_g_ratio)
{
    // G-ratio = inner / outer = inner / (inner + 2*thickness)
    // thickness = inner * (1 - g_ratio) / (2 * g_ratio)
    // thickness = num_lamellae * lamella_thickness_um
    // num_lamellae = thickness / lamella_thickness_um

    target_g_ratio = MYELIN_CLAMP(target_g_ratio,
                                   NIMCP_MYELIN_G_RATIO_MIN,
                                   NIMCP_MYELIN_G_RATIO_MAX);

    float lamella_thickness_um = NIMCP_MYELIN_LAMELLA_THICKNESS_NM / 1000.0F;
    float thickness = axon_diameter * (1.0F - target_g_ratio) / (2.0F * target_g_ratio);
    uint32_t num_lamellae = (uint32_t)(thickness / lamella_thickness_um + 0.5F);

    return (uint32_t)MYELIN_CLAMP(num_lamellae,
                                   NIMCP_MYELIN_MIN_LAMELLAE,
                                   NIMCP_MYELIN_MAX_LAMELLAE);
}

float myelin_compute_g_ratio(float inner_diameter, uint32_t num_lamellae)
{
    float lamella_thickness_um = NIMCP_MYELIN_LAMELLA_THICKNESS_NM / 1000.0F;
    float thickness = (float)num_lamellae * lamella_thickness_um;
    float outer_diameter = inner_diameter + 2.0F * thickness;

    if (outer_diameter < MYELIN_EPSILON) return 1.0F;
    return inner_diameter / outer_diameter;
}

float myelin_compute_thickness(uint32_t num_lamellae)
{
    float lamella_thickness_um = NIMCP_MYELIN_LAMELLA_THICKNESS_NM / 1000.0F;
    return (float)num_lamellae * lamella_thickness_um;
}

//=============================================================================
// Conduction Properties
//=============================================================================

float myelin_segment_compute_velocity(const myelin_segment_t* segment)
{
    if (!segment) return NIMCP_MYELIN_BASE_VELOCITY_MS;

    // Hursh's law: v = k * d for myelinated axons
    // k ≈ 6 m/s per micrometer of diameter
    // Efficiency depends on g-ratio (optimal around 0.77)

    float diameter = segment->inner_diameter_um;
    float g_ratio = segment->g_ratio;
    uint32_t lamellae = segment->num_lamellae;

    // Base velocity from diameter
    float base_velocity = NIMCP_MYELIN_VELOCITY_COEFF * diameter;

    // G-ratio efficiency factor (peaks at optimal g-ratio)
    float g_optimal = NIMCP_MYELIN_G_RATIO_OPTIMAL;
    float g_deviation = fabsf(g_ratio - g_optimal);
    float g_efficiency = 1.0F - (g_deviation / NIMCP_MYELIN_G_RATIO_TOLERANCE);
    g_efficiency = MYELIN_CLAMP(g_efficiency, 0.5F, 1.0F);

    // Myelination factor based on lamellae
    float myelin_fraction = (float)lamellae / (float)NIMCP_MYELIN_OPTIMAL_LAMELLAE;
    myelin_fraction = MYELIN_CLAMP(myelin_fraction, 0.0F, 1.0F);

    // Compaction factor
    float compaction_factor = segment->compaction_score;

    // Integrity factor
    float integrity_factor = segment->integrity;

    // Final velocity
    float velocity = base_velocity * g_efficiency * myelin_fraction *
                     compaction_factor * integrity_factor;

    // Clamp to biological range
    velocity = MYELIN_CLAMP(velocity, NIMCP_MYELIN_BASE_VELOCITY_MS,
                            NIMCP_MYELIN_MAX_VELOCITY_MS);

    return velocity;
}

float myelin_segment_compute_delay(const myelin_segment_t* segment)
{
    if (!segment) return 0.0F;

    float velocity = segment->local_velocity_ms;
    if (velocity < MYELIN_EPSILON) {
        velocity = NIMCP_MYELIN_BASE_VELOCITY_MS;
    }

    // Convert: length (um) / velocity (m/s) = delay (ms)
    // length_um * 1e-6 (m) / velocity_ms * 1e3 (ms)
    float delay_ms = (segment->length_um * 1e-6F) / velocity * 1e3F;

    return delay_ms;
}

void myelin_sheath_update_conduction(myelin_sheath_t* sheath)
{
    if (!sheath || sheath->num_segments == 0) return;

    nimcp_spinlock_lock(&sheath->lock);

    float total_delay = 0.0F;
    float total_g_ratio = 0.0F;
    float total_lamellae = 0.0F;

    for (uint32_t i = 0; i < sheath->num_segments; i++) {
        myelin_segment_t* seg = sheath->segments[i];
        if (seg) {
            // Recompute segment velocity and delay
            seg->local_velocity_ms = myelin_segment_compute_velocity(seg);
            seg->propagation_delay_ms = myelin_segment_compute_delay(seg);

            total_delay += seg->propagation_delay_ms;

            // Add node of Ranvier delay (small fixed delay)
            if (i < sheath->num_segments - 1) {
                total_delay += 0.01F;  // ~10us per node
            }

            total_g_ratio += seg->g_ratio;
            total_lamellae += (float)seg->num_lamellae;
        }
    }

    sheath->total_delay_ms = total_delay;
    sheath->mean_g_ratio = total_g_ratio / (float)sheath->num_segments;
    sheath->mean_lamellae = total_lamellae / (float)sheath->num_segments;

    // Effective velocity from total length and delay
    if (total_delay > MYELIN_EPSILON) {
        // velocity (m/s) = length (um) * 1e-6 / delay (ms) * 1e3
        sheath->effective_velocity_ms = (sheath->total_length_um * 1e-6F) /
                                        (total_delay * 1e-3F);
    } else {
        sheath->effective_velocity_ms = NIMCP_MYELIN_MAX_VELOCITY_MS;
    }

    // Velocity ratio vs unmyelinated
    sheath->velocity_ratio = sheath->effective_velocity_ms / NIMCP_MYELIN_BASE_VELOCITY_MS;

    sheath->last_update_time = myelin_get_time_us();

    nimcp_spinlock_unlock(&sheath->lock);
}

float myelin_sheath_get_velocity_ratio(const myelin_sheath_t* sheath)
{
    if (!sheath) return 1.0F;
    return sheath->velocity_ratio;
}

//=============================================================================
// Health and Integrity
//=============================================================================

nimcp_result_t myelin_segment_apply_damage(myelin_segment_t* segment,
                                            float damage_amount,
                                            uint64_t current_time)
{
    NIMCP_CHECK_THROW(segment, NIMCP_ERROR_NULL_POINTER, "segment is NULL");

    segment->damage_accumulated += damage_amount;
    segment->integrity -= damage_amount;
    segment->integrity = MYELIN_CLAMP(segment->integrity, 0.0F, 1.0F);

    if (segment->damage_onset_time == 0 && damage_amount > 0) {
        segment->damage_onset_time = current_time;
    }

    myelin_segment_update_health(segment, current_time);

    return NIMCP_SUCCESS;
}

nimcp_result_t myelin_segment_repair(myelin_segment_t* segment,
                                      float repair_amount,
                                      uint64_t current_time)
{
    NIMCP_CHECK_THROW(segment, NIMCP_ERROR_NULL_POINTER, "segment is NULL");

    segment->integrity += repair_amount;
    segment->integrity = MYELIN_CLAMP(segment->integrity, 0.0F, 1.0F);

    if (segment->integrity > NIMCP_MYELIN_INTEGRITY_HEALTHY) {
        segment->damage_accumulated = 0.0F;
        segment->damage_onset_time = 0;
    }

    myelin_segment_update_health(segment, current_time);

    return NIMCP_SUCCESS;
}

void myelin_segment_update_health(myelin_segment_t* segment, uint64_t current_time)
{
    if (!segment) return;

    float integrity = segment->integrity;
    myelin_health_state_t old_state = segment->health;
    myelin_health_state_t new_state = old_state;

    // State machine for health transitions
    if (integrity >= NIMCP_MYELIN_INTEGRITY_HEALTHY) {
        if (old_state == MYELIN_HEALTH_REMYELINATING) {
            new_state = MYELIN_HEALTH_INTACT;
        } else if (old_state != MYELIN_HEALTH_DEMYELINATED) {
            new_state = MYELIN_HEALTH_INTACT;
        }
    } else if (integrity >= NIMCP_MYELIN_INTEGRITY_DAMAGED) {
        if (old_state == MYELIN_HEALTH_INTACT) {
            new_state = MYELIN_HEALTH_STRESSED;
        } else if (old_state == MYELIN_HEALTH_DEMYELINATED ||
                   old_state == MYELIN_HEALTH_DEMYELINATING) {
            new_state = MYELIN_HEALTH_REMYELINATING;
        }
    } else if (integrity >= NIMCP_MYELIN_INTEGRITY_CRITICAL) {
        new_state = MYELIN_HEALTH_DAMAGED;
    } else if (integrity > 0.0F) {
        new_state = MYELIN_HEALTH_DEMYELINATING;
    } else {
        new_state = MYELIN_HEALTH_DEMYELINATED;
    }

    segment->health = new_state;
    segment->last_update_time = current_time;
}

void myelin_sheath_update_health(myelin_sheath_t* sheath)
{
    if (!sheath || sheath->num_segments == 0) return;

    nimcp_spinlock_lock(&sheath->lock);

    float total_integrity = 0.0F;
    float min_integrity = 1.0F;
    uint32_t damaged_count = 0;

    for (uint32_t i = 0; i < sheath->num_segments; i++) {
        myelin_segment_t* seg = sheath->segments[i];
        if (seg) {
            total_integrity += seg->integrity;
            if (seg->integrity < min_integrity) {
                min_integrity = seg->integrity;
            }
            if (seg->health != MYELIN_HEALTH_INTACT) {
                damaged_count++;
            }
        }
    }

    sheath->mean_integrity = total_integrity / (float)sheath->num_segments;
    sheath->min_integrity = min_integrity;
    sheath->damaged_segment_count = damaged_count;

    // Overall health based on mean integrity
    if (sheath->mean_integrity >= NIMCP_MYELIN_INTEGRITY_HEALTHY) {
        sheath->overall_health = MYELIN_HEALTH_INTACT;
    } else if (sheath->mean_integrity >= NIMCP_MYELIN_INTEGRITY_DAMAGED) {
        sheath->overall_health = MYELIN_HEALTH_STRESSED;
    } else if (sheath->mean_integrity >= NIMCP_MYELIN_INTEGRITY_CRITICAL) {
        sheath->overall_health = MYELIN_HEALTH_DAMAGED;
    } else if (sheath->mean_integrity > 0.0F) {
        sheath->overall_health = MYELIN_HEALTH_DEMYELINATING;
    } else {
        sheath->overall_health = MYELIN_HEALTH_DEMYELINATED;
    }

    nimcp_spinlock_unlock(&sheath->lock);
}

const char* myelin_health_state_to_string(myelin_health_state_t state)
{
    switch (state) {
        case MYELIN_HEALTH_INTACT:       return "intact";
        case MYELIN_HEALTH_STRESSED:     return "stressed";
        case MYELIN_HEALTH_DAMAGED:      return "damaged";
        case MYELIN_HEALTH_DEMYELINATING: return "demyelinating";
        case MYELIN_HEALTH_DEMYELINATED: return "demyelinated";
        case MYELIN_HEALTH_REMYELINATING: return "remyelinating";
        default:                          return "unknown";
    }
}

//=============================================================================
// Paranodal Junctions
//=============================================================================

nimcp_result_t myelin_segment_set_paranodes(myelin_segment_t* segment,
                                             paranode_state_t proximal,
                                             paranode_state_t distal)
{
    NIMCP_CHECK_THROW(segment, NIMCP_ERROR_NULL_POINTER, "segment is NULL");

    segment->proximal_paranode = proximal;
    segment->distal_paranode = distal;

    // Update integrity based on paranode states
    if (proximal == PARANODE_MATURE && distal == PARANODE_MATURE) {
        segment->paranode_integrity = 1.0F;
    } else if (proximal == PARANODE_DISRUPTED || distal == PARANODE_DISRUPTED) {
        segment->paranode_integrity = 0.3F;
    } else if (proximal == PARANODE_FORMING || distal == PARANODE_FORMING) {
        segment->paranode_integrity = 0.6F;
    } else {
        segment->paranode_integrity = 0.0F;
    }

    segment->last_update_time = myelin_get_time_us();
    return NIMCP_SUCCESS;
}

void myelin_segment_update_paranode_integrity(myelin_segment_t* segment,
                                               float integrity)
{
    if (!segment) return;
    segment->paranode_integrity = MYELIN_CLAMP(integrity, 0.0F, 1.0F);
    segment->last_update_time = myelin_get_time_us();
}

bool myelin_segment_paranodes_functional(const myelin_segment_t* segment)
{
    if (!segment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "myelin_segment_paranodes_functional: segment is NULL");
        return false;
    }
    return (segment->proximal_paranode == PARANODE_MATURE &&
            segment->distal_paranode == PARANODE_MATURE &&
            segment->paranode_integrity > 0.5F);
}

//=============================================================================
// Metabolic Support
//=============================================================================

void myelin_segment_update_metabolism(myelin_segment_t* segment, float dt)
{
    if (!segment || dt <= 0.0F) return;

    // ATP consumption for myelin maintenance
    float atp_cost = NIMCP_MYELIN_ATP_MAINTENANCE * dt;
    segment->atp_level -= atp_cost;

    // Lactate can be converted to ATP
    if (segment->lactate_received > 0.0F) {
        float atp_from_lactate = segment->lactate_received * NIMCP_MYELIN_LACTATE_TRANSFER_RATE;
        segment->atp_level += atp_from_lactate;
        segment->lactate_received *= 0.9F;  // Decay
    }

    // Clamp ATP
    segment->atp_level = MYELIN_CLAMP(segment->atp_level, 0.0F, 1.0F);

    // Low ATP causes damage
    if (segment->atp_level < 0.2F) {
        segment->integrity -= 0.001F * dt;
        segment->integrity = MYELIN_CLAMP(segment->integrity, 0.0F, 1.0F);
    }

    // Trophic support affects health
    if (segment->trophic_support < NIMCP_MYELIN_TROPHIC_THRESHOLD) {
        segment->integrity -= 0.0005F * dt;
        segment->integrity = MYELIN_CLAMP(segment->integrity, 0.0F, 1.0F);
    }

    segment->last_update_time = myelin_get_time_us();
}

void myelin_segment_receive_lactate(myelin_segment_t* segment, float lactate_amount)
{
    if (!segment) return;
    segment->lactate_received += lactate_amount;
}

void myelin_segment_set_trophic_support(myelin_segment_t* segment, float trophic_level)
{
    if (!segment) return;
    segment->trophic_support = MYELIN_CLAMP(trophic_level, 0.0F, 1.0F);
}

bool myelin_segment_metabolically_healthy(const myelin_segment_t* segment)
{
    if (!segment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "myelin_segment_metabolically_healthy: segment is NULL");
        return false;
    }
    return (segment->atp_level > 0.3F &&
            segment->trophic_support > NIMCP_MYELIN_TROPHIC_THRESHOLD);
}

//=============================================================================
// Dynamics and Simulation
//=============================================================================

void myelin_segment_step(myelin_segment_t* segment, float dt, uint64_t current_time)
{
    if (!segment || dt <= 0.0F) return;

    // Update metabolism
    myelin_segment_update_metabolism(segment, dt);

    // Natural damage decay (slight degradation over time if not maintained)
    segment->integrity -= NIMCP_MYELIN_DAMAGE_DECAY_RATE * dt;
    segment->integrity = MYELIN_CLAMP(segment->integrity, 0.0F, 1.0F);

    // Compaction maturation
    if (segment->compaction == MYELIN_COMPACT_PARTIAL) {
        segment->compaction_score += 0.01F * dt;
        if (segment->compaction_score >= 1.0F) {
            segment->compaction = MYELIN_COMPACT_FULL;
            segment->compaction_score = 1.0F;
            segment->mdl_formation = 1.0F;
            segment->ipl_formation = 1.0F;
        }
    }

    // Paranode maturation
    if (segment->proximal_paranode == PARANODE_FORMING) {
        segment->paranode_integrity += 0.02F * dt;
        if (segment->paranode_integrity >= 0.9F) {
            segment->proximal_paranode = PARANODE_MATURE;
        }
    }
    if (segment->distal_paranode == PARANODE_FORMING) {
        segment->paranode_integrity += 0.02F * dt;
        if (segment->paranode_integrity >= 0.9F) {
            segment->distal_paranode = PARANODE_MATURE;
        }
    }

    // Update health state
    myelin_segment_update_health(segment, current_time);

    // Recalculate conduction
    segment->local_velocity_ms = myelin_segment_compute_velocity(segment);
    segment->propagation_delay_ms = myelin_segment_compute_delay(segment);

    segment->last_update_time = current_time;
}

void myelin_sheath_step(myelin_sheath_t* sheath, float dt, uint64_t current_time)
{
    if (!sheath || dt <= 0.0F) return;

    nimcp_spinlock_lock(&sheath->lock);

    // Step all segments
    for (uint32_t i = 0; i < sheath->num_segments; i++) {
        if (sheath->segments[i]) {
            myelin_segment_step(sheath->segments[i], dt, current_time);
        }
    }

    // Update aggregates
    nimcp_spinlock_unlock(&sheath->lock);

    myelin_sheath_update_conduction(sheath);
    myelin_sheath_update_health(sheath);

    // Check for maturation
    if (!sheath->is_mature && sheath->mean_integrity > 0.9F &&
        sheath->overall_health == MYELIN_HEALTH_INTACT) {
        sheath->is_mature = true;
        sheath->maturation_time = current_time;
    }

    sheath->last_update_time = current_time;
}

void myelin_sheath_myelinate(myelin_sheath_t* sheath, float rate, float dt)
{
    if (!sheath || rate <= 0.0F || dt <= 0.0F) return;

    nimcp_spinlock_lock(&sheath->lock);

    float lamellae_to_add = rate * dt;
    sheath->myelination_rate = rate;

    for (uint32_t i = 0; i < sheath->num_segments; i++) {
        myelin_segment_t* seg = sheath->segments[i];
        if (seg && seg->num_lamellae < NIMCP_MYELIN_OPTIMAL_LAMELLAE) {
            uint32_t new_lamellae = seg->num_lamellae + (uint32_t)(lamellae_to_add + 0.5F);
            myelin_segment_set_lamellae(seg, new_lamellae);
        }
    }

    nimcp_spinlock_unlock(&sheath->lock);

    myelin_sheath_update_conduction(sheath);
}

void myelin_sheath_demyelinate(myelin_sheath_t* sheath, float rate, float dt)
{
    if (!sheath || rate <= 0.0F || dt <= 0.0F) return;

    nimcp_spinlock_lock(&sheath->lock);

    float lamellae_to_remove = rate * dt;
    sheath->demyelination_rate = rate;

    for (uint32_t i = 0; i < sheath->num_segments; i++) {
        myelin_segment_t* seg = sheath->segments[i];
        if (seg && seg->num_lamellae > NIMCP_MYELIN_MIN_LAMELLAE) {
            int32_t new_lamellae = (int32_t)seg->num_lamellae - (int32_t)(lamellae_to_remove + 0.5F);
            if (new_lamellae < (int32_t)NIMCP_MYELIN_MIN_LAMELLAE) {
                new_lamellae = (int32_t)NIMCP_MYELIN_MIN_LAMELLAE;
            }
            myelin_segment_set_lamellae(seg, (uint32_t)new_lamellae);
        }
    }

    nimcp_spinlock_unlock(&sheath->lock);

    myelin_sheath_update_conduction(sheath);
}

//=============================================================================
// Copy-on-Write Support
//=============================================================================

myelin_sheath_t* myelin_sheath_cow_copy(myelin_sheath_t* sheath)
{
    if (!sheath) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sheath is NULL");

        return NULL;

    }

    nimcp_spinlock_lock(&sheath->lock);

    // Increment reference count on original
    sheath->cow_ref_count++;

    // Create shallow copy
    myelin_sheath_t* copy = nimcp_calloc(1, sizeof(myelin_sheath_t));
    if (!copy) {
        sheath->cow_ref_count--;
        nimcp_spinlock_unlock(&sheath->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "myelin_sheath_cow_copy: copy is NULL");
        return NULL;
    }

    // Copy all fields
    memcpy(copy, sheath, sizeof(myelin_sheath_t));

    // Copy segment pointer array (shallow - pointers still point to original segments)
    copy->segments = nimcp_malloc(sheath->max_segments * sizeof(myelin_segment_t*));
    if (!copy->segments) {
        sheath->cow_ref_count--;
        nimcp_free(copy);
        nimcp_spinlock_unlock(&sheath->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "myelin_sheath_cow_copy: copy->segments is NULL");
        return NULL;
    }
    memcpy(copy->segments, sheath->segments, sheath->max_segments * sizeof(myelin_segment_t*));

    // Mark as CoW copy
    copy->cow_original = sheath;
    copy->cow_modified = false;
    copy->cow_ref_count = 1;  // Copy has its own reference count

    // Initialize new lock for copy
    nimcp_spinlock_init(&copy->lock);

    nimcp_spinlock_unlock(&sheath->lock);

    return copy;
}

nimcp_result_t myelin_sheath_cow_prepare_write(myelin_sheath_t* sheath)
{
    NIMCP_CHECK_THROW(sheath, NIMCP_ERROR_NULL_POINTER, "sheath is NULL");

    // If not a CoW copy or already modified, nothing to do
    if (!sheath->cow_original || sheath->cow_modified) {
        return NIMCP_SUCCESS;
    }

    nimcp_spinlock_lock(&sheath->lock);

    // Deep copy segments
    myelin_segment_t** new_segments = nimcp_malloc(sheath->max_segments * sizeof(myelin_segment_t*));
    if (!new_segments) {
        nimcp_spinlock_unlock(&sheath->lock);
        return NIMCP_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < sheath->num_segments; i++) {
        if (sheath->segments[i]) {
            new_segments[i] = nimcp_malloc(sizeof(myelin_segment_t));
            if (!new_segments[i]) {
                // Cleanup on failure
                for (uint32_t j = 0; j < i; j++) {
                    if (new_segments[j]) nimcp_free(new_segments[j]);
                }
                nimcp_free(new_segments);
                nimcp_spinlock_unlock(&sheath->lock);
                return NIMCP_ERROR_MEMORY;
            }
            memcpy(new_segments[i], sheath->segments[i], sizeof(myelin_segment_t));
        } else {
            new_segments[i] = NULL;
        }
    }

    // Clear remaining slots
    for (uint32_t i = sheath->num_segments; i < sheath->max_segments; i++) {
        new_segments[i] = NULL;
    }

    // Replace segment array
    nimcp_free(sheath->segments);
    sheath->segments = new_segments;

    // Decrement original's reference count
    if (sheath->cow_original) {
        sheath->cow_original->cow_ref_count--;
    }

    sheath->cow_original = NULL;
    sheath->cow_modified = true;

    nimcp_spinlock_unlock(&sheath->lock);

    return NIMCP_SUCCESS;
}

void myelin_sheath_cow_release(myelin_sheath_t* sheath)
{
    if (!sheath) return;

    nimcp_spinlock_lock(&sheath->lock);

    // If this is a CoW copy, decrement the original's ref count
    myelin_sheath_t* original = sheath->cow_original;
    if (original) {
        nimcp_spinlock_lock(&original->lock);
        original->cow_ref_count--;
        nimcp_spinlock_unlock(&original->lock);
        // Don't null cow_original here - destroy needs it to know this is a CoW copy
    }

    sheath->cow_ref_count--;

    if (sheath->cow_ref_count == 0) {
        nimcp_spinlock_unlock(&sheath->lock);
        // Destroy will check cow_original to determine if segments should be freed
        myelin_sheath_destroy(sheath);
    } else {
        nimcp_spinlock_unlock(&sheath->lock);
    }
}

bool myelin_sheath_is_cow_copy(const myelin_sheath_t* sheath)
{
    if (!sheath) {
        return false;
    }
    return (sheath->cow_original != NULL && !sheath->cow_modified);
}

uint32_t myelin_sheath_cow_ref_count(const myelin_sheath_t* sheath)
{
    if (!sheath) return 0;
    return sheath->cow_ref_count;
}

//=============================================================================
// Network Management
//=============================================================================

myelin_sheath_network_t* myelin_network_create(const myelin_network_config_t* config)
{
    if (!config) {
        myelin_network_config_t default_config = myelin_network_default_config();
        config = &default_config;
    }

    myelin_sheath_network_t* network = nimcp_calloc(1, sizeof(myelin_sheath_network_t));
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "network is NULL");

        return NULL;
    }

    network->capacity = config->max_sheaths;
    network->num_sheaths = 0;
    network->config = *config;

    // Allocate sheath array
    network->sheaths = nimcp_calloc(config->max_sheaths, sizeof(myelin_sheath_t*));
    if (!network->sheaths) {
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "myelin_network_create: network->sheaths is NULL");
        return NULL;
    }

    // Create memory pools if enabled
    if (config->use_memory_pools) {
        network->sheath_pool = myelin_sheath_pool_create(config->max_sheaths);
        network->segment_pool = myelin_segment_pool_create(
            config->max_sheaths * config->max_segments_per_sheath);
    }

    // Initialize hash maps as NULL (can be implemented with KD-tree or hash table)
    network->axon_to_sheath_map = NULL;
    network->oligo_to_sheaths_map = NULL;
    network->spatial_index = NULL;
    network->spatial_index_valid = false;

    // Statistics defaults
    network->total_segments = 0;
    network->mean_network_integrity = 1.0F;
    network->mean_network_g_ratio = NIMCP_MYELIN_G_RATIO_OPTIMAL;
    network->demyelinating_count = 0;
    network->remyelinating_count = 0;

    // Timing
    network->current_time = myelin_get_time_us();
    network->last_step_time = network->current_time;

    nimcp_mutex_init(&network->lock, NULL);

    return network;
}

myelin_sheath_network_t* myelin_network_create_default(uint32_t capacity)
{
    myelin_network_config_t config = myelin_network_default_config();
    config.max_sheaths = capacity;
    return myelin_network_create(&config);
}

void myelin_network_destroy(myelin_sheath_network_t* network)
{
    if (!network) return;

    nimcp_mutex_lock(&network->lock);

    // Destroy all sheaths
    if (network->sheaths) {
        for (uint32_t i = 0; i < network->num_sheaths; i++) {
            if (network->sheaths[i]) {
                myelin_sheath_destroy(network->sheaths[i]);
            }
        }
        nimcp_free(network->sheaths);
    }

    // Destroy memory pools
    if (network->sheath_pool) {
        myelin_sheath_pool_destroy(network->sheath_pool);
    }
    if (network->segment_pool) {
        myelin_segment_pool_destroy(network->segment_pool);
    }

    nimcp_mutex_unlock(&network->lock);
    nimcp_mutex_destroy(&network->lock);

    nimcp_free(network);
}

nimcp_result_t myelin_network_add_sheath(myelin_sheath_network_t* network,
                                          myelin_sheath_t* sheath)
{
    NIMCP_CHECK_THROW(network && sheath, NIMCP_ERROR_NULL_POINTER, "network or sheath is NULL");

    nimcp_mutex_lock(&network->lock);

    if (network->num_sheaths >= network->capacity) {
        nimcp_mutex_unlock(&network->lock);
        return NIMCP_BUFFER_FULL;
    }

    network->sheaths[network->num_sheaths] = sheath;
    network->num_sheaths++;
    network->total_segments += sheath->num_segments;
    network->spatial_index_valid = false;

    nimcp_mutex_unlock(&network->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t myelin_network_remove_sheath(myelin_sheath_network_t* network,
                                             uint32_t sheath_id)
{
    NIMCP_CHECK_THROW(network, NIMCP_ERROR_NULL_POINTER, "network is NULL");

    nimcp_mutex_lock(&network->lock);

    // Find sheath
    int32_t index = -1;
    for (uint32_t i = 0; i < network->num_sheaths; i++) {
        if (network->sheaths[i] && network->sheaths[i]->id == sheath_id) {
            index = (int32_t)i;
            break;
        }
    }

    if (index < 0) {
        nimcp_mutex_unlock(&network->lock);
        return NIMCP_NOT_FOUND;
    }

    // Update statistics
    network->total_segments -= network->sheaths[index]->num_segments;

    // Don't destroy - caller may want to keep it
    // Shift remaining sheaths
    for (uint32_t i = (uint32_t)index; i < network->num_sheaths - 1; i++) {
        network->sheaths[i] = network->sheaths[i + 1];
    }
    network->num_sheaths--;
    network->sheaths[network->num_sheaths] = NULL;
    network->spatial_index_valid = false;

    nimcp_mutex_unlock(&network->lock);

    return NIMCP_SUCCESS;
}

myelin_sheath_t* myelin_network_find_sheath(myelin_sheath_network_t* network,
                                             uint32_t sheath_id)
{
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < network->num_sheaths; i++) {
        if (network->sheaths[i] && network->sheaths[i]->id == sheath_id) {
            return network->sheaths[i];
        }
    }
    return NULL;  /* Not found - no sheath with this ID */
}

myelin_sheath_t* myelin_network_find_by_axon(myelin_sheath_network_t* network,
                                              uint32_t axon_id)
{
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < network->num_sheaths; i++) {
        if (network->sheaths[i] && network->sheaths[i]->axon_id == axon_id) {
            return network->sheaths[i];
        }
    }
    return NULL;  /* Not found - no sheath for this axon */
}

uint32_t myelin_network_find_by_oligo(myelin_sheath_network_t* network,
                                       uint32_t oligo_id,
                                       myelin_sheath_t** results,
                                       uint32_t max_results)
{
    if (!network || !results || max_results == 0) return 0;

    uint32_t found = 0;
    for (uint32_t i = 0; i < network->num_sheaths && found < max_results; i++) {
        if (network->sheaths[i] &&
            network->sheaths[i]->oligodendrocyte_id == oligo_id) {
            results[found++] = network->sheaths[i];
        }
    }
    return found;
}

void myelin_network_step(myelin_sheath_network_t* network,
                          float dt, uint64_t current_time)
{
    if (!network || dt <= 0.0F) return;

    nimcp_mutex_lock(&network->lock);

    // Reset counters
    network->demyelinating_count = 0;
    network->remyelinating_count = 0;

    float total_integrity = 0.0F;
    float total_g_ratio = 0.0F;
    uint64_t total_segments = 0;

    // Step all sheaths
    for (uint32_t i = 0; i < network->num_sheaths; i++) {
        myelin_sheath_t* sheath = network->sheaths[i];
        if (sheath) {
            myelin_sheath_step(sheath, dt, current_time);

            total_integrity += sheath->mean_integrity * (float)sheath->num_segments;
            total_g_ratio += sheath->mean_g_ratio * (float)sheath->num_segments;
            total_segments += sheath->num_segments;

            if (sheath->overall_health == MYELIN_HEALTH_DEMYELINATING) {
                network->demyelinating_count++;
            } else if (sheath->overall_health == MYELIN_HEALTH_REMYELINATING) {
                network->remyelinating_count++;
            }
        }
    }

    // Update network statistics
    network->total_segments = total_segments;
    if (total_segments > 0) {
        network->mean_network_integrity = total_integrity / (float)total_segments;
        network->mean_network_g_ratio = total_g_ratio / (float)total_segments;
    }

    network->current_time = current_time;
    network->last_step_time = current_time;

    nimcp_mutex_unlock(&network->lock);
}

void myelin_network_get_stats(const myelin_sheath_network_t* network,
                               myelin_network_stats_t* stats)
{
    if (!network || !stats) return;

    memset(stats, 0, sizeof(myelin_network_stats_t));

    stats->total_sheaths = network->num_sheaths;
    stats->total_segments = (uint32_t)network->total_segments;
    stats->mean_integrity = network->mean_network_integrity;
    stats->mean_g_ratio = network->mean_network_g_ratio;
    stats->demyelinating_sheaths = network->demyelinating_count;
    stats->remyelinating_sheaths = network->remyelinating_count;

    // Count healthy/damaged
    float total_length = 0.0F;
    float total_velocity_ratio = 0.0F;

    for (uint32_t i = 0; i < network->num_sheaths; i++) {
        myelin_sheath_t* sheath = network->sheaths[i];
        if (sheath) {
            if (sheath->overall_health == MYELIN_HEALTH_INTACT) {
                stats->healthy_sheaths++;
            } else {
                stats->damaged_sheaths++;
            }
            total_length += sheath->total_length_um;
            total_velocity_ratio += sheath->velocity_ratio;
        }
    }

    stats->total_myelinated_length_um = total_length;
    if (network->num_sheaths > 0) {
        stats->mean_velocity_ratio = total_velocity_ratio / (float)network->num_sheaths;
    }

    // Pool stats
    if (network->sheath_pool) {
        stats->pool_sheaths_allocated = network->sheath_pool->allocated_count;
    }
    if (network->segment_pool) {
        stats->pool_segments_allocated = network->segment_pool->allocated_count;
    }
}

void myelin_network_rebuild_spatial_index(myelin_sheath_network_t* network)
{
    if (!network) return;

    // Placeholder - would integrate with KD-tree
    network->spatial_index_valid = true;
}

//=============================================================================
// Integration Helpers
//=============================================================================

myelin_sheath_t* myelin_network_create_sheath_for_axon(
    myelin_sheath_network_t* network,
    uint32_t axon_id,
    uint32_t oligo_id,
    float axon_length,
    float axon_diameter,
    float start_position)
{
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;

    }

    // Check if sheath already exists for this axon
    myelin_sheath_t* existing = myelin_network_find_by_axon(network, axon_id);
    if (existing) {
        return existing;  // Already myelinated
    }

    // Generate sheath ID
    uint32_t sheath_id = network->num_sheaths;

    // Create sheath with automatic segmentation
    myelin_sheath_t* sheath = myelin_sheath_create_for_axon(
        sheath_id, axon_id, oligo_id, axon_length, axon_diameter,
        network->config.max_segments_per_sheath);

    if (!sheath) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sheath is NULL");


        return NULL;


    }

    // Add to network
    if (myelin_network_add_sheath(network, sheath) != NIMCP_SUCCESS) {
        myelin_sheath_destroy(sheath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "myelin_network_create_sheath_for_axon: validation failed");
        return NULL;
    }

    return sheath;
}

float myelin_network_get_myelination_factor(myelin_sheath_network_t* network,
                                             uint32_t axon_id)
{
    if (!network) return 0.0F;

    myelin_sheath_t* sheath = myelin_network_find_by_axon(network, axon_id);
    if (!sheath) return 0.0F;

    // Weighted factor based on coverage, integrity, and lamellae
    float coverage = sheath->coverage_fraction;
    float integrity = sheath->mean_integrity;
    float lamellae_fraction = sheath->mean_lamellae / (float)NIMCP_MYELIN_OPTIMAL_LAMELLAE;
    lamellae_fraction = MYELIN_CLAMP(lamellae_fraction, 0.0F, 1.0F);

    return coverage * integrity * lamellae_fraction;
}

float myelin_network_get_velocity(myelin_sheath_network_t* network,
                                   uint32_t axon_id)
{
    if (!network) return NIMCP_MYELIN_BASE_VELOCITY_MS;

    myelin_sheath_t* sheath = myelin_network_find_by_axon(network, axon_id);
    if (!sheath) return NIMCP_MYELIN_BASE_VELOCITY_MS;

    return sheath->effective_velocity_ms;
}

float myelin_network_get_delay(myelin_sheath_network_t* network,
                                uint32_t axon_id)
{
    if (!network) return 0.0F;

    myelin_sheath_t* sheath = myelin_network_find_by_axon(network, axon_id);
    if (!sheath) return 0.0F;

    return sheath->total_delay_ms;
}

void myelin_network_apply_activity(myelin_sheath_network_t* network,
                                    uint32_t axon_id,
                                    float activity_level,
                                    float dt)
{
    if (!network || !network->config.enable_activity_dependence) return;

    myelin_sheath_t* sheath = myelin_network_find_by_axon(network, axon_id);
    if (!sheath) return;

    // Activity-dependent myelination
    if (activity_level > network->config.myelination_threshold) {
        // Increase myelination based on activity
        float rate = activity_level * 0.1F;  // Lamellae per second per Hz
        myelin_sheath_myelinate(sheath, rate, dt);
    }
}

//=============================================================================
// Enhanced Biophysics Functions (from nimcp_myelin_math.h integration)
//=============================================================================

float myelin_segment_compute_velocity_enhanced(myelin_segment_t* segment)
{
    if (!segment) return NIMCP_MYELIN_BASE_VELOCITY_MS;

    // Update cable parameters first
    myelin_segment_update_cable_params(segment);

    // Use enhanced saltatory conduction model
    float velocity = nimcp_myelin_saltatory_velocity(
        segment->inner_diameter_um,
        segment->length_um,
        segment->num_lamellae,
        segment->g_ratio,
        segment->compaction_score,
        segment->integrity,
        &segment->saltatory
    );

    // Store block probability
    segment->block_probability = segment->saltatory.block_probability;
    segment->is_conducting = !segment->saltatory.is_blocked;

    // Update local velocity
    segment->local_velocity_ms = velocity;
    segment->propagation_delay_ms = nimcp_myelin_propagation_delay(
        segment->length_um, velocity);

    return velocity;
}

void myelin_segment_update_cable_params(myelin_segment_t* segment)
{
    if (!segment) return;

    nimcp_myelin_compute_cable_params(
        segment->inner_diameter_um,
        segment->num_lamellae,
        &segment->cable_params
    );

    // Update optimal values based on diameter
    segment->optimal_g_ratio = nimcp_myelin_optimal_g_ratio(segment->inner_diameter_um);
    segment->optimal_internode_um = nimcp_myelin_optimal_internode(segment->inner_diameter_um);
    segment->internode_efficiency = nimcp_myelin_internode_efficiency(
        segment->length_um, segment->inner_diameter_um);
}

float myelin_segment_compute_optimal_g_ratio(myelin_segment_t* segment)
{
    if (!segment) return NIMCP_MYELIN_G_RATIO_OPTIMAL;

    segment->optimal_g_ratio = nimcp_myelin_optimal_g_ratio(segment->inner_diameter_um);
    return segment->optimal_g_ratio;
}

float myelin_segment_compute_optimal_internode(myelin_segment_t* segment)
{
    if (!segment) return NIMCP_MYELIN_MIN_INTERNODE_UM;

    segment->optimal_internode_um = nimcp_myelin_optimal_internode(segment->inner_diameter_um);
    return segment->optimal_internode_um;
}

bool myelin_segment_check_block(myelin_segment_t* segment, float temperature_c)
{
    if (!segment) return true;  // Block by default if invalid

    nimcp_conduction_block_params_t params = nimcp_myelin_block_params_default();

    // Use deterministic check (threshold-based)
    float block_prob = nimcp_myelin_block_probability(
        segment->integrity, temperature_c, &params);

    segment->block_probability = block_prob;
    segment->is_conducting = (block_prob < 0.5F);

    return !segment->is_conducting;
}

float myelin_segment_get_block_probability(myelin_segment_t* segment, float temperature_c)
{
    if (!segment) return 1.0F;

    nimcp_conduction_block_params_t params = nimcp_myelin_block_params_default();

    segment->block_probability = nimcp_myelin_block_probability(
        segment->integrity, temperature_c, &params);

    return segment->block_probability;
}

float myelin_segment_apply_activity_plasticity(myelin_segment_t* segment,
                                                float activity,
                                                float dt)
{
    if (!segment || dt <= 0.0F) return 0.0F;

    nimcp_myelination_kinetics_t kinetics = nimcp_myelin_kinetics_default();

    // Compute rate and update lamellae
    float current_lamellae = (float)segment->num_lamellae;
    float rate = nimcp_myelin_compute_myelination_rate(activity, current_lamellae, &kinetics);

    float new_lamellae = nimcp_myelin_update_lamellae(current_lamellae, activity, dt, &kinetics);

    // Apply change
    float delta = new_lamellae - current_lamellae;

    if (fabsf(delta) >= 0.5F) {
        uint32_t updated_lamellae = (uint32_t)(new_lamellae + 0.5F);
        updated_lamellae = (updated_lamellae < NIMCP_MYELIN_MIN_LAMELLAE) ?
                           NIMCP_MYELIN_MIN_LAMELLAE : updated_lamellae;
        updated_lamellae = (updated_lamellae > NIMCP_MYELIN_MAX_LAMELLAE) ?
                           NIMCP_MYELIN_MAX_LAMELLAE : updated_lamellae;

        myelin_segment_set_lamellae(segment, updated_lamellae);
    }

    return delta;
}

nimcp_result_t myelin_sheath_init_biophysics(myelin_sheath_t* sheath,
                                              bool use_stochastic,
                                              uint64_t seed)
{
    NIMCP_CHECK_THROW(sheath, NIMCP_ERROR_NULL_POINTER, "sheath is NULL");

    // Create biophysics state
    sheath->biophysics = nimcp_myelin_biophysics_create(use_stochastic, seed);
    if (!sheath->biophysics) {
        return NIMCP_ERROR_MEMORY;
    }

    // Initialize temperature to normal body temperature
    sheath->current_temperature_c = 37.0F;
    sheath->activity_ema = 0.0F;

    // Update all segments with enhanced calculations
    myelin_sheath_update_biophysics(sheath);

    return NIMCP_SUCCESS;
}

void myelin_sheath_set_temperature(myelin_sheath_t* sheath, float temperature_c)
{
    if (!sheath) return;

    sheath->current_temperature_c = MYELIN_CLAMP(temperature_c, 20.0F, 45.0F);

    if (sheath->biophysics) {
        sheath->biophysics->temperature_c = sheath->current_temperature_c;
    }
}

void myelin_sheath_compute_metabolic_efficiency(myelin_sheath_t* sheath)
{
    if (!sheath || sheath->num_segments == 0) return;

    // Calculate mean compaction and integrity
    float mean_compaction = 0.0F;
    float mean_integrity = 0.0F;

    for (uint32_t i = 0; i < sheath->num_segments; i++) {
        if (sheath->segments[i]) {
            mean_compaction += sheath->segments[i]->compaction_score;
            mean_integrity += sheath->segments[i]->integrity;
        }
    }
    mean_compaction /= (float)sheath->num_segments;
    mean_integrity /= (float)sheath->num_segments;

    // Compute metabolic efficiency
    nimcp_myelin_compute_metabolic_efficiency(
        sheath->total_length_um,
        sheath->segments[0] ? sheath->segments[0]->inner_diameter_um : 1.0F,
        sheath->num_segments,
        mean_compaction,
        mean_integrity,
        &sheath->metabolic_efficiency
    );
}

float myelin_sheath_get_atp_per_ap(const myelin_sheath_t* sheath)
{
    if (!sheath) return 0.0F;
    return sheath->metabolic_efficiency.atp_per_ap;
}

float myelin_sheath_get_efficiency_ratio(const myelin_sheath_t* sheath)
{
    if (!sheath) return 1.0F;
    return sheath->metabolic_efficiency.efficiency_ratio;
}

void myelin_sheath_update_biophysics(myelin_sheath_t* sheath)
{
    if (!sheath) return;

    nimcp_spinlock_lock(&sheath->lock);

    // Update each segment
    for (uint32_t i = 0; i < sheath->num_segments; i++) {
        myelin_segment_t* seg = sheath->segments[i];
        if (seg) {
            // Update cable parameters
            myelin_segment_update_cable_params(seg);

            // Update velocity with enhanced model
            myelin_segment_compute_velocity_enhanced(seg);

            // Update block probability
            myelin_segment_get_block_probability(seg, sheath->current_temperature_c);
        }
    }

    nimcp_spinlock_unlock(&sheath->lock);

    // Update aggregate conduction
    myelin_sheath_update_conduction(sheath);

    // Update metabolic efficiency
    myelin_sheath_compute_metabolic_efficiency(sheath);
}

void myelin_sheath_apply_variability(myelin_sheath_t* sheath)
{
    if (!sheath || !sheath->biophysics || !sheath->biophysics->use_stochastic) return;

    nimcp_spinlock_lock(&sheath->lock);

    nimcp_myelin_rng_t* rng = &sheath->biophysics->rng;

    for (uint32_t i = 0; i < sheath->num_segments; i++) {
        myelin_segment_t* seg = sheath->segments[i];
        if (seg) {
            // Apply variability to lamellae count
            uint32_t varied_lamellae = nimcp_myelin_vary_lamellae(rng, seg->num_lamellae);
            if (varied_lamellae != seg->num_lamellae) {
                myelin_segment_set_lamellae(seg, varied_lamellae);
            }

            // Apply variability to internode length
            seg->length_um = nimcp_myelin_vary_internode(rng, seg->length_um);

            // Apply variability to g-ratio (recalculate from varied lamellae)
            float varied_g = nimcp_myelin_vary_g_ratio(rng, seg->g_ratio);
            seg->g_ratio = varied_g;
        }
    }

    nimcp_spinlock_unlock(&sheath->lock);

    // Recalculate after variability applied
    myelin_sheath_update_biophysics(sheath);
}

float myelin_sheath_get_frequency_threshold(const myelin_sheath_t* sheath,
                                             float frequency_hz)
{
    if (!sheath) return 1.0F;

    nimcp_conduction_block_params_t params = nimcp_myelin_block_params_default();

    return nimcp_myelin_frequency_threshold(
        frequency_hz, sheath->current_temperature_c, &params);
}

void myelin_segment_get_saltatory_result(const myelin_segment_t* segment,
                                          nimcp_saltatory_result_t* result)
{
    if (!segment || !result) return;
    *result = segment->saltatory;
}

void myelin_segment_get_cable_params(const myelin_segment_t* segment,
                                      nimcp_cable_params_t* params)
{
    if (!segment || !params) return;
    *params = segment->cable_params;
}
