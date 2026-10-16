/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_hh_hub_bridge.h - Hodgkin-Huxley to Cognitive Hub Bridge
//=============================================================================
/**
 * @file nimcp_hh_hub_bridge.h
 * @brief Bridge between HH biophysics and cognitive integration hub
 *
 * WHAT: Bidirectional integration between Hodgkin-Huxley neuron dynamics and
 *       the cognitive integration hub, enabling biophysically-grounded
 *       higher cognitive functions.
 *
 * WHY:  The cognitive hub orchestrates high-level cognition requiring
 *       biophysical grounding. HH neurons provide:
 *       - Precise timing for temporal binding
 *       - Ion channel dynamics for cognitive state encoding
 *       - Temperature/metabolic signals for resource allocation
 *       - Population synchrony for conscious integration
 *
 * HOW:  - Registers HH as a cognitive module with the hub
 *       - Maps HH activity patterns to cognitive events
 *       - Receives top-down cognitive control signals
 *       - Coordinates with other cognitive modules via hub
 *       - Provides biophysical constraints to cognitive processing
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * COGNITIVE HUB INTEGRATION:
 * --------------------------
 * The cognitive hub implements global workspace theory (GWT), where:
 * - HH neurons form the biophysical substrate
 * - Action potentials enable information broadcasting
 * - Ion channel states encode processing capacity
 * - Synchrony enables conscious access
 *
 * HH TO COGNITIVE MAPPING:
 * ------------------------
 * 1. Spike Timing for Temporal Binding:
 *    - Precise spike times enable feature binding
 *    - Gamma-band synchrony (30-100 Hz) for local binding
 *    - Cross-regional synchrony for global integration
 *
 * 2. Population Activity for Cognitive States:
 *    - Firing rate encodes evidence/activation
 *    - Synchrony encodes confidence/attention
 *    - Burst patterns encode salience
 *
 * 3. Ion Channel State for Capacity:
 *    - Na+ availability: Processing capacity
 *    - K+ state: Recovery/refractory status
 *    - Ca2+: Working memory maintenance
 *
 * 4. Temperature/Metabolic for Resources:
 *    - Q10 effects: Processing speed
 *    - ATP level: Cognitive effort capacity
 *    - Temperature: Arousal/alertness
 *
 * COGNITIVE TO HH EFFECTS:
 * ------------------------
 * 1. Attention Modulation:
 *    - Focused attention: Increase g_Na (amplify signals)
 *    - Divided attention: Decrease gain
 *
 * 2. Working Memory Maintenance:
 *    - Sustained activity patterns
 *    - Ca2+-dependent persistent activity
 *
 * 3. Executive Control:
 *    - Top-down threshold modulation
 *    - Response suppression (inhibitory)
 *
 * 4. Cognitive Load Management:
 *    - Resource allocation signals
 *    - Fatigue/recovery tracking
 *
 * COGNITIVE EVENT TYPES:
 * ----------------------
 * - SPIKE_BINDING: Spike timing for feature binding
 * - ATTENTION_SIGNAL: Attention-related activity
 * - WORKING_MEMORY: Activity for WM maintenance
 * - DECISION_THRESHOLD: Threshold crossing for decisions
 * - COGNITIVE_BROADCAST: Hub-wide activity broadcast
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_HH_HUB_BRIDGE_H
#define NIMCP_HH_HUB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define HH_HUB_MODULE_NAME              "hh_hub_bridge"

/** Module ID in cognitive hub */
#define HH_HUB_MODULE_ID                0x48480001

/** Maximum cognitive event queue */
#define HH_HUB_MAX_EVENT_QUEUE          512

/** Gamma band frequency range (Hz) */
#define HH_HUB_GAMMA_MIN                30.0f
#define HH_HUB_GAMMA_MAX                100.0f

/** Theta band frequency range (Hz) */
#define HH_HUB_THETA_MIN                4.0f
#define HH_HUB_THETA_MAX                8.0f

/** Synchrony threshold for binding */
#define HH_HUB_BINDING_SYNC_THRESH      0.7f

/** Cognitive broadcast threshold */
#define HH_HUB_BROADCAST_THRESH         0.8f

/** Working memory maintenance tau (ms) */
#define HH_HUB_WM_TAU_MS                500.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Cognitive event type from HH
 */
typedef enum {
    HH_COG_EVENT_SPIKE_BINDING = 0,   /**< Spike timing for binding */
    HH_COG_EVENT_ATTENTION,           /**< Attention-related activity */
    HH_COG_EVENT_WORKING_MEMORY,      /**< WM maintenance activity */
    HH_COG_EVENT_DECISION,            /**< Decision threshold crossed */
    HH_COG_EVENT_BROADCAST,           /**< Global broadcast activity */
    HH_COG_EVENT_SYNCHRONY,           /**< Synchrony state change */
    HH_COG_EVENT_RESOURCE_STATE,      /**< Resource availability change */
    HH_COG_EVENT_FATIGUE,             /**< Cognitive fatigue signal */
    HH_COG_EVENT_COUNT
} hh_cog_event_type_t;

/**
 * @brief Cognitive module category
 */
typedef enum {
    HH_COG_CATEGORY_PERCEPTION = 0,   /**< Perceptual processing */
    HH_COG_CATEGORY_ATTENTION,        /**< Attention systems */
    HH_COG_CATEGORY_MEMORY,           /**< Memory systems */
    HH_COG_CATEGORY_EXECUTIVE,        /**< Executive functions */
    HH_COG_CATEGORY_EMOTION,          /**< Emotional processing */
    HH_COG_CATEGORY_MOTOR,            /**< Motor planning/execution */
    HH_COG_CATEGORY_LANGUAGE,         /**< Language processing */
    HH_COG_CATEGORY_COUNT
} hh_cog_category_t;

/**
 * @brief Cognitive control signal type
 */
typedef enum {
    HH_CONTROL_ATTENTION_FOCUS = 0,   /**< Focus attention on this */
    HH_CONTROL_ATTENTION_SUPPRESS,    /**< Suppress this signal */
    HH_CONTROL_WM_MAINTAIN,           /**< Maintain in working memory */
    HH_CONTROL_WM_RELEASE,            /**< Release from working memory */
    HH_CONTROL_RESPONSE_INHIBIT,      /**< Inhibit response */
    HH_CONTROL_EFFORT_INCREASE,       /**< Increase cognitive effort */
    HH_CONTROL_EFFORT_DECREASE        /**< Decrease effort (fatigue) */
} hh_control_type_t;

/**
 * @brief Cognitive state
 */
typedef enum {
    HH_COG_STATE_IDLE = 0,            /**< Minimal cognitive load */
    HH_COG_STATE_MONITORING,          /**< Passive monitoring */
    HH_COG_STATE_PROCESSING,          /**< Active processing */
    HH_COG_STATE_DECIDING,            /**< Decision making */
    HH_COG_STATE_EXECUTING,           /**< Action execution */
    HH_COG_STATE_FATIGUED             /**< Cognitively fatigued */
} hh_cog_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Cognitive event from HH activity
 */
typedef struct {
    hh_cog_event_type_t type;         /**< Event type */
    hh_cog_category_t category;       /**< Cognitive category */
    uint32_t source_id;               /**< Source HH neuron/population */
    float event_strength;             /**< Event strength [0, 1] */
    float confidence;                 /**< Confidence level [0, 1] */
    float timing_precision_ms;        /**< Timing precision */
    float synchrony_level;            /**< Associated synchrony */
    float frequency_hz;               /**< Associated frequency band */
    uint64_t timestamp_us;            /**< Event timestamp */
} hh_cog_event_t;

/**
 * @brief Spike timing for feature binding
 */
typedef struct {
    uint32_t feature_id;              /**< Feature identifier */
    uint32_t* neuron_ids;             /**< Participating neurons */
    float* spike_times;               /**< Spike times (ms) */
    uint32_t num_spikes;              /**< Number of spikes */
    float mean_phase;                 /**< Mean phase in cycle */
    float phase_coherence;            /**< Phase locking value */
    float binding_strength;           /**< Binding strength [0, 1] */
} hh_hub_binding_t;

/**
 * @brief Working memory item maintained by HH
 */
typedef struct {
    uint32_t item_id;                 /**< WM item identifier */
    uint32_t* maintenance_neurons;    /**< Neurons maintaining item */
    uint32_t num_neurons;             /**< Number of maintenance neurons */
    float activation_level;           /**< Current activation [0, 1] */
    float ca_accumulation;            /**< Ca2+ for persistence */
    float maintenance_duration_ms;    /**< Duration maintained */
    float decay_rate;                 /**< Activation decay rate */
    bool refreshed_recently;          /**< Recently rehearsed */
} hh_hub_wm_item_t;

/**
 * @brief Decision evidence from HH
 */
typedef struct {
    uint32_t decision_id;             /**< Decision identifier */
    uint32_t option_id;               /**< Option being evaluated */
    float evidence_accumulation;      /**< Accumulated evidence */
    float threshold;                  /**< Decision threshold */
    float distance_to_threshold;      /**< Distance remaining */
    float accumulation_rate;          /**< Evidence accumulation rate */
    bool threshold_crossed;           /**< Threshold crossed */
    float reaction_time_ms;           /**< Time to threshold */
} hh_hub_decision_t;

/**
 * @brief Cognitive control signal to HH
 */
typedef struct {
    hh_control_type_t type;           /**< Control signal type */
    uint32_t target_neuron;           /**< Target neuron (0 = population) */
    uint32_t target_population;       /**< Target population */
    float control_strength;           /**< Signal strength [0, 1] */
    float g_na_modulation;            /**< Na+ conductance modulation */
    float threshold_modulation;       /**< Threshold modulation (mV) */
    float current_injection;          /**< Current injection (uA/cm^2) */
    float duration_ms;                /**< Control duration */
} hh_hub_control_t;

/**
 * @brief Cognitive resource state from HH
 */
typedef struct {
    float processing_capacity;        /**< Available capacity [0, 1] */
    float fatigue_level;              /**< Fatigue accumulation [0, 1] */
    float recovery_rate;              /**< Recovery rate */
    float temperature_factor;         /**< Temperature effect on speed */
    float na_availability;            /**< Na+ channel availability */
    float k_readiness;                /**< K+ recovery state */
    float ca_level;                   /**< Ca2+ for persistence */
    hh_cog_state_t current_state;     /**< Current cognitive state */
} hh_hub_resource_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Hub registration */
    uint32_t module_id;               /**< Module ID in hub */
    const char* module_name;          /**< Module display name */
    hh_cog_category_t primary_category; /**< Primary cognitive category */

    /* Event generation */
    float binding_sync_threshold;     /**< Synchrony for binding events */
    float broadcast_sync_threshold;   /**< Synchrony for broadcast */
    float decision_threshold;         /**< Default decision threshold */
    float attention_sensitivity;      /**< Attention event sensitivity */

    /* Frequency bands */
    float gamma_min_hz;               /**< Gamma band minimum */
    float gamma_max_hz;               /**< Gamma band maximum */
    float theta_min_hz;               /**< Theta band minimum */
    float theta_max_hz;               /**< Theta band maximum */

    /* Working memory */
    bool enable_wm_maintenance;       /**< Enable WM maintenance */
    float wm_decay_tau_ms;            /**< WM decay time constant */
    float wm_refresh_threshold;       /**< Refresh threshold */

    /* Resource tracking */
    bool enable_resource_tracking;    /**< Track cognitive resources */
    float fatigue_accumulation_rate;  /**< Fatigue accumulation rate */
    float recovery_rate;              /**< Recovery rate */

    /* Control reception */
    bool enable_control_reception;    /**< Receive control signals */
    float max_attention_boost;        /**< Maximum attention boost */
    float max_threshold_shift_mv;     /**< Maximum threshold shift */

    /* Update parameters */
    float update_interval_ms;         /**< Bridge update interval */
} hh_hub_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Event generation */
    uint64_t events_generated;        /**< Total events generated */
    uint64_t binding_events;          /**< Binding events */
    uint64_t attention_events;        /**< Attention events */
    uint64_t wm_events;               /**< Working memory events */
    uint64_t decision_events;         /**< Decision events */
    uint64_t broadcast_events;        /**< Broadcast events */

    /* Control reception */
    uint64_t control_signals_received; /**< Control signals received */
    uint64_t attention_controls;      /**< Attention controls */
    uint64_t wm_controls;             /**< WM controls */
    uint64_t inhibition_controls;     /**< Inhibition controls */

    /* Working memory */
    uint32_t active_wm_items;         /**< Currently maintained items */
    float avg_wm_duration_ms;         /**< Average maintenance duration */
    uint64_t wm_refreshes;            /**< WM refresh events */
    uint64_t wm_decays;               /**< WM decay events */

    /* Decision making */
    uint64_t decisions_completed;     /**< Decisions completed */
    float avg_decision_time_ms;       /**< Average decision time */
    float avg_evidence_at_decision;   /**< Average evidence level */

    /* Resource state */
    float current_capacity;           /**< Current processing capacity */
    float current_fatigue;            /**< Current fatigue level */
    float avg_capacity;               /**< Average capacity */

    /* Performance */
    float last_update_ms;             /**< Last update timestamp */
    float processing_latency_us;      /**< Processing latency */
} hh_hub_stats_t;

/** Opaque bridge handle */
typedef struct hh_hub_bridge_struct hh_hub_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Initialize configuration with cognitive defaults
 * WHY:  Easy creation with neuroscience-motivated parameters
 * HOW:  Set gamma/theta bands, WM enabled, resource tracking
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_default_config(hh_hub_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create HH-hub bridge
 *
 * WHAT: Initialize bridge for cognitive integration
 * WHY:  Enable HH participation in cognitive hub
 * HOW:  Register with hub, initialize event generation
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT hh_hub_bridge_t* hh_hub_bridge_create(
    const hh_hub_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void hh_hub_bridge_destroy(hh_hub_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_bridge_reset(hh_hub_bridge_t* bridge);

//=============================================================================
// Hub Connection API
//=============================================================================

/**
 * @brief Connect to cognitive hub
 *
 * WHAT: Register bridge with cognitive integration hub
 * WHY:  Enable bidirectional communication
 * HOW:  Register as module, set up event handlers
 *
 * @param bridge Bridge handle
 * @param hub Cognitive hub handle (opaque)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_connect(
    hh_hub_bridge_t* bridge,
    void* hub
);

/**
 * @brief Disconnect from cognitive hub
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_disconnect(hh_hub_bridge_t* bridge);

/**
 * @brief Check if connected to hub
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
NIMCP_EXPORT bool hh_hub_is_connected(const hh_hub_bridge_t* bridge);

//=============================================================================
// Event Generation API (HH to Hub)
//=============================================================================

/**
 * @brief Generate cognitive event from HH activity
 *
 * WHAT: Create cognitive event from HH state
 * WHY:  Translate biophysics to cognitive meaning
 * HOW:  Analyze activity, classify event type
 *
 * @param bridge Bridge handle
 * @param source_id Source neuron/population
 * @param firing_rate Current firing rate (Hz)
 * @param synchrony Population synchrony [0, 1]
 * @param frequency Dominant frequency (Hz)
 * @param event_out Output cognitive event
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_generate_event(
    hh_hub_bridge_t* bridge,
    uint32_t source_id,
    float firing_rate,
    float synchrony,
    float frequency,
    hh_cog_event_t* event_out
);

/**
 * @brief Report spike binding
 *
 * WHAT: Report synchronized spikes for feature binding
 * WHY:  Binding by synchrony mechanism
 * HOW:  Analyze phase coherence, report to hub
 *
 * @param bridge Bridge handle
 * @param binding Binding specification
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_report_binding(
    hh_hub_bridge_t* bridge,
    const hh_hub_binding_t* binding
);

/**
 * @brief Report decision threshold crossing
 *
 * WHAT: Report decision evidence crossing threshold
 * WHY:  Signal decision completion to hub
 * HOW:  Send decision event with evidence level
 *
 * @param bridge Bridge handle
 * @param decision Decision specification
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_report_decision(
    hh_hub_bridge_t* bridge,
    const hh_hub_decision_t* decision
);

/**
 * @brief Request cognitive broadcast
 *
 * WHAT: Request global broadcast through hub
 * WHY:  Global workspace ignition
 * HOW:  High synchrony triggers broadcast
 *
 * @param bridge Bridge handle
 * @param content_id Content to broadcast
 * @param strength Broadcast strength [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_request_broadcast(
    hh_hub_bridge_t* bridge,
    uint32_t content_id,
    float strength
);

//=============================================================================
// Working Memory API
//=============================================================================

/**
 * @brief Maintain item in working memory
 *
 * WHAT: Activate HH neurons for WM maintenance
 * WHY:  Persistent activity for WM
 * HOW:  Sustained firing via Ca2+ dynamics
 *
 * @param bridge Bridge handle
 * @param item WM item to maintain
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_wm_maintain(
    hh_hub_bridge_t* bridge,
    const hh_hub_wm_item_t* item
);

/**
 * @brief Refresh working memory item
 *
 * WHAT: Rehearse/refresh WM item
 * WHY:  Prevent decay
 * HOW:  Boost activation, reset decay
 *
 * @param bridge Bridge handle
 * @param item_id Item to refresh
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_wm_refresh(
    hh_hub_bridge_t* bridge,
    uint32_t item_id
);

/**
 * @brief Release item from working memory
 *
 * WHAT: Stop maintaining WM item
 * WHY:  Free capacity for new items
 * HOW:  Allow decay, stop maintenance activity
 *
 * @param bridge Bridge handle
 * @param item_id Item to release
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_wm_release(
    hh_hub_bridge_t* bridge,
    uint32_t item_id
);

/**
 * @brief Get working memory item state
 *
 * @param bridge Bridge handle
 * @param item_id Item to query
 * @param item_out Output item state
 * @return 0 on success, -1 if not found
 */
NIMCP_EXPORT int hh_hub_wm_get_item(
    const hh_hub_bridge_t* bridge,
    uint32_t item_id,
    hh_hub_wm_item_t* item_out
);

//=============================================================================
// Control Reception API (Hub to HH)
//=============================================================================

/**
 * @brief Receive control signal from hub
 *
 * WHAT: Process control signal from cognitive hub
 * WHY:  Top-down cognitive control of HH
 * HOW:  Apply modulation based on control type
 *
 * @param bridge Bridge handle
 * @param control Control signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_receive_control(
    hh_hub_bridge_t* bridge,
    const hh_hub_control_t* control
);

/**
 * @brief Get pending control effects for neuron
 *
 * WHAT: Query accumulated control effects
 * WHY:  Apply to HH parameters
 * HOW:  Sum all active controls
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to query
 * @param g_na_mod Output Na+ modulation
 * @param threshold_mod Output threshold modulation
 * @param current_mod Output current modulation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_get_control_effects(
    const hh_hub_bridge_t* bridge,
    uint32_t neuron_id,
    float* g_na_mod,
    float* threshold_mod,
    float* current_mod
);

//=============================================================================
// Resource Tracking API
//=============================================================================

/**
 * @brief Update cognitive resource state
 *
 * WHAT: Update resource state from HH metrics
 * WHY:  Track cognitive capacity
 * HOW:  Aggregate channel availability, temperature
 *
 * @param bridge Bridge handle
 * @param na_availability Na+ channel availability
 * @param k_readiness K+ recovery state
 * @param ca_level Ca2+ level
 * @param temperature Current temperature
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_update_resources(
    hh_hub_bridge_t* bridge,
    float na_availability,
    float k_readiness,
    float ca_level,
    float temperature
);

/**
 * @brief Get current resource state
 *
 * @param bridge Bridge handle
 * @param resource_out Output resource state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_get_resources(
    const hh_hub_bridge_t* bridge,
    hh_hub_resource_t* resource_out
);

/**
 * @brief Report fatigue to hub
 *
 * WHAT: Signal cognitive fatigue
 * WHY:  Enable hub to reduce load
 * HOW:  Send fatigue event with level
 *
 * @param bridge Bridge handle
 * @param fatigue_level Fatigue level [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_report_fatigue(
    hh_hub_bridge_t* bridge,
    float fatigue_level
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic bridge housekeeping
 * WHY:  Decay WM, accumulate fatigue, process controls
 * HOW:  Time-based state transitions
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_bridge_update(
    hh_hub_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_get_stats(
    const hh_hub_bridge_t* bridge,
    hh_hub_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_hub_reset_stats(hh_hub_bridge_t* bridge);

/**
 * @brief Get cognitive event type name
 *
 * @param type Event type
 * @return Static string name
 */
NIMCP_EXPORT const char* hh_hub_event_type_name(hh_cog_event_type_t type);

/**
 * @brief Get cognitive state name
 *
 * @param state Cognitive state
 * @return Static string name
 */
NIMCP_EXPORT const char* hh_hub_state_name(hh_cog_state_t state);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle (NULL safe)
 */
NIMCP_EXPORT void hh_hub_print_summary(const hh_hub_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HH_HUB_BRIDGE_H */