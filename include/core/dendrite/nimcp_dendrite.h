/**
 * @file nimcp_dendrite.h
 * @brief NIMCP Dendrite Module - Dendritic Integration and Computation
 *
 * WHAT: Models dendritic trees with spines for spatiotemporal integration
 * WHY:  Dendrites perform sophisticated computations:
 *       - Spatiotemporal integration of synaptic inputs
 *       - Coincidence detection and pattern recognition
 *       - Active signal amplification (NMDA spikes, Ca²⁺ plateaus)
 *       - Local plasticity and structural modification
 * HOW:  Integrates tree morphology, cable theory, spines, and active properties
 *
 * BIOLOGICAL BACKGROUND:
 * - Basal dendrites: 100-300 μm, local integration, ~20-50 per neuron
 * - Apical dendrites: 500-1000 μm, cross-layer signaling, 1-2 per pyramidal neuron
 * - Dendritic spines: 2000-10000 per cortical pyramidal neuron
 * - Spine types: thin (learning), stubby (intermediate), mushroom (memory)
 * - Active properties: NMDA spikes, Ca²⁺ spikes, backpropagating action potentials
 *
 * ARCHITECTURE:
 * - Strategy Pattern: Different dendrite types (basal, apical, tuft)
 * - Composite Pattern: Tree structure for dendritic segments
 * - Factory Pattern: dendrite_create() with type-specific initialization
 * - Observer Pattern: Activity callbacks for plasticity
 *
 * PERFORMANCE:
 * - O(N) integration where N = number of active spines
 * - Memory: ~400 bytes per dendrite (base), ~80 bytes per segment, ~120 bytes per spine
 *
 * NIMCP STANDARDS:
 * - Functions < 50 lines
 * - Guard clauses for NULL checks
 * - WHAT/WHY/HOW documentation
 * - nimcp_malloc/nimcp_free for memory
 *
 * @version Phase 1.5.7: Dendrite Module
 * @date 2025-11-24
 */

#ifndef NIMCP_DENDRITE_H
#define NIMCP_DENDRITE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_tier_optimization.h"

//=============================================================================
// CONSTANTS
//=============================================================================

#define NIMCP_DENDRITE_MAX_SEGMENTS 512        // Max segments per dendrite
#define NIMCP_DENDRITE_MAX_SPINES 2000         // Max spines per dendrite
#define NIMCP_DENDRITE_MAX_CHILDREN 8          // Max children per segment
#define NIMCP_DENDRITE_NETWORK_MAX_DENDRITES 100000
#define NIMCP_DENDRITE_SPINE_POOL_SIZE 4096    // Memory pool for spines

#define NIMCP_DENDRITE_DEFAULT_DIAMETER_UM 2.0f
#define NIMCP_DENDRITE_MIN_DIAMETER_UM 0.5f
#define NIMCP_DENDRITE_MAX_DIAMETER_UM 5.0f

#define NIMCP_SPINE_DEFAULT_HEAD_DIAMETER_UM 0.5f
#define NIMCP_SPINE_DEFAULT_NECK_LENGTH_UM 0.8f
#define NIMCP_SPINE_DEFAULT_NECK_DIAMETER_UM 0.2f

// Electrical properties (typical values)
#define NIMCP_DENDRITE_R_M_DEFAULT 20000.0f    // Membrane resistance (Ω·cm²)
#define NIMCP_DENDRITE_C_M_DEFAULT 1.0f        // Membrane capacitance (μF/cm²)
#define NIMCP_DENDRITE_R_A_DEFAULT 150.0f      // Axial resistance (Ω·cm)

// Plasticity thresholds (Ca²⁺ concentrations in μM)
#define NIMCP_DENDRITE_LTP_THRESHOLD_DEFAULT 1.0f
#define NIMCP_DENDRITE_LTD_THRESHOLD_DEFAULT 0.5f

//-----------------------------------------------------------------------------
// NMDA Spike and Active Properties Constants
//-----------------------------------------------------------------------------
#define NIMCP_NMDA_SPIKE_THRESHOLD_MV 20.0f    // NMDA spike initiation threshold
#define NIMCP_NMDA_PLATEAU_DURATION_MS 100.0f  // Plateau potential duration
#define NIMCP_NMDA_SPIKE_AMPLITUDE_MV 40.0f    // NMDA spike amplitude
#define NIMCP_BAP_VELOCITY_UM_MS 500.0f        // Backpropagation velocity
#define NIMCP_BAP_ATTENUATION_PER_UM 0.003f    // bAP attenuation per μm
#define NIMCP_BAP_CALCIUM_FACTOR 0.5f          // Ca²⁺ influx from bAP

//-----------------------------------------------------------------------------
// STDP Constants (dendrite-specific values — include this header before
// nimcp_learning_constants.h to ensure these biological values win)
//-----------------------------------------------------------------------------
#ifndef NIMCP_STDP_TAU_PLUS_MS
#define NIMCP_STDP_TAU_PLUS_MS 17.0f           // LTP time constant
#endif
#ifndef NIMCP_STDP_TAU_MINUS_MS
#define NIMCP_STDP_TAU_MINUS_MS 34.0f          // LTD time constant
#endif
#ifndef NIMCP_STDP_A_PLUS
#define NIMCP_STDP_A_PLUS 0.005f               // LTP amplitude
#endif
#ifndef NIMCP_STDP_A_MINUS
#define NIMCP_STDP_A_MINUS 0.00525f            // LTD amplitude (asymmetric)
#endif
#define NIMCP_STDP_WINDOW_MS 100.0f            // STDP time window

//-----------------------------------------------------------------------------
// Spatial Clustering Constants
//-----------------------------------------------------------------------------
#define NIMCP_CLUSTER_RADIUS_UM 10.0f          // Cluster detection radius
#define NIMCP_CLUSTER_MIN_SPINES 3             // Minimum spines for cluster
#define NIMCP_CLUSTER_NONLINEAR_FACTOR 1.5f    // Nonlinear boost for clusters

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

typedef struct dendrite_struct dendrite_t;
typedef struct dendrite_network_struct dendrite_network_t;
typedef struct dendritic_spine_struct dendritic_spine_t;
typedef struct dendritic_segment_struct dendritic_segment_t;
typedef struct nmda_spike_state_struct nmda_spike_state_t;
typedef struct bap_state_struct bap_state_t;
typedef struct stdp_state_struct stdp_state_t;
typedef struct spine_cluster_struct spine_cluster_t;
typedef struct dendrite_spine_pool_struct dendrite_spine_pool_t;

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @brief Dendrite type classification
 */
typedef enum {
    DENDRITE_TYPE_BASAL = 0,      // Short, local integration (100-300 μm)
    DENDRITE_TYPE_APICAL = 1,     // Long, cross-layer signaling (500-1000 μm)
    DENDRITE_TYPE_APICAL_TUFT = 2,// Distal tuft in Layer 1
    DENDRITE_TYPE_OBLIQUE = 3,    // Mid-trunk branches
    DENDRITE_TYPE_COUNT
} dendrite_type_t;

/**
 * @brief Dendrite state machine
 */
typedef enum {
    DENDRITE_STATE_NORMAL = 0,    // Normal passive integration
    DENDRITE_STATE_PASSIVE = 0,   // Alias for NORMAL - Passive integration only
    DENDRITE_STATE_ACTIVE = 1,    // Active backpropagation
    DENDRITE_STATE_PLATEAU = 2,   // Plateau potential (NMDA spike)
    DENDRITE_STATE_DAMAGED = 3    // Non-functional
} dendrite_state_t;

/**
 * @brief Dendritic spine morphological types
 */
typedef enum {
    SPINE_TYPE_THIN = 0,       // Learning spines (small, plastic)
    SPINE_TYPE_STUBBY = 1,     // Intermediate state
    SPINE_TYPE_MUSHROOM = 2,   // Memory spines (large, stable)
    SPINE_TYPE_FILOPODIA = 3,  // Exploratory (motile)
    SPINE_TYPE_COUNT
} spine_type_t;

/**
 * @brief Spine functional state
 */
typedef enum {
    SPINE_STATE_STABLE = 0,       // Stable spine
    SPINE_STATE_RESTING = 0,      // Alias - Resting state
    SPINE_STATE_ACTIVE = 1,
    SPINE_STATE_POTENTIATED = 2,  // After LTP
    SPINE_STATE_DEPRESSED = 3,    // After LTD
    SPINE_STATE_PRUNING = 4,      // Being pruned
    SPINE_STATE_RETRACTING = 4,   // Alias - Structural pruning
    SPINE_STATE_ELIMINATED = 5    // Fully eliminated
} spine_state_t;

/**
 * @brief Dendritic segment types
 * NOTE: Prefixed with DENDRITE_ to avoid conflict with axon segment types
 */
typedef enum {
    DENDRITE_SEGMENT_PROXIMAL = 0,  // Proximal to soma
    DENDRITE_SEGMENT_SHAFT = 0,     // Alias - Main dendritic shaft
    DENDRITE_SEGMENT_DISTAL = 1,    // Distal from soma
    DENDRITE_SEGMENT_BRANCH = 1,    // Alias - Branch point
    DENDRITE_SEGMENT_TERMINAL = 2,  // Distal terminal
    DENDRITE_SEGMENT_OBLIQUE = 3    // Oblique branch
} dendrite_segment_type_t;

//=============================================================================
// ADVANCED PHYSIOLOGY STRUCTURES
//=============================================================================

/**
 * @brief NMDA spike/plateau potential state
 *
 * WHAT: Models regenerative NMDA receptor-mediated dendritic spikes
 * WHY:  NMDA spikes enable:
 *       - Burst firing induction at soma
 *       - Nonlinear integration for pattern detection
 *       - Enhanced plasticity via prolonged calcium influx
 * HOW:  Track spike state, duration, and calcium dynamics
 *
 * BIOLOGICAL:
 * - Threshold: ~20mV depolarization with glutamate
 * - Duration: 50-200ms (plateau potential)
 * - Ca²⁺ influx: 5-10x passive response
 * - Voltage-dependent Mg²⁺ block removal enables regeneration
 */
struct nmda_spike_state_struct {
    bool active;                    // NMDA spike currently active
    uint64_t onset_time;            // Spike onset timestamp (μs)
    float duration_remaining_ms;    // Remaining plateau duration
    float peak_voltage_mv;          // Peak voltage achieved
    float calcium_influx_rate;      // Current Ca²⁺ influx (μM/ms)
    uint32_t segment_id;            // Segment where spike initiated
    float mg_block_factor;          // Mg²⁺ block (0=full, 1=unblocked)
    float nmda_conductance;         // Current NMDA conductance (nS)
};

/**
 * @brief Backpropagating action potential (bAP) state
 *
 * WHAT: Models action potential propagation from soma into dendrites
 * WHY:  bAPs provide:
 *       - Coincidence signal for STDP
 *       - Dendritic calcium influx
 *       - Burst firing facilitation
 * HOW:  Track wavefront position and attenuation
 *
 * BIOLOGICAL:
 * - Velocity: 300-500 μm/ms in apical dendrite
 * - Attenuation: ~50% at 400 μm
 * - Enhanced by A-type K+ current reduction
 * - Boosted by dendritic Na+ channels
 */
struct bap_state_struct {
    bool active;                    // bAP currently propagating
    uint64_t onset_time;            // When AP fired at soma (μs)
    float wavefront_position_um;    // Current wavefront distance from soma
    float peak_amplitude_mv;        // Original AP amplitude
    float current_amplitude_mv;     // Current attenuated amplitude
    float velocity_um_ms;           // Propagation velocity
    uint32_t segments_reached;      // Count of segments reached
};

/**
 * @brief Spike-timing dependent plasticity (STDP) state per spine
 *
 * WHAT: Tracks pre/post spike timing for STDP computation
 * WHY:  STDP is primary mechanism for:
 *       - Hebbian learning ("neurons that fire together wire together")
 *       - Temporal sequence learning
 *       - Causal relationship detection
 * HOW:  Exponential traces updated by spikes
 *
 * BIOLOGICAL:
 * - Pre-before-post (Δt > 0): LTP (τ+ ≈ 17ms)
 * - Post-before-pre (Δt < 0): LTD (τ- ≈ 34ms)
 * - Asymmetric windows for causal learning
 */
struct stdp_state_struct {
    float pre_trace;                // Presynaptic eligibility trace (0-1)
    float post_trace;               // Postsynaptic eligibility trace (0-1)
    uint64_t last_pre_spike;        // Last presynaptic spike time (μs)
    uint64_t last_post_spike;       // Last postsynaptic spike time (μs)
    float accumulated_ltp;          // Accumulated LTP signal
    float accumulated_ltd;          // Accumulated LTD signal
    bool eligible_for_update;       // Ready for weight update
};

/**
 * @brief Spatial cluster of coactive spines
 *
 * WHAT: Group of nearby spines for nonlinear integration detection
 * WHY:  Clustered inputs enable:
 *       - Supralinear summation (1+1 > 2)
 *       - NMDA spike generation
 *       - Feature binding
 * HOW:  Track member spines and cluster properties
 *
 * BIOLOGICAL:
 * - Typical radius: 10-30 μm
 * - Minimum cluster size: 3-5 spines
 * - Nonlinear boost: 1.3-2x for clustered vs distributed
 */
struct spine_cluster_struct {
    uint32_t spine_ids[NIMCP_MAX_SPINE_IDS];  // Tier-optimized spine array
    uint32_t num_spines;                       // Active spines in cluster
    uint32_t segment_id;            // Segment containing cluster
    float center_position_um;       // Cluster center on segment
    float radius_um;                // Cluster radius
    float total_input;              // Sum of clustered inputs
    float nonlinear_output;         // Output after nonlinear boost
    bool nmda_spike_eligible;       // Can trigger NMDA spike
    uint64_t last_update_time;      // Last cluster update (μs)
};

/**
 * @brief Memory pool for spine allocation (O(1) alloc/free)
 *
 * WHAT: Pre-allocated pool for spine structures
 * WHY:  Eliminates malloc/free overhead for:
 *       - High-frequency spine creation/destruction
 *       - Deterministic allocation time
 *       - Cache-friendly memory layout
 * HOW:  Fixed-size blocks with bitmap allocation
 */
struct dendrite_spine_pool_struct {
    dendritic_spine_t* spines;      // Pre-allocated spine array
    uint64_t* allocation_bitmap;    // 1 bit per spine (1=allocated)
    uint32_t capacity;              // Total spines in pool
    uint32_t allocated_count;       // Currently allocated
    uint32_t next_free_hint;        // Hint for next free slot
    nimcp_mutex_t lock;           // Thread safety
};

//=============================================================================
// CONFIGURATION STRUCTURES
//=============================================================================

/**
 * @brief Configuration for creating a dendritic segment
 *
 * WHAT: Parameters for segment initialization
 * WHY:  Enable flexible segment creation with custom properties
 * HOW:  Pass to dendrite_create_segments() for batch creation
 */
typedef struct {
    dendrite_segment_type_t type;  // Segment type (use DENDRITE_SEGMENT_* constants)
    uint32_t parent_segment;       // Parent segment index (UINT32_MAX = root)
    float position[3];             // 3D position (μm)
    float length;                  // Segment length (μm)
    float diameter;                // Segment diameter (μm)
    float path_distance;           // Distance from soma (μm)
    bool has_active_properties;    // Enable active conductances
} segment_config_t;

/**
 * @brief Configuration for creating a dendrite
 *
 * WHAT: Parameters for dendrite initialization
 * WHY:  Enable flexible dendrite creation with custom properties
 * HOW:  Pass to dendrite_create() for creation
 */
typedef struct {
    uint32_t id;                    // Unique dendrite ID
    dendrite_type_t type;           // Dendrite type
    uint32_t target_neuron_id;      // Owning neuron ID
    float total_length;             // Total dendritic length (μm)
    float mean_diameter;            // Mean diameter (μm)
    float start_pos[3];             // Position at soma junction (μm)
    float integration_window_ms;    // Temporal summation window (ms)
    float structural_plasticity;    // Spine growth/pruning rate (0-1)
    float ltp_threshold;            // LTP calcium threshold (μM)
    float ltd_threshold;            // LTD calcium threshold (μM)
} dendrite_config_t;

//=============================================================================
// ACTIVITY STATISTICS
//=============================================================================

/**
 * @brief Dendrite activity tracking
 *
 * WHAT: Tracks dendritic input activity and calcium dynamics
 * WHY:  Essential for:
 *       - Activity-dependent plasticity
 *       - Homeostatic regulation
 *       - Structural plasticity (spine growth/pruning)
 * HOW:  Exponential moving averages and event counting
 */
typedef struct {
    uint64_t total_inputs;      // Lifetime synaptic events
    uint32_t recent_inputs;     // Recent window (last second)
    float input_rate;           // Hz
    float activity_ema;         // Exponential moving average (0-1)
    uint64_t last_input_time;   // μs
    float mean_iii;             // Mean inter-input interval (ms)
    float calcium_ema;          // Average Ca²⁺ level (μM)
    float plasticity_signal;    // For structural plasticity (0-1)
    float mean_voltage;         // Mean voltage across segments (mV)
    float mean_calcium;         // Mean calcium across segments (μM)
    float mean_input_rate;      // Mean input rate (Hz)
    uint32_t ltp_events;        // Count of LTP events
    uint32_t ltd_events;        // Count of LTD events
    uint32_t spine_formations;  // Count of new spines formed
    uint32_t spine_eliminations; // Count of spines eliminated
} dendrite_activity_stats_t;

//=============================================================================
// DENDRITIC SPINE STRUCTURE
//=============================================================================

/**
 * @brief Single dendritic spine with morphology and state
 *
 * WHAT: Postsynaptic protrusion receiving synaptic input
 * WHY:  Spines provide:
 *       - Electrical compartmentalization (high neck resistance)
 *       - Local Ca²⁺ signaling for plasticity
 *       - Structural plasticity (growth, shrinkage, elimination)
 *       - Synapse-dendrite isolation
 * HOW:  Models head, neck morphology, electrical properties, plasticity
 *
 * BIOLOGICAL:
 * - Length: ~1-2 μm, Head diameter: 0.3-1.2 μm
 * - Neck resistance: 50-500 MΩ (electrical compartmentalization)
 * - Lifetime: hours (thin) to years (mushroom)
 * - Volume change: 2-fold during LTP, 50% during LTD
 *
 * MEMORY: ~120 bytes
 */
struct dendritic_spine_struct {
    // Identification
    uint32_t id;
    spine_type_t type;
    spine_state_t state;

    // Connectivity
    uint32_t dendrite_id;       // Parent dendrite
    uint32_t segment_id;        // Segment attachment point
    uint32_t synapse_id;        // Synapse contacting this spine (0 = no synapse)

    // Morphology (μm)
    float neck_length;          // Spine neck length
    float neck_diameter;        // Spine neck diameter
    float head_diameter;        // Spine head diameter
    float head_volume;          // Spine head volume (μm³)

    // Electrical Properties
    float neck_resistance;      // Electrical coupling to dendrite (MΩ)
    float head_capacitance;     // Membrane capacitance (fF)

    // State Variables
    float voltage;              // Spine head voltage (mV)
    float calcium;              // Spine [Ca²⁺] (μM)

    // Plasticity
    float synaptic_weight;      // Current synapse strength (0-1)
    float ampa_receptors;       // AMPA receptor count (unitless)
    float nmda_receptors;       // NMDA receptor count (unitless)
    uint64_t last_potentiation; // Last LTP event (μs)
    uint64_t last_depression;   // Last LTD event (μs)

    // Structural Plasticity
    float growth_factor;        // Actin polymerization signal (0-1)
    float stability;            // Resistance to pruning (0-1)
    uint64_t creation_time;     // Birth timestamp (μs)

    // Activity
    uint64_t total_inputs;
    uint64_t last_input_time;

    // STDP state for this spine
    stdp_state_t stdp;

    // Active conductances (spine-type specific)
    float g_ampa;                   // AMPA conductance (nS)
    float g_nmda;                   // NMDA conductance (nS)
    float g_gaba;                   // GABA conductance (nS, for inhibitory)
};

//=============================================================================
// DENDRITIC SEGMENT STRUCTURE
//=============================================================================

/**
 * @brief Single segment in dendritic tree with cable properties
 *
 * WHAT: Individual compartment in dendrite with electrical and spatial properties
 * WHY:  Enables:
 *       - Realistic voltage attenuation modeling
 *       - Tree morphology representation
 *       - Active conductance localization
 *       - Spine attachment tracking
 * HOW:  Cable theory with parent-child tree structure
 *
 * BIOLOGICAL:
 * - Diameter tapers distally: 2-5 μm (proximal) to 0.5-1 μm (distal)
 * - Branch points increase electrical isolation
 * - Active conductances concentrated in specific regions
 *
 * MEMORY: ~80 bytes (without children arrays)
 */
struct dendritic_segment_struct {
    // Identification
    uint32_t id;
    dendrite_segment_type_t type;

    // Tree Structure
    uint32_t parent_segment;    // Parent in tree (UINT32_MAX = root/soma)
    uint32_t child_segments[NIMCP_DENDRITE_MAX_CHILDREN];
    uint32_t num_children;

    // Spatial Properties (μm)
    float position[3];          // 3D position from soma
    float length;               // Segment length
    float diameter;             // Segment diameter
    float path_distance;        // Distance from soma

    // Cable Properties
    float R_m;                  // Membrane resistance (Ω·cm²)
    float C_m;                  // Membrane capacitance (μF/cm²)
    float R_a;                  // Axial resistance (Ω·cm)
    float path_resistance;      // Cumulative resistance to soma (MΩ)

    // State Variables
    float voltage;              // Local voltage (mV)
    float calcium;              // Local [Ca²⁺] (μM)

    // Spines on this segment (tier-optimized)
    uint32_t spine_ids[NIMCP_MAX_SPINE_IDS];  // Tier-based for memory efficiency
    uint32_t num_spines;

    // Active Conductances (optional)
    float g_na;                 // Sodium conductance (nS)
    float g_k;                  // Potassium conductance (nS)
    float g_ca;                 // Calcium conductance (nS)
    bool has_active_properties; // Enable NMDA spikes, backprop

    // Inter-segment coupling (cable theory)
    float I_axial_parent;       // Axial current from parent (pA)
    float I_axial_children;     // Axial current from children (pA)
    float g_axial;              // Axial conductance (nS)

    // Rall's 3/2 power rule for branching
    float branch_power_ratio;   // d_parent^3/2 / sum(d_child^3/2)

    // NMDA spike state for this segment
    bool nmda_spike_active;
    float nmda_spike_voltage;
    float nmda_spike_remaining_ms;

    // bAP state for this segment
    bool bap_reached;
    float bap_amplitude;
    uint64_t bap_arrival_time;
};

//=============================================================================
// MAIN DENDRITE STRUCTURE
//=============================================================================

/**
 * @brief Complete dendrite with tree morphology, spines, and active properties
 *
 * WHAT: Full dendritic tree with realistic morphology and computation
 * WHY:  Dendrites are computational units performing:
 *       - Spatiotemporal integration (coincidence detection)
 *       - Nonlinear amplification (NMDA spikes, Ca²⁺ spikes)
 *       - Local plasticity (spine-specific LTP/LTD)
 *       - Pattern separation and feature detection
 * HOW:  Multi-compartment cable model with active properties and plasticity
 *
 * BIOLOGICAL CONTEXT:
 * - Basal dendrites: 100-300 μm, 20-50 per pyramidal neuron, local integration
 * - Apical dendrites: 500-1000 μm, 1-2 per neuron, cross-layer signaling
 * - Total spines: 2000-10000 per cortical pyramidal neuron
 * - Electrical isolation: 5-20 MΩ input resistance
 * - Active properties enable plateau potentials and burst firing
 *
 * MEMORY: ~400 bytes base + num_segments × 80 + num_spines × 120
 */
struct dendrite_struct {
    //--- Identification ---
    uint32_t id;
    dendrite_type_t type;
    dendrite_state_t state;

    //--- Connectivity ---
    uint32_t target_neuron_id;                    // Owning neuron
    uint32_t synapse_ids[NIMCP_MAX_SYNAPSE_IDS];  // Tier-optimized synapse array
    uint32_t num_synapses;

    //--- Morphology ---
    float total_length;            // Total dendritic length (μm)
    float mean_diameter;           // Average diameter (μm)
    float surface_area;            // Total surface area (μm²)
    float start_pos[3];            // Junction with soma (μm)

    // Tree Structure
    uint32_t num_segments;
    dendritic_segment_t* segments; // Segment array

    // Spines
    uint32_t num_spines;
    dendritic_spine_t* spines;     // Spine array

    //--- Electrical Properties ---
    float input_resistance;        // Total input resistance (MΩ)
    float time_constant;           // Membrane time constant (ms)
    float attenuation_factor;      // Voltage attenuation soma→distal (0-1)
    float integration_window_ms;   // Temporal summation window

    //--- State Variables ---
    float somatic_voltage;         // Voltage at soma junction (mV)
    float mean_voltage;            // Average dendritic voltage (mV)
    float calcium_level;           // Average [Ca²⁺] (μM)

    //--- Activity Tracking ---
    dendrite_activity_stats_t activity;

    //--- Plasticity ---
    float structural_plasticity;   // Spine growth/pruning signal (0-1)
    float ltp_threshold;           // LTP induction threshold (μM Ca²⁺)
    float ltd_threshold;           // LTD induction threshold (μM Ca²⁺)

    //--- Metabolic State ---
    float atp_level;               // ATP concentration (0-1)

    //--- Pathology ---
    float damage;                  // Damage level (0-1)
    bool is_functional;            // Is dendrite functional?

    //--- Advanced Physiology ---
    nmda_spike_state_t nmda_state;                         // Current NMDA spike state
    bap_state_t bap_state;                                 // Current bAP state
    spine_cluster_t clusters[NIMCP_HISTORY_SIZE_MEDIUM];   // Tier-optimized clusters
    uint32_t num_clusters;                                 // Active cluster count

    //--- Memory Pool (O(1) spine allocation) ---
    dendrite_spine_pool_t* spine_pool; // Pool for spine allocation
    bool use_spine_pool;           // Enable pool allocation

    //--- Skip-Frame Optimization ---
    uint32_t update_counter;       // Step counter for skip-frame logic
    float cached_mean_dv;          // Cached voltage gradient from last full computation

    //--- Copy-on-Write Support ---
    uint32_t cow_ref_count;        // CoW reference counter
    bool cow_modified;             // Modified since last CoW copy

    //--- Thread Safety ---
    nimcp_mutex_t lock;
};

//=============================================================================
// DENDRITE NETWORK STRUCTURES
//=============================================================================

/**
 * @brief Synaptic input event for dendrite processing
 */
typedef struct {
    uint32_t dendrite_id;
    uint32_t synapse_id;
    uint32_t spine_id;
    uint64_t arrival_time;          // μs
    float current;                  // pA
    uint32_t source_neuron_id;
} dendrite_input_event_t;

/**
 * @brief Network-wide dendrite statistics
 */
typedef struct {
    uint32_t total_dendrites;
    uint32_t basal_count;
    uint32_t apical_count;
    float mean_length;
    float mean_attenuation;
    uint32_t total_spines;
    uint32_t total_segments;
    float mean_input_rate;
    float mean_calcium;
    float mean_voltage;
    uint32_t damaged_count;
    uint32_t total_ltp_events;
    uint32_t total_ltd_events;
    uint32_t total_spine_formations;
    uint32_t total_spine_eliminations;
} dendrite_network_stats_t;

//=============================================================================
// DENDRITE NETWORK STRUCTURE
//=============================================================================

/**
 * @brief Network container for multiple dendrites
 *
 * WHAT: Manages a population of dendrites for network-level operations
 * WHY:  Enables efficient batch processing and population statistics
 * HOW:  Array-based storage with mutex for thread safety
 */
struct dendrite_network_struct {
    dendrite_t** dendrites;     // Array of dendrite pointers
    uint32_t num_dendrites;     // Current number of dendrites
    uint32_t max_dendrites;     // Maximum capacity
    nimcp_mutex_t lock;       // Thread safety lock
};

//=============================================================================
// API: CREATION AND DESTRUCTION
//=============================================================================

/**
 * WHAT: Create dendrite with configuration
 * WHY:  Initialize dendrite for integration into neuron
 * HOW:  Allocate structure, set defaults from config
 *
 * @param config Configuration structure with dendrite parameters
 * @return Allocated dendrite or NULL on failure
 */
dendrite_t* dendrite_create(dendrite_config_t* config);

/**
 * WHAT: Destroy dendrite and free all memory
 * WHY:  Prevent memory leaks
 * HOW:  Free segments, spines, then dendrite structure
 *
 * @param dendrite Dendrite to destroy (can be NULL)
 */
void dendrite_destroy(dendrite_t* dendrite);

//=============================================================================
// API: MORPHOLOGY
//=============================================================================

/**
 * WHAT: Create dendritic segments with configurations
 * WHY:  Build multi-compartment tree structure
 * HOW:  Allocate segment array, compute cable properties
 *
 * @param dendrite Target dendrite
 * @param num_segments Number of segments to create
 * @param segment_configs Array of segment configurations
 * @return true on success
 */
bool dendrite_create_segments(dendrite_t* dendrite, uint32_t num_segments,
                              segment_config_t* segment_configs);

/**
 * WHAT: Add branch to existing segment
 * WHY:  Build realistic dendritic tree morphology
 * HOW:  Create child segment, update parent, recompute properties
 *
 * @param dendrite Target dendrite
 * @param parent_segment_id Parent segment index
 * @param length Branch length (μm)
 * @param diameter Branch diameter (μm)
 * @param branch_angle Angle of branch from parent (radians)
 * @return Segment index or UINT32_MAX on failure
 */
uint32_t dendrite_add_branch(dendrite_t* dendrite, uint32_t parent_segment_id,
                             float length, float diameter, float branch_angle);

/**
 * WHAT: Add dendritic spine to segment
 * WHY:  Create synaptic contact point with morphology
 * HOW:  Allocate spine, compute electrical properties, attach to segment
 *
 * @param dendrite Target dendrite
 * @param segment_id Segment to attach spine
 * @param type Spine morphological type
 * @param synapse_id Synapse ID to connect to this spine
 * @return Spine index or UINT32_MAX on failure
 */
uint32_t dendrite_add_spine(dendrite_t* dendrite, uint32_t segment_id,
                            spine_type_t type, uint32_t synapse_id);

/**
 * WHAT: Remove dendritic spine
 * WHY:  Support structural plasticity and spine elimination
 * HOW:  Remove from segment list, mark as eliminated
 *
 * @param dendrite Target dendrite
 * @param spine_id Spine index to remove
 * @return true on success
 */
bool dendrite_remove_spine(dendrite_t* dendrite, uint32_t spine_id);

//=============================================================================
// API: SIGNAL INTEGRATION
//=============================================================================

/**
 * WHAT: Receive synaptic input at segment
 * WHY:  Process incoming synaptic current at compartment
 * HOW:  Update segment voltage/calcium
 *
 * @param dendrite Target dendrite
 * @param segment_id Segment receiving input
 * @param current Synaptic current (pA)
 * @param timestamp Current simulation time (μs)
 * @return true on success
 */
bool dendrite_receive_input(dendrite_t* dendrite, uint32_t segment_id,
                            float current, uint64_t timestamp);

/**
 * WHAT: Compute integrated current delivered to soma
 * WHY:  Calculate dendrite's contribution to somatic membrane potential
 * HOW:  Integrate all segment voltages with attenuation
 *
 * @param dendrite Target dendrite
 * @return Somatic current (pA)
 */
float dendrite_compute_somatic_current(dendrite_t* dendrite);

/**
 * WHAT: Get voltage attenuation for specific segment
 * WHY:  Understand electrical isolation from soma
 * HOW:  Use path resistance and cable theory
 *
 * @param dendrite Target dendrite
 * @param segment_id Segment index
 * @return Attenuation factor (0-1)
 */
float dendrite_get_attenuation(dendrite_t* dendrite, uint32_t segment_id);

//=============================================================================
// API: TIME EVOLUTION
//=============================================================================

/**
 * WHAT: Update dendrite state for one timestep
 * WHY:  Evolve voltages, calcium, plasticity
 * HOW:  Integrate cable equations, update spines, check thresholds
 *
 * @param dendrite Target dendrite
 * @param dt_ms Time step (ms)
 * @param timestamp Current simulation time (μs)
 */
void dendrite_step(dendrite_t* dendrite, float dt_ms, uint64_t timestamp);

/**
 * WHAT: Update calcium dynamics
 * WHY:  Ca²⁺ drives plasticity and active properties
 * HOW:  Calcium diffusion, buffering, decay
 *
 * @param dendrite Target dendrite
 * @param dt Time step (ms)
 */
void dendrite_update_calcium(dendrite_t* dendrite, float dt);

//=============================================================================
// API: PLASTICITY
//=============================================================================

/**
 * WHAT: Induce LTP at specific spine
 * WHY:  Strengthen synapse based on coincident activity
 * HOW:  Increase AMPA receptors, enlarge spine head
 *
 * @param dendrite Target dendrite
 * @param spine_id Spine index
 * @param magnitude LTP magnitude (0-1)
 */
void dendrite_induce_ltp(dendrite_t* dendrite, uint32_t spine_id, float magnitude);

/**
 * WHAT: Induce LTD at specific spine
 * WHY:  Weaken synapse based on uncorrelated activity
 * HOW:  Decrease AMPA receptors, shrink spine head
 *
 * @param dendrite Target dendrite
 * @param spine_id Spine index
 * @param magnitude LTD magnitude (0-1)
 */
void dendrite_induce_ltd(dendrite_t* dendrite, uint32_t spine_id, float magnitude);

/**
 * WHAT: Update structural plasticity (spine growth/pruning)
 * WHY:  Long-term adaptation to activity patterns
 * HOW:  Activity-dependent spine creation and elimination
 *
 * @param dendrite Target dendrite
 * @param timestamp Current simulation time (μs)
 */
void dendrite_update_structural_plasticity(dendrite_t* dendrite, uint64_t timestamp);

//=============================================================================
// API: QUERIES
//=============================================================================

/**
 * WHAT: Check if dendrite is in plateau potential state
 * WHY:  Plateau potentials enable burst firing and learning
 * HOW:  Check voltage and calcium thresholds
 *
 * @param dendrite Target dendrite
 * @return true if in plateau state
 */
bool dendrite_is_in_plateau(dendrite_t* dendrite);

/**
 * WHAT: Get spine by synapse ID
 * WHY:  Map synapse to spine for plasticity operations
 * HOW:  Linear search through spine array
 *
 * @param dendrite Target dendrite
 * @param synapse_id Synapse ID
 * @return Pointer to spine or NULL if not found
 */
dendritic_spine_t* dendrite_get_spine_by_synapse(dendrite_t* dendrite, uint32_t synapse_id);

/**
 * WHAT: Calculate total dendritic surface area
 * WHY:  Determines capacitance and current requirements
 * HOW:  Sum cylinder surface areas of all segments
 *
 * @param dendrite Target dendrite
 * @return Surface area (μm²)
 */
float dendrite_calculate_surface_area(dendrite_t* dendrite);

/**
 * WHAT: Get dendrite activity statistics
 * WHY:  Monitor dendrite function and plasticity
 * HOW:  Return copy of activity stats structure
 *
 * @param dendrite Target dendrite
 * @return Activity statistics structure
 */
dendrite_activity_stats_t dendrite_get_activity_stats(dendrite_t* dendrite);

//=============================================================================
// API: DENDRITE NETWORK
//=============================================================================

/**
 * WHAT: Create network container for multiple dendrites
 * WHY:  Manage population-level operations and spatial queries
 * HOW:  Allocate container with capacity
 *
 * @param capacity Maximum number of dendrites
 * @return Allocated network or NULL on failure
 */
dendrite_network_t* dendrite_network_create(uint32_t capacity);

/**
 * WHAT: Destroy dendrite network and all dendrites
 * WHY:  Clean up entire population
 * HOW:  Destroy each dendrite, then network structure
 *
 * @param network Network to destroy (can be NULL)
 */
void dendrite_network_destroy(dendrite_network_t* network);

/**
 * WHAT: Add dendrite to network
 * WHY:  Register dendrite for population operations
 * HOW:  Add to array, update spatial index
 *
 * @param network Target network
 * @param dendrite Dendrite to add (takes ownership)
 * @return true on success
 */
bool dendrite_network_add(dendrite_network_t* network, dendrite_t* dendrite);

/**
 * WHAT: Process all inputs for network
 * WHY:  Update all dendrites in single timestep
 * HOW:  Iterate dendrites, call dendrite_step()
 *
 * @param network Target network
 * @param dt_ms Time step (ms)
 * @param timestamp Current simulation time (μs)
 */
void dendrite_network_step(dendrite_network_t* network, float dt_ms, uint64_t timestamp);

/**
 * WHAT: Get network-wide statistics
 * WHY:  Monitor population health and activity
 * HOW:  Aggregate statistics from all dendrites
 *
 * @param network Target network
 * @return Statistics structure
 */
dendrite_network_stats_t dendrite_network_get_stats(dendrite_network_t* network);

//=============================================================================
// API: NMDA SPIKES AND PLATEAU POTENTIALS
//=============================================================================

/**
 * WHAT: Initiate NMDA spike at segment
 * WHY:  Trigger regenerative dendritic spike for nonlinear integration
 * HOW:  Check threshold, initiate plateau if conditions met
 *
 * @param dendrite Target dendrite
 * @param segment_id Segment to initiate spike
 * @param timestamp Current simulation time (μs)
 * @return true if NMDA spike initiated
 */
bool dendrite_initiate_nmda_spike(dendrite_t* dendrite, uint32_t segment_id,
                                   uint64_t timestamp);

/**
 * WHAT: Update NMDA spike state
 * WHY:  Evolve plateau potential over time
 * HOW:  Decay voltage, update calcium, check termination
 *
 * @param dendrite Target dendrite
 * @param dt_ms Time step (ms)
 */
void dendrite_update_nmda_spike(dendrite_t* dendrite, float dt_ms);

/**
 * WHAT: Check if segment can generate NMDA spike
 * WHY:  Test conditions for dendritic spike generation
 * HOW:  Check voltage, glutamate, Mg²⁺ block
 *
 * @param dendrite Target dendrite
 * @param segment_id Segment to test
 * @return true if conditions met
 */
bool dendrite_can_generate_nmda_spike(dendrite_t* dendrite, uint32_t segment_id);

//=============================================================================
// API: BACKPROPAGATING ACTION POTENTIALS
//=============================================================================

/**
 * WHAT: Initiate bAP from soma
 * WHY:  Propagate action potential into dendrites for STDP
 * HOW:  Set wavefront at soma, begin propagation
 *
 * @param dendrite Target dendrite
 * @param amplitude_mv AP amplitude at soma (mV)
 * @param timestamp Current simulation time (μs)
 * @return true if bAP initiated
 */
bool dendrite_initiate_bap(dendrite_t* dendrite, float amplitude_mv,
                            uint64_t timestamp);

/**
 * WHAT: Update bAP propagation
 * WHY:  Advance wavefront through dendrite
 * HOW:  Move wavefront, calculate attenuation, inject calcium
 *
 * @param dendrite Target dendrite
 * @param dt_ms Time step (ms)
 */
void dendrite_update_bap(dendrite_t* dendrite, float dt_ms);

/**
 * WHAT: Check if bAP reached specific spine
 * WHY:  Determine STDP timing at spine location
 * HOW:  Compare wavefront position to spine location
 *
 * @param dendrite Target dendrite
 * @param spine_id Spine to check
 * @return true if bAP has reached this spine
 */
bool dendrite_bap_reached_spine(dendrite_t* dendrite, uint32_t spine_id);

//=============================================================================
// API: INTER-SEGMENT COUPLING (CABLE THEORY)
//=============================================================================

/**
 * WHAT: Calculate axial current between segments
 * WHY:  Implement cable theory for realistic current flow
 * HOW:  Apply V_diff / R_axial between connected segments
 *
 * @param dendrite Target dendrite
 * @param dt_ms Time step (ms)
 */
void dendrite_update_axial_currents(dendrite_t* dendrite, float dt_ms);

/**
 * WHAT: Calculate Rall's 3/2 power ratio for branch point
 * WHY:  Determine impedance matching at branch points
 * HOW:  d_parent^(3/2) = sum(d_child^(3/2))
 *
 * @param dendrite Target dendrite
 * @param segment_id Parent segment at branch point
 * @return Ratio of parent to children (1.0 = impedance matched)
 */
float dendrite_calculate_rall_ratio(dendrite_t* dendrite, uint32_t segment_id);

//=============================================================================
// API: SPATIAL CLUSTERING DETECTION
//=============================================================================

/**
 * WHAT: Detect clusters of recently active spines
 * WHY:  Identify conditions for nonlinear summation
 * HOW:  Group nearby active spines, compute cluster properties
 *
 * @param dendrite Target dendrite
 * @param timestamp Current simulation time (μs)
 */
void dendrite_detect_spine_clusters(dendrite_t* dendrite, uint64_t timestamp);

/**
 * WHAT: Apply nonlinear boost to clustered inputs
 * WHY:  Model supralinear summation of clustered synaptic inputs
 * HOW:  Multiply cluster output by nonlinear factor
 *
 * @param dendrite Target dendrite
 * @param cluster_id Cluster index
 * @return Boosted output (pA)
 */
float dendrite_apply_cluster_boost(dendrite_t* dendrite, uint32_t cluster_id);

//=============================================================================
// API: STDP INTEGRATION
//=============================================================================

/**
 * WHAT: Register presynaptic spike for STDP
 * WHY:  Update pre-spike eligibility trace
 * HOW:  Increment trace, record timing, check for LTD
 *
 * @param dendrite Target dendrite
 * @param spine_id Spine receiving presynaptic input
 * @param timestamp Spike arrival time (μs)
 */
void dendrite_stdp_pre_spike(dendrite_t* dendrite, uint32_t spine_id,
                              uint64_t timestamp);

/**
 * WHAT: Register postsynaptic spike for STDP
 * WHY:  Update post-spike eligibility trace (bAP)
 * HOW:  Increment trace, record timing, check for LTP
 *
 * @param dendrite Target dendrite
 * @param timestamp Spike time (μs)
 */
void dendrite_stdp_post_spike(dendrite_t* dendrite, uint64_t timestamp);

/**
 * WHAT: Apply accumulated STDP weight changes
 * WHY:  Update synaptic weights based on timing-dependent plasticity
 * HOW:  Integrate eligibility traces, apply bounded weight change
 *
 * @param dendrite Target dendrite
 */
void dendrite_stdp_apply_weight_changes(dendrite_t* dendrite);

//=============================================================================
// API: SPINE MEMORY POOL
//=============================================================================

/**
 * WHAT: Create spine memory pool for dendrite
 * WHY:  Enable O(1) spine allocation/deallocation
 * HOW:  Pre-allocate spine array with bitmap tracking
 *
 * @param dendrite Target dendrite
 * @param capacity Pool capacity (number of spines)
 * @return true on success
 */
bool dendrite_create_spine_pool(dendrite_t* dendrite, uint32_t capacity);

/**
 * WHAT: Allocate spine from pool
 * WHY:  O(1) spine creation without malloc
 * HOW:  Find free slot in bitmap, return pointer
 *
 * @param dendrite Target dendrite
 * @return Pointer to allocated spine or NULL if pool full
 */
dendritic_spine_t* dendrite_pool_alloc_spine(dendrite_t* dendrite);

/**
 * WHAT: Free spine back to pool
 * WHY:  O(1) spine destruction without free
 * HOW:  Clear bitmap bit, reset spine data
 *
 * @param dendrite Target dendrite
 * @param spine Spine to free
 */
void dendrite_pool_free_spine(dendrite_t* dendrite, dendritic_spine_t* spine);

//=============================================================================
// API: COPY-ON-WRITE SUPPORT
//=============================================================================

/**
 * WHAT: Create shallow copy of dendrite (CoW)
 * WHY:  Efficient read-only sharing without full copy
 * HOW:  Increment reference count, share data until write
 *
 * @param dendrite Source dendrite
 * @return New dendrite handle sharing data with source
 */
dendrite_t* dendrite_cow_copy(dendrite_t* dendrite);

/**
 * WHAT: Prepare dendrite for write (CoW)
 * WHY:  Ensure exclusive ownership before modification
 * HOW:  If shared, make full copy; otherwise no-op
 *
 * @param dendrite Dendrite to prepare for writing
 * @return true if now exclusively owned
 */
bool dendrite_cow_prepare_write(dendrite_t* dendrite);

/**
 * WHAT: Release CoW reference
 * WHY:  Decrement reference count, free if zero
 * HOW:  Atomic decrement, destroy if last reference
 *
 * @param dendrite Dendrite to release
 */
void dendrite_cow_release(dendrite_t* dendrite);

//=============================================================================
// API: SPINE-LEVEL INPUT PROCESSING
//=============================================================================

/**
 * WHAT: Process input at specific spine
 * WHY:  Apply spine-level filtering and dynamics
 * HOW:  RC filtering, calcium dynamics, receptor activation
 *
 * @param dendrite Target dendrite
 * @param spine_id Spine receiving input
 * @param current Input current (pA)
 * @param timestamp Current time (μs)
 * @return Filtered current delivered to dendrite (pA)
 */
float dendrite_spine_process_input(dendrite_t* dendrite, uint32_t spine_id,
                                    float current, uint64_t timestamp);

/**
 * WHAT: Update all spine conductances
 * WHY:  Evolve AMPA, NMDA, GABA dynamics
 * HOW:  Kinetic models for each receptor type
 *
 * @param dendrite Target dendrite
 * @param dt_ms Time step (ms)
 */
void dendrite_update_spine_conductances(dendrite_t* dendrite, float dt_ms);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_DENDRITE_H
