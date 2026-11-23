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
#define LTP_THRESHOLD_DEFAULT 0.8f          // μM (calcium threshold for LTP)
#define LTD_THRESHOLD_DEFAULT 0.3f          // μM (calcium threshold for LTD)
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
#define BAP_CALCIUM_PER_MV 0.005f           // Ca²⁺ influx (μM) per mV of local bAP
                                            // (x NIMCP_BAP_CALCIUM_FACTOR: 100 mV -> 0.25 μM)

// ----------------------------------------------------------------------------
// UNIT CONVENTIONS (2026-11 unit audit)
//
//   segment->voltage, spine->voltage : VOLTS, relative to rest (0 = rest).
//                                      Absolute potential = E_REST + 1000*V mV.
//   segment->R_m  : Ω    (absolute: R_m_specific / area)
//   segment->C_m  : μF   (absolute: C_m_specific * area)   -> pF = C_m * 1e6
//   segment->R_a  : Ω    (absolute: R_i * L / cross-section)
//   segment->path_resistance : Ω, cumulative axial resistance to the soma
//   segment->g_axial : nS
//   spine->neck_resistance : MΩ ;  spine->head_capacitance : fF
//   spine->g_ampa/g_nmda/g_gaba : nS
//   currents : pA ;  time : ms (timestamps μs) ;  lengths : μm ; Ca : μM
//
//   Handy identities: nS * mV = pA ; pA * ms / pF = mV ; nS * MΩ = 1e-3.
// ----------------------------------------------------------------------------
#define DENDRITE_E_REST_MV      (-70.0f)    // resting potential (absolute)
#define DENDRITE_E_EXC_REL_MV   (70.0f)     // AMPA/NMDA reversal (0 mV abs)
#define DENDRITE_E_GABA_REL_MV  (0.0f)      // GABA_A reversal (-70 mV abs, shunting)
#define DENDRITE_V_MIN_REL      (-0.020f)   // safety floor: E_K (-90 mV abs), volts rel
#define DENDRITE_V_MAX_REL      (0.125f)    // safety ceiling: E_Na (+55 mV abs), volts rel
#define DENDRITE_INPUT_PULSE_MS 1.0f        // dendrite_receive_input(): current is a 1 ms pulse
#define DENDRITE_CM_PF_PER_UM2  0.01f       // 1 μF/cm² = 0.01 pF/μm²
#define DENDRITE_SOMA_LOAD_PF   100.0f      // capacitance of the soma + rest of the cell that
                                            // the dendrite drains into (tau_m -> 5 nS, 200 MΩ)
#define DENDRITE_MAX_SUBSTEP_MS 0.5f        // integration sub-step for segment<->soma coupling

// Spine synaptic receptor kinetics / quantal sizes
#define SPINE_TAU_AMPA_MS        2.0f       // AMPA decay
#define SPINE_TAU_NMDA_MS        100.0f     // NMDA decay
#define SPINE_TAU_GABA_MS        6.0f       // GABA_A decay
#define SPINE_G_AMPA_PER_INPUT_NS 0.5f      // peak AMPA conductance per unit input at weight 1
#define SPINE_G_NMDA_PER_INPUT_NS 0.25f     // peak NMDA conductance per unit input at weight 1
#define SPINE_AMPA_RECEPTORS_REF 100.0f     // receptor count at creation (scales g)
#define SPINE_NMDA_RECEPTORS_REF 20.0f
#define SPINE_G_MAX_NS           20.0f      // saturation of receptor pool per spine
#define SPINE_CA_UM_PER_FC_NMDA  0.0025f    // spine Ca²⁺ per fC of NMDA charge (~0.2 μM per
                                            // isolated EPSP at rest, ~0.7 μM when depolarised)
#define NMDA_MG_HALF_BLOCK_MM    3.57f      // Jahr & Stevens (1990)
#define NMDA_MIN_UNBLOCK         0.10f      // B(V) needed for regenerative NMDA spike (~-54 mV)

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
static void update_segment_calcium(dendritic_segment_t* segment, float dt_ms);
static float calculate_attenuation(dendrite_t* dendrite, uint32_t segment_id);
static void flush_pending_locked(dendrite_t* dendrite);
static float calculate_mg_block(float voltage_abs_mv);
static void finalize_tree_properties_locked(dendrite_t* dendrite);
static bool cow_unshare_locked(dendrite_t* dendrite);

/** Segment membrane capacitance in pF (C_m is stored in μF). */
static inline float segment_cap_pf(const dendritic_segment_t* s) {
    return s->C_m * 1e6F;
}

/** Segment membrane time constant in ms (R_m Ω * C_m μF * 1e-6 F/μF * 1e3 ms/s). */
static inline float segment_tau_ms(const dendritic_segment_t* s) {
    float tau = s->R_m * s->C_m * 1e-3F;
    return (isfinite(tau) && tau > 1e-3F) ? tau : 20.0F;
}

/** Length constant λ (μm) of a cylinder of diameter d (μm): sqrt(Rm*d/(4*Ri)). */
static inline float length_constant_um(float diameter_um) {
    if (!(diameter_um > 0.0F)) return 0.0F;
    float lambda_cm = sqrtf(DENDRITE_R_M * (diameter_um * 1e-4F) / (4.0F * DENDRITE_R_A));
    return lambda_cm * 1e4F;
}

static inline float clamp_seg_voltage(float v) {
    if (!isfinite(v)) return 0.0F;
    if (v < DENDRITE_V_MIN_REL) return DENDRITE_V_MIN_REL;
    if (v > DENDRITE_V_MAX_REL) return DENDRITE_V_MAX_REL;
    return v;
}

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

    // Segment/spine arrays may be shared with CoW copies (dendrite_cow_copy):
    // only the last holder frees them.
    bool free_arrays = true;
    if (dendrite->cow_shared_refs) {
        uint32_t left = __atomic_sub_fetch(dendrite->cow_shared_refs, 1U, __ATOMIC_ACQ_REL);
        if (left == 0) {
            nimcp_free(dendrite->cow_shared_refs);
        } else {
            free_arrays = false;
        }
        dendrite->cow_shared_refs = NULL;
    }

    if (free_arrays) {
        if (dendrite->segments) nimcp_free(dendrite->segments);
        if (dendrite->spines) nimcp_free(dendrite->spines);
    }
    dendrite->segments = NULL;
    dendrite->spines = NULL;

    // Free spine pool (never shared: CoW copies do not inherit it)
    if (dendrite->spine_pool) {
        nimcp_mutex_destroy(&dendrite->spine_pool->lock);
        nimcp_free(dendrite->spine_pool->allocation_bitmap);
        nimcp_free(dendrite->spine_pool->spines);
        nimcp_free(dendrite->spine_pool);
        dendrite->spine_pool = NULL;
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

    nimcp_mutex_lock(&dendrite->lock);

    // Guard: Already has segments
    if (dendrite->segments) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "dendrite_create_segments: dendrite already has segments");
        return false;
    }

    // Allocate segments
    dendrite->segments = (dendritic_segment_t*)nimcp_calloc(
        num_segments,
        sizeof(dendritic_segment_t)
    );
    if (!dendrite->segments) {
        nimcp_mutex_unlock(&dendrite->lock);
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

        // Active properties (specific conductances, mS/cm²; informational —
        // active behaviour is modelled by the NMDA-spike and bAP mechanisms)
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

    finalize_tree_properties_locked(dendrite);

    nimcp_mutex_unlock(&dendrite->lock);

    LOG_DEBUG(LOG_MODULE, "dendrite_create_segments: Created %u segments for dendrite %u",
              num_segments, dendrite->id);

    return true;
}

/**
 * WHAT: Recompute tree-level electrical properties after a morphology change
 * WHY:  path_resistance must be cumulative to the soma; the dendrite-level
 *       input_resistance / time_constant / attenuation_factor / surface_area
 *       were never computed (left 0).
 * HOW:  Walk each segment's ancestor chain (bounded, cycle-safe).
 *       Caller holds dendrite->lock.
 */
static void finalize_tree_properties_locked(dendrite_t* dendrite) {
    if (!dendrite || !dendrite->segments || dendrite->num_segments == 0) return;

    double g_total = 0.0;       // S, sum of membrane leak conductances
    float area_um2 = 0.0F;
    float min_att = 1.0F;
    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        dendritic_segment_t* s = &dendrite->segments[i];
        float r = s->R_a;
        uint32_t p = s->parent_segment;
        for (uint32_t hops = 0; p < dendrite->num_segments && p != i &&
             hops < dendrite->num_segments; hops++) {
            r += dendrite->segments[p].R_a;
            p = dendrite->segments[p].parent_segment;
        }
        s->path_resistance = r;
        if (s->R_m > 0.0F) g_total += 1.0 / (double)s->R_m;
        area_um2 += (float)M_PI * s->diameter * s->length;
        float a = calculate_attenuation(dendrite, i);
        if (a < min_att) min_att = a;
        s->att_soma = a;

        // Axial resistance from the input site (path_distance, but at least
        // the segment midpoint) to the soma: ancestors' R_a plus the own
        // cylinder over the remaining distance.
        float r_anc = 0.0F, len_anc = 0.0F;
        uint32_t q = s->parent_segment;
        for (uint32_t hops = 0; q < dendrite->num_segments && q != i &&
             hops < dendrite->num_segments; hops++) {
            r_anc += dendrite->segments[q].R_a;
            len_anc += dendrite->segments[q].length;
            q = dendrite->segments[q].parent_segment;
        }
        float r_per_um = (s->length > 0.0F) ? s->R_a / s->length : 0.0F;
        float own_um = fmaxf(s->path_distance - len_anc, 0.5F * s->length);
        float r_soma = r_anc + r_per_um * own_um;
        s->g_soma_ns = (r_soma > 0.0F && isfinite(r_soma)) ? 1e9F / r_soma : 0.0F;
    }
    dendrite->surface_area = area_um2;
    // Membrane-only (sealed, isolated tree) input resistance, MΩ
    dendrite->input_resistance = (g_total > 0.0) ? (float)(1e-6 / g_total) : 0.0F;
    dendrite->time_constant = segment_tau_ms(&dendrite->segments[0]);  // Rm*Cm, ~20 ms
    dendrite->attenuation_factor = min_att;
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

    nimcp_mutex_lock(&dendrite->lock);

    // Guard: Invalid parent
    if (parent_segment_id >= dendrite->num_segments) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_add_branch: invalid parent segment id");
        return UINT32_MAX;
    }

    // Guard: arrays shared with a CoW copy must be made private before realloc
    if (!cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_add_branch: CoW unshare failed");
        return UINT32_MAX;
    }
    flush_pending_locked(dendrite);

    dendritic_segment_t* parent = &dendrite->segments[parent_segment_id];

    // Guard: Parent has too many children
    if (parent->num_children >= NIMCP_DENDRITE_MAX_CHILDREN) {
        nimcp_mutex_unlock(&dendrite->lock);
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
        nimcp_mutex_unlock(&dendrite->lock);
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

    finalize_tree_properties_locked(dendrite);

    nimcp_mutex_unlock(&dendrite->lock);

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

    nimcp_mutex_lock(&dendrite->lock);

    // Guard: Invalid segment
    if (segment_id >= dendrite->num_segments) {
        nimcp_mutex_unlock(&dendrite->lock);
        LOG_ERROR(LOG_MODULE, "dendrite_add_spine: Invalid segment %u", segment_id);
        return UINT32_MAX;
    }

    // Guard: arrays shared with a CoW copy must be made private before realloc
    if (!cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        LOG_ERROR(LOG_MODULE, "dendrite_add_spine: CoW unshare failed");
        return UINT32_MAX;
    }

    dendritic_segment_t* segment = &dendrite->segments[segment_id];

    // Guard: Segment has too many spines (use tier-optimized limit)
    if (segment->num_spines >= NIMCP_MAX_SPINE_IDS) {
        nimcp_mutex_unlock(&dendrite->lock);
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
        nimcp_mutex_unlock(&dendrite->lock);
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
    // Neck: R = 4*Ri*L/(pi*d^2). With L, d in μm: L_cm/d_cm^2 = 1e4 * L/d^2,
    // so R[Ω] = 4*Ri*1e4*L/(pi d^2) and R[MΩ] = 4*Ri*1e-2*L/(pi d^2).
    // (thin spine: L=1, d=0.1 -> ~190 MΩ; literature 50-500 MΩ)
    spine->neck_resistance = (4.0F * DENDRITE_R_A * spine->neck_length * 1e-2F) /
                             (M_PI * spine->neck_diameter * spine->neck_diameter);
    // Head: C = Cm * 4*pi*r^2 ; 1 μF/cm² = 10 fF/μm²
    spine->head_capacitance = DENDRITE_C_M * 10.0F * 4.0F * M_PI * radius * radius;  // fF

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

    nimcp_mutex_unlock(&dendrite->lock);

    LOG_DEBUG(LOG_MODULE, "dendrite_add_spine: Added spine %u (type=%d) to segment %u",
              new_id, type, segment_id);

    return new_id;
}

/**
 * WHAT: Remove dendritic spine
 * WHY:  Support structural plasticity and spine elimination
 * HOW:  Remove from segment list, mark as inactive
 */
static bool dendrite_remove_spine_locked(dendrite_t* dendrite, uint32_t spine_id);

bool dendrite_remove_spine(dendrite_t* dendrite, uint32_t spine_id) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_remove_spine: dendrite is NULL");
        return false;
    }
    nimcp_mutex_lock(&dendrite->lock);
    bool ok = cow_unshare_locked(dendrite) && dendrite_remove_spine_locked(dendrite, spine_id);
    nimcp_mutex_unlock(&dendrite->lock);
    return ok;
}

/* Caller holds dendrite->lock and has unshared CoW arrays. */
static bool dendrite_remove_spine_locked(dendrite_t* dendrite, uint32_t spine_id) {
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
 * HOW:  The current (pA) is treated as a DENDRITE_INPUT_PULSE_MS (1 ms) pulse:
 *       its charge q = I * 1 ms is deposited on the segment membrane
 *       capacitance (C = 1 μF/cm² * area), dV[mV] = I[pA] * 1[ms] / C[pF].
 *       (Previously V += I * R_m with R_m in Ω: 1 pA -> ~1e9 "V".)
 */
bool dendrite_receive_input(
    dendrite_t* dendrite,
    uint32_t segment_id,
    float current,
    uint64_t timestamp
) {
    (void)timestamp;
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_receive_input: dendrite is NULL");
        return false;
    }
    if (!isfinite(current)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_receive_input: non-finite current");
        return false;
    }

    nimcp_mutex_lock(&dendrite->lock);

    if (segment_id >= dendrite->num_segments) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_receive_input: segment_id out of range");
        return false;
    }
    if (!cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_receive_input: CoW unshare failed");
        return false;
    }
    // Apply any lazily-deferred passive decay before adding new charge
    flush_pending_locked(dendrite);

    dendritic_segment_t* segment = &dendrite->segments[segment_id];

    float c_pf = segment_cap_pf(segment);
    if (c_pf > 1e-9F) {
        float dv_mv = current * DENDRITE_INPUT_PULSE_MS / c_pf;
        segment->voltage = clamp_seg_voltage(segment->voltage + dv_mv * 1e-3F);
    }

    // Update calcium based on NMDA component
    if (current > 0) {
        segment->calcium += current * 0.1F;  // Simplified calcium influx
        if (segment->calcium > CALCIUM_BUFFER_CAPACITY) segment->calcium = CALCIUM_BUFFER_CAPACITY;
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
 * HOW:  The segments drain through their axial paths (G_c,i) into a passive
 *       soma-load compartment; the current arriving at the soma is the
 *       cable-attenuated axial current att_i * G_c,i * (V_i - V_soma).
 *       Returns the MEAN current (pA) delivered to the soma since the
 *       previous call (poll once per consumer step: its integral is the
 *       delivered charge, independent of dendrite_step's skip-frames); if
 *       no simulated time has elapsed since the previous call, returns the
 *       instantaneous current. Linear in input; saturates only near the
 *       synaptic reversal potential.
 *       Also sets somatic_voltage (volts rel. rest): the soma EPSP after the
 *       charge now in the dendrite equilibrates (sub-ms):
 *         (C_s V_s + sum att_i C_i V_i) / (C_s + sum C_i).
 *       (Previously: sum of V*att returned as a "current" -> saturated at
 *       ~0.04 regardless of input.)
 */
float dendrite_compute_somatic_current(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) {
        return 0.0F;
    }

    nimcp_mutex_lock(&dendrite->lock);

    if (dendrite->pending_dt_ms > 0.0F && cow_unshare_locked(dendrite)) {
        flush_pending_locked(dendrite);
    }

    const double c_s = DENDRITE_SOMA_LOAD_PF;
    double vs_mv = (double)dendrite->soma_v * 1000.0;
    double i_inst = 0.0;                 // pA
    double q_num = c_s * vs_mv, c_den = c_s;

    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        dendritic_segment_t* segment = &dendrite->segments[i];
        double v_mv = (double)segment->voltage * 1000.0;
        double c = segment_cap_pf(segment);
        i_inst += (double)segment->att_soma * segment->g_soma_ns * (v_mv - vs_mv);
        q_num += (double)segment->att_soma * c * v_mv;
        c_den += c;
    }

    double total_current;
    if (dendrite->soma_charge_dt_ms > 0.0F) {
        total_current = (double)dendrite->soma_charge_fc / dendrite->soma_charge_dt_ms;  // fC/ms = pA
    } else {
        total_current = i_inst;
    }
    dendrite->soma_charge_fc = 0.0F;
    dendrite->soma_charge_dt_ms = 0.0F;
    if (!isfinite(total_current)) total_current = 0.0;

    double v_eq = q_num / c_den * 1e-3;
    dendrite->somatic_voltage = isfinite(v_eq) ? (float)v_eq : 0.0F;

    nimcp_mutex_unlock(&dendrite->lock);

    return (float)total_current;
}

/**
 * WHAT: Calculate signal attenuation from segment to soma
 * WHY:  Determine how much signal reaches soma (cable theory)
 * HOW:  Electrotonic distance X = sum over the path of L_k / λ_k with
 *       λ = sqrt(R_m * d / (4 * R_i)) (R_m Ω·cm², R_i Ω·cm, d cm);
 *       attenuation = exp(-X) (infinite-cable steady state).
 *       The path is the segment's ancestor chain; if the configured
 *       path_distance is not consistent with it, path_distance / λ_own is
 *       used. (Previously λ = sqrt(R_m/R_a) of absolute resistances, which
 *       drops the diameter: 0.27 at 75 μm instead of ~0.9.)
 *       Caller holds dendrite->lock (or owns the dendrite exclusively).
 */
static float calculate_attenuation(dendrite_t* dendrite, uint32_t segment_id) {
    // Guard: Invalid inputs
    if (!dendrite || segment_id >= dendrite->num_segments) {
        return 0.0F;
    }

    dendritic_segment_t* segment = &dendrite->segments[segment_id];
    float lambda_self = length_constant_um(segment->diameter);
    if (!(lambda_self > 0.0F)) return 0.0F;

    float dist = segment->path_distance > 0.0F ? segment->path_distance : 0.0F;

    // Electrotonic length of ancestors
    float x_anc = 0.0F, len_anc = 0.0F;
    uint32_t p = segment->parent_segment;
    for (uint32_t hops = 0; p < dendrite->num_segments && p != segment_id &&
         hops < dendrite->num_segments; hops++) {
        const dendritic_segment_t* a = &dendrite->segments[p];
        float lam = length_constant_um(a->diameter);
        if (lam > 0.0F) x_anc += a->length / lam;
        len_anc += a->length;
        p = a->parent_segment;
    }

    float X;
    if (len_anc > 0.0F && len_anc <= dist + 1e-3F) {
        X = x_anc + (dist - len_anc) / lambda_self;
    } else {
        X = dist / lambda_self;
    }

    float attenuation = expf(-fminf(X, 80.0F));
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
 * WHAT: Integrate the passive dendrite + soma-load over the deferred time
 * WHY:  dendrite_step() only does the full compartment update every 10th
 *       call; the skipped time must not be lost (previously it was, so the
 *       effective time constant was 10x too long after the first 10 steps).
 * HOW:  Each segment i (C_i, leak G_m,i, tau = Rm*Cm ~ 20 ms) is coupled by
 *       G_c,i (axial path) to a passive soma-load compartment (100 pF,
 *       tau_m). Per sub-step (<= 0.5 ms) the segment is advanced exactly
 *       (exponential Euler, soma voltage held), the charge it passes to the
 *       soma is integrated exactly and delivered attenuated by the cable
 *       (att_i = exp(-X)); the soma then decays with tau_m. Unconditionally
 *       stable. Idle dendrites take a fast path. Caller holds dendrite->lock
 *       and has unshared CoW arrays.
 */
static void flush_pending_locked(dendrite_t* dendrite) {
    float dt = dendrite->pending_dt_ms;
    dendrite->pending_dt_ms = 0.0F;
    if (!(dt > 0.0F) || !isfinite(dt)) return;

    // Fast path: nothing to integrate except calcium decay
    bool idle = fabsf(dendrite->soma_v) < 1e-9F;
    for (uint32_t i = 0; idle && i < dendrite->num_segments; i++) {
        if (fabsf(dendrite->segments[i].voltage) >= 1e-9F) idle = false;
    }
    if (idle) {
        for (uint32_t i = 0; i < dendrite->num_segments; i++) {
            dendrite->segments[i].voltage = 0.0F;
            update_segment_calcium(&dendrite->segments[i], dt);
        }
        dendrite->soma_v = 0.0F;
        dendrite->soma_charge_dt_ms += dt;
        return;
    }

    const float c_s = DENDRITE_SOMA_LOAD_PF;
    float tau_s = dendrite->num_segments > 0 ? segment_tau_ms(&dendrite->segments[0]) : 20.0F;
    int nsub = (int)ceilf(dt / DENDRITE_MAX_SUBSTEP_MS);
    if (nsub < 1) nsub = 1;
    if (nsub > 100000) nsub = 100000;
    float h = dt / (float)nsub;
    float e_s = expf(-h / tau_s);
    float e_s_half = expf(-0.5F * h / tau_s);

    for (int k = 0; k < nsub; k++) {
        float vs = dendrite->soma_v * 1000.0F;   // mV
        double q_soma = 0.0;                      // fC
        for (uint32_t i = 0; i < dendrite->num_segments; i++) {
            dendritic_segment_t* seg = &dendrite->segments[i];
            float c = segment_cap_pf(seg);
            if (!(c > 0.0F)) continue;
            float g_m = (seg->R_m > 0.0F) ? 1e9F / seg->R_m : 0.0F;   // nS
            float g_c = seg->g_soma_ns;
            float g = g_m + g_c;
            float v = seg->voltage * 1000.0F;
            if (!(g > 0.0F)) continue;
            float tau = c / g;                     // pF/nS = ms
            float v_inf = g_c * vs / g;
            float e = expf(-fminf(h / tau, 80.0F));
            float v_new = v_inf + (v - v_inf) * e;
            // charge through g_c during h: g_c * integral(v(t) - vs) dt
            double q = (double)g_c * ((double)(v - v_inf) * tau * (1.0F - e) +
                                      (double)(v_inf - vs) * h);
            q_soma += (double)seg->att_soma * q;
            seg->voltage = clamp_seg_voltage(v_new * 1e-3F);
        }
        if (!isfinite(q_soma)) q_soma = 0.0;
        float vs_new = vs * e_s + (float)(q_soma / c_s) * e_s_half;
        dendrite->soma_v = clamp_seg_voltage(vs_new * 1e-3F);
        dendrite->soma_charge_fc += (float)q_soma;
    }
    dendrite->soma_charge_dt_ms += dt;

    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        update_segment_calcium(&dendrite->segments[i], dt);
    }
}

/**
 * WHAT: Advance dendrite simulation by one timestep
 * WHY:  Update voltage, calcium, and plasticity
 * HOW:  Integrate differential equations for all segments. Full compartment
 *       update every 10th call (every call for the first 10); in between the
 *       elapsed time is accumulated in pending_dt_ms and applied exactly at
 *       the next full update or before any API reads/writes segment state.
 *       mean_voltage / calcium_level are decayed exactly on skipped frames.
 */
void dendrite_step(dendrite_t* dendrite, float dt_ms, uint64_t timestamp) {
    (void)timestamp;
    // Guard: NULL dendrite
    if (!dendrite) {
        return;
    }
    if (!(dt_ms > 0.0F) || !isfinite(dt_ms)) {
        return;
    }

    nimcp_mutex_lock(&dendrite->lock);

    dendrite->update_counter++;
    dendrite->pending_dt_ms += dt_ms;

    if (dendrite->update_counter % 10 != 0 && dendrite->update_counter > 10) {
        // Skip-frame: aggregate means follow the exact passive decay
        float tau_m = dendrite->time_constant > 0.0F ? dendrite->time_constant : 20.0F;
        dendrite->mean_voltage *= expf(-dt_ms / tau_m);
        dendrite->calcium_level *= expf(-dt_ms / CALCIUM_DECAY_TAU);
        dendrite->activity.mean_voltage = dendrite->mean_voltage;
        dendrite->activity.mean_calcium = dendrite->calcium_level;
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }

    if (!cow_unshare_locked(dendrite)) {
        // OOM: keep the time pending, try again next step
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }

    float old_mean_voltage = dendrite->mean_voltage;
    float elapsed_ms = dendrite->pending_dt_ms;

    // Update all segments (full compartmental model computation)
    flush_pending_locked(dendrite);

    // BUGFIX: Guard against division by zero when num_segments is 0
    if (dendrite->num_segments > 0) {
        // Update mean voltage
        float voltage_sum = 0.0F;
        for (uint32_t i = 0; i < dendrite->num_segments; i++) {
            voltage_sum += dendrite->segments[i].voltage;
        }
        dendrite->mean_voltage = voltage_sum / (float)dendrite->num_segments;

        // Cache voltage gradient (V/s) over the elapsed interval
        float dt_s = elapsed_ms / 1000.0F;
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

    if (spine_id >= dendrite->num_spines || !cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }

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

    if (spine_id >= dendrite->num_spines || !cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }

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

    if (!cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }

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
                // Already holding dendrite->lock: use the unlocked variant
                // (the public one now locks).
                dendrite_remove_spine_locked(dendrite, i);
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
 * WHAT: Calculate Mg²⁺ block factor (fraction of NMDA conductance unblocked)
 * WHY:  NMDA receptors are voltage-gated via Mg²⁺
 * HOW:  Jahr & Stevens (1990): B(V) = 1 / (1 + [Mg]_o/3.57 mM * exp(-0.062*V))
 *       with V the ABSOLUTE membrane potential in mV (B(-70) ~ 0.04,
 *       B(-20) ~ 0.5, B(0) ~ 0.78). (Previously V relative to rest and no
 *       /3.57: B = 0.5 at rest.)
 */
static float calculate_mg_block(float voltage_abs_mv) {
    if (!isfinite(voltage_abs_mv)) return 0.0F;
    float block = 1.0F / (1.0F + (NMDA_MG_CONCENTRATION / NMDA_MG_HALF_BLOCK_MM) *
                          expf(fminf(-NMDA_MG_BLOCK_CONSTANT * voltage_abs_mv, 80.0f)));
    return isfinite(block) ? block : 0.0F;
}

/** Absolute membrane potential (mV) of a relative segment/spine voltage (V). */
static inline float abs_mv(float v_rel_volts) {
    return DENDRITE_E_REST_MV + v_rel_volts * 1000.0F;
}

bool dendrite_initiate_nmda_spike(dendrite_t* dendrite, uint32_t segment_id,
                                   uint64_t timestamp) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_initiate_nmda_spike: dendrite is NULL");
        return false;
    }

    nimcp_mutex_lock(&dendrite->lock);

    // Guard: Invalid segment
    if (segment_id >= dendrite->num_segments) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_initiate_nmda_spike: segment_id out of range");
        return false;
    }

    // Guard: Already in NMDA spike (checked under the lock: no TOCTOU)
    if (dendrite->nmda_state.active) {
        nimcp_mutex_unlock(&dendrite->lock);
        return false;
    }

    if (!cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_initiate_nmda_spike: CoW unshare failed");
        return false;
    }
    flush_pending_locked(dendrite);

    dendritic_segment_t* segment = &dendrite->segments[segment_id];

    // Check threshold conditions (depolarisation relative to rest, mV).
    // Not reaching threshold is a normal outcome, not an error.
    float depol_mv = segment->voltage * 1000.0F;
    if (depol_mv < NIMCP_NMDA_SPIKE_THRESHOLD_MV || !segment->has_active_properties) {
        nimcp_mutex_unlock(&dendrite->lock);
        return false;
    }

    // Initiate NMDA spike
    dendrite->nmda_state.active = true;
    dendrite->nmda_state.onset_time = timestamp;
    dendrite->nmda_state.duration_remaining_ms = NIMCP_NMDA_PLATEAU_DURATION_MS;
    dendrite->nmda_state.peak_voltage_mv = NIMCP_NMDA_SPIKE_AMPLITUDE_MV;
    dendrite->nmda_state.segment_id = segment_id;
    // Block at the plateau potential that the spike establishes
    float plateau_rel_v = fmaxf(segment->voltage, NIMCP_NMDA_SPIKE_AMPLITUDE_MV / 1000.0F);
    dendrite->nmda_state.mg_block_factor = calculate_mg_block(abs_mv(plateau_rel_v));
    dendrite->nmda_state.calcium_influx_rate =
        NMDA_CALCIUM_PERMEABILITY * dendrite->nmda_state.mg_block_factor;

    // Set segment NMDA state
    segment->nmda_spike_active = true;
    segment->nmda_spike_voltage = NIMCP_NMDA_SPIKE_AMPLITUDE_MV / 1000.0F;
    segment->nmda_spike_remaining_ms = NIMCP_NMDA_PLATEAU_DURATION_MS;
    segment->voltage = clamp_seg_voltage(fmaxf(segment->voltage, segment->nmda_spike_voltage));

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

    nimcp_mutex_lock(&dendrite->lock);

    // Guard: No active NMDA spike (checked under the lock)
    if (!dendrite->nmda_state.active ||
        dendrite->nmda_state.segment_id >= dendrite->num_segments ||
        !cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }
    flush_pending_locked(dendrite);

    dendritic_segment_t* segment = &dendrite->segments[dendrite->nmda_state.segment_id];

    // Decay plateau duration
    dendrite->nmda_state.duration_remaining_ms -= dt_ms;
    segment->nmda_spike_remaining_ms -= dt_ms;

    // Calcium influx through unblocked NMDA receptors at the current voltage
    dendrite->nmda_state.mg_block_factor = calculate_mg_block(abs_mv(segment->voltage));
    dendrite->nmda_state.calcium_influx_rate =
        NMDA_CALCIUM_PERMEABILITY * dendrite->nmda_state.mg_block_factor;
    float ca_influx = dendrite->nmda_state.calcium_influx_rate * dt_ms;
    segment->calcium += ca_influx;

    // Clamp calcium
    if (segment->calcium > CALCIUM_BUFFER_CAPACITY) {
        segment->calcium = CALCIUM_BUFFER_CAPACITY;
    }

    // Maintain plateau voltage (slight decay)
    float decay_factor = expf(fminf(-dt_ms / (NIMCP_NMDA_PLATEAU_DURATION_MS * 2.0F), 80.0f));
    segment->nmda_spike_voltage *= decay_factor;
    segment->voltage = clamp_seg_voltage(fmaxf(segment->voltage, segment->nmda_spike_voltage));

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

    nimcp_mutex_lock(&dendrite->lock);
    bool ok = false;
    if (segment_id < dendrite->num_segments) {
        dendritic_segment_t* segment = &dendrite->segments[segment_id];
        // Deferred passive decay (if any) applied analytically for the read
        float v = segment->voltage;
        if (dendrite->pending_dt_ms > 0.0F) v *= expf(-dendrite->pending_dt_ms / segment_tau_ms(segment));
        float depol_mv = v * 1000.0F;
        ok = depol_mv >= NIMCP_NMDA_SPIKE_THRESHOLD_MV * 0.8F &&
             segment->has_active_properties &&
             !segment->nmda_spike_active &&
             // Mg²⁺ block sufficiently relieved for a regenerative spike
             // (B >= 0.10 at >= -54 mV absolute)
             calculate_mg_block(abs_mv(v)) >= NMDA_MIN_UNBLOCK;
    }
    nimcp_mutex_unlock(&dendrite->lock);
    return ok;
}

// ============================================================================
// Backpropagating Action Potential (bAP) Implementation
// ============================================================================

bool dendrite_initiate_bap(dendrite_t* dendrite, float amplitude_mv,
                            uint64_t timestamp) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_initiate_bap: dendrite is NULL");
        return false;
    }
    if (!isfinite(amplitude_mv)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_initiate_bap: non-finite amplitude");
        return false;
    }

    nimcp_mutex_lock(&dendrite->lock);

    // Guard: Already propagating (checked under the lock: no TOCTOU).
    // A new AP during a propagating bAP is a normal event, not an error.
    if (dendrite->bap_state.active || !cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        return false;
    }
    flush_pending_locked(dendrite);

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

    nimcp_mutex_lock(&dendrite->lock);

    // Guard: No active bAP (checked under the lock)
    if (!dendrite->bap_state.active || !cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }
    flush_pending_locked(dendrite);

    // Advance wavefront
    float distance_traveled = dendrite->bap_state.velocity_um_ms * dt_ms;
    dendrite->bap_state.wavefront_position_um += distance_traveled;

    // Attenuate amplitude
    float attenuation = expf(fminf(-NIMCP_BAP_ATTENUATION_PER_UM * distance_traveled, 80.0f));
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
                                           segment->path_distance, 80.0f));

            // Boost at distal segments with active properties
            if (segment->has_active_properties && segment->path_distance > 200.0F) {
                local_attenuation *= BAP_NA_BOOST_FACTOR;
            }

            // bap_amplitude in volts (relative to rest), like segment->voltage
            segment->bap_amplitude = (dendrite->bap_state.peak_amplitude_mv / 1000.0F) *
                                     local_attenuation;

            // Inject calcium: per mV of local bAP (was applied to volts ->
            // 1000x too small)
            float bap_mv = segment->bap_amplitude * 1000.0F;
            if (bap_mv > 0.0F) {
                segment->calcium += bap_mv * BAP_CALCIUM_PER_MV * NIMCP_BAP_CALCIUM_FACTOR;
                if (segment->calcium > CALCIUM_BUFFER_CAPACITY) segment->calcium = CALCIUM_BUFFER_CAPACITY;
            }

            // Update segment voltage (transient; decays with tau_m)
            segment->voltage = clamp_seg_voltage(fmaxf(segment->voltage, segment->bap_amplitude));

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

    nimcp_mutex_lock(&dendrite->lock);
    bool reached = false;
    if (spine_id < dendrite->num_spines) {
        uint32_t segment_id = dendrite->spines[spine_id].segment_id;
        if (segment_id < dendrite->num_segments) {
            reached = dendrite->segments[segment_id].bap_reached;
        }
    }
    nimcp_mutex_unlock(&dendrite->lock);
    return reached;
}

// ============================================================================
// Inter-Segment Coupling (Cable Theory) Implementation
// ============================================================================

/**
 * WHAT: Axial current flow between connected compartments
 * WHY:  Cable coupling between segments
 * HOW:  Coupling conductance between a segment and its parent (centre to
 *       centre) g = 1 / (R_a,child/2 + R_a,parent/2), in nS. Each
 *       parent-child pair relaxes exactly towards its capacitance-weighted
 *       mean: dV_diff decays as exp(-g (1/C1 + 1/C2) dt). Charge-conserving
 *       and unconditionally stable. (Previously nS*V / μF was integrated
 *       with forward Euler: 1000x unit error, and coupling time constants
 *       of ~0.1 ms with dt = 1 ms made it blow up into the clamps.)
 *       I_axial_parent / I_axial_children record the currents (pA) at the
 *       start of the step. NOTE: dendrite_compute_somatic_current() applies
 *       analytic cable attenuation to each segment; combining it with
 *       explicit axial coupling counts the cable twice for distal inputs.
 */
void dendrite_update_axial_currents(dendrite_t* dendrite, float dt_ms) {
    // Guard: NULL dendrite
    if (!dendrite) return;
    if (!(dt_ms > 0.0F) || !isfinite(dt_ms)) return;

    nimcp_mutex_lock(&dendrite->lock);

    if (!cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }
    flush_pending_locked(dendrite);

    // First pass: conductances and (diagnostic) currents
    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        dendritic_segment_t* segment = &dendrite->segments[i];
        segment->I_axial_parent = 0.0F;
        segment->I_axial_children = 0.0F;
        segment->g_axial = 0.0F;
        if (segment->parent_segment < dendrite->num_segments && segment->parent_segment != i) {
            dendritic_segment_t* parent = &dendrite->segments[segment->parent_segment];
            float r_ohm = 0.5F * segment->R_a + 0.5F * parent->R_a;
            segment->g_axial = (r_ohm > 0.0F) ? 1e9F / r_ohm : 0.0F;   // nS
        }
    }
    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        dendritic_segment_t* segment = &dendrite->segments[i];
        if (segment->g_axial <= 0.0F) continue;
        dendritic_segment_t* parent = &dendrite->segments[segment->parent_segment];
        float i_pa = segment->g_axial * (parent->voltage - segment->voltage) * 1000.0F;  // nS*mV = pA
        segment->I_axial_parent += i_pa;
        parent->I_axial_children -= i_pa;
    }

    // Second pass: exact pairwise relaxation (operator splitting over edges)
    for (uint32_t i = 0; i < dendrite->num_segments; i++) {
        dendritic_segment_t* seg = &dendrite->segments[i];
        if (seg->g_axial <= 0.0F) continue;
        dendritic_segment_t* par = &dendrite->segments[seg->parent_segment];
        float c1 = segment_cap_pf(seg), c2 = segment_cap_pf(par);
        if (!(c1 > 0.0F) || !(c2 > 0.0F)) continue;
        // g[nS] / C[pF] = 1/ms
        float k = seg->g_axial * (1.0F / c1 + 1.0F / c2);
        float v_mean = (c1 * seg->voltage + c2 * par->voltage) / (c1 + c2);
        float diff = (seg->voltage - par->voltage) * expf(-fminf(k * dt_ms, 80.0F));
        // seg - par = diff ; c1*seg + c2*par = (c1+c2)*v_mean
        seg->voltage = clamp_seg_voltage(v_mean + diff * c2 / (c1 + c2));
        par->voltage = clamp_seg_voltage(v_mean - diff * c1 / (c1 + c2));
    }

    nimcp_mutex_unlock(&dendrite->lock);
}

static float dendrite_calculate_rall_ratio_locked(dendrite_t* dendrite, uint32_t segment_id);

float dendrite_calculate_rall_ratio(dendrite_t* dendrite, uint32_t segment_id) {
    // Guard clauses
    if (!dendrite) return 0.0F;
    nimcp_mutex_lock(&dendrite->lock);
    float ratio = 0.0F;
    if (segment_id < dendrite->num_segments && cow_unshare_locked(dendrite)) {
        ratio = dendrite_calculate_rall_ratio_locked(dendrite, segment_id);
    }
    nimcp_mutex_unlock(&dendrite->lock);
    return ratio;
}

static float dendrite_calculate_rall_ratio_locked(dendrite_t* dendrite, uint32_t segment_id) {
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
                if (spine->state != SPINE_STATE_ELIMINATED && spine->total_inputs > 0 &&
                    timestamp >= spine->last_input_time &&
                    timestamp - spine->last_input_time < window_us) {
                    active_spines[num_active++] = spine_id;
                }
            }
        }

        // Need minimum spines for a cluster
        if (num_active < NIMCP_CLUSTER_MIN_SPINES) continue;

        // Simple clustering: all active spines on same segment form a cluster
        // (More sophisticated spatial clustering could use actual positions)
        // Bounds follow the tier-optimised array sizes (previously hard-coded
        // 32 clusters / 16 spines: overflowed on tiers with smaller arrays)
        if (dendrite->num_clusters < (uint32_t)NIMCP_HISTORY_SIZE_MEDIUM) {
            spine_cluster_t* cluster = &dendrite->clusters[dendrite->num_clusters];

            cluster->segment_id = seg_i;
            cluster->num_spines = (num_active > (uint32_t)NIMCP_MAX_SPINE_IDS) ?
                                  (uint32_t)NIMCP_MAX_SPINE_IDS : num_active;

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

            // Check NMDA spike eligibility (segment voltage in volts rel.
            // to rest: half of the spike threshold)
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

    nimcp_mutex_lock(&dendrite->lock);
    if (cluster_id >= dendrite->num_clusters) {
        nimcp_mutex_unlock(&dendrite->lock);
        return 0.0F;
    }

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
    float out = cluster->nonlinear_output;

    nimcp_mutex_unlock(&dendrite->lock);
    return out;
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
    if (cow_unshare_locked(dendrite)) {
        dendrite_stdp_pre_spike_unlocked(dendrite, spine_id, timestamp);
    }
    nimcp_mutex_unlock(&dendrite->lock);
}

void dendrite_stdp_post_spike(dendrite_t* dendrite, uint64_t timestamp) {
    // Guard: NULL dendrite
    if (!dendrite) return;

    nimcp_mutex_lock(&dendrite->lock);

    if (!cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }

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

    if (!cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }

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

    // Guard: zero capacity (bitmap/modulo arithmetic below needs >= 1)
    if (capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_create_spine_pool: capacity is zero");
        return false;
    }

    nimcp_mutex_lock(&dendrite->lock);

    // Guard: Already has pool (checked under the lock)
    if (dendrite->spine_pool) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_create_spine_pool: pool already exists");
        return false;
    }

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

/*
 * CoW model (fixed 2026-11): every handle (original or copy) is an
 * independently owned dendrite_t. Handles created by dendrite_cow_copy()
 * share the segment and spine arrays; sharers hold a pointer to one heap
 * counter (cow_shared_refs, atomically updated) of how many handles share
 * those arrays. Every API that writes segment/spine state first calls
 * cow_unshare_locked(), which gives the handle private arrays, so shared
 * arrays are immutable while shared. dendrite_destroy() / dendrite_cow_release()
 * drop one handle and free the arrays only when the last sharer goes.
 *
 * Previously: the copy got cow_ref_count = 1, so its prepare_write was a
 * no-op and writes (including realloc in add_spine/add_branch) went to the
 * shared arrays; destroying either handle freed arrays still used by the
 * other (use-after-free / double free); the spine pool pointer was shared
 * too.
 */

/* Caller holds dendrite->lock. Returns false on OOM (arrays stay shared and
 * must not be written). */
static bool cow_unshare_locked(dendrite_t* dendrite) {
    uint32_t* refs = dendrite->cow_shared_refs;
    if (!refs) return true;

    if (__atomic_load_n(refs, __ATOMIC_ACQUIRE) <= 1U) {
        // Sole remaining holder (new sharers can only be created from a
        // handle under its own lock, i.e. ours): take exclusive ownership.
        nimcp_free(refs);
        dendrite->cow_shared_refs = NULL;
        dendrite->cow_ref_count = 1;
        return true;
    }

    dendritic_segment_t* new_segments = NULL;
    dendritic_spine_t* new_spines = NULL;
    if (dendrite->segments && dendrite->num_segments > 0) {
        new_segments = (dendritic_segment_t*)nimcp_malloc(
            dendrite->num_segments * sizeof(dendritic_segment_t));
        if (!new_segments) return false;
        memcpy(new_segments, dendrite->segments,
               dendrite->num_segments * sizeof(dendritic_segment_t));
    }
    if (dendrite->spines && dendrite->num_spines > 0) {
        new_spines = (dendritic_spine_t*)nimcp_malloc(
            dendrite->num_spines * sizeof(dendritic_spine_t));
        if (!new_spines) {
            nimcp_free(new_segments);
            return false;
        }
        memcpy(new_spines, dendrite->spines,
               dendrite->num_spines * sizeof(dendritic_spine_t));
    }

    dendritic_segment_t* old_segments = dendrite->segments;
    dendritic_spine_t* old_spines = dendrite->spines;
    dendrite->segments = new_segments;
    dendrite->spines = new_spines;
    dendrite->cow_shared_refs = NULL;
    dendrite->cow_ref_count = 1;

    if (__atomic_sub_fetch(refs, 1U, __ATOMIC_ACQ_REL) == 0) {
        // Every other sharer unshared concurrently: old arrays are orphaned
        nimcp_free(old_segments);
        nimcp_free(old_spines);
        nimcp_free(refs);
    }
    return true;
}

dendrite_t* dendrite_cow_copy(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_cow_copy: dendrite is NULL");
        return NULL;
    }

    nimcp_mutex_lock(&dendrite->lock);

    dendrite_t* copy = (dendrite_t*)nimcp_calloc(1, sizeof(dendrite_t));
    if (!copy) {
        nimcp_mutex_unlock(&dendrite->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_cow_copy: copy is NULL");
        return NULL;
    }

    if (!dendrite->cow_shared_refs) {
        dendrite->cow_shared_refs = (uint32_t*)nimcp_malloc(sizeof(uint32_t));
        if (!dendrite->cow_shared_refs) {
            nimcp_free(copy);
            nimcp_mutex_unlock(&dendrite->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_cow_copy: refcount alloc failed");
            return NULL;
        }
        __atomic_store_n(dendrite->cow_shared_refs, 1U, __ATOMIC_RELEASE);
    }
    uint32_t sharers = __atomic_add_fetch(dendrite->cow_shared_refs, 1U, __ATOMIC_ACQ_REL);

    // Copy header data; segment/spine arrays are shared (shallow)
    memcpy(copy, dendrite, sizeof(dendrite_t));

    // The spine pool is owned by the original only
    copy->spine_pool = NULL;
    copy->use_spine_pool = false;

    // Fresh lock for the copy (the memcpy'd mutex bytes are not a valid mutex)
    nimcp_mutex_init(&copy->lock, NULL);

    copy->cow_modified = false;
    copy->cow_ref_count = sharers;      // informational: handles sharing arrays
    dendrite->cow_ref_count = sharers;

    nimcp_mutex_unlock(&dendrite->lock);

    LOG_DEBUG(LOG_MODULE, "dendrite_cow_copy: CoW copy created (sharers=%u)", sharers);

    return copy;
}

bool dendrite_cow_prepare_write(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_cow_prepare_write: dendrite is NULL");
        return false;
    }

    nimcp_mutex_lock(&dendrite->lock);
    bool ok = cow_unshare_locked(dendrite);
    if (ok) dendrite->cow_modified = true;
    nimcp_mutex_unlock(&dendrite->lock);

    if (!ok) {
        LOG_ERROR(LOG_MODULE, "dendrite_cow_prepare_write: Failed to allocate private copy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendrite_cow_prepare_write: unshare failed");
    }
    return ok;
}

/**
 * Drop this handle. Arrays shared with other CoW handles survive until the
 * last sharer is released/destroyed. Equivalent to dendrite_destroy().
 */
void dendrite_cow_release(dendrite_t* dendrite) {
    // Guard: NULL dendrite
    if (!dendrite) return;
    dendrite_destroy(dendrite);
}

// ============================================================================
// Spine-Level Input Processing Implementation
// ============================================================================

/*
 * Spine synapse model (fixed 2026-11; previously g_ampa/g_nmda were never
 * raised, so every spine returned 0 pA):
 *
 *   Presynaptic input x >= 0 (1.0 = one release event; graded values for
 *   graded release) opens conductances in proportion to weight * x:
 *     g_ampa += 0.5  nS * w * x * (AMPA receptors / 100)   tau 2 ms
 *     g_nmda += 0.25 nS * w * x * (NMDA receptors / 20)    tau 100 ms
 *   (each capped at 20 nS). Synaptic current (pA, + = depolarising):
 *     I = g_ampa (E - V_h) + g_nmda B(V_h) (E - V_h) + g_gaba (E_gaba - V_h)
 *   E = 0 mV abs; B = Jahr-Stevens Mg²⁺ block at the absolute head voltage.
 *   The head is coupled to the dendrite through the neck resistance (head
 *   RC ~ 1 μs, i.e. quasi-static): V_h = V_seg + I * R_neck, solved by
 *   fixed-point iteration. spine->voltage = V_h (volts, rel. to rest).
 */

typedef struct {
    float i_total;   // pA
    float i_nmda;    // pA
    float v_head;    // mV rel. to rest
} spine_current_t;

/* Caller holds dendrite->lock. v_seg_mv: segment voltage, mV rel. to rest. */
static spine_current_t spine_current_locked(const dendritic_spine_t* spine, float v_seg_mv) {
    spine_current_t r = {0.0F, 0.0F, v_seg_mv};
    float gA = spine->g_ampa, gN = spine->g_nmda, gG = spine->g_gaba;
    if (!(gA > 0.0F) && !(gN > 0.0F) && !(gG > 0.0F)) return r;
    float rn = spine->neck_resistance > 0.0F ? spine->neck_resistance * 1e-3F : 0.0F; // MΩ->(mV per pA)
    float vh = v_seg_mv;
    float B = 0.0F;
    for (int it = 0; it < 4; it++) {
        B = calculate_mg_block(DENDRITE_E_REST_MV + vh);
        float g_exc = gA + gN * B;               // nS
        float g_all = g_exc + gG;
        float drive = g_exc * DENDRITE_E_EXC_REL_MV + gG * DENDRITE_E_GABA_REL_MV; // nS*mV
        // I = drive - g_all*V_h, V_h = V_seg + I*rn  ->  I = (drive - g_all V_seg) / (1 + g_all rn)
        float I = (drive - g_all * v_seg_mv) / (1.0F + g_all * rn);
        vh = v_seg_mv + I * rn;
        r.i_total = I;
    }
    r.i_nmda = gN * B * (DENDRITE_E_EXC_REL_MV - vh);
    r.v_head = vh;
    if (!isfinite(r.i_total)) { r.i_total = 0.0F; r.i_nmda = 0.0F; r.v_head = v_seg_mv; }
    return r;
}

/* Segment voltage (mV rel. to rest) with any deferred passive decay applied. */
static float spine_segment_mv_locked(dendrite_t* dendrite, const dendritic_spine_t* spine) {
    if (spine->segment_id >= dendrite->num_segments) return 0.0F;
    return dendrite->segments[spine->segment_id].voltage * 1000.0F;
}

float dendrite_spine_process_input(dendrite_t* dendrite, uint32_t spine_id,
                                    float current, uint64_t timestamp) {
    // Guard clauses
    if (!dendrite) return 0.0F;

    nimcp_mutex_lock(&dendrite->lock);

    if (spine_id >= dendrite->num_spines || !cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        return 0.0F;
    }
    flush_pending_locked(dendrite);

    dendritic_spine_t* spine = &dendrite->spines[spine_id];
    if (spine->state == SPINE_STATE_ELIMINATED) {
        nimcp_mutex_unlock(&dendrite->lock);
        return 0.0F;
    }

    float x = (isfinite(current) && current > 0.0F) ? current : 0.0F;

    // Event-driven kinetics: when nobody calls dendrite_update_spine_conductances(),
    // decay the conductances / Ca²⁺ over the time since this spine's last input.
    if (!dendrite->spine_kinetics_stepped && spine->total_inputs > 0 &&
        timestamp > spine->last_input_time) {
        float el_ms = (float)(timestamp - spine->last_input_time) * 1e-3F;
        spine->g_ampa *= expf(-fminf(el_ms / SPINE_TAU_AMPA_MS, 80.0F));
        spine->g_nmda *= expf(-fminf(el_ms / SPINE_TAU_NMDA_MS, 80.0F));
        spine->g_gaba *= expf(-fminf(el_ms / SPINE_TAU_GABA_MS, 80.0F));
        spine->calcium *= expf(-fminf(el_ms / CALCIUM_DECAY_TAU, 80.0F));
    }

    // Update input timing
    spine->last_input_time = timestamp;
    spine->total_inputs++;

    // Open receptors in proportion to weight x input
    float w = spine->synaptic_weight > 0.0F ? spine->synaptic_weight : 0.0F;
    float ampa_scale = spine->ampa_receptors / SPINE_AMPA_RECEPTORS_REF;
    float nmda_scale = spine->nmda_receptors / SPINE_NMDA_RECEPTORS_REF;
    if (!(ampa_scale > 0.0F)) ampa_scale = 0.0F;
    if (!(nmda_scale > 0.0F)) nmda_scale = 0.0F;
    float dgN = SPINE_G_NMDA_PER_INPUT_NS * w * x * nmda_scale;
    spine->g_ampa = fminf(spine->g_ampa + SPINE_G_AMPA_PER_INPUT_NS * w * x * ampa_scale, SPINE_G_MAX_NS);
    spine->g_nmda = fminf(spine->g_nmda + dgN, SPINE_G_MAX_NS);

    spine_current_t sc = spine_current_locked(spine, spine_segment_mv_locked(dendrite, spine));
    spine->voltage = sc.v_head * 1e-3F;

    // Event-driven mode: charge the expected NMDA Ca²⁺ of this release now
    // (stepped mode integrates it continuously instead).
    if (!dendrite->spine_kinetics_stepped && dgN > 0.0F) {
        float B = calculate_mg_block(DENDRITE_E_REST_MV + sc.v_head);
        float q_fc = dgN * B * (DENDRITE_E_EXC_REL_MV - sc.v_head) * SPINE_TAU_NMDA_MS; // nS*mV*ms = fC
        if (q_fc > 0.0F) spine->calcium += q_fc * SPINE_CA_UM_PER_FC_NMDA;
    }
    if (spine->calcium > CALCIUM_BUFFER_CAPACITY) {
        spine->calcium = CALCIUM_BUFFER_CAPACITY;
    }

    // Register pre-spike for STDP (use unlocked variant - we already hold dendrite->lock)
    if (x > 0.0F) {
        dendrite_stdp_pre_spike_unlocked(dendrite, spine_id, timestamp);
    }

    nimcp_mutex_unlock(&dendrite->lock);

    return sc.i_total;
}

/*
 * Time-stepped spine kinetics: for each spine, the synaptic charge delivered
 * during dt (exact integral of the exponentially decaying conductances at the
 * start-of-step driving force) is deposited on the parent segment's membrane
 * capacitance (dV[mV] = Q[fC] / C[pF]); NMDA charge loads spine Ca²⁺; then the
 * conductances decay. Call before dendrite_step(dt) each step. Once called,
 * dendrite_spine_process_input() leaves decay to this function.
 */
void dendrite_update_spine_conductances(dendrite_t* dendrite, float dt_ms) {
    // Guard: NULL dendrite
    if (!dendrite) return;
    if (!(dt_ms > 0.0F) || !isfinite(dt_ms)) return;

    nimcp_mutex_lock(&dendrite->lock);

    if (!cow_unshare_locked(dendrite)) {
        nimcp_mutex_unlock(&dendrite->lock);
        return;
    }
    flush_pending_locked(dendrite);
    dendrite->spine_kinetics_stepped = true;

    float decay_ampa = expf(fminf(-dt_ms / SPINE_TAU_AMPA_MS, 80.0f));
    float decay_nmda = expf(fminf(-dt_ms / SPINE_TAU_NMDA_MS, 80.0f));
    float decay_gaba = expf(fminf(-dt_ms / SPINE_TAU_GABA_MS, 80.0f));
    float decay_ca = expf(fminf(-dt_ms / CALCIUM_DECAY_TAU, 80.0f));
    // Effective time a conductance is "on" during dt: tau (1 - exp(-dt/tau))
    float teff_ampa = SPINE_TAU_AMPA_MS * (1.0F - decay_ampa);
    float teff_nmda = SPINE_TAU_NMDA_MS * (1.0F - decay_nmda);
    float teff_gaba = SPINE_TAU_GABA_MS * (1.0F - decay_gaba);

    for (uint32_t i = 0; i < dendrite->num_spines; i++) {
        dendritic_spine_t* spine = &dendrite->spines[i];
        if (spine->state == SPINE_STATE_ELIMINATED) continue;

        if (spine->g_ampa > 1e-9F || spine->g_nmda > 1e-9F || spine->g_gaba > 1e-9F) {
            float v_seg = spine_segment_mv_locked(dendrite, spine);
            spine_current_t sc = spine_current_locked(spine, v_seg);
            float vh = sc.v_head;
            float B = calculate_mg_block(DENDRITE_E_REST_MV + vh);
            float q_nmda = spine->g_nmda * B * (DENDRITE_E_EXC_REL_MV - vh) * teff_nmda;  // fC
            float q = spine->g_ampa * (DENDRITE_E_EXC_REL_MV - vh) * teff_ampa + q_nmda +
                      spine->g_gaba * (DENDRITE_E_GABA_REL_MV - vh) * teff_gaba;
            if (spine->segment_id < dendrite->num_segments && isfinite(q)) {
                dendritic_segment_t* seg = &dendrite->segments[spine->segment_id];
                float c_pf = segment_cap_pf(seg);
                if (c_pf > 1e-9F) {
                    float v_new_mv = seg->voltage * 1000.0F + q / c_pf;
                    // A synaptic current cannot drive past its reversal potential
                    if (q > 0.0F && v_new_mv > DENDRITE_E_EXC_REL_MV) v_new_mv = fmaxf(DENDRITE_E_EXC_REL_MV, seg->voltage * 1000.0F);
                    seg->voltage = clamp_seg_voltage(v_new_mv * 1e-3F);
                }
            }
            if (q_nmda > 0.0F && isfinite(q_nmda)) spine->calcium += q_nmda * SPINE_CA_UM_PER_FC_NMDA;
            spine->voltage = vh * 1e-3F;
        } else {
            spine->voltage = spine_segment_mv_locked(dendrite, spine) * 1e-3F;
        }

        // Decay conductances
        spine->g_ampa *= decay_ampa;
        spine->g_nmda *= decay_nmda;
        spine->g_gaba *= decay_gaba;

        // Decay calcium
        spine->calcium *= decay_ca;
        if (spine->calcium > CALCIUM_BUFFER_CAPACITY) spine->calcium = CALCIUM_BUFFER_CAPACITY;
    }

    nimcp_mutex_unlock(&dendrite->lock);
}
