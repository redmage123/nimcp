/**
 * @file nimcp_dendrite.c
 * @brief Dendrite module implementation with biological accuracy
 *
 * WHAT: Complete dendrite implementation with segments, spines, and dynamics
 * WHY:  Enable realistic dendritic computation, compartmentalization, and plasticity
 * HOW:  Cable theory, spine morphology, calcium dynamics, LTP/LTD integration
 *
 * FEATURES:
 * - Dendritic segments with cable properties
 * - Dendritic spines (thin, stubby, mushroom, filopodia)
 * - Calcium dynamics and plasticity
 * - Structural plasticity (spine formation/elimination)
 * - Attenuation and integration windows
 * - Thread-safe operations
 *
 * INTEGRATION:
 * - Neurons: Multiple dendrites per neuron
 * - Synapses: Connect to specific spines
 * - Axons: Complete signal propagation chain
 *
 * @version Phase: Dendrite Integration
 * @date 2025-11-24
 */

#include "core/dendrite/nimcp_dendrite.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include <math.h>
#include <string.h>

#include <stddef.h>  /* for NULL */
// === BIO-ASYNC + UNIFIED MEMORY INTEGRATION ===
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "dendrite"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dendrite)

#define BIO_MODULE_ID 0x0130


// ============================================================================
// Constants
// ============================================================================

// Biological constants
#define DENDRITE_DEFAULT_DIAMETER 2.0f      // μm
#define DENDRITE_DEFAULT_LENGTH 100.0f      // μm
#define DENDRITE_R_M 20000.0f              // Ω·cm² (membrane resistance)
#define DENDRITE_C_M 1.0f                   // μF/cm² (membrane capacitance)
#define DENDRITE_R_A 150.0f                 // Ω·cm (axial resistance)

// Spine constants
#define SPINE_NECK_LENGTH_THIN 1.0f         // μm
#define SPINE_NECK_LENGTH_STUBBY 0.2f       // μm
#define SPINE_NECK_LENGTH_MUSHROOM 0.8f     // μm
#define SPINE_NECK_LENGTH_FILOPODIA 2.0f    // μm

#define SPINE_HEAD_DIAMETER_THIN 0.3f       // μm
#define SPINE_HEAD_DIAMETER_STUBBY 0.6f     // μm
#define SPINE_HEAD_DIAMETER_MUSHROOM 0.8f   // μm
#define SPINE_HEAD_DIAMETER_FILOPODIA 0.2f  // μm

// Plasticity constants
#define LTP_THRESHOLD_DEFAULT 0.8f          // mV (calcium threshold for LTP)
#define LTD_THRESHOLD_DEFAULT 0.3f          // mV (calcium threshold for LTD)
#define STRUCTURAL_PLASTICITY_RATE 0.001f   // Per timestep

// Calcium dynamics
#define CALCIUM_DECAY_TAU 20.0f             // ms
#define CALCIUM_BUFFER_CAPACITY 100.0f      // Buffering capacity

// NMDA spike parameters
#define NMDA_MG_BLOCK_CONSTANT 0.062f       // Mg²⁺ block voltage sensitivity (1/mV)
#define NMDA_MG_CONCENTRATION 1.0f          // External [Mg²⁺] (mM)
#define NMDA_CALCIUM_PERMEABILITY 10.0f     // Ca²⁺ permeability factor

// bAP parameters
#define BAP_NA_BOOST_FACTOR 1.5f            // Na+ channel boost at distal
#define BAP_CALCIUM_PER_MV 0.1f             // Ca²⁺ influx per mV of bAP

// ============================================================================
// Helper Function Prototypes
// ============================================================================

static dendritic_segment_t* create_segment(
    uint32_t id,
    dendrite_segment_type_t type,
    uint32_t parent_segment,
    float position[3],
    float length,
    float diameter
);

static dendritic_spine_t* create_spine(
    uint32_t id,
    spine_type_t type,
    uint32_t dendrite_id,
    uint32_t segment_id
);

static void calculate_cable_properties(dendritic_segment_t* segment);
static void update_segment_voltage(dendritic_segment_t* segment, float dt_ms);
static void update_segment_calcium(dendritic_segment_t* segment, float dt_ms);
static float calculate_attenuation(dendrite_t* dendrite, uint32_t segment_id);

// ============================================================================
// Dendrite Creation and Destruction
// ============================================================================

/**
 * WHAT: Create dendrite with specified configuration
 * WHY:  Initialize dendrite structure with biological properties
 * HOW:  Allocate memory, set parameters, initialize mutex
 */
dendrite_t* dendrite_create(dendrite_config_t* config) {
    // Guard: NULL config
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_create: config is NULL");
        return NULL;
    }

    // Allocate dendrite
    dendrite_t* dendrite = (dendrite_t*)nimcp_calloc(1, sizeof(dendrite_t));
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_create: failed to allocate dendrite");
        return NULL;
    }

    // Initialize basic properties
    dendrite->id = config->id;
    dendrite->type = config->type;
    dendrite->state = DENDRITE_STATE_NORMAL;
    dendrite->target_neuron_id = config->target_neuron_id;

    // Initialize morphology
    dendrite->total_length = config->total_length > 0 ? config->total_length : DENDRITE_DEFAULT_LENGTH;
    dendrite->mean_diameter = config->mean_diameter > 0 ? config->mean_diameter : DENDRITE_DEFAULT_DIAMETER;

    memcpy(dendrite->start_pos, config->start_pos, sizeof(float) * 3);

    // Allocate segments array
    dendrite->num_segments = 0;
    dendrite->segments = NULL;  // Will be allocated when creating segments

    // Allocate spines array
    dendrite->num_spines = 0;
    dendrite->spines = NULL;  // Will be allocated when adding spines

    // Initialize electrical properties (will be calculated after adding segments)
    dendrite->input_resistance = 0.0F;
    dendrite->time_constant = 0.0F;
    dendrite->attenuation_factor = 0.0F;
    dendrite->integration_window_ms = config->integration_window_ms > 0 ?
                                      config->integration_window_ms : 20.0F;

    // Initialize voltages
    dendrite->somatic_voltage = 0.0F;
    dendrite->mean_voltage = 0.0F;
    dendrite->calcium_level = 0.0F;

    // Initialize activity stats
    memset(&dendrite->activity, 0, sizeof(dendrite_activity_stats_t));

    // Initialize plasticity
    dendrite->structural_plasticity = config->structural_plasticity;
    dendrite->ltp_threshold = config->ltp_threshold > 0 ?
                              config->ltp_threshold : LTP_THRESHOLD_DEFAULT;
    dendrite->ltd_threshold = config->ltd_threshold > 0 ?
                              config->ltd_threshold : LTD_THRESHOLD_DEFAULT;

    // Initialize health
    dendrite->atp_level = 1.0F;
    dendrite->damage = 0.0F;
    dendrite->is_functional = true;

    // Initialize mutex
    nimcp_mutex_init(&dendrite->lock, NULL);

    LOG_DEBUG(LOG_MODULE, "dendrite_create: Created dendrite %u (type=%d)",
              dendrite->id, dendrite->type);

    return dendrite;
}

/**
 * WHAT: Destroy dendrite and free all resources
 * WHY:  Clean up memory when dendrite no longer needed
 * HOW:  Free segments, spines, destroy mutex
 */
void dendrite_destroy(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) {
        return;
    }

    LOG_DEBUG(LOG_MODULE, "dendrite_destroy: Destroying dendrite %u", dendrite->id);

    // Free segments
    if (dendrite->segments) {
        nimcp_free(dendrite->segments);
        dendrite->segments = NULL;
    }

    // Free spines
    if (dendrite->spines) {
        nimcp_free(dendrite->spines);
        dendrite->spines = NULL;
    }

    // Destroy mutex
    nimcp_mutex_destroy(&dendrite->lock);

    // Free dendrite
    nimcp_free(dendrite);
}

// ============================================================================
// Segment Management
// ============================================================================

/**
 * WHAT: Create dendritic segments for the dendrite
 * WHY:  Build compartmental structure for cable theory
 * HOW:  Allocate segment array, set up parent-child relationships
 */
bool dendrite_create_segments(
    dendrite_t* dendrite,
    uint32_t num_segments,
    segment_config_t* segment_configs
) {
    // Guard: NULL inputs
    if (!dendrite || !segment_configs || num_segments == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_create_segments: invalid inputs (NULL or zero segments)");
        return false;
    }

    // Guard: Already has segments
    if (dendrite->segments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "dendrite_create_segments: dendrite already has segments");
        return false;
    }

    // Allocate segments
    dendrite->segments = (dendritic_segment_t*)nimcp_calloc(
        num_segments,
        sizeof(dendritic_segment_t)
    );
    if (!dendrite->segments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_create_segments: failed to allocate segments");
        return false;
    }

    // Create each segment
    for (uint32_t i = 0; i < num_segments; i++) {
        segment_config_t* config = &segment_configs[i];

        dendritic_segment_t* segment = &dendrite->segments[i];
        segment->id = i;
        segment->type = config->type;
        segment->parent_segment = config->parent_segment;
        segment->num_children = 0;

        memcpy(segment->position, config->position, sizeof(float) * 3);
        segment->length = config->length;
        segment->diameter = config->diameter;
        segment->path_distance = config->path_distance;

        // Calculate cable properties
        calculate_cable_properties(segment);

        // Initialize electrical state
        segment->voltage = 0.0F;
        segment->calcium = 0.0F;

        // Initialize spines
        segment->num_spines = 0;
        memset(segment->spine_ids, 0, sizeof(segment->spine_ids));

        // Active properties
        segment->has_active_properties = config->has_active_properties;
        if (segment->has_active_properties) {
            segment->g_na = 120.0F;  // mS/cm²
            segment->g_k = 36.0F;    // mS/cm²
            segment->g_ca = 1.0F;    // mS/cm²
        } else {
            segment->g_na = 0.0F;
            segment->g_k = 0.0F;
            segment->g_ca = 0.0F;
        }
    }

    dendrite->num_segments = num_segments;

    // Build parent-child relationships with bounds checking
    for (uint32_t i = 0; i < num_segments; i++) {
        dendritic_segment_t* segment = &dendrite->segments[i];
        if (segment->parent_segment != UINT32_MAX) {
            // Bounds check: parent_segment must be valid index
            if (segment->parent_segment >= num_segments) {
                LOG_ERROR(LOG_MODULE, "dendrite_create_segments: Invalid parent_segment %u for segment %u",
                          segment->parent_segment, i);
                continue;  // Skip invalid parent reference
            }
            dendritic_segment_t* parent = &dendrite->segments[segment->parent_segment];
            if (parent->num_children < NIMCP_DENDRITE_MAX_CHILDREN) {
                parent->child_segments[parent->num_children++] = i;
            } else {
                LOG_WARN(LOG_MODULE, "dendrite_create_segments: Parent segment %u has max children",
                         segment->parent_segment);
            }
        }
    }

    LOG_DEBUG(LOG_MODULE, "dendrite_create_segments: Created %u segments for dendrite %u",
              num_segments, dendrite->id);

    return true;
}

/**
 * WHAT: Add branch to dendrite at specified segment
 * WHY:  Support dendritic tree morphology
 * HOW:  Create new segment, attach to parent
 */
uint32_t dendrite_add_branch(
    dendrite_t* dendrite,
    uint32_t parent_segment_id,
    float length,
    float diameter,
    float branch_angle
) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_add_branch: dendrite is NULL");
        return UINT32_MAX;
    }

    // Guard: Invalid parent
    if (parent_segment_id >= dendrite->num_segments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_add_branch: invalid parent segment id");
        return UINT32_MAX;
    }

    dendritic_segment_t* parent = &dendrite->segments[parent_segment_id];

    // Guard: Parent has too many children
    if (parent->num_children >= NIMCP_DENDRITE_MAX_CHILDREN) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "dendrite_add_branch: parent has max children");
        return UINT32_MAX;
    }

    // Reallocate segments array
    uint32_t new_id = dendrite->num_segments;
    uint32_t new_count = dendrite->num_segments + 1;

    // Use temp variable to prevent memory leak if realloc fails
    dendritic_segment_t* new_segments = (dendritic_segment_t*)nimcp_realloc(
        dendrite->segments,
        new_count * sizeof(dendritic_segment_t)
    );
    if (!new_segments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_add_branch: failed to reallocate segments");
        return UINT32_MAX;
    }
    dendrite->segments = new_segments;
    dendrite->num_segments = new_count;

    // Re-get parent pointer after realloc (memory may have moved)
    parent = &dendrite->segments[parent_segment_id];

    // Initialize new segment
    dendritic_segment_t* new_segment = &dendrite->segments[new_id];
    memset(new_segment, 0, sizeof(dendritic_segment_t));

    new_segment->id = new_id;
    new_segment->type = DENDRITE_SEGMENT_OBLIQUE;
    new_segment->parent_segment = parent_segment_id;
    new_segment->num_children = 0;

    // Calculate position based on parent and branch angle
    float dx = cosf(branch_angle) * length;
    float dy = sinf(branch_angle) * length;
    new_segment->position[0] = parent->position[0] + dx;
    new_segment->position[1] = parent->position[1] + dy;
    new_segment->position[2] = parent->position[2];

    new_segment->length = length;
    new_segment->diameter = diameter;
    new_segment->path_distance = parent->path_distance + length;

    // Calculate cable properties
    calculate_cable_properties(new_segment);

    // Add to parent's children
    parent->child_segments[parent->num_children++] = new_id;

    LOG_DEBUG(LOG_MODULE, "dendrite_add_branch: Added branch segment %u to dendrite %u",
              new_id, dendrite->id);

    return new_id;
}

// ============================================================================
// Spine Management
// ============================================================================

/**
 * WHAT: Add dendritic spine to segment
 * WHY:  Enable synaptic connections with morphological detail
 * HOW:  Create spine structure, link to segment
 */
uint32_t dendrite_add_spine(
    dendrite_t* dendrite,
    uint32_t segment_id,
    spine_type_t type,
    uint32_t synapse_id
) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_add_spine: dendrite is NULL");
        return UINT32_MAX;
    }

    // Guard: Invalid segment
    if (segment_id >= dendrite->num_segments) {
        LOG_ERROR(LOG_MODULE, "dendrite_add_spine: Invalid segment %u", segment_id);
        return UINT32_MAX;
    }

    dendritic_segment_t* segment = &dendrite->segments[segment_id];

    // Guard: Segment has too many spines (use tier-optimized limit)
    if (segment->num_spines >= NIMCP_MAX_SPINE_IDS) {
        LOG_ERROR(LOG_MODULE, "dendrite_add_spine: Segment %u has max spines (%u)",
                  segment_id, NIMCP_MAX_SPINE_IDS);
        return UINT32_MAX;
    }

    // Reallocate spines array
    uint32_t new_id = dendrite->num_spines;
    uint32_t new_count = dendrite->num_spines + 1;

    // Use temp variable to prevent memory leak if realloc fails
    dendritic_spine_t* new_spines = (dendritic_spine_t*)nimcp_realloc(
        dendrite->spines,
        new_count * sizeof(dendritic_spine_t)
    );
    if (!new_spines) {
        LOG_ERROR(LOG_MODULE, "dendrite_add_spine: Failed to reallocate spines");
        return UINT32_MAX;
    }
    dendrite->spines = new_spines;
    dendrite->num_spines = new_count;

    // Initialize new spine
    dendritic_spine_t* spine = &dendrite->spines[new_id];
    memset(spine, 0, sizeof(dendritic_spine_t));

    spine->id = new_id;
    spine->type = type;
    spine->state = SPINE_STATE_STABLE;
    spine->dendrite_id = dendrite->id;
    spine->segment_id = segment_id;
    spine->synapse_id = synapse_id;

    // Set morphology based on type
    switch (type) {
        case SPINE_TYPE_THIN:
            spine->neck_length = SPINE_NECK_LENGTH_THIN;
            spine->neck_diameter = 0.1F;
            spine->head_diameter = SPINE_HEAD_DIAMETER_THIN;
            break;
        case SPINE_TYPE_STUBBY:
            spine->neck_length = SPINE_NECK_LENGTH_STUBBY;
            spine->neck_diameter = 0.3F;
            spine->head_diameter = SPINE_HEAD_DIAMETER_STUBBY;
            break;
        case SPINE_TYPE_MUSHROOM:
            spine->neck_length = SPINE_NECK_LENGTH_MUSHROOM;
            spine->neck_diameter = 0.15F;
            spine->head_diameter = SPINE_HEAD_DIAMETER_MUSHROOM;
            break;
        case SPINE_TYPE_FILOPODIA:
            spine->neck_length = SPINE_NECK_LENGTH_FILOPODIA;
            spine->neck_diameter = 0.05F;
            spine->head_diameter = SPINE_HEAD_DIAMETER_FILOPODIA;
            break;
        default:
            spine->neck_length = 1.0F;
            spine->neck_diameter = 0.1F;
            spine->head_diameter = 0.3F;
            break;
    }

    // Calculate head volume (sphere approximation)
    float radius = spine->head_diameter / 2.0F;
    spine->head_volume = (4.0F / 3.0F) * M_PI * radius * radius * radius;

    // Calculate electrical properties
    spine->neck_resistance = (4.0F * DENDRITE_R_A * spine->neck_length) /
                             (M_PI * spine->neck_diameter * spine->neck_diameter);
    spine->head_capacitance = DENDRITE_C_M * 4.0F * M_PI * radius * radius;

    // Initialize state
    spine->voltage = 0.0F;
    spine->calcium = 0.0F;

    // Initialize synaptic properties
    spine->synaptic_weight = 1.0F;
    spine->ampa_receptors = 100.0F;
    spine->nmda_receptors = 20.0F;

    // Initialize plasticity
    spine->growth_factor = 1.0F;
    spine->stability = 0.5F;
    spine->creation_time = 0;  // Will be set by caller

    // Add to segment's spine list
    segment->spine_ids[segment->num_spines++] = new_id;

    LOG_DEBUG(LOG_MODULE, "dendrite_add_spine: Added spine %u (type=%d) to segment %u",
              new_id, type, segment_id);

    return new_id;
}

/**
 * WHAT: Remove dendritic spine
 * WHY:  Support structural plasticity and spine elimination
 * HOW:  Remove from segment list, mark as inactive
 */
bool dendrite_remove_spine(dendrite_t* dendrite, uint32_t spine_id) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_remove_spine: dendrite is NULL");
        return false;
    }

    // Guard: Invalid spine ID
    if (spine_id >= dendrite->num_spines) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_remove_spine: capacity exceeded");
        return false;
    }

    dendritic_spine_t* spine = &dendrite->spines[spine_id];
    uint32_t segment_id = spine->segment_id;

    // Guard: Invalid segment
    if (segment_id >= dendrite->num_segments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_remove_spine: capacity exceeded");
        return false;
    }

    dendritic_segment_t* segment = &dendrite->segments[segment_id];

    // Remove from segment's spine list
    for (uint32_t i = 0; i < segment->num_spines; i++) {
        if (segment->spine_ids[i] == spine_id) {
            // Shift remaining spines
            for (uint32_t j = i; j < segment->num_spines - 1; j++) {
                segment->spine_ids[j] = segment->spine_ids[j + 1];
            }
            segment->num_spines--;
            break;
        }
    }

    // Mark spine as eliminated
    spine->state = SPINE_STATE_ELIMINATED;
    spine->stability = 0.0F;

    return true;
}

// ============================================================================
// Cable Theory and Electrical Properties
// ============================================================================

/**
 * WHAT: Calculate cable properties for segment
 * WHY:  Determine electrical behavior using cable theory
 * HOW:  Compute R_m, C_m, R_a from morphology
 */
static void calculate_cable_properties(dendritic_segment_t* segment) {
    // Guard: NULL segment
    if (!segment) {
        return;
    }

    // Surface area (cylinder)
    float surface_area = M_PI * segment->diameter * segment->length;  // μm²

    // Convert to cm²
    surface_area *= 1e-8F;  // 1 μm² = 1e-8 cm²

    // Membrane resistance (Ω)
    segment->R_m = DENDRITE_R_M / surface_area;

    // Membrane capacitance (μF)
    segment->C_m = DENDRITE_C_M * surface_area;

    // Axial resistance (Ω)
    float cross_section = M_PI * (segment->diameter / 2.0F) * (segment->diameter / 2.0F);  // μm²
    cross_section *= 1e-8F;  // Convert to cm²
    segment->R_a = (DENDRITE_R_A * segment->length * 1e-4F) / cross_section;  // Convert length to cm

    // Path resistance (accumulated)
    segment->path_resistance = segment->R_a;
}

/**
 * WHAT: Receive input at specific segment
 * WHY:  Inject synaptic current at compartment location
 * HOW:  Add current to segment voltage
 */
bool dendrite_receive_input(
    dendrite_t* dendrite,
    uint32_t segment_id,
    float current,
    uint64_t timestamp
) {
    // Guard: NULL dendrite
    if (!dendrite || segment_id >= dendrite->num_segments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_receive_input: dendrite is NULL");
        return false;
    }

    nimcp_mutex_lock(&dendrite->lock);

    dendritic_segment_t* segment = &dendrite->segments[segment_id];

    // Add current (will be integrated in voltage update)
    segment->voltage += current * segment->R_m;

    // Update calcium based on NMDA component
    if (current > 0) {
        segment->calcium += current * 0.1F;  // Simplified calcium influx
    }

    // Update activity stats
    dendrite->activity.total_inputs++;
    dendrite->activity.mean_input_rate =
        (dendrite->activity.mean_input_rate * 0.99F) + 0.01F;

    nimcp_mutex_unlock(&dendrite->lock);

    return true;
}

/**
 * WHAT: Compute total somatic current from dendrite
 * WHY:  Integrate dendritic inputs to soma
 * HOW:  Sum attenuated currents from all segments
 */
float dendrite_compute_somatic_current(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) {
        return 0.0F;
    }

    nimcp_mutex_lock(&dendrite->lock);

    float total_current = 0.0F;

    // Sum attenuated voltages from all segments
    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        dendritic_segment_t* segment = &dendrite->segments[i];

        // Calculate attenuation factor
        float attenuation = calculate_attenuation(dendrite, i);

        // Add attenuated current
        total_current += segment->voltage * attenuation;
    }

    dendrite->somatic_voltage = total_current;

    nimcp_mutex_unlock(&dendrite->lock);

    return total_current;
}

/**
 * WHAT: Calculate signal attenuation from segment to soma
 * WHY:  Determine how much signal reaches soma (cable theory)
 * HOW:  Use path resistance and length constant
 */
static float calculate_attenuation(dendrite_t* dendrite, uint32_t segment_id) {
    // Guard: Invalid inputs
    if (!dendrite || segment_id >= dendrite->num_segments) {
        return 0.0F;
    }

    dendritic_segment_t* segment = &dendrite->segments[segment_id];

    // Length constant λ = sqrt(R_m / R_a)
    float lambda = (segment->R_a > 1e-10f) ? sqrtf(segment->R_m / segment->R_a) : 1.0f;

    // Attenuation factor: exp(-distance / lambda)
    float attenuation = (lambda > 1e-10f)
        ? expf(fminf(-segment->path_distance / lambda, 88.0f))
        : 0.0f;
    if (!isfinite(attenuation)) attenuation = 0.0f;

    return attenuation;
}

/**
 * WHAT: Get attenuation factor for segment
 * WHY:  Query cable properties for analysis
 * HOW:  Calculate and return attenuation
 */
float dendrite_get_attenuation(dendrite_t* dendrite, uint32_t segment_id) {
    // Guard: NULL dendrite
    if (!dendrite) {
        return 0.0F;
    }

    nimcp_mutex_lock(&dendrite->lock);
    float attenuation = calculate_attenuation(dendrite, segment_id);
    nimcp_mutex_unlock(&dendrite->lock);

    return attenuation;
}

// ============================================================================
// Time Evolution
// ============================================================================

/**
 * WHAT: Advance dendrite simulation by one timestep
 * WHY:  Update voltage, calcium, and plasticity
 * HOW:  Integrate differential equations for all segments
 */
void dendrite_step(dendrite_t* dendrite, float dt_ms, uint64_t timestamp) {
    // Guard: NULL dendrite
    if (!dendrite) {
        return;
    }

    nimcp_mutex_lock(&dendrite->lock);

    // Dendritic cable equations change slowly — full computation every 10 steps
    // Interpolate between full computations using cached gradient
    dendrite->update_counter++;
    if (dendrite->update_counter % 10 != 0 && dendrite->update_counter > 10) {
        // Interpolate: use cached gradient from last full computation
        dendrite->mean_voltage += dendrite->cached_mean_dv * (dt_ms / 1000.0F);
        // Clamp interpolated voltage
        if (dendrite->mean_voltage < -0.080F) dendrite->mean_voltage = -0.080F;
        if (dendrite->mean_voltage > 0.040F) dendrite->mean_voltage = 0.040F;
        dendrite->activity.mean_voltage = dendrite->mean_voltage;
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }

    float old_mean_voltage = dendrite->mean_voltage;

    // Update all segments (full compartmental model computation)
    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        update_segment_voltage(&dendrite->segments[i], dt_ms);
        update_segment_calcium(&dendrite->segments[i], dt_ms);
    }

    // BUGFIX: Guard against division by zero when num_segments is 0
    if (dendrite->num_segments > 0) {
        // Update mean voltage
        float voltage_sum = 0.0F;
        for (uint32_t i = 0; i < dendrite->num_segments; i++) {
            voltage_sum += dendrite->segments[i].voltage;
        }
        dendrite->mean_voltage = voltage_sum / (float)dendrite->num_segments;

        // Cache voltage gradient for interpolation in skip-frames
        float dt_s = dt_ms / 1000.0F;
        if (dt_s > 1e-10F) {
            dendrite->cached_mean_dv = (dendrite->mean_voltage - old_mean_voltage) / dt_s;
        }

        // Update calcium level (mean across segments)
        float calcium_sum = 0.0F;
        for (uint32_t i = 0; i < dendrite->num_segments; i++) {
            calcium_sum += dendrite->segments[i].calcium;
        }
        dendrite->calcium_level = calcium_sum / (float)dendrite->num_segments;
        if (!isfinite(dendrite->calcium_level)) dendrite->calcium_level = 0.0F;

        // Update activity stats
        dendrite->activity.mean_voltage = dendrite->mean_voltage;
        dendrite->activity.mean_calcium = dendrite->calcium_level;
    } else {
        // No segments - set to resting values
        dendrite->mean_voltage = 0.0F;
        dendrite->calcium_level = 0.0F;
        dendrite->activity.mean_voltage = 0.0F;
        dendrite->activity.mean_calcium = 0.0F;
        dendrite->cached_mean_dv = 0.0F;
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

/**
 * WHAT: Update segment voltage using cable equation
 * WHY:  Model voltage dynamics in compartment
 * HOW:  dV/dt = (I_syn - V/R_m + I_axial) / C_m
 */
static void update_segment_voltage(dendritic_segment_t* segment, float dt_ms) {
    // Guard: NULL segment
    if (!segment) {
        return;
    }

    // Leak current
    float I_leak = -segment->voltage / segment->R_m;

    // Axial current (simplified - from parent)
    float I_axial = 0.0F;  // Would need parent reference for full implementation

    // Active currents (if segment has active properties)
    float I_active = 0.0F;
    if (segment->has_active_properties && segment->voltage > 0.5F) {
        // Simplified Hodgkin-Huxley
        I_active = -segment->g_na * segment->voltage;  // Sodium
        I_active += -segment->g_k * (segment->voltage + 0.012F);  // Potassium
    }

    // Total current
    float I_total = I_leak + I_axial + I_active;

    // Update voltage: dV/dt = I / C_m
    float dV = (segment->C_m > 1e-10f) ? (I_total / segment->C_m) * (dt_ms / 1000.0F) : 0.0F;
    segment->voltage += dV;
    if (!isfinite(segment->voltage)) segment->voltage = 0.0F;

    // Clamp voltage
    if (segment->voltage < -0.080F) segment->voltage = -0.080F;  // -80 mV
    if (segment->voltage > 0.040F) segment->voltage = 0.040F;    // +40 mV
}

/**
 * WHAT: Update segment calcium concentration
 * WHY:  Model calcium dynamics for plasticity
 * HOW:  Exponential decay with buffering
 */
static void update_segment_calcium(dendritic_segment_t* segment, float dt_ms) {
    // Guard: NULL segment
    if (!segment) {
        return;
    }

    // Calcium decay: dCa/dt = -Ca / tau
    float decay_factor = expf(fminf(-dt_ms / CALCIUM_DECAY_TAU, 88.0f));
    segment->calcium *= decay_factor;

    // Clamp calcium
    if (segment->calcium < 0.0F) segment->calcium = 0.0F;
    if (segment->calcium > CALCIUM_BUFFER_CAPACITY) {
        segment->calcium = CALCIUM_BUFFER_CAPACITY;
    }
}

/**
 * WHAT: Update calcium level for dendrite
 * WHY:  Integrate calcium across all segments
 * HOW:  Average segment calcium levels
 */
void dendrite_update_calcium(dendrite_t* dendrite, float dt_ms) {
    // Guard: NULL dendrite
    if (!dendrite) {
        return;
    }

    nimcp_mutex_lock(&dendrite->lock);

    // Already updated in dendrite_step
    // This function provides explicit calcium update interface

    nimcp_mutex_unlock(&dendrite->lock);
}

// ============================================================================
// Plasticity
// ============================================================================

/**
 * WHAT: Induce LTP at dendrite
 * WHY:  Strengthen synaptic connections
 * HOW:  Increase spine receptor density based on calcium
 */
void dendrite_induce_ltp(dendrite_t* dendrite, uint32_t spine_id, float magnitude) {
    // Guard: Invalid inputs
    if (!dendrite || spine_id >= dendrite->num_spines) {
        return;
    }

    nimcp_mutex_lock(&dendrite->lock);

    dendritic_spine_t* spine = &dendrite->spines[spine_id];

    // Check calcium threshold
    if (spine->calcium >= dendrite->ltp_threshold) {
        // Increase synaptic weight
        spine->synaptic_weight += magnitude * 0.1F;

        // Increase receptor density
        spine->ampa_receptors *= (1.0F + magnitude * 0.05F);
        spine->nmda_receptors *= (1.0F + magnitude * 0.03F);

        // Increase spine size (mushroom-like)
        spine->head_diameter *= (1.0F + magnitude * 0.02F);

        // Update stability
        spine->stability = fminf(1.0F, spine->stability + magnitude * 0.1F);

        // Update activity stats
        dendrite->activity.ltp_events++;

        LOG_DEBUG(LOG_MODULE, "dendrite_induce_ltp: LTP at spine %u (magnitude=%.3f)",
                  spine_id, magnitude);
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

/**
 * WHAT: Induce LTD at dendrite
 * WHY:  Weaken synaptic connections
 * HOW:  Decrease spine receptor density
 */
void dendrite_induce_ltd(dendrite_t* dendrite, uint32_t spine_id, float magnitude) {
    // Guard: Invalid inputs
    if (!dendrite || spine_id >= dendrite->num_spines) {
        return;
    }

    nimcp_mutex_lock(&dendrite->lock);

    dendritic_spine_t* spine = &dendrite->spines[spine_id];

    // Check calcium threshold (lower than LTP)
    if (spine->calcium >= dendrite->ltd_threshold &&
        spine->calcium < dendrite->ltp_threshold) {
        // Decrease synaptic weight
        spine->synaptic_weight -= magnitude * 0.05F;
        if (spine->synaptic_weight < 0.1F) spine->synaptic_weight = 0.1F;

        // Decrease receptor density
        spine->ampa_receptors *= (1.0F - magnitude * 0.03F);
        spine->nmda_receptors *= (1.0F - magnitude * 0.02F);

        // Decrease spine size
        spine->head_diameter *= (1.0F - magnitude * 0.01F);

        // Update stability
        spine->stability = fmaxf(0.0F, spine->stability - magnitude * 0.1F);

        // Update activity stats
        dendrite->activity.ltd_events++;

        LOG_DEBUG(LOG_MODULE, "dendrite_induce_ltd: LTD at spine %u (magnitude=%.3f)",
                  spine_id, magnitude);
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

/**
 * WHAT: Update structural plasticity
 * WHY:  Add/remove spines based on activity
 * HOW:  Check stability, prune weak spines
 */
void dendrite_update_structural_plasticity(dendrite_t* dendrite, uint64_t timestamp) {
    // Guard: NULL dendrite
    if (!dendrite) {
        return;
    }

    // Guard: Structural plasticity disabled
    if (dendrite->structural_plasticity <= 0.0F) {
        return;
    }

    nimcp_mutex_lock(&dendrite->lock);

    // Check each spine for elimination
    for (uint32_t i = 0; i < dendrite->num_spines; i++) {
        dendritic_spine_t* spine = &dendrite->spines[i];

        // Skip already eliminated spines
        if (spine->state == SPINE_STATE_ELIMINATED) {
            continue;
        }

        // Prune unstable spines
        if (spine->stability < 0.2F) {
            spine->state = SPINE_STATE_PRUNING;

            // Gradual elimination
            spine->stability -= dendrite->structural_plasticity;
            if (spine->stability <= 0.0F) {
                dendrite_remove_spine(dendrite, i);
                dendrite->activity.spine_eliminations++;
            }
        }
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

// ============================================================================
// Query Functions
// ============================================================================

/**
 * WHAT: Check if dendrite is in plateau potential state
 * WHY:  Detect dendritic spikes and nonlinear integration
 * HOW:  Check if voltage exceeds threshold
 */
bool dendrite_is_in_plateau(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) {
        return false;
    }

    nimcp_mutex_lock(&dendrite->lock);

    // Plateau threshold: ~40 mV above rest
    bool in_plateau = (dendrite->mean_voltage > 0.030F);

    nimcp_mutex_unlock(&dendrite->lock);

    return in_plateau;
}

/**
 * WHAT: Get spine by synapse ID
 * WHY:  Look up spine for synapse
 * HOW:  Linear search through spines
 */
dendritic_spine_t* dendrite_get_spine_by_synapse(
    dendrite_t* dendrite,
    uint32_t synapse_id
) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_get_spine_by_synapse: dendrite is NULL");
        return NULL;
    }

    nimcp_mutex_lock(&dendrite->lock);

    dendritic_spine_t* result = NULL;

    for (uint32_t i = 0; i < dendrite->num_spines; i++) {
        if (dendrite->spines[i].synapse_id == synapse_id) {
            result = &dendrite->spines[i];
            break;
        }
    }

    nimcp_mutex_unlock(&dendrite->lock);

    return result;
}

/**
 * WHAT: Calculate total surface area
 * WHY:  Determine dendrite size for normalization
 * HOW:  Sum cylinder surface areas of all segments
 */
float dendrite_calculate_surface_area(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) {
        return 0.0F;
    }

    nimcp_mutex_lock(&dendrite->lock);

    float total_area = 0.0F;

    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        dendritic_segment_t* seg = &dendrite->segments[i];
        total_area += M_PI * seg->diameter * seg->length;
    }

    dendrite->surface_area = total_area;

    nimcp_mutex_unlock(&dendrite->lock);

    return total_area;
}

/**
 * WHAT: Get dendrite activity statistics
 * WHY:  Monitor dendrite function
 * HOW:  Return activity struct
 */
dendrite_activity_stats_t dendrite_get_activity_stats(dendrite_t* dendrite) {
    dendrite_activity_stats_t stats = {0};

    // Guard: NULL dendrite
    if (!dendrite) {
        return stats;
    }

    nimcp_mutex_lock(&dendrite->lock);
    stats = dendrite->activity;
    nimcp_mutex_unlock(&dendrite->lock);

    return stats;
}

// ============================================================================
// Dendrite Network Management
// ============================================================================

/**
 * WHAT: Create dendrite network manager
 * WHY:  Manage collection of dendrites
 * HOW:  Allocate network structure
 */
dendrite_network_t* dendrite_network_create(uint32_t max_dendrites) {
    // Guard: Invalid size
    if (max_dendrites == 0) {
        LOG_ERROR(LOG_MODULE, "dendrite_network_create: Invalid max_dendrites");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_network_create: max_dendrites is zero");
        return NULL;
    }

    // Allocate network
    dendrite_network_t* network = (dendrite_network_t*)nimcp_calloc(
        1, sizeof(dendrite_network_t)
    );
    if (!network) {
        LOG_ERROR(LOG_MODULE, "dendrite_network_create: Failed to allocate network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_network_create: network is NULL");
        return NULL;
    }

    // Allocate dendrite array
    network->dendrites = (dendrite_t**)nimcp_calloc(
        max_dendrites, sizeof(dendrite_t*)
    );
    if (!network->dendrites) {
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_network_create: network->dendrites is NULL");
        return NULL;
    }

    network->num_dendrites = 0;
    network->max_dendrites = max_dendrites;
    nimcp_mutex_init(&network->lock, NULL);

    LOG_DEBUG(LOG_MODULE, "dendrite_network_create: Created network (max=%u)",
              max_dendrites);

    return network;
}

/**
 * WHAT: Destroy dendrite network
 * WHY:  Clean up all dendrites
 * HOW:  Destroy each dendrite, free arrays
 */
void dendrite_network_destroy(dendrite_network_t* network) {
    // Guard: NULL network
    if (!network) {
        return;
    }

    // Destroy all dendrites
    for (uint32_t i = 0; i < network->num_dendrites; i++) {
        if (network->dendrites[i]) {
            dendrite_destroy(network->dendrites[i]);
        }
    }

    // Free arrays
    nimcp_free(network->dendrites);

    // Destroy mutex
    nimcp_mutex_destroy(&network->lock);

    // Free network
    nimcp_free(network);
}

/**
 * WHAT: Add dendrite to network
 * WHY:  Register dendrite for network-wide operations
 * HOW:  Add to array if space available, thread-safe
 */
bool dendrite_network_add(dendrite_network_t* network, dendrite_t* dendrite) {
    // Guard: NULL parameters
    if (!network || !dendrite) {
        LOG_ERROR(LOG_MODULE, "dendrite_network_add: NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_network_add: required parameter is NULL (network, dendrite)");
        return false;
    }

    nimcp_mutex_lock(&network->lock);

    // Guard: Network full
    if (network->num_dendrites >= network->max_dendrites) {
        nimcp_mutex_unlock(&network->lock);
        LOG_ERROR(LOG_MODULE, "dendrite_network_add: Network full (%u/%u)",
                  network->num_dendrites, network->max_dendrites);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "dendrite_network_add: capacity exceeded");
        return false;
    }

    // Add dendrite
    network->dendrites[network->num_dendrites] = dendrite;
    network->num_dendrites++;

    nimcp_mutex_unlock(&network->lock);

    LOG_DEBUG(LOG_MODULE, "dendrite_network_add: Added dendrite %u (total=%u)",
              dendrite->id, network->num_dendrites);

    return true;
}

/**
 * WHAT: Advance all dendrites in network
 * WHY:  Update entire dendritic system
 * HOW:  Call dendrite_step for each dendrite
 */
void dendrite_network_step(
    dendrite_network_t* network,
    float dt_ms,
    uint64_t timestamp
) {
    // Guard: NULL network
    if (!network) {
        return;
    }

    nimcp_mutex_lock(&network->lock);

    for (uint32_t i = 0; i < network->num_dendrites; i++) {
        if (network->dendrites[i]) {
            dendrite_step(network->dendrites[i], dt_ms, timestamp);
        }
    }

    nimcp_mutex_unlock(&network->lock);
}

/**
 * WHAT: Get network-wide statistics
 * WHY:  Monitor overall dendritic system
 * HOW:  Aggregate stats from all dendrites
 */
dendrite_network_stats_t dendrite_network_get_stats(dendrite_network_t* network) {
    dendrite_network_stats_t stats = {0};

    // Guard: NULL network
    if (!network) {
        return stats;
    }

    nimcp_mutex_lock(&network->lock);

    stats.total_dendrites = network->num_dendrites;

    for (uint32_t i = 0; i < network->num_dendrites; i++) {
        dendrite_t* d = network->dendrites[i];
        if (!d) continue;

        stats.total_segments += d->num_segments;
        stats.total_spines += d->num_spines;
        stats.mean_voltage += d->mean_voltage;
        stats.mean_calcium += d->calcium_level;
        stats.total_ltp_events += d->activity.ltp_events;
        stats.total_ltd_events += d->activity.ltd_events;
        stats.total_spine_formations += d->activity.spine_formations;
        stats.total_spine_eliminations += d->activity.spine_eliminations;
    }

    // Calculate means
    if (network->num_dendrites > 0) {
        stats.mean_voltage /= (float)network->num_dendrites;
        stats.mean_calcium /= (float)network->num_dendrites;
    }

    nimcp_mutex_unlock(&network->lock);

    return stats;
}

// ============================================================================
// NMDA Spike / Plateau Potential Implementation
// ============================================================================

/**
 * WHAT: Calculate Mg²⁺ block factor
 * WHY:  NMDA receptors are voltage-gated via Mg²⁺
 * HOW:  Boltzmann function: B(V) = 1 / (1 + [Mg]_o * exp(-0.062*V))
 */
static float calculate_mg_block(float voltage_mv) {
    if (!isfinite(voltage_mv)) return 0.0F;
    float block = 1.0F / (1.0F + NMDA_MG_CONCENTRATION *
                          expf(fminf(-NMDA_MG_BLOCK_CONSTANT * voltage_mv, 88.0f)));
    return isfinite(block) ? block : 0.0F;
}

bool dendrite_initiate_nmda_spike(dendrite_t* dendrite, uint32_t segment_id,
                                   uint64_t timestamp) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_initiate_nmda_spike: dendrite is NULL");
        return false;
    }

    // Guard: Invalid segment
    if (segment_id >= dendrite->num_segments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_initiate_nmda_spike: segment_id out of range");
        return false;
    }

    // Guard: Already in NMDA spike
    if (dendrite->nmda_state.active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_initiate_nmda_spike: NMDA spike already active");
        return false;
    }

    nimcp_mutex_lock(&dendrite->lock);

    dendritic_segment_t* segment = &dendrite->segments[segment_id];

    // Check threshold conditions
    float voltage_mv = segment->voltage * 1000.0F;  // Convert to mV
    if (voltage_mv < NIMCP_NMDA_SPIKE_THRESHOLD_MV) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_initiate_nmda_spike: voltage below threshold");
        return false;
    }

    // Check if segment has active properties
    if (!segment->has_active_properties) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_initiate_nmda_spike: segment lacks active properties");
        return false;
    }

    // Initiate NMDA spike
    dendrite->nmda_state.active = true;
    dendrite->nmda_state.onset_time = timestamp;
    dendrite->nmda_state.duration_remaining_ms = NIMCP_NMDA_PLATEAU_DURATION_MS;
    dendrite->nmda_state.peak_voltage_mv = NIMCP_NMDA_SPIKE_AMPLITUDE_MV;
    dendrite->nmda_state.segment_id = segment_id;
    dendrite->nmda_state.mg_block_factor = calculate_mg_block(voltage_mv);
    dendrite->nmda_state.calcium_influx_rate =
        NMDA_CALCIUM_PERMEABILITY * dendrite->nmda_state.mg_block_factor;

    // Set segment NMDA state
    segment->nmda_spike_active = true;
    segment->nmda_spike_voltage = NIMCP_NMDA_SPIKE_AMPLITUDE_MV / 1000.0F;
    segment->nmda_spike_remaining_ms = NIMCP_NMDA_PLATEAU_DURATION_MS;

    // Update dendrite state
    dendrite->state = DENDRITE_STATE_PLATEAU;

    nimcp_mutex_unlock(&dendrite->lock);

    LOG_DEBUG(LOG_MODULE, "dendrite_initiate_nmda_spike: NMDA spike at seg %u",
              segment_id);

    return true;
}

void dendrite_update_nmda_spike(dendrite_t* dendrite, float dt_ms) {
    // Guard: NULL dendrite
    if (!dendrite) return;

    // Guard: No active NMDA spike
    if (!dendrite->nmda_state.active) return;

    nimcp_mutex_lock(&dendrite->lock);

    dendritic_segment_t* segment = &dendrite->segments[dendrite->nmda_state.segment_id];

    // Decay plateau duration
    dendrite->nmda_state.duration_remaining_ms -= dt_ms;
    segment->nmda_spike_remaining_ms -= dt_ms;

    // Calculate calcium influx
    float ca_influx = dendrite->nmda_state.calcium_influx_rate * dt_ms;
    segment->calcium += ca_influx;

    // Clamp calcium
    if (segment->calcium > CALCIUM_BUFFER_CAPACITY) {
        segment->calcium = CALCIUM_BUFFER_CAPACITY;
    }

    // Maintain plateau voltage (slight decay)
    float decay_factor = expf(fminf(-dt_ms / (NIMCP_NMDA_PLATEAU_DURATION_MS * 2.0F), 88.0f));
    segment->nmda_spike_voltage *= decay_factor;
    segment->voltage = fmaxf(segment->voltage, segment->nmda_spike_voltage);

    // Check for termination
    if (dendrite->nmda_state.duration_remaining_ms <= 0.0F) {
        dendrite->nmda_state.active = false;
        segment->nmda_spike_active = false;
        segment->nmda_spike_voltage = 0.0F;
        dendrite->state = DENDRITE_STATE_NORMAL;

        LOG_DEBUG(LOG_MODULE, "dendrite_update_nmda_spike: NMDA spike ended");
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

bool dendrite_can_generate_nmda_spike(dendrite_t* dendrite, uint32_t segment_id) {
    // Guard clauses
    if (!dendrite) {
        return false;
    }
    if (segment_id >= dendrite->num_segments) {
        return false;
    }

    dendritic_segment_t* segment = &dendrite->segments[segment_id];

    // Check voltage threshold
    float voltage_mv = segment->voltage * 1000.0F;
    if (voltage_mv < NIMCP_NMDA_SPIKE_THRESHOLD_MV * 0.8F) {
        return false;
    }

    // Check active properties
    if (!segment->has_active_properties) {
        return false;
    }

    // Check if already in NMDA spike
    if (segment->nmda_spike_active) {
        return false;
    }

    // Check Mg²⁺ block is sufficiently relieved
    float mg_block = calculate_mg_block(voltage_mv);
    if (mg_block < 0.5F) {
        return false;
    }

    return true;
}

// ============================================================================
// Backpropagating Action Potential (bAP) Implementation
// ============================================================================

bool dendrite_initiate_bap(dendrite_t* dendrite, float amplitude_mv,
                            uint64_t timestamp) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_can_generate_nmda_spike: dendrite is NULL");
        return false;
    }

    // Guard: Already propagating
    if (dendrite->bap_state.active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_can_generate_nmda_spike: validation failed");
        return false;
    }

    nimcp_mutex_lock(&dendrite->lock);

    // Initialize bAP state
    dendrite->bap_state.active = true;
    dendrite->bap_state.onset_time = timestamp;
    dendrite->bap_state.wavefront_position_um = 0.0F;  // Start at soma
    dendrite->bap_state.peak_amplitude_mv = amplitude_mv;
    dendrite->bap_state.current_amplitude_mv = amplitude_mv;
    dendrite->bap_state.velocity_um_ms = NIMCP_BAP_VELOCITY_UM_MS;
    dendrite->bap_state.segments_reached = 0;

    // Reset all segment bAP states
    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        dendrite->segments[i].bap_reached = false;
        dendrite->segments[i].bap_amplitude = 0.0F;
        dendrite->segments[i].bap_arrival_time = 0;
    }

    // Mark first segment (at soma) as reached
    if (dendrite->num_segments > 0) {
        dendrite->segments[0].bap_reached = true;
        dendrite->segments[0].bap_amplitude = amplitude_mv / 1000.0F;
        dendrite->segments[0].bap_arrival_time = timestamp;
        dendrite->bap_state.segments_reached = 1;
    }

    dendrite->state = DENDRITE_STATE_ACTIVE;

    nimcp_mutex_unlock(&dendrite->lock);

    LOG_DEBUG(LOG_MODULE, "dendrite_initiate_bap: bAP started (%.1f mV)",
              amplitude_mv);

    return true;
}

void dendrite_update_bap(dendrite_t* dendrite, float dt_ms) {
    // Guard: NULL dendrite
    if (!dendrite) return;

    // Guard: No active bAP
    if (!dendrite->bap_state.active) return;

    nimcp_mutex_lock(&dendrite->lock);

    // Advance wavefront
    float distance_traveled = dendrite->bap_state.velocity_um_ms * dt_ms;
    dendrite->bap_state.wavefront_position_um += distance_traveled;

    // Attenuate amplitude
    float attenuation = expf(fminf(-NIMCP_BAP_ATTENUATION_PER_UM * distance_traveled, 88.0f));
    dendrite->bap_state.current_amplitude_mv *= attenuation;

    // Update segments reached by wavefront
    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        dendritic_segment_t* segment = &dendrite->segments[i];

        // Skip already reached segments
        if (segment->bap_reached) continue;

        // Check if wavefront has reached this segment
        if (segment->path_distance <= dendrite->bap_state.wavefront_position_um) {
            segment->bap_reached = true;
            segment->bap_arrival_time =
                dendrite->bap_state.onset_time +
                (uint64_t)(segment->path_distance / NIMCP_BAP_VELOCITY_UM_MS * 1000.0F);

            // Calculate local amplitude with distance-based attenuation
            float local_attenuation = expf(fminf(-NIMCP_BAP_ATTENUATION_PER_UM *
                                           segment->path_distance, 88.0f));

            // Boost at distal segments with active properties
            if (segment->has_active_properties && segment->path_distance > 200.0F) {
                local_attenuation *= BAP_NA_BOOST_FACTOR;
            }

            segment->bap_amplitude = (dendrite->bap_state.peak_amplitude_mv / 1000.0F) *
                                     local_attenuation;

            // Inject calcium
            segment->calcium += segment->bap_amplitude * BAP_CALCIUM_PER_MV *
                                NIMCP_BAP_CALCIUM_FACTOR;

            // Update segment voltage (transient)
            segment->voltage = fmaxf(segment->voltage, segment->bap_amplitude);

            dendrite->bap_state.segments_reached++;
        }
    }

    // Check if bAP has reached all segments or attenuated below threshold
    bool all_reached = (dendrite->bap_state.segments_reached >= dendrite->num_segments);
    bool too_weak = (dendrite->bap_state.current_amplitude_mv < 1.0F);

    if (all_reached || too_weak) {
        dendrite->bap_state.active = false;
        dendrite->state = DENDRITE_STATE_NORMAL;
        LOG_DEBUG(LOG_MODULE, "dendrite_update_bap: bAP complete (%u segments)",
                  dendrite->bap_state.segments_reached);
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

bool dendrite_bap_reached_spine(dendrite_t* dendrite, uint32_t spine_id) {
    // Guard clauses
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_bap_reached_spine: dendrite is NULL");
        return false;
    }
    if (spine_id >= dendrite->num_spines) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_bap_reached_spine: capacity exceeded");
        return false;
    }

    dendritic_spine_t* spine = &dendrite->spines[spine_id];
    uint32_t segment_id = spine->segment_id;

    if (segment_id >= dendrite->num_segments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_bap_reached_spine: capacity exceeded");
        return false;
    }

    return dendrite->segments[segment_id].bap_reached;
}

// ============================================================================
// Inter-Segment Coupling (Cable Theory) Implementation
// ============================================================================

void dendrite_update_axial_currents(dendrite_t* dendrite, float dt_ms) {
    // Guard: NULL dendrite
    if (!dendrite) return;

    nimcp_mutex_lock(&dendrite->lock);

    // First pass: Calculate axial conductances and currents
    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        dendritic_segment_t* segment = &dendrite->segments[i];

        // Reset axial currents
        segment->I_axial_parent = 0.0F;
        segment->I_axial_children = 0.0F;

        // Calculate axial conductance: g = pi * d^2 / (4 * Ra * L)
        float radius_cm = (segment->diameter / 2.0F) * 1e-4F;
        float length_cm = segment->length * 1e-4F;
        segment->g_axial = (M_PI * radius_cm * radius_cm) /
                           (DENDRITE_R_A * length_cm) * 1e9F;  // Convert to nS

        // Current from parent
        if (segment->parent_segment < dendrite->num_segments) {
            dendritic_segment_t* parent = &dendrite->segments[segment->parent_segment];

            float V_diff = parent->voltage - segment->voltage;
            segment->I_axial_parent = segment->g_axial * V_diff;
        }

        // Current from children
        for (uint32_t c = 0; c < segment->num_children; c++) {
            uint32_t child_id = segment->child_segments[c];
            if (child_id < dendrite->num_segments) {
                dendritic_segment_t* child = &dendrite->segments[child_id];

                float V_diff = child->voltage - segment->voltage;
                segment->I_axial_children += child->g_axial * V_diff;
            }
        }
    }

    // Second pass: Apply axial currents to voltage
    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        dendritic_segment_t* segment = &dendrite->segments[i];

        float I_total_axial = segment->I_axial_parent + segment->I_axial_children;

        // dV = I * dt / C
        float dV = (I_total_axial / segment->C_m) * (dt_ms / 1000.0F);
        segment->voltage += dV;

        // Clamp voltage
        segment->voltage = fmaxf(-0.080F, fminf(0.040F, segment->voltage));
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

float dendrite_calculate_rall_ratio(dendrite_t* dendrite, uint32_t segment_id) {
    // Guard clauses
    if (!dendrite) return 0.0F;
    if (segment_id >= dendrite->num_segments) return 0.0F;

    dendritic_segment_t* segment = &dendrite->segments[segment_id];

    // No children = no branching
    if (segment->num_children == 0) {
        segment->branch_power_ratio = 1.0F;
        return 1.0F;
    }

    // Parent diameter^(3/2)
    float d_parent = segment->diameter;
    float parent_power = powf(d_parent, 1.5F);

    // Sum of children diameter^(3/2)
    float children_power = 0.0F;
    for (uint32_t c = 0; c < segment->num_children; c++) {
        uint32_t child_id = segment->child_segments[c];
        if (child_id < dendrite->num_segments) {
            float d_child = dendrite->segments[child_id].diameter;
            children_power += powf(d_child, 1.5F);
        }
    }

    // Rall ratio (1.0 = impedance matched)
    float ratio = (children_power > 0.0F) ? (parent_power / children_power) : 1.0F;
    segment->branch_power_ratio = ratio;

    return ratio;
}

// ============================================================================
// Spatial Clustering Detection Implementation
// ============================================================================

void dendrite_detect_spine_clusters(dendrite_t* dendrite, uint64_t timestamp) {
    // Guard: NULL dendrite
    if (!dendrite) return;

    nimcp_mutex_lock(&dendrite->lock);

    // Reset clusters
    dendrite->num_clusters = 0;

    // For each segment, find clusters of recently active spines
    for (uint32_t seg_i = 0; seg_i < dendrite->num_segments; seg_i++) {
        dendritic_segment_t* segment = &dendrite->segments[seg_i];

        if (segment->num_spines < NIMCP_CLUSTER_MIN_SPINES) continue;

        // Find recently active spines on this segment
        uint32_t active_spines[64];
        uint32_t num_active = 0;
        uint64_t window_us = 50000;  // 50ms window

        for (uint32_t sp_i = 0; sp_i < segment->num_spines && num_active < 64; sp_i++) {
            uint32_t spine_id = segment->spine_ids[sp_i];
            if (spine_id < dendrite->num_spines) {
                dendritic_spine_t* spine = &dendrite->spines[spine_id];
                if (timestamp - spine->last_input_time < window_us) {
                    active_spines[num_active++] = spine_id;
                }
            }
        }

        // Need minimum spines for a cluster
        if (num_active < NIMCP_CLUSTER_MIN_SPINES) continue;

        // Simple clustering: all active spines on same segment form a cluster
        // (More sophisticated spatial clustering could use actual positions)
        if (dendrite->num_clusters < 32) {
            spine_cluster_t* cluster = &dendrite->clusters[dendrite->num_clusters];

            cluster->segment_id = seg_i;
            cluster->num_spines = (num_active > 16) ? 16 : num_active;

            for (uint32_t i = 0; i < cluster->num_spines; i++) {
                cluster->spine_ids[i] = active_spines[i];
            }

            cluster->radius_um = NIMCP_CLUSTER_RADIUS_UM;
            cluster->last_update_time = timestamp;

            // Calculate total input
            cluster->total_input = 0.0F;
            for (uint32_t i = 0; i < cluster->num_spines; i++) {
                uint32_t spine_id = cluster->spine_ids[i];
                cluster->total_input += dendrite->spines[spine_id].voltage;
            }

            // Check NMDA spike eligibility
            cluster->nmda_spike_eligible =
                (cluster->num_spines >= NIMCP_CLUSTER_MIN_SPINES) &&
                (segment->voltage > NIMCP_NMDA_SPIKE_THRESHOLD_MV / 2000.0F);

            dendrite->num_clusters++;
        }
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

float dendrite_apply_cluster_boost(dendrite_t* dendrite, uint32_t cluster_id) {
    // Guard clauses
    if (!dendrite) return 0.0F;
    if (cluster_id >= dendrite->num_clusters) return 0.0F;

    spine_cluster_t* cluster = &dendrite->clusters[cluster_id];

    // Nonlinear boost based on cluster size
    float boost_factor = 1.0F;
    if (cluster->num_spines >= NIMCP_CLUSTER_MIN_SPINES) {
        boost_factor = NIMCP_CLUSTER_NONLINEAR_FACTOR;
    }
    if (cluster->num_spines >= NIMCP_CLUSTER_MIN_SPINES * 2) {
        boost_factor = NIMCP_CLUSTER_NONLINEAR_FACTOR * 1.3F;  // More boost for larger clusters
    }

    cluster->nonlinear_output = cluster->total_input * boost_factor;

    return cluster->nonlinear_output;
}

// ============================================================================
// STDP Integration Implementation
// ============================================================================

static void dendrite_stdp_pre_spike_unlocked(dendrite_t* dendrite, uint32_t spine_id,
                              uint64_t timestamp) {
    // Guard clauses
    if (!dendrite) return;
    if (spine_id >= dendrite->num_spines) return;

    // NOTE: caller must hold dendrite->lock
    dendritic_spine_t* spine = &dendrite->spines[spine_id];
    stdp_state_t* stdp = &spine->stdp;

    // Update presynaptic trace
    stdp->pre_trace += 1.0F;
    if (stdp->pre_trace > 1.0F) stdp->pre_trace = 1.0F;

    stdp->last_pre_spike = timestamp;

    // Check for LTD (pre after post)
    if (stdp->last_post_spike > 0) {
        float dt_ms = (float)(timestamp - stdp->last_post_spike) / 1000.0F;
        if (dt_ms > 0.0F && dt_ms < NIMCP_STDP_WINDOW_MS) {
            // LTD: pre came after post
            float ltd_magnitude = NIMCP_STDP_A_MINUS *
                                  expf(fminf(-dt_ms / NIMCP_STDP_TAU_MINUS_MS, 88.0f)) *
                                  stdp->post_trace;
            if (isfinite(ltd_magnitude)) {
                stdp->accumulated_ltd += ltd_magnitude;
            }
            stdp->eligible_for_update = true;
        }
    }
}

void dendrite_stdp_pre_spike(dendrite_t* dendrite, uint32_t spine_id,
                              uint64_t timestamp) {
    if (!dendrite) return;
    if (spine_id >= dendrite->num_spines) return;
    nimcp_mutex_lock(&dendrite->lock);
    dendrite_stdp_pre_spike_unlocked(dendrite, spine_id, timestamp);
    nimcp_mutex_unlock(&dendrite->lock);
}

void dendrite_stdp_post_spike(dendrite_t* dendrite, uint64_t timestamp) {
    // Guard: NULL dendrite
    if (!dendrite) return;

    nimcp_mutex_lock(&dendrite->lock);

    // Update all spines with postsynaptic spike
    for (uint32_t i = 0; i < dendrite->num_spines; i++) {
        dendritic_spine_t* spine = &dendrite->spines[i];
        stdp_state_t* stdp = &spine->stdp;

        // Update postsynaptic trace
        stdp->post_trace += 1.0F;
        if (stdp->post_trace > 1.0F) stdp->post_trace = 1.0F;

        stdp->last_post_spike = timestamp;

        // Check for LTP (post after pre)
        if (stdp->last_pre_spike > 0) {
            float dt_ms = (float)(timestamp - stdp->last_pre_spike) / 1000.0F;
            if (dt_ms > 0.0F && dt_ms < NIMCP_STDP_WINDOW_MS) {
                // LTP: post came after pre
                float ltp_magnitude = NIMCP_STDP_A_PLUS *
                                      expf(fminf(-dt_ms / NIMCP_STDP_TAU_PLUS_MS, 88.0f)) *
                                      stdp->pre_trace;
                if (isfinite(ltp_magnitude)) {
                    stdp->accumulated_ltp += ltp_magnitude;
                }
                stdp->eligible_for_update = true;
            }
        }
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

void dendrite_stdp_apply_weight_changes(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) return;

    nimcp_mutex_lock(&dendrite->lock);

    for (uint32_t i = 0; i < dendrite->num_spines; i++) {
        dendritic_spine_t* spine = &dendrite->spines[i];
        stdp_state_t* stdp = &spine->stdp;

        if (!stdp->eligible_for_update) continue;

        // Net weight change
        float delta_w = stdp->accumulated_ltp - stdp->accumulated_ltd;

        // Apply to synaptic weight
        float new_weight = spine->synaptic_weight + delta_w;
        if (isfinite(new_weight)) {
            spine->synaptic_weight = new_weight;
        }

        // Bounds
        if (spine->synaptic_weight < 0.1F) spine->synaptic_weight = 0.1F;
        if (spine->synaptic_weight > 2.0F) spine->synaptic_weight = 2.0F;

        // Update receptor counts proportionally
        if (delta_w > 0) {
            spine->ampa_receptors *= (1.0F + delta_w * 0.1F);
            dendrite->activity.ltp_events++;
        } else {
            spine->ampa_receptors *= (1.0F + delta_w * 0.1F);
            dendrite->activity.ltd_events++;
        }

        // Reset STDP state
        stdp->accumulated_ltp = 0.0F;
        stdp->accumulated_ltd = 0.0F;
        stdp->eligible_for_update = false;
    }

    // Decay traces
    for (uint32_t i = 0; i < dendrite->num_spines; i++) {
        stdp_state_t* stdp = &dendrite->spines[i].stdp;
        stdp->pre_trace *= 0.95F;
        stdp->post_trace *= 0.95F;
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

// ============================================================================
// Spine Memory Pool Implementation
// ============================================================================

bool dendrite_create_spine_pool(dendrite_t* dendrite, uint32_t capacity) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_create_spine_pool: dendrite is NULL");
        return false;
    }

    // Guard: Already has pool
    if (dendrite->spine_pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_create_spine_pool: validation failed");
        return false;
    }

    nimcp_mutex_lock(&dendrite->lock);

    // Allocate pool structure
    dendrite->spine_pool = (dendrite_spine_pool_t*)nimcp_calloc(
        1, sizeof(dendrite_spine_pool_t)
    );
    if (!dendrite->spine_pool) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_create_spine_pool: dendrite->spine_pool is NULL");
        return false;
    }

    // Allocate spine array
    dendrite->spine_pool->spines = (dendritic_spine_t*)nimcp_calloc(
        capacity, sizeof(dendritic_spine_t)
    );
    if (!dendrite->spine_pool->spines) {
        nimcp_free(dendrite->spine_pool);
        dendrite->spine_pool = NULL;
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_create_spine_pool: dendrite->spine_pool->spines is NULL");
        return false;
    }

    // Allocate bitmap (1 bit per spine)
    uint32_t bitmap_size = (capacity + 63) / 64;
    dendrite->spine_pool->allocation_bitmap = (uint64_t*)nimcp_calloc(
        bitmap_size, sizeof(uint64_t)
    );
    if (!dendrite->spine_pool->allocation_bitmap) {
        nimcp_free(dendrite->spine_pool->spines);
        nimcp_free(dendrite->spine_pool);
        dendrite->spine_pool = NULL;
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_create_spine_pool: dendrite->spine_pool->allocation_bitmap is NULL");
        return false;
    }

    dendrite->spine_pool->capacity = capacity;
    dendrite->spine_pool->allocated_count = 0;
    dendrite->spine_pool->next_free_hint = 0;
    nimcp_mutex_init(&dendrite->spine_pool->lock, NULL);

    dendrite->use_spine_pool = true;

    nimcp_mutex_unlock(&dendrite->lock);

    LOG_DEBUG(LOG_MODULE, "dendrite_create_spine_pool: Pool created (%u capacity)",
              capacity);

    return true;
}

dendritic_spine_t* dendrite_pool_alloc_spine(dendrite_t* dendrite) {
    // Guard: NULL dendrite or no pool
    if (!dendrite || !dendrite->spine_pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_pool_alloc_spine: required parameter is NULL (dendrite, dendrite->spine_pool)");
        return NULL;
    }

    dendrite_spine_pool_t* pool = dendrite->spine_pool;

    nimcp_mutex_lock(&pool->lock);

    // Check capacity
    if (pool->allocated_count >= pool->capacity) {
        nimcp_mutex_unlock(&pool->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_pool_alloc_spine: capacity exceeded");
        return NULL;
    }

    // Find free slot starting from hint
    uint32_t start = pool->next_free_hint;
    for (uint32_t i = 0; i < pool->capacity; i++) {
        uint32_t idx = (start + i) % pool->capacity;
        uint32_t word = idx / 64;
        uint32_t bit = idx % 64;

        if (!(pool->allocation_bitmap[word] & (1ULL << bit))) {
            // Found free slot
            pool->allocation_bitmap[word] |= (1ULL << bit);
            pool->allocated_count++;
            pool->next_free_hint = (idx + 1) % pool->capacity;

            dendritic_spine_t* spine = &pool->spines[idx];
            memset(spine, 0, sizeof(dendritic_spine_t));

            nimcp_mutex_unlock(&pool->lock);
            return spine;
        }
    }

    nimcp_mutex_unlock(&pool->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_pool_alloc_spine: operation failed");
    return NULL;
}

void dendrite_pool_free_spine(dendrite_t* dendrite, dendritic_spine_t* spine) {
    // Guard clauses
    if (!dendrite || !dendrite->spine_pool || !spine) return;

    dendrite_spine_pool_t* pool = dendrite->spine_pool;

    // Verify spine is in pool range
    ptrdiff_t offset = spine - pool->spines;
    if (offset < 0 || offset >= (ptrdiff_t)pool->capacity) {
        return;  // Not from this pool
    }

    nimcp_mutex_lock(&pool->lock);

    uint32_t idx = (uint32_t)offset;
    uint32_t word = idx / 64;
    uint32_t bit = idx % 64;

    // Clear bit
    if (pool->allocation_bitmap[word] & (1ULL << bit)) {
        pool->allocation_bitmap[word] &= ~(1ULL << bit);
        pool->allocated_count--;

        // Update hint for faster next alloc
        if (idx < pool->next_free_hint) {
            pool->next_free_hint = idx;
        }
    }

    nimcp_mutex_unlock(&pool->lock);
}

// ============================================================================
// Copy-on-Write (CoW) Implementation
// ============================================================================

dendrite_t* dendrite_cow_copy(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_cow_copy: dendrite is NULL");
        return NULL;
    }

    nimcp_mutex_lock(&dendrite->lock);

    // Increment reference count
    dendrite->cow_ref_count++;

    // Create shallow copy (shares data)
    dendrite_t* copy = (dendrite_t*)nimcp_calloc(1, sizeof(dendrite_t));
    if (!copy) {
        dendrite->cow_ref_count--;
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_cow_copy: copy is NULL");
        return NULL;
    }

    // Copy header data
    memcpy(copy, dendrite, sizeof(dendrite_t));

    // Share segment and spine arrays (shallow)
    // These will be copied on first write

    // Initialize copy's lock
    nimcp_mutex_init(&copy->lock, NULL);

    // Mark as shared (not modified)
    copy->cow_modified = false;
    copy->cow_ref_count = 1;

    nimcp_mutex_unlock(&dendrite->lock);

    LOG_DEBUG(LOG_MODULE, "dendrite_cow_copy: CoW copy created (ref=%u)",
              dendrite->cow_ref_count);

    return copy;
}

bool dendrite_cow_prepare_write(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_cow_prepare_write: dendrite is NULL");
        return false;
    }

    nimcp_mutex_lock(&dendrite->lock);

    // If already exclusive owner, no copy needed
    if (dendrite->cow_ref_count <= 1 || dendrite->cow_modified) {
        dendrite->cow_modified = true;
        nimcp_mutex_unlock(&dendrite->lock);
        return true;
    }

    // Need to make a deep copy of shared data
    // Copy segments
    dendritic_segment_t* new_segments = NULL;
    if (dendrite->segments && dendrite->num_segments > 0) {
        new_segments = (dendritic_segment_t*)nimcp_malloc(
            dendrite->num_segments * sizeof(dendritic_segment_t)
        );
        if (new_segments) {
            memcpy(new_segments, dendrite->segments,
                   dendrite->num_segments * sizeof(dendritic_segment_t));
        }
    }

    // Copy spines
    dendritic_spine_t* new_spines = NULL;
    if (dendrite->spines && dendrite->num_spines > 0 && !dendrite->use_spine_pool) {
        new_spines = (dendritic_spine_t*)nimcp_malloc(
            dendrite->num_spines * sizeof(dendritic_spine_t)
        );
        if (!new_spines && new_segments) {
            // BUGFIX: Free new_segments if new_spines allocation fails
            nimcp_free(new_segments);
            nimcp_mutex_unlock(&dendrite->lock);
            LOG_ERROR(LOG_MODULE, "dendrite_cow_prepare_write: Failed to allocate spines copy");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_cow_prepare_write: new_spines is NULL");
            return false;
        }
        if (new_spines) {
            memcpy(new_spines, dendrite->spines,
                   dendrite->num_spines * sizeof(dendritic_spine_t));
        }
    }

    // Apply the copies after both allocations succeed
    if (new_segments) {
        dendrite->segments = new_segments;
    }
    if (new_spines) {
        dendrite->spines = new_spines;
    }

    dendrite->cow_modified = true;
    dendrite->cow_ref_count = 1;

    nimcp_mutex_unlock(&dendrite->lock);

    LOG_DEBUG(LOG_MODULE, "dendrite_cow_prepare_write: Deep copy made");

    return true;
}

void dendrite_cow_release(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) return;

    nimcp_mutex_lock(&dendrite->lock);

    if (dendrite->cow_ref_count > 0) {
        dendrite->cow_ref_count--;
    }

    if (dendrite->cow_ref_count == 0) {
        nimcp_mutex_unlock(&dendrite->lock);
        // Last reference - full cleanup
        dendrite_destroy(dendrite);
        return;
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

// ============================================================================
// Spine-Level Input Processing Implementation
// ============================================================================

float dendrite_spine_process_input(dendrite_t* dendrite, uint32_t spine_id,
                                    float current, uint64_t timestamp) {
    // Guard clauses
    if (!dendrite) return 0.0F;
    if (spine_id >= dendrite->num_spines) return 0.0F;

    nimcp_mutex_lock(&dendrite->lock);

    dendritic_spine_t* spine = &dendrite->spines[spine_id];

    // Update input timing
    spine->last_input_time = timestamp;
    spine->total_inputs++;

    // Spine neck RC filtering
    // Time constant: tau = R_neck * C_head
    float tau_ms = spine->neck_resistance * spine->head_capacitance * 1e-6F;  // Convert fF to pF
    if (tau_ms < 0.1F) tau_ms = 0.1F;

    // Simple first-order filter
    float alpha = 0.1F;  // Integration rate
    spine->voltage = (1.0F - alpha) * spine->voltage + alpha * (current * spine->neck_resistance * 1e-6F);
    if (!isfinite(spine->voltage)) spine->voltage = 0.0F;

    // AMPA receptor activation
    float I_ampa = spine->g_ampa * spine->ampa_receptors * spine->voltage;

    // NMDA receptor activation (voltage-dependent via Mg block)
    float mg_block = calculate_mg_block(spine->voltage * 1000.0F);
    float I_nmda = spine->g_nmda * spine->nmda_receptors * spine->voltage * mg_block;

    // Calcium influx (primarily through NMDA)
    spine->calcium += I_nmda * 0.01F * mg_block;
    if (spine->calcium > CALCIUM_BUFFER_CAPACITY) {
        spine->calcium = CALCIUM_BUFFER_CAPACITY;
    }

    // Total current delivered to dendrite (attenuated by neck)
    float I_dendrite = (I_ampa + I_nmda) / spine->neck_resistance * 1e6F;

    // Apply synaptic weight
    I_dendrite *= spine->synaptic_weight;

    // Register pre-spike for STDP (use unlocked variant - we already hold dendrite->lock)
    dendrite_stdp_pre_spike_unlocked(dendrite, spine_id, timestamp);

    nimcp_mutex_unlock(&dendrite->lock);

    return I_dendrite;
}

void dendrite_update_spine_conductances(dendrite_t* dendrite, float dt_ms) {
    // Guard: NULL dendrite
    if (!dendrite) return;

    nimcp_mutex_lock(&dendrite->lock);

    // Time constants for receptor kinetics
    const float tau_ampa = 2.0F;   // AMPA decay (ms)
    const float tau_nmda = 100.0F; // NMDA decay (ms)
    const float tau_gaba = 6.0F;   // GABA decay (ms)

    float decay_ampa = expf(fminf(-dt_ms / tau_ampa, 88.0f));
    float decay_nmda = expf(fminf(-dt_ms / tau_nmda, 88.0f));
    float decay_gaba = expf(fminf(-dt_ms / tau_gaba, 88.0f));

    for (uint32_t i = 0; i < dendrite->num_spines; i++) {
        dendritic_spine_t* spine = &dendrite->spines[i];

        // Decay conductances
        spine->g_ampa *= decay_ampa;
        spine->g_nmda *= decay_nmda;
        spine->g_gaba *= decay_gaba;

        // Decay voltage
        float tau_voltage = spine->neck_resistance * spine->head_capacitance * 1e-6F;
        if (tau_voltage < 0.1F) tau_voltage = 0.1F;
        spine->voltage *= expf(fminf(-dt_ms / tau_voltage, 88.0f));

        // Decay calcium
        spine->calcium *= expf(fminf(-dt_ms / CALCIUM_DECAY_TAU, 88.0f));
    }

    nimcp_mutex_unlock(&dendrite->lock);
}
