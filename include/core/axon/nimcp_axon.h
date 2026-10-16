/**
 * @file nimcp_axon.h
 * @brief NIMCP Axon Module - Action Potential Propagation and Morphology
 *
 * WHAT: Models axonal signal propagation from neuron soma to synaptic terminals
 * WHY:  Axons are critical for:
 *       - Signal timing (conduction delays affect synchronization)
 *       - Myelination-dependent speed (50x faster with myelin)
 *       - Activity-dependent plasticity (firing patterns shape networks)
 *       - Metabolic efficiency (saltatory conduction)
 * HOW:  Integrates with oligodendrocytes for myelination, neurons for spike
 *       generation, synapses for signal delivery, and vesicles for release
 *
 * BIOLOGICAL BACKGROUND:
 * - Unmyelinated axons: ~1 m/s conduction velocity
 * - Myelinated axons: ~50-100 m/s (saltatory conduction)
 * - Action potential travels via voltage-gated Na+/K+ channels
 * - Nodes of Ranvier between myelin segments allow AP regeneration
 * - Axon diameter affects conduction speed (larger = faster)
 *
 * ARCHITECTURE:
 * - Strategy Pattern: Different axon types (passive, active, myelinated)
 * - Factory Pattern: axon_create() with type-specific initialization
 * - Observer Pattern: Activity callbacks for plasticity systems
 *
 * PERFORMANCE:
 * - O(1) spike propagation with precomputed delays
 * - O(log N) spatial queries via KD-tree integration
 * - Memory: ~200 bytes per axon (base), ~40 bytes per segment
 *
 * NIMCP STANDARDS:
 * - Functions < 50 lines
 * - Guard clauses for NULL checks
 * - WHAT/WHY/HOW documentation
 * - nimcp_malloc/nimcp_free for memory
 *
 * @author NIMCP Development Team
 * @version 1.0.0
 * @date November 2024
 */

#ifndef NIMCP_AXON_H
#define NIMCP_AXON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/thread/nimcp_thread.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

typedef struct axon_struct axon_t;
typedef struct axon_segment_struct axon_segment_t;
typedef struct axon_network_struct axon_network_t;
typedef struct axon_spike_queue_struct axon_spike_queue_t;
typedef struct axon_spike_pool_struct axon_spike_pool_t;
typedef struct axon_segment_pool_struct axon_segment_pool_t;

//=============================================================================
// CONSTANTS AND CONFIGURATION
//=============================================================================

/** Maximum segments per axon (nodes of Ranvier + internodes) */
#define NIMCP_AXON_MAX_SEGMENTS 64

/** Maximum axons per network */
#define NIMCP_AXON_NETWORK_MAX_AXONS 100000

/** Maximum pending spikes in queue */
#define NIMCP_AXON_SPIKE_QUEUE_SIZE 10000

/** Memory pool size for spike events */
#define NIMCP_AXON_SPIKE_POOL_SIZE 8192

/** Memory pool size for axon segments */
#define NIMCP_AXON_SEGMENT_POOL_SIZE 4096

/** Default unmyelinated conduction velocity (m/s) */
#define NIMCP_AXON_BASE_VELOCITY_MS 1.0f

/** Maximum myelinated conduction velocity (m/s) */
#define NIMCP_AXON_MAX_VELOCITY_MS 120.0f

/** Myelination velocity multiplier at full myelination */
#define NIMCP_AXON_MYELIN_MULTIPLIER 50.0f

/** Default axon diameter (micrometers) */
#define NIMCP_AXON_DEFAULT_DIAMETER_UM 1.0f

/** Minimum axon diameter (micrometers) */
#define NIMCP_AXON_MIN_DIAMETER_UM 0.2f

/** Maximum axon diameter (micrometers) */
#define NIMCP_AXON_MAX_DIAMETER_UM 20.0f

/** Refractory period after spike (milliseconds) */
#define NIMCP_AXON_REFRACTORY_PERIOD_MS 1.0f

/** Activity decay time constant (milliseconds) */
#define NIMCP_AXON_ACTIVITY_TAU_MS 100.0f

/** Spike amplitude (mV) */
#define NIMCP_AXON_SPIKE_AMPLITUDE_MV 100.0f

//=============================================================================
// AXON TYPE ENUMERATION
//=============================================================================

/**
 * @brief Axon type classification
 *
 * WHAT: Different axon types with distinct properties
 * WHY:  Biological diversity (sensory, motor, interneuron axons)
 * HOW:  Strategy pattern selects appropriate behavior
 */
typedef enum {
    /** Unmyelinated axon - slow conduction (~1 m/s) */
    AXON_TYPE_UNMYELINATED = 0,

    /** Myelinated axon - fast saltatory conduction */
    AXON_TYPE_MYELINATED = 1,

    /** A-alpha fibers - largest, fastest (proprioception) */
    AXON_TYPE_A_ALPHA = 2,

    /** A-beta fibers - touch, pressure */
    AXON_TYPE_A_BETA = 3,

    /** A-delta fibers - fast pain, temperature */
    AXON_TYPE_A_DELTA = 4,

    /** C fibers - unmyelinated, slow pain */
    AXON_TYPE_C_FIBER = 5,

    /** Number of axon types */
    AXON_TYPE_COUNT
} axon_type_t;

/**
 * @brief Axon state enumeration
 *
 * WHAT: Current functional state of the axon
 * WHY:  Track axon health and activity status
 * HOW:  State machine with transitions based on activity/damage
 */
typedef enum {
    /** Normal resting state */
    AXON_STATE_RESTING = 0,

    /** Currently propagating action potential */
    AXON_STATE_ACTIVE = 1,

    /** Refractory period after spike */
    AXON_STATE_REFRACTORY = 2,

    /** Demyelinating (pathological) */
    AXON_STATE_DEMYELINATING = 3,

    /** Damaged/non-functional */
    AXON_STATE_DAMAGED = 4
} axon_state_t;

/**
 * @brief Segment type for axon morphology
 *
 * WHAT: Type of axon segment
 * WHY:  Distinguish myelinated internodes from nodes of Ranvier
 * HOW:  Affects local conduction properties
 */
typedef enum {
    /** Axon initial segment (near soma) */
    SEGMENT_TYPE_AIS = 0,

    /** Node of Ranvier (between myelin segments) */
    SEGMENT_TYPE_NODE = 1,

    /** Myelinated internode */
    SEGMENT_TYPE_INTERNODE = 2,

    /** Synaptic terminal (bouton) */
    SEGMENT_TYPE_TERMINAL = 3
} axon_segment_type_t;

//=============================================================================
// AXON SEGMENT STRUCTURE
//=============================================================================

/**
 * @brief Single axon segment
 *
 * WHAT: One segment of axon (node, internode, or terminal)
 * WHY:  Model spatial properties for realistic conduction
 * HOW:  Segments chain together to form complete axon
 *
 * BIOLOGICAL: Each internode is wrapped by one oligodendrocyte process
 */
struct axon_segment_struct {
    /** Segment type */
    axon_segment_type_t type;

    /** 3D position (micrometers from soma) */
    float position[3];

    /** Segment length (micrometers) */
    float length;

    /** Local diameter (micrometers) */
    float diameter;

    /** Myelination level (0-1, for internodes) */
    float myelination;

    /** G-ratio (inner/outer diameter) */
    float g_ratio;

    /** Oligodendrocyte ID providing myelin (0 = unmyelinated) */
    uint32_t oligo_id;

    /** Local conduction velocity (m/s) */
    float local_velocity;

    /** Cumulative delay to this segment (ms) */
    float cumulative_delay;

    //--- Enhanced Biophysics (from nimcp_myelin_math.h) ---
    /** Cable theory parameters for this segment */
    nimcp_cable_params_t cable_params;

    /** Saltatory conduction result (detailed velocity breakdown) */
    nimcp_saltatory_result_t saltatory;

    /** Optimal G-ratio for this diameter */
    float optimal_g_ratio;

    /** Conduction block probability (0-1) */
    float block_probability;

    /** Is segment conducting? */
    bool is_conducting;
};

//=============================================================================
// SPIKE EVENT STRUCTURE
//=============================================================================

/**
 * @brief Spike event for propagation queue
 *
 * WHAT: Represents a spike traveling along an axon
 * WHY:  Queue-based processing for temporal accuracy
 * HOW:  Sorted by arrival_time for efficient processing
 */
typedef struct {
    /** Axon carrying the spike */
    uint32_t axon_id;

    /** Time spike was initiated (microseconds) */
    uint64_t initiation_time;

    /** Time spike arrives at terminal (microseconds) */
    uint64_t arrival_time;

    /** Spike amplitude (may attenuate) */
    float amplitude;

    /** Source neuron ID */
    uint32_t source_neuron_id;

    /** Target synapse ID */
    uint32_t target_synapse_id;
} axon_spike_event_t;

//=============================================================================
// AXON ACTIVITY STATISTICS
//=============================================================================

/**
 * @brief Activity tracking for plasticity
 *
 * WHAT: Tracks axon activity for activity-dependent plasticity
 * WHY:  Myelination and synaptic strength depend on activity
 * HOW:  Exponential moving average of spike rate
 */
typedef struct {
    /** Total spikes transmitted (lifetime) */
    uint64_t total_spikes;

    /** Spikes in current window */
    uint32_t recent_spikes;

    /** Current firing rate (Hz) */
    float firing_rate;

    /** Exponential moving average of activity */
    float activity_ema;

    /** Time of last spike (microseconds) */
    uint64_t last_spike_time;

    /** Inter-spike interval average (ms) */
    float mean_isi;

    /** Activity level for myelination (0-1) */
    float myelination_signal;
} axon_activity_stats_t;

//=============================================================================
// MEMORY POOL STRUCTURES
//=============================================================================

/**
 * @brief Memory pool for spike events
 *
 * WHAT: Pre-allocated pool of spike event slots
 * WHY:  Avoid malloc/free in hot path during spike propagation
 * HOW:  Bitmap allocation with O(1) acquire/release
 *
 * PERFORMANCE: Eliminates ~1-2μs malloc overhead per spike
 */
struct axon_spike_pool_struct {
    /** Pre-allocated spike events */
    axon_spike_event_t* events;

    /** Allocation bitmap (1 bit per slot) */
    uint64_t* allocation_bitmap;

    /** Total capacity */
    uint32_t capacity;

    /** Number of allocated slots */
    uint32_t allocated_count;

    /** Number of 64-bit words in bitmap */
    uint32_t bitmap_words;

    /** Next search position for allocation */
    uint32_t next_search_pos;

    /** High water mark (max simultaneous allocations) */
    uint32_t high_water_mark;
};

/**
 * @brief Memory pool for axon segments
 *
 * WHAT: Pre-allocated pool of axon segment slots
 * WHY:  Efficient segment allocation for axon morphology
 * HOW:  Bitmap allocation with O(1) acquire/release
 */
struct axon_segment_pool_struct {
    /** Pre-allocated segments */
    axon_segment_t* segments;

    /** Allocation bitmap (1 bit per slot) */
    uint64_t* allocation_bitmap;

    /** Total capacity */
    uint32_t capacity;

    /** Number of allocated slots */
    uint32_t allocated_count;

    /** Number of 64-bit words in bitmap */
    uint32_t bitmap_words;

    /** Next search position for allocation */
    uint32_t next_search_pos;
};

//=============================================================================
// MAIN AXON STRUCTURE
//=============================================================================

/**
 * @brief Complete axon structure
 *
 * WHAT: Full axon with morphology, conduction, and activity tracking
 * WHY:  Central data structure for axon module
 * HOW:  Integrates segments, activity, and connectivity
 *
 * MEMORY: ~200 bytes base + segments
 */
struct axon_struct {
    //--- Identification ---
    /** Unique axon identifier */
    uint32_t id;

    /** Axon type (myelinated, unmyelinated, etc.) */
    axon_type_t type;

    /** Current state (resting, active, refractory) */
    axon_state_t state;

    //--- Connectivity ---
    /** Source neuron ID (pre-synaptic) */
    uint32_t source_neuron_id;

    /** Target synapse ID (post-synaptic terminal) */
    uint32_t target_synapse_id;

    /** Target neuron ID (for convenience) */
    uint32_t target_neuron_id;

    //--- Morphology ---
    /** Total axon length (micrometers) */
    float length;

    /** Average diameter (micrometers) */
    float diameter;

    /** Start position (soma) */
    float start_pos[3];

    /** End position (terminal) */
    float end_pos[3];

    /** Number of segments */
    uint32_t num_segments;

    /** Segment array */
    axon_segment_t* segments;

    //--- Conduction Properties ---
    /** Base conduction velocity (m/s, unmyelinated) */
    float base_velocity;

    /** Current effective velocity (m/s, with myelination) */
    float effective_velocity;

    /** Total propagation delay (milliseconds) */
    float propagation_delay_ms;

    /** Overall myelination level (0-1) */
    float myelination_level;

    /** G-ratio (average across segments) */
    float mean_g_ratio;

    //--- Activity Tracking ---
    /** Activity statistics */
    axon_activity_stats_t activity;

    //--- Refractory Period ---
    /** End of refractory period (microseconds) */
    uint64_t refractory_end;

    //--- Metabolic State ---
    /** ATP level (0-1, affects spike reliability) */
    float atp_level;

    /** Lactate received from oligodendrocytes */
    float lactate_level;

    //--- Pathology ---
    /** Damage level (0-1, 0=healthy) */
    float damage;

    /** Is axon functional? */
    bool is_functional;

    //--- Enhanced Biophysics (from nimcp_myelin_math.h) ---
    /** Comprehensive biophysics state */
    nimcp_myelin_biophysics_t* biophysics;

    /** Metabolic efficiency metrics */
    nimcp_metabolic_efficiency_t metabolic_efficiency;

    /** Current temperature for block modeling (°C) */
    float temperature_c;

    /** Mean space constant λ across segments (μm) */
    float mean_lambda_um;

    /** Overall block probability (0-1) */
    float overall_block_probability;

    //--- Thread Safety ---
    /** Spinlock for concurrent access */
    nimcp_mutex_t lock;

    //--- Memory Pool ---
    /** Segment memory pool (optional) */
    axon_segment_pool_t* segment_pool;

    /** Use pool for segment allocation */
    bool use_segment_pool;

    //--- Copy-on-Write Support ---
    /** Reference count for CoW sharing */
    uint32_t cow_ref_count;

    /** Has this instance been modified since creation? */
    bool cow_modified;

    /** Original axon if this is a CoW copy */
    axon_t* cow_original;
};

//=============================================================================
// AXON NETWORK STRUCTURE
//=============================================================================

/**
 * @brief Network of axons with spatial indexing
 *
 * WHAT: Collection of axons with efficient lookup
 * WHY:  Brain contains millions of axons, need efficient queries
 * HOW:  Hash table by ID, spatial index for proximity queries
 */
struct axon_network_struct {
    /** Array of axon pointers */
    axon_t** axons;

    /** Maximum capacity */
    uint32_t capacity;

    /** Current count */
    uint32_t count;

    /** Spike event queue */
    axon_spike_queue_t* spike_queue;

    /** Current simulation time (microseconds) */
    uint64_t current_time;

    /** Total spikes processed */
    uint64_t total_spikes_processed;

    /** Thread lock */
    nimcp_mutex_t lock;

    /** KD-tree for spatial queries (optional) */
    void* spatial_index;

    /** Is spatial index valid? */
    bool spatial_index_valid;

    //--- Memory Pool ---
    /** Spike event memory pool */
    axon_spike_pool_t* spike_pool;

    /** Use pool for spike events */
    bool use_spike_pool;
};

//=============================================================================
// AXON NETWORK STATISTICS
//=============================================================================

/**
 * @brief Network-level statistics
 *
 * WHAT: Aggregate statistics across all axons
 * WHY:  Monitor network health and activity
 * HOW:  Computed on demand from individual axon stats
 */
typedef struct {
    /** Total axons in network */
    uint32_t total_axons;

    /** Myelinated axon count */
    uint32_t myelinated_count;

    /** Unmyelinated axon count */
    uint32_t unmyelinated_count;

    /** Average myelination level */
    float mean_myelination;

    /** Average conduction velocity (m/s) */
    float mean_velocity;

    /** Average propagation delay (ms) */
    float mean_delay;

    /** Total spikes in transit */
    uint32_t spikes_in_transit;

    /** Average firing rate (Hz) */
    float mean_firing_rate;

    /** Damaged axon count */
    uint32_t damaged_count;
} axon_network_stats_t;

//=============================================================================
// CALLBACK TYPES
//=============================================================================

/**
 * @brief Callback when spike arrives at terminal
 *
 * @param axon Axon that delivered spike
 * @param spike Spike event
 * @param user_data User context
 */
typedef void (*axon_spike_callback_t)(axon_t* axon,
                                       const axon_spike_event_t* spike,
                                       void* user_data);

/**
 * @brief Callback for activity-dependent events
 *
 * @param axon Axon with activity change
 * @param old_rate Previous firing rate
 * @param new_rate New firing rate
 * @param user_data User context
 */
typedef void (*axon_activity_callback_t)(axon_t* axon,
                                          float old_rate,
                                          float new_rate,
                                          void* user_data);

//=============================================================================
// AXON CREATION AND DESTRUCTION
//=============================================================================

/**
 * @brief Create a new axon
 *
 * WHAT: Allocates and initializes an axon structure
 * WHY:  Factory function for axon creation
 * HOW:  Allocates memory, sets defaults, optionally creates segments
 *
 * COMPLEXITY: O(num_segments) for segment allocation
 *
 * @param id Unique axon identifier
 * @param type Axon type (myelinated, unmyelinated, etc.)
 * @param source_neuron_id Pre-synaptic neuron
 * @param target_synapse_id Post-synaptic synapse
 * @param length Total length in micrometers
 * @param diameter Average diameter in micrometers
 *
 * @return New axon or NULL on failure
 */
axon_t* axon_create(uint32_t id,
                    axon_type_t type,
                    uint32_t source_neuron_id,
                    uint32_t target_synapse_id,
                    float length,
                    float diameter);

/**
 * @brief Create axon with 3D endpoints
 *
 * WHAT: Create axon with explicit spatial positions
 * WHY:  For anatomically-realistic networks
 * HOW:  Computes length from positions, stores endpoints
 *
 * @param id Unique identifier
 * @param type Axon type
 * @param source_neuron_id Pre-synaptic neuron
 * @param target_synapse_id Post-synaptic synapse
 * @param start_pos Start position (x, y, z in micrometers)
 * @param end_pos End position (x, y, z in micrometers)
 * @param diameter Axon diameter
 *
 * @return New axon or NULL on failure
 */
axon_t* axon_create_with_positions(uint32_t id,
                                    axon_type_t type,
                                    uint32_t source_neuron_id,
                                    uint32_t target_synapse_id,
                                    const float start_pos[3],
                                    const float end_pos[3],
                                    float diameter);

/**
 * @brief Destroy an axon
 *
 * WHAT: Frees axon memory
 * WHY:  Clean resource management
 * HOW:  Frees segments, then axon structure
 *
 * @param axon Axon to destroy (NULL-safe)
 */
void axon_destroy(axon_t* axon);

//=============================================================================
// AXON SEGMENTATION
//=============================================================================

/**
 * @brief Add segments to axon
 *
 * WHAT: Divide axon into segments (nodes and internodes)
 * WHY:  Model myelination pattern and saltatory conduction
 * HOW:  Creates alternating node/internode pattern
 *
 * @param axon Axon to segment
 * @param num_segments Number of segments to create
 * @param internode_length Length of each internode (micrometers)
 *
 * @return true on success, false on failure
 */
bool axon_create_segments(axon_t* axon,
                          uint32_t num_segments,
                          float internode_length);

/**
 * @brief Set myelination for a segment
 *
 * WHAT: Set myelination level for specific segment
 * WHY:  Activity-dependent myelination varies by segment
 * HOW:  Updates segment and recalculates delays
 *
 * @param axon Axon containing segment
 * @param segment_index Index of segment
 * @param myelination Myelination level (0-1)
 * @param oligo_id Oligodendrocyte providing myelin
 *
 * @return true on success
 */
bool axon_set_segment_myelination(axon_t* axon,
                                   uint32_t segment_index,
                                   float myelination,
                                   uint32_t oligo_id);

//=============================================================================
// SPIKE PROPAGATION
//=============================================================================

/**
 * @brief Initiate spike at axon hillock
 *
 * WHAT: Start action potential propagation
 * WHY:  Core function - neuron fires, spike travels along axon
 * HOW:  Creates spike event, queues for delivery at arrival_time
 *
 * COMPLEXITY: O(1)
 *
 * @param axon Axon to propagate spike
 * @param current_time Current simulation time (microseconds)
 * @param amplitude Spike amplitude (default 1.0)
 *
 * @return true if spike initiated, false if refractory or damaged
 */
bool axon_initiate_spike(axon_t* axon,
                         uint64_t current_time,
                         float amplitude);

/**
 * @brief Check if spike has arrived at terminal
 *
 * WHAT: Test if propagating spike reached destination
 * WHY:  Trigger synaptic transmission when spike arrives
 * HOW:  Compare current_time with arrival_time
 *
 * @param axon Axon to check
 * @param current_time Current simulation time
 *
 * @return true if spike arrived this timestep
 */
bool axon_spike_arrived(axon_t* axon, uint64_t current_time);

/**
 * @brief Get propagation delay
 *
 * WHAT: Time for spike to travel from soma to terminal
 * WHY:  Used in network timing calculations
 * HOW:  delay = length / velocity (accounts for myelination)
 *
 * @param axon Axon to query
 *
 * @return Delay in milliseconds
 */
float axon_get_propagation_delay(const axon_t* axon);

//=============================================================================
// MYELINATION INTERFACE
//=============================================================================

/**
 * @brief Update myelination state
 *
 * WHAT: Set overall myelination level
 * WHY:  Oligodendrocytes call this when myelination changes
 * HOW:  Updates velocity and delay calculations
 *
 * @param axon Axon to update
 * @param myelination_level New myelination (0-1)
 */
void axon_set_myelination(axon_t* axon, float myelination_level);

/**
 * @brief Get myelination signal for oligodendrocytes
 *
 * WHAT: Activity-based signal for myelination decisions
 * WHY:  Activity-dependent myelination uses this signal
 * HOW:  Based on firing rate and recent activity
 *
 * @param axon Axon to query
 *
 * @return Myelination signal (0-1)
 */
float axon_get_myelination_signal(const axon_t* axon);

/**
 * @brief Receive lactate from oligodendrocyte
 *
 * WHAT: Metabolic support via lactate shuttle
 * WHY:  Oligodendrocytes provide energy to axons
 * HOW:  Increases ATP, supports spike reliability
 *
 * @param axon Axon receiving lactate
 * @param lactate_amount Amount of lactate (arbitrary units)
 */
void axon_receive_lactate(axon_t* axon, float lactate_amount);

//=============================================================================
// ACTIVITY TRACKING
//=============================================================================

/**
 * @brief Update activity statistics
 *
 * WHAT: Update firing rate and activity metrics
 * WHY:  Track activity for plasticity and monitoring
 * HOW:  Exponential moving average of spike rate
 *
 * @param axon Axon to update
 * @param current_time Current simulation time
 */
void axon_update_activity(axon_t* axon, uint64_t current_time);

/**
 * @brief Get current firing rate
 *
 * @param axon Axon to query
 * @return Firing rate in Hz
 */
float axon_get_firing_rate(const axon_t* axon);

/**
 * @brief Get activity statistics
 *
 * @param axon Axon to query
 * @param stats Output statistics structure
 */
void axon_get_activity_stats(const axon_t* axon, axon_activity_stats_t* stats);

//=============================================================================
// CONDUCTION VELOCITY
//=============================================================================

/**
 * @brief Calculate conduction velocity
 *
 * WHAT: Compute current conduction velocity
 * WHY:  Velocity depends on diameter and myelination
 * HOW:  Hursh's law for myelinated, diameter-proportional for unmyelinated
 *
 * BIOLOGICAL:
 * - Unmyelinated: v = k * sqrt(diameter) [~1 m/s for 1um]
 * - Myelinated: v = k * diameter [~6 m/s per um diameter]
 *
 * @param axon Axon to calculate
 *
 * @return Velocity in m/s
 */
float axon_calculate_velocity(const axon_t* axon);

/**
 * @brief Update conduction properties
 *
 * WHAT: Recalculate velocity and delay
 * WHY:  Call after myelination or diameter changes
 * HOW:  Updates effective_velocity and propagation_delay_ms
 *
 * @param axon Axon to update
 */
void axon_update_conduction(axon_t* axon);

//=============================================================================
// STATE MANAGEMENT
//=============================================================================

/**
 * @brief Step axon simulation
 *
 * WHAT: Advance axon state by one timestep
 * WHY:  Core simulation loop
 * HOW:  Update refractory state, activity, metabolic state
 *
 * @param axon Axon to step
 * @param current_time Current simulation time (microseconds)
 * @param dt Time step (milliseconds)
 */
void axon_step(axon_t* axon, uint64_t current_time, float dt);

/**
 * @brief Check if axon is in refractory period
 *
 * @param axon Axon to check
 * @param current_time Current time
 *
 * @return true if refractory
 */
bool axon_is_refractory(const axon_t* axon, uint64_t current_time);

/**
 * @brief Get axon state as string
 *
 * @param state State to convert
 * @return String representation
 */
const char* axon_state_to_string(axon_state_t state);

/**
 * @brief Get axon type as string
 *
 * @param type Type to convert
 * @return String representation
 */
const char* axon_type_to_string(axon_type_t type);

//=============================================================================
// AXON NETWORK API
//=============================================================================

/**
 * @brief Create axon network
 *
 * WHAT: Create network container for axons
 * WHY:  Manage large numbers of axons efficiently
 * HOW:  Hash table for ID lookup, queue for spike events
 *
 * @param capacity Maximum number of axons
 *
 * @return New network or NULL on failure
 */
axon_network_t* axon_network_create(uint32_t capacity);

/**
 * @brief Destroy axon network
 *
 * WHAT: Free network and all contained axons
 * WHY:  Clean resource management
 * HOW:  Destroys all axons, frees network structure
 *
 * @param network Network to destroy
 */
void axon_network_destroy(axon_network_t* network);

/**
 * @brief Add axon to network
 *
 * WHAT: Register axon with network
 * WHY:  Enable network-level operations
 * HOW:  Adds to array and spatial index
 *
 * @param network Network
 * @param axon Axon to add
 *
 * @return true on success
 */
bool axon_network_add(axon_network_t* network, axon_t* axon);

/**
 * @brief Remove axon from network
 *
 * WHAT: Unregister axon
 * WHY:  Support dynamic network changes
 * HOW:  Removes from array and spatial index
 *
 * @param network Network
 * @param axon_id Axon ID to remove
 *
 * @return Removed axon (caller owns) or NULL
 */
axon_t* axon_network_remove(axon_network_t* network, uint32_t axon_id);

/**
 * @brief Find axon by ID
 *
 * @param network Network to search
 * @param axon_id ID to find
 *
 * @return Axon or NULL if not found
 */
axon_t* axon_network_find(axon_network_t* network, uint32_t axon_id);

/**
 * @brief Find axons by source neuron
 *
 * @param network Network
 * @param neuron_id Source neuron ID
 * @param results Output buffer
 * @param max_results Buffer size
 *
 * @return Number of axons found
 */
uint32_t axon_network_find_by_source(axon_network_t* network,
                                      uint32_t neuron_id,
                                      axon_t** results,
                                      uint32_t max_results);

/**
 * @brief Step network simulation
 *
 * WHAT: Advance all axons by one timestep
 * WHY:  Main simulation entry point
 * HOW:  Process spike queue, step each axon
 *
 * @param network Network to step
 * @param current_time Current time (microseconds)
 * @param dt Time step (milliseconds)
 */
void axon_network_step(axon_network_t* network,
                       uint64_t current_time,
                       float dt);

/**
 * @brief Process pending spike arrivals
 *
 * WHAT: Deliver spikes that have arrived at terminals
 * WHY:  Trigger synaptic transmission
 * HOW:  Check queue for spikes with arrival_time <= current_time
 *
 * @param network Network
 * @param current_time Current time
 * @param callback Function to call for each arrived spike
 * @param user_data User context for callback
 *
 * @return Number of spikes delivered
 */
uint32_t axon_network_process_arrivals(axon_network_t* network,
                                        uint64_t current_time,
                                        axon_spike_callback_t callback,
                                        void* user_data);

/**
 * @brief Get network statistics
 *
 * @param network Network
 * @param stats Output statistics
 */
void axon_network_get_stats(const axon_network_t* network,
                            axon_network_stats_t* stats);

//=============================================================================
// SPIKE QUEUE API
//=============================================================================

/**
 * @brief Create spike queue
 *
 * @param capacity Maximum pending spikes
 * @return New queue or NULL
 */
axon_spike_queue_t* axon_spike_queue_create(uint32_t capacity);

/**
 * @brief Destroy spike queue
 *
 * @param queue Queue to destroy
 */
void axon_spike_queue_destroy(axon_spike_queue_t* queue);

/**
 * @brief Add spike to queue
 *
 * @param queue Queue
 * @param event Spike event to add
 * @return true on success
 */
bool axon_spike_queue_push(axon_spike_queue_t* queue,
                           const axon_spike_event_t* event);

/**
 * @brief Get next spike arriving before time
 *
 * @param queue Queue
 * @param current_time Time threshold
 * @param event Output event
 * @return true if event available
 */
bool axon_spike_queue_pop(axon_spike_queue_t* queue,
                          uint64_t current_time,
                          axon_spike_event_t* event);

/**
 * @brief Get number of pending spikes
 *
 * @param queue Queue
 * @return Pending count
 */
uint32_t axon_spike_queue_size(const axon_spike_queue_t* queue);

/**
 * @brief Spike queue statistics structure
 *
 * WHAT: Performance metrics for spike queue
 * WHY:  Monitor queue health and identify bottlenecks
 * HOW:  Tracked during push/pop operations
 */
typedef struct {
    uint64_t total_pushed;          /**< Total spikes enqueued */
    uint64_t total_popped;          /**< Total spikes dequeued */
    uint64_t total_dropped_full;    /**< Dropped due to full queue */
    uint64_t total_dropped_invalid; /**< Dropped due to invalid data */
    uint64_t current_size;          /**< Current queue size */
    uint64_t peak_size;             /**< Maximum queue size reached */
    double mean_latency_us;         /**< Mean spike propagation latency (us) */
    uint64_t latency_samples;       /**< Number of latency samples */
} axon_spike_queue_stats_t;

/**
 * @brief Get spike queue statistics
 *
 * WHAT: Retrieve performance metrics from spike queue
 * WHY:  Monitor queue health and optimize capacity
 * HOW:  Thread-safe copy of statistics structure
 *
 * @param queue Queue to query
 * @param stats Output statistics structure
 */
void axon_spike_queue_get_stats(const axon_spike_queue_t* queue,
                                 axon_spike_queue_stats_t* stats);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Calculate distance between two 3D points
 *
 * @param a First point
 * @param b Second point
 * @return Euclidean distance
 */
float axon_distance_3d(const float a[3], const float b[3]);

/**
 * @brief Validate axon parameters
 *
 * @param length Axon length
 * @param diameter Axon diameter
 * @return true if valid
 */
bool axon_validate_params(float length, float diameter);

//=============================================================================
// MEMORY POOL API
//=============================================================================

/**
 * @brief Create spike event memory pool
 *
 * WHAT: Create pre-allocated pool for spike events
 * WHY:  O(1) allocation during spike propagation
 * HOW:  Allocates contiguous memory with bitmap tracking
 *
 * @param capacity Number of spike events to pre-allocate
 * @return New pool or NULL on failure
 */
axon_spike_pool_t* axon_spike_pool_create(uint32_t capacity);

/**
 * @brief Destroy spike event memory pool
 *
 * @param pool Pool to destroy
 */
void axon_spike_pool_destroy(axon_spike_pool_t* pool);

/**
 * @brief Allocate spike event from pool
 *
 * WHAT: Get a spike event slot from the pool
 * WHY:  Fast O(1) allocation without malloc
 * HOW:  Bitmap scan for free slot
 *
 * @param pool Pool to allocate from
 * @return Pointer to spike event or NULL if pool exhausted
 */
axon_spike_event_t* axon_spike_pool_alloc(axon_spike_pool_t* pool);

/**
 * @brief Return spike event to pool
 *
 * @param pool Pool to return to
 * @param event Event to return
 */
void axon_spike_pool_free(axon_spike_pool_t* pool, axon_spike_event_t* event);

/**
 * @brief Get pool statistics
 *
 * @param pool Pool to query
 * @param allocated Output: number of allocated slots
 * @param capacity Output: total capacity
 * @param high_water Output: maximum simultaneous allocations
 */
void axon_spike_pool_stats(const axon_spike_pool_t* pool,
                            uint32_t* allocated,
                            uint32_t* capacity,
                            uint32_t* high_water);

/**
 * @brief Create segment memory pool
 *
 * @param capacity Number of segments to pre-allocate
 * @return New pool or NULL on failure
 */
axon_segment_pool_t* axon_segment_pool_create(uint32_t capacity);

/**
 * @brief Destroy segment memory pool
 *
 * @param pool Pool to destroy
 */
void axon_segment_pool_destroy(axon_segment_pool_t* pool);

/**
 * @brief Allocate segment from pool
 *
 * @param pool Pool to allocate from
 * @return Pointer to segment or NULL if pool exhausted
 */
axon_segment_t* axon_segment_pool_alloc(axon_segment_pool_t* pool);

/**
 * @brief Return segment to pool
 *
 * @param pool Pool to return to
 * @param segment Segment to return
 */
void axon_segment_pool_free(axon_segment_pool_t* pool, axon_segment_t* segment);

/**
 * @brief Enable spike pool for network
 *
 * @param network Network to enable pool for
 * @param capacity Pool capacity
 * @return true on success
 */
bool axon_network_enable_spike_pool(axon_network_t* network, uint32_t capacity);

/**
 * @brief Enable segment pool for axon
 *
 * @param axon Axon to enable pool for
 * @param capacity Pool capacity
 * @return true on success
 */
bool axon_enable_segment_pool(axon_t* axon, uint32_t capacity);

//=============================================================================
// COPY-ON-WRITE API
//=============================================================================

/**
 * @brief Create a CoW copy of an axon
 *
 * WHAT: Create a shallow copy that shares data until modification
 * WHY:  Efficient cloning for branching/analysis
 * HOW:  Increments reference count, defers deep copy until write
 *
 * @param axon Axon to copy
 * @return CoW copy or NULL on failure
 */
axon_t* axon_cow_copy(axon_t* axon);

/**
 * @brief Prepare axon for modification (CoW)
 *
 * WHAT: Ensure axon has its own copy of data before modification
 * WHY:  Implements copy-on-write semantics
 * HOW:  Deep copy if reference count > 1
 *
 * @param axon Axon to prepare for writing
 * @return true if axon is safe to modify
 */
bool axon_cow_prepare_write(axon_t* axon);

/**
 * @brief Release CoW reference
 *
 * WHAT: Decrement reference count and free if last reference
 * WHY:  Proper cleanup of CoW copies
 * HOW:  Decrements count, frees when count reaches 0
 *
 * @param axon Axon to release
 */
void axon_cow_release(axon_t* axon);

/**
 * @brief Check if axon is a CoW copy
 *
 * @param axon Axon to check
 * @return true if this is a CoW copy (not the original)
 */
bool axon_is_cow_copy(const axon_t* axon);

/**
 * @brief Get CoW reference count
 *
 * @param axon Axon to query
 * @return Current reference count
 */
uint32_t axon_cow_ref_count(const axon_t* axon);

//=============================================================================
// ENHANCED BIOPHYSICS API (from nimcp_myelin_math.h integration)
//=============================================================================

/**
 * @brief Initialize enhanced biophysics for axon
 *
 * WHAT: Create and attach biophysics state for advanced calculations
 * WHY:  Enable cable theory, saltatory conduction, and block modeling
 * HOW:  Allocates nimcp_myelin_biophysics_t and initializes all segments
 *
 * @param axon Axon to initialize
 * @param use_stochastic Enable stochastic variability
 * @param seed Random seed (0 for time-based)
 * @return true on success
 */
bool axon_init_biophysics(axon_t* axon, bool use_stochastic, uint64_t seed);

/**
 * @brief Update segment cable theory parameters
 *
 * WHAT: Recalculate λ and τ for a segment
 * WHY:  Cable parameters affect passive signal propagation
 * HOW:  Uses nimcp_myelin_compute_cable_params
 *
 * @param axon Axon containing segment
 * @param segment_index Index of segment to update
 */
void axon_update_segment_cable_params(axon_t* axon, uint32_t segment_index);

/**
 * @brief Compute segment velocity with enhanced model
 *
 * WHAT: Calculate velocity using full saltatory conduction model
 * WHY:  More accurate than simple Hursh's law
 * HOW:  Accounts for cable theory, g-ratio efficiency, compaction
 *
 * @param axon Axon containing segment
 * @param segment_index Index of segment
 * @return Velocity in m/s (0 if blocked)
 */
float axon_compute_segment_velocity_enhanced(axon_t* axon, uint32_t segment_index);

/**
 * @brief Check for conduction block at segment
 *
 * WHAT: Determine if signal can propagate through segment
 * WHY:  Model pathological conditions and temperature effects
 * HOW:  Sigmoid probability with temperature modulation
 *
 * @param axon Axon containing segment
 * @param segment_index Index of segment
 * @return true if blocked, false if conducting
 */
bool axon_check_segment_block(axon_t* axon, uint32_t segment_index);

/**
 * @brief Set axon temperature for block modeling
 *
 * @param axon Axon to update
 * @param temperature_c Temperature in Celsius
 */
void axon_set_temperature(axon_t* axon, float temperature_c);

/**
 * @brief Compute metabolic efficiency for axon
 *
 * WHAT: Calculate energy costs and efficiency ratio
 * WHY:  Quantify metabolic benefit of myelination
 * HOW:  Compare myelinated vs unmyelinated energy costs
 *
 * @param axon Axon to analyze
 */
void axon_compute_metabolic_efficiency(axon_t* axon);

/**
 * @brief Get ATP cost per action potential
 *
 * @param axon Axon to query
 * @return ATP molecules per AP
 */
float axon_get_atp_per_ap(const axon_t* axon);

/**
 * @brief Get energy efficiency ratio vs unmyelinated
 *
 * @param axon Axon to query
 * @return Efficiency ratio (>1 means more efficient)
 */
float axon_get_efficiency_ratio(const axon_t* axon);

/**
 * @brief Update all enhanced biophysics for axon
 *
 * WHAT: Recalculate all biophysics parameters for all segments
 * WHY:  Keep cached calculations current after changes
 * HOW:  Iterate segments and update cable, velocity, block, etc.
 *
 * @param axon Axon to update
 */
void axon_update_biophysics(axon_t* axon);

/**
 * @brief Apply activity-dependent myelination using Hill kinetics
 *
 * WHAT: Adjust lamellae based on neural activity
 * WHY:  Activity-dependent plasticity is biological
 * HOW:  Hill-function kinetics with saturation
 *
 * @param axon Axon to update
 * @param activity Activity level (0-1 normalized)
 * @param dt Time step (seconds)
 * @return Total change in lamellae across all segments
 */
float axon_apply_activity_myelination(axon_t* axon, float activity, float dt);

/**
 * @brief Get optimal g-ratio for axon diameter
 *
 * WHAT: Calculate diameter-dependent optimal g-ratio
 * WHY:  Smaller axons have different optimal myelination
 * HOW:  Rushton model with exponential diameter correction
 *
 * @param axon Axon to query
 * @return Optimal g-ratio
 */
float axon_get_optimal_g_ratio(const axon_t* axon);

/**
 * @brief Get space constant for axon
 *
 * WHAT: Return mean space constant λ across segments
 * WHY:  λ determines passive signal attenuation
 * HOW:  Average of segment cable parameters
 *
 * @param axon Axon to query
 * @return Mean λ in micrometers
 */
float axon_get_space_constant(const axon_t* axon);

/**
 * @brief Get frequency-dependent block threshold
 *
 * WHAT: Calculate minimum integrity for given stimulation frequency
 * WHY:  High-frequency conduction fails first
 * HOW:  Inverse of block probability with frequency factor
 *
 * @param axon Axon to query
 * @param frequency_hz Stimulation frequency (Hz)
 * @return Minimum myelination integrity for reliable conduction
 */
float axon_get_frequency_threshold(const axon_t* axon, float frequency_hz);

/**
 * @brief Apply stochastic variability to axon myelination
 *
 * WHAT: Add biological variability to structural parameters
 * WHY:  Real myelin has natural variation
 * HOW:  Log-normal and normal distributions
 *
 * @param axon Axon to modify
 */
void axon_apply_myelination_variability(axon_t* axon);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AXON_H */
