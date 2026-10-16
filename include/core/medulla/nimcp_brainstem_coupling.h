/**
 * @file nimcp_brainstem_coupling.h
 * @brief Bidirectional coupling between brainstem (medulla) and cortex
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Communication infrastructure for brainstem-cortex signal exchange
 * WHY:  Models ascending reticular activating system (ARAS) and descending pathways;
 *       essential for arousal, autonomic control, and cortical-subcortical integration
 * HOW:  Bottom-up signals (arousal, threats) and top-down modulation (attention, voluntary
 *       control) routed through prioritized buffers with bio-async messaging
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * ASCENDING PATHWAYS (Bottom-Up: Brainstem → Cortex):
 * ---------------------------------------------------
 * 1. Reticular Activating System (RAS):
 *    - Origin: Reticular formation in medulla, pons, midbrain
 *    - Projects to: Thalamus → widespread cortical areas
 *    - Function: Arousal, wakefulness, attention, consciousness
 *    - Neurotransmitters: ACh, NE, DA, 5-HT, Histamine
 *    - Reference: Moruzzi & Magoun (1949) "Brain stem reticular formation and
 *      activation of the EEG"
 *
 * 2. Threat/Salience Signaling:
 *    - Origin: Nucleus tractus solitarius (NTS), parabrachial nucleus
 *    - Conveys: Visceral threats, homeostatic disturbances, pain
 *    - Targets: Amygdala, insula, anterior cingulate cortex
 *    - Drives: Threat detection, autonomic responses, cortical alerting
 *    - Reference: Saper (2002) "The central autonomic nervous system"
 *
 * 3. Metabolic Signaling:
 *    - Origin: Area postrema, NTS (detect glucose, O2, CO2, pH)
 *    - Conveys: Energy state, respiratory drive, cardiovascular status
 *    - Targets: Hypothalamus → prefrontal cortex
 *    - Modulates: Cognitive resource allocation, fatigue, vigilance
 *    - Reference: Guyenet & Bayliss (2015) "Neural control of breathing"
 *
 * 4. Circadian Arousal:
 *    - Origin: Locus coeruleus (LC), raphe nuclei modulated by SCN
 *    - Conveys: Time-of-day arousal signals
 *    - Targets: Widespread cortical and subcortical regions
 *    - Drives: Sleep-wake transitions, circadian performance rhythms
 *    - Reference: Aston-Jones & Cohen (2005) "Adaptive gain and the role of the LC-NE"
 *
 * DESCENDING PATHWAYS (Top-Down: Cortex → Brainstem):
 * ---------------------------------------------------
 * 1. Attention Modulation:
 *    - Origin: Prefrontal cortex, parietal cortex
 *    - Targets: Reticular formation, LC, raphe nuclei
 *    - Function: Selective enhancement of arousal for attended stimuli
 *    - Effect: Gates sensory input, modulates NE/5-HT release
 *    - Reference: Corbetta & Shulman (2002) "Control of goal-directed and
 *      stimulus-driven attention"
 *
 * 2. Executive Override:
 *    - Origin: Dorsolateral prefrontal cortex (dlPFC), anterior cingulate (ACC)
 *    - Targets: Periaqueductal gray (PAG), medullary autonomic nuclei
 *    - Function: Volitional control over autonomic responses
 *    - Examples: Voluntary breathing, emotional regulation, pain modulation
 *    - Reference: Thayer & Lane (2009) "Claude Bernard and the heart-brain connection"
 *
 * 3. Voluntary Motor Control:
 *    - Origin: Primary motor cortex (M1), premotor cortex
 *    - Targets: Cranial nerve nuclei (VII, IX, X, XII), respiratory centers
 *    - Function: Voluntary control of speech, swallowing, breathing
 *    - Pathways: Corticobulbar tracts
 *    - Reference: Kuypers (1958) "Corticobulbar connexions to the pons and
 *      lower brain-stem in man"
 *
 * SIGNAL LATENCIES (Biological):
 * ------------------------------
 * - Arousal signals: ~50-100 ms (fast NE/ACh release)
 * - Threat signals: ~20-50 ms (pain/autonomic reflexes)
 * - Metabolic signals: 500-2000 ms (slower integration)
 * - Circadian signals: ~10-60 min (gradual modulation)
 * - Top-down modulation: 100-300 ms (cortical processing delay)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                     BRAINSTEM-CORTEX COUPLING                              ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              BOTTOM-UP PATHWAYS (Brainstem → Cortex)               │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────────┐          ┌──────────────────────┐            │  ║
 * ║   │   │  MEDULLA/PONS   │          │   CORTICAL MODULES   │            │  ║
 * ║   │   │  ─────────────  │          │   ───────────────    │            │  ║
 * ║   │   │                 │          │                      │            │  ║
 * ║   │   │  Arousal   ────────────────▶  Global alertness   │            │  ║
 * ║   │   │  Threat    ────────────────▶  Amygdala/ACC       │            │  ║
 * ║   │   │  Metabolic ────────────────▶  Resource control   │            │  ║
 * ║   │   │  Circadian ────────────────▶  Sleep/wake state   │            │  ║
 * ║   │   │                 │          │                      │            │  ║
 * ║   │   └─────────────────┘          └──────────────────────┘            │  ║
 * ║   │                                                                     │  ║
 * ║   │   Signal Properties:                                                │  ║
 * ║   │   • Priority-based delivery                                         │  ║
 * ║   │   • Latency modeling (20-2000 ms)                                   │  ║
 * ║   │   • Module filtering                                                │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              TOP-DOWN PATHWAYS (Cortex → Brainstem)                │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────┐          ┌─────────────────┐            │  ║
 * ║   │   │   CORTICAL MODULES   │          │  MEDULLA/PONS   │            │  ║
 * ║   │   │   ───────────────    │          │  ─────────────  │            │  ║
 * ║   │   │                      │          │                 │            │  ║
 * ║   │   │  Attention    ───────────────▶  RAS modulation   │            │  ║
 * ║   │   │  Executive    ───────────────▶  Autonomic control│            │  ║
 * ║   │   │  Voluntary    ───────────────▶  Motor nuclei     │            │  ║
 * ║   │   │                      │          │                 │            │  ║
 * ║   │   └──────────────────────┘          └─────────────────┘            │  ║
 * ║   │                                                                     │  ║
 * ║   │   Modulation Effects:                                               │  ║
 * ║   │   • Selective arousal gating                                        │  ║
 * ║   │   • Autonomic override                                              │  ║
 * ║   │   • Voluntary motor commands                                        │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAINSTEM_COUPLING_H
#define NIMCP_BRAINSTEM_COUPLING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "utils/platform/nimcp_platform.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * SIGNAL TYPE ENUMERATIONS
 *============================================================================*/

/**
 * @brief Bottom-up signal types (Brainstem → Cortex)
 *
 * WHAT: Ascending signals from brainstem to cortical modules
 * WHY:  Different signals have distinct biological origins and effects
 * HOW:  Enum-based categorization matching biological pathways
 */
typedef enum {
    /* Ascending Reticular Activating System */
    BRAINSTEM_AROUSAL_SIGNAL = 0,      /**< General arousal/wakefulness (RAS) */
    BRAINSTEM_THREAT_SIGNAL,           /**< Threat/salience from NTS/parabrachial */
    BRAINSTEM_METABOLIC_SIGNAL,        /**< Energy/respiratory/cardiovascular state */
    BRAINSTEM_CIRCADIAN_SIGNAL,        /**< Time-of-day arousal modulation */

    BRAINSTEM_BOTTOM_UP_COUNT          /**< Total bottom-up signal types */
} brainstem_bottom_up_signal_t;

/**
 * @brief Top-down signal types (Cortex → Brainstem)
 *
 * WHAT: Descending signals from cortex to brainstem nuclei
 * WHY:  Cortical control over arousal, autonomic, and motor systems
 * HOW:  Enum-based categorization matching descending pathways
 */
typedef enum {
    /* Descending Cortical Control */
    BRAINSTEM_ATTENTION_MODULATION = 0, /**< PFC/parietal → RAS/LC modulation */
    BRAINSTEM_EXECUTIVE_OVERRIDE,       /**< dlPFC/ACC → autonomic override */
    BRAINSTEM_VOLUNTARY_CONTROL,        /**< M1/premotor → cranial motor nuclei */

    BRAINSTEM_TOP_DOWN_COUNT            /**< Total top-down signal types */
} brainstem_top_down_signal_t;

/*=============================================================================
 * SIGNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Signal priority levels
 *
 * WHAT: Priority for signal delivery
 * WHY:  Threat signals should preempt metabolic signals
 */
typedef enum {
    SIGNAL_PRIORITY_LOW = 0,      /**< Slow metabolic/circadian updates */
    SIGNAL_PRIORITY_MEDIUM = 1,   /**< Normal arousal signals */
    SIGNAL_PRIORITY_HIGH = 2,     /**< Urgent threat signals */
    SIGNAL_PRIORITY_CRITICAL = 3  /**< Life-threatening conditions */
} brainstem_signal_priority_t;

/**
 * @brief Bottom-up signal payload
 *
 * WHAT: Data packet for ascending signal
 * WHY:  Encapsulates signal type, intensity, and metadata
 */
typedef struct {
    brainstem_bottom_up_signal_t type;  /**< Signal type */
    float intensity;                     /**< Signal strength (0.0-1.0) */
    brainstem_signal_priority_t priority; /**< Delivery priority */
    uint32_t source_module;              /**< Originating brainstem module ID */
    uint64_t timestamp;                  /**< Signal generation time (ms) */
    uint32_t latency_ms;                 /**< Biological latency to delivery */
} brainstem_bottom_up_payload_t;

/**
 * @brief Top-down signal payload
 *
 * WHAT: Data packet for descending signal
 * WHY:  Encapsulates cortical modulation commands
 */
typedef struct {
    brainstem_top_down_signal_t type;    /**< Signal type */
    float modulation;                    /**< Modulation strength (-1.0 to 1.0) */
    uint32_t target_module;              /**< Target brainstem module ID */
    uint64_t timestamp;                  /**< Signal generation time (ms) */
} brainstem_top_down_payload_t;

/*=============================================================================
 * CONFIGURATION
 *============================================================================*/

/**
 * @brief Brainstem coupling configuration
 *
 * WHAT: Configuration parameters for coupling system
 * WHY:  Allow tuning of buffer sizes, latencies, filtering
 */
typedef struct {
    /* Buffer capacities */
    uint32_t bottom_up_buffer_size;     /**< Ascending signal buffer */
    uint32_t top_down_buffer_size;      /**< Descending signal buffer */

    /* Module registration */
    uint32_t max_registered_modules;    /**< Max cortical modules */

    /* Bio-async integration */
    bool enable_bio_async;              /**< Use bio-async messaging */
    uint32_t bio_async_inbox_capacity;  /**< Messages per inbox */

    /* Signal processing */
    bool apply_latency_model;           /**< Delay signals by biological latency */
    bool enable_priority_queue;         /**< Sort by priority before delivery */
} brainstem_coupling_config_t;

/*=============================================================================
 * MAIN STRUCTURE
 *============================================================================*/

/**
 * @brief Brainstem-cortex coupling system (opaque)
 *
 * WHAT: Main coupling infrastructure
 * WHY:  Encapsulates all state for bidirectional communication
 * HOW:  Opaque pointer pattern for implementation hiding
 */
typedef struct brainstem_coupling_struct brainstem_coupling_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

/**
 * @brief Create brainstem coupling system
 *
 * WHAT: Allocates and initializes coupling infrastructure
 * WHY:  Required before any signal exchange can occur
 * HOW:  Allocates buffers, initializes mutex, registers bio-async module
 *
 * @param config Configuration parameters (NULL uses defaults)
 * @return Coupling system handle, or NULL on failure
 */
brainstem_coupling_t* brainstem_coupling_create(const brainstem_coupling_config_t* config);

/**
 * @brief Destroy brainstem coupling system
 *
 * WHAT: Frees all resources and disconnects bio-async
 * WHY:  Prevent memory leaks and ensure clean shutdown
 * HOW:  Drains buffers, destroys mutex, unregisters from bio-async
 *
 * @param coupling Coupling system to destroy
 */
void brainstem_coupling_destroy(brainstem_coupling_t* coupling);

/**
 * @brief Get default configuration
 *
 * WHAT: Populates config struct with sensible defaults
 * WHY:  Convenience function for common use cases
 * HOW:  Sets buffer sizes, latency defaults based on biology
 *
 * @param config Configuration struct to populate (must be non-NULL)
 * @return 0 on success, negative on error
 */
int brainstem_coupling_default_config(brainstem_coupling_config_t* config);

/*=============================================================================
 * SIGNAL TRANSMISSION
 *============================================================================*/

/**
 * @brief Send bottom-up signal (brainstem → cortex)
 *
 * WHAT: Transmits ascending signal to cortical modules
 * WHY:  Models RAS and other ascending pathways
 * HOW:  Adds signal to buffer, applies latency if enabled, routes via bio-async
 *
 * @param coupling Coupling system
 * @param payload Signal data
 * @return 0 on success, negative on error (buffer full, invalid params)
 */
int brainstem_coupling_send_bottom_up(
    brainstem_coupling_t* coupling,
    const brainstem_bottom_up_payload_t* payload
);

/**
 * @brief Send top-down signal (cortex → brainstem)
 *
 * WHAT: Transmits descending modulation signal
 * WHY:  Models cortical control over brainstem nuclei
 * HOW:  Adds signal to buffer, routes to target brainstem module
 *
 * @param coupling Coupling system
 * @param payload Signal data
 * @return 0 on success, negative on error
 */
int brainstem_coupling_send_top_down(
    brainstem_coupling_t* coupling,
    const brainstem_top_down_payload_t* payload
);

/*=============================================================================
 * MODULE REGISTRATION
 *============================================================================*/

/**
 * @brief Register cortical module to receive bottom-up signals
 *
 * WHAT: Adds module to signal distribution list
 * WHY:  Only registered modules receive ascending signals
 * HOW:  Stores module ID in registered list
 *
 * @param coupling Coupling system
 * @param module_id Bio-async module ID (from nimcp_bio_messages.h)
 * @return 0 on success, negative on error (already registered, list full)
 */
int brainstem_coupling_register_module(
    brainstem_coupling_t* coupling,
    uint32_t module_id
);

/**
 * @brief Unregister cortical module
 *
 * WHAT: Removes module from signal distribution
 * WHY:  Module no longer wants to receive signals
 * HOW:  Removes module ID from registered list
 *
 * @param coupling Coupling system
 * @param module_id Module to unregister
 * @return 0 on success, negative on error (not registered)
 */
int brainstem_coupling_unregister_module(
    brainstem_coupling_t* coupling,
    uint32_t module_id
);

/*=============================================================================
 * SIGNAL RETRIEVAL
 *============================================================================*/

/**
 * @brief Get pending bottom-up signals
 *
 * WHAT: Retrieves all pending ascending signals
 * WHY:  Cortical modules need to poll for new signals
 * HOW:  Copies signals from buffer, applies priority sorting if enabled
 *
 * @param coupling Coupling system
 * @param out_signals Output buffer for signals
 * @param max_signals Size of output buffer
 * @param out_count Number of signals retrieved
 * @return 0 on success, negative on error
 */
int brainstem_coupling_get_pending_signals(
    brainstem_coupling_t* coupling,
    brainstem_bottom_up_payload_t* out_signals,
    uint32_t max_signals,
    uint32_t* out_count
);

/**
 * @brief Process all pending signals
 *
 * WHAT: Delivers all pending signals to registered modules via bio-async
 * WHY:  Central processing step for signal distribution
 * HOW:  Iterates buffers, applies latency/priority, sends bio-async messages
 *
 * @param coupling Coupling system
 * @return Number of signals processed, or negative on error
 */
int brainstem_coupling_process_signals(brainstem_coupling_t* coupling);

/*=============================================================================
 * PRIORITY AND FILTERING
 *============================================================================*/

/**
 * @brief Set signal priority
 *
 * WHAT: Updates priority for a signal type
 * WHY:  Different biological contexts require different priorities
 * HOW:  Stores priority mapping for signal type
 *
 * @param coupling Coupling system
 * @param signal_type Bottom-up signal type
 * @param priority New priority level
 * @return 0 on success, negative on error
 */
int brainstem_coupling_set_priority(
    brainstem_coupling_t* coupling,
    brainstem_bottom_up_signal_t signal_type,
    brainstem_signal_priority_t priority
);

/**
 * @brief Set biological latency for signal type
 *
 * WHAT: Configures transmission latency for a signal type
 * WHY:  Different pathways have different conduction speeds
 * HOW:  Stores latency mapping, applied during signal delivery
 *
 * @param coupling Coupling system
 * @param signal_type Signal type
 * @param latency_ms Latency in milliseconds
 * @return 0 on success, negative on error
 */
int brainstem_coupling_set_latency(
    brainstem_coupling_t* coupling,
    brainstem_bottom_up_signal_t signal_type,
    uint32_t latency_ms
);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *============================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Registers coupling module with bio-async infrastructure
 * WHY:  Enables inter-module messaging for signal distribution
 * HOW:  Calls bio_router_register_module() with coupling context
 *
 * @param coupling Coupling system
 * @return 0 on success, negative on error
 */
int brainstem_coupling_connect_bio_async(brainstem_coupling_t* coupling);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregisters coupling module from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregisters module, disables bio-async flag
 *
 * @param coupling Coupling system
 * @return 0 on success, negative on error
 */
int brainstem_coupling_disconnect_bio_async(brainstem_coupling_t* coupling);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query connection status
 * WHY:  Determine if messaging is available
 * HOW:  Returns bio_async_enabled flag
 *
 * @param coupling Coupling system
 * @return true if connected, false otherwise
 */
bool brainstem_coupling_is_bio_async_connected(const brainstem_coupling_t* coupling);

/*=============================================================================
 * STATISTICS AND MONITORING
 *============================================================================*/

/**
 * @brief Coupling statistics
 *
 * WHAT: Performance and usage metrics
 * WHY:  Monitor signal flow and detect bottlenecks
 */
typedef struct {
    uint64_t bottom_up_sent;        /**< Total ascending signals sent */
    uint64_t top_down_sent;         /**< Total descending signals sent */
    uint64_t signals_processed;     /**< Total signals delivered */
    uint64_t signals_dropped;       /**< Signals dropped (buffer full) */
    uint32_t registered_modules;    /**< Current registered module count */
    uint32_t pending_bottom_up;     /**< Current ascending buffer count */
    uint32_t pending_top_down;      /**< Current descending buffer count */
} brainstem_coupling_stats_t;

/**
 * @brief Get coupling statistics
 *
 * WHAT: Retrieves current statistics
 * WHY:  Monitor system health and performance
 * HOW:  Copies internal counters to output struct
 *
 * @param coupling Coupling system
 * @param out_stats Output statistics struct
 * @return 0 on success, negative on error
 */
int brainstem_coupling_get_stats(
    const brainstem_coupling_t* coupling,
    brainstem_coupling_stats_t* out_stats
);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Zeros all statistics counters
 * WHY:  Start fresh monitoring period
 * HOW:  Sets all counters to zero
 *
 * @param coupling Coupling system
 * @return 0 on success, negative on error
 */
int brainstem_coupling_reset_stats(brainstem_coupling_t* coupling);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAINSTEM_COUPLING_H */
