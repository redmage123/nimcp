/**
 * @file nimcp_working_memory_substrate_bridge.h
 * @brief Working Memory-Substrate Bridge for metabolic modulation of working memory
 *
 * WHAT: Bidirectional integration between neural substrate and working memory
 * WHY: Working memory capacity and maintenance critically depend on metabolic state
 * HOW: ATP and temperature modulate capacity, decay rates, and encoding strength
 *
 * BIOLOGICAL BASIS:
 * Working memory (WM) is metabolically expensive, requiring sustained neural firing
 * to maintain representations. Metabolic factors affect WM performance:
 *
 * 1. ATP Depletion → Reduced Capacity:
 *    - Normal WM: 7±2 items (Miller's law)
 *    - Low ATP: Reduced to 3-5 items
 *    - Critical ATP: Down to 1-2 items
 *    - Biological: Persistent firing requires continuous ATP for Na+/K+ pumps
 *
 * 2. Temperature → Faster Decay:
 *    - Normal (37°C): Standard decay rate
 *    - Fever (>38°C): Accelerated forgetting
 *    - Hypothermia (<36°C): Slowed decay
 *    - Biological: Temperature affects membrane kinetics and synaptic reliability
 *
 * 3. Metabolic Stress → Impaired Refresh:
 *    - Attention-based rehearsal requires prefrontal-parietal coordination
 *    - Low ATP reduces refresh efficiency
 *    - Biological: Attentional refresh is an active, energy-demanding process
 *
 * 4. Substrate Health → Encoding Strength:
 *    - New WM items require synaptic potentiation
 *    - Metabolic deficit weakens initial encoding
 *    - Biological: LTP induction needs ATP for AMPA receptor insertion
 *
 * RESEARCH FOUNDATION:
 * - Baddeley (1986): Working memory model with phonological loop and visuospatial sketchpad
 * - Miller (1956): The magical number 7±2
 * - Cowan (2001): Capacity limit of ~4 chunks under metabolic constraint
 * - Engle et al. (1999): Working memory capacity and attentional control
 * - Todd & Marois (2004): Neural basis of capacity limits in prefrontal/parietal cortex
 */

#ifndef NIMCP_WORKING_MEMORY_SUBSTRATE_BRIDGE_H
#define NIMCP_WORKING_MEMORY_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/nimcp_working_memory.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Working Memory Capacity Constants
 *
 * WHAT: Normal and minimal WM capacity limits
 * WHY: Define healthy and impaired states
 * HOW: Based on Miller's 7±2 and metabolic constraints
 */
#define WM_NORMAL_CAPACITY 7        // Miller's magical number
#define WM_REDUCED_CAPACITY 4       // Cowan's revised estimate
#define WM_MINIMAL_CAPACITY 2       // Critical metabolic deficit

/**
 * ATP Thresholds for WM Capacity
 *
 * WHAT: ATP levels that trigger capacity changes
 * WHY: Working memory is highly ATP-dependent
 * HOW: Thresholds based on persistent firing requirements
 */
#define WM_ATP_THRESHOLD_FULL 0.8f      // Full capacity (7 items)
#define WM_ATP_THRESHOLD_REDUCED 0.5f   // Reduced capacity (4 items)
#define WM_ATP_THRESHOLD_MINIMAL 0.3f   // Minimal capacity (2 items)

/**
 * Temperature Effects on WM
 *
 * WHAT: Normal temperature range for optimal WM
 * WHY: Temperature affects neural dynamics and decay rates
 * HOW: Define thresholds for fever and hypothermia effects
 */
#define WM_TEMP_NORMAL_MIN 36.5f    // Normal body temperature range
#define WM_TEMP_NORMAL_MAX 37.5f
#define WM_TEMP_FEVER 38.0f          // Fever threshold
#define WM_TEMP_HYPO 36.0f           // Hypothermia threshold

/**
 * Bio-async Module ID
 *
 * WHAT: Unique identifier for WM-substrate bridge in bio-async router
 * WHY: Enables inter-module communication about metabolic state
 * HOW: Allocated in substrate bridge range (0x1200-0x12FF)
 */
#define BIO_MODULE_SUBSTRATE_WORKING_MEMORY 0x1200

/* ═══════════════════════════════════════════════════════════════════════════
 * STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Working Memory Substrate Effects
 *
 * WHAT: Computed metabolic effects on WM performance
 * WHY: Quantify how substrate state modulates WM parameters
 * HOW: Factors in [0-1] range (except decay_rate_mod)
 *
 * BIOLOGICAL BASIS:
 * - capacity_factor: ATP depletion reduces maximum items held
 * - decay_rate_mod: Temperature affects forgetting rate
 * - refresh_efficiency: Metabolic stress impairs attentional rehearsal
 * - encoding_strength: Substrate health affects new item encoding
 * - is_impaired: Below-threshold flag for critical states
 */
typedef struct {
    float capacity_factor;       // [0-1] WM capacity reduction (1.0 = full 7 items)
    float decay_rate_mod;        // [0.5-2.0] Decay rate multiplier (1.0 = normal)
    float refresh_efficiency;    // [0-1] Attention refresh effectiveness
    float encoding_strength;     // [0-1] New item encoding strength
    bool is_impaired;            // true if below critical ATP threshold
} wm_substrate_effects_t;

/**
 * Working Memory Substrate Configuration
 *
 * WHAT: Configuration for WM-substrate integration
 * WHY: Control which metabolic effects are enabled
 * HOW: Boolean flags for each modulation type
 *
 * USAGE:
 * - enable_capacity_modulation: ATP modulates max items
 * - enable_decay_modulation: Temperature affects decay
 * - enable_refresh_modulation: Metabolic state affects rehearsal
 * - enable_bio_async: Enable bio-async messaging
 * - atp_sensitivity: Scaling factor for ATP effects [0-2]
 * - temperature_sensitivity: Scaling factor for temperature effects [0-2]
 */
typedef struct {
    bool enable_capacity_modulation;    // ATP → capacity
    bool enable_decay_modulation;       // Temperature → decay
    bool enable_refresh_modulation;     // Metabolic state → refresh
    bool enable_bio_async;              // Bio-async messaging
    float atp_sensitivity;              // [0-2] ATP effect scaling
    float temperature_sensitivity;      // [0-2] Temperature effect scaling
} wm_substrate_config_t;

/**
 * Working Memory Substrate Statistics
 *
 * WHAT: Runtime statistics for WM-substrate integration
 * WHY: Monitor metabolic impact on WM performance
 * HOW: Counters and min/avg tracking
 *
 * METRICS:
 * - total_updates: Number of effect updates
 * - capacity_limited_events: Times capacity was reduced
 * - encoding_failures: Failed encodings due to low substrate
 * - min_capacity_factor: Lowest capacity reached
 * - avg_decay_rate_mod: Average decay rate multiplier
 */
typedef struct {
    uint64_t total_updates;             // Total effect updates
    uint32_t capacity_limited_events;   // Times capacity was reduced
    uint32_t encoding_failures;         // Encoding attempts while impaired
    float min_capacity_factor;          // Minimum capacity factor seen
    float avg_decay_rate_mod;           // Average decay rate multiplier
} wm_substrate_stats_t;

/**
 * Working Memory Substrate Bridge
 *
 * WHAT: Main bridge structure connecting substrate and working memory
 * WHY: Encapsulate all state for metabolic modulation of WM
 * HOW: Pointers to substrate/WM, computed effects, config, stats
 *
 * LIFECYCLE:
 * 1. Create: wm_substrate_bridge_create()
 * 2. Update: wm_substrate_update() computes effects
 * 3. Query: wm_substrate_get_*() retrieve effects
 * 4. Destroy: wm_substrate_bridge_destroy()
 *
 * THREAD SAFETY:
 * - Mutex protects effects, stats, and config
 * - Safe for concurrent access from WM and substrate threads
 */
typedef struct wm_substrate_bridge_t {
    bridge_base_t base;                  /**< MUST be first: base bridge infrastructure */

    neural_substrate_t* substrate;      // Pointer to neural substrate
    working_memory_t* wm;               // Pointer to working memory system
    wm_substrate_effects_t effects;     // Computed metabolic effects
    wm_substrate_config_t config;       // Bridge configuration
    wm_substrate_stats_t stats;         // Runtime statistics
} wm_substrate_bridge_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * API FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Initialize default WM-substrate configuration
 *
 * WHAT: Set default configuration for WM-substrate bridge
 * WHY: Provide sensible defaults for all modulation types
 * HOW: Enable all modulations with standard sensitivity
 *
 * @param config Configuration structure to initialize
 * @return 0 on success, negative on error
 *
 * DEFAULTS:
 * - All modulations enabled
 * - Standard sensitivity (1.0) for ATP and temperature
 * - Bio-async disabled by default
 *
 * BIOLOGICAL RATIONALE:
 * Default enables all metabolic effects to model realistic WM behavior
 */
int wm_substrate_default_config(wm_substrate_config_t* config);

/**
 * Create working memory substrate bridge
 *
 * WHAT: Allocate and initialize WM-substrate bridge
 * WHY: Connect substrate metabolic state to WM performance
 * HOW: Create structure, initialize effects to neutral state
 *
 * @param config Bridge configuration (or NULL for defaults)
 * @param substrate Pointer to neural substrate
 * @param wm Pointer to working memory system
 * @return Bridge pointer on success, NULL on error
 *
 * INITIALIZATION:
 * - Allocates bridge structure
 * - Initializes mutex for thread safety
 * - Sets effects to neutral (capacity_factor=1.0, etc.)
 * - Zeros statistics
 *
 * ERROR CONDITIONS:
 * - NULL substrate or wm
 * - Memory allocation failure
 * - Mutex initialization failure
 */
wm_substrate_bridge_t* wm_substrate_bridge_create(
    const wm_substrate_config_t* config,
    neural_substrate_t* substrate,
    working_memory_t* wm
);

/**
 * Destroy working memory substrate bridge
 *
 * WHAT: Clean up and free WM-substrate bridge
 * WHY: Release resources when integration no longer needed
 * HOW: Disconnect bio-async, destroy mutex, free memory
 *
 * @param bridge Bridge to destroy
 *
 * CLEANUP:
 * - Disconnects from bio-async router if connected
 * - Destroys mutex
 * - Frees bridge structure
 *
 * SAFETY:
 * - NULL-safe (no-op if bridge is NULL)
 * - Does not destroy substrate or WM (caller owns those)
 */
void wm_substrate_bridge_destroy(wm_substrate_bridge_t* bridge);

/**
 * Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async messaging system
 * WHY: Enable inter-module communication about metabolic state
 * HOW: Register module with BIO_MODULE_SUBSTRATE_WORKING_MEMORY ID
 *
 * @param bridge Bridge to connect
 * @return 0 on success, negative on error
 *
 * MESSAGING:
 * - Registers with bio-async router
 * - Creates inbox for receiving substrate state updates
 * - Sets bio_async_enabled flag
 *
 * ERROR CONDITIONS:
 * - NULL bridge
 * - Already connected
 * - Bio-async router unavailable
 */
int wm_substrate_connect_bio_async(wm_substrate_bridge_t* bridge);

/**
 * Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async system
 * WHY: Clean disconnection when bio-async no longer needed
 * HOW: Unregister module and clear connection state
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, negative on error
 *
 * CLEANUP:
 * - Unregisters from bio-async router
 * - Clears bio_async_enabled flag
 * - Closes inbox
 */
int wm_substrate_disconnect_bio_async(wm_substrate_bridge_t* bridge);

/**
 * Check bio-async connection status
 *
 * WHAT: Query whether bridge is connected to bio-async
 * WHY: Verify messaging availability before sending
 * HOW: Return bio_async_enabled flag
 *
 * @param bridge Bridge to query
 * @return true if connected, false otherwise
 */
bool wm_substrate_is_bio_async_connected(const wm_substrate_bridge_t* bridge);

/**
 * Update substrate effects on working memory
 *
 * WHAT: Compute metabolic effects based on current substrate state
 * WHY: Keep WM performance synchronized with metabolic state
 * HOW: Read substrate ATP/temperature, compute modulation factors
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative on error
 *
 * COMPUTATION:
 * 1. Read substrate ATP level
 *    - capacity_factor = f(ATP) using thresholds
 *    - encoding_strength = ATP^(atp_sensitivity)
 * 2. Read substrate temperature
 *    - decay_rate_mod = temperature_factor * temperature_sensitivity
 * 3. Compute refresh efficiency
 *    - refresh_efficiency = capacity_factor * encoding_strength
 * 4. Set is_impaired flag if ATP < WM_ATP_THRESHOLD_MINIMAL
 * 5. Update statistics
 *
 * THREAD SAFETY:
 * - Acquires mutex before updating effects
 * - Safe to call from any thread
 *
 * BIOLOGICAL COMPUTATION:
 * - ATP → Capacity: Piecewise linear mapping to capacity factor
 * - Temperature → Decay: Exponential sensitivity (Q10 ~= 2-3)
 * - Combined → Refresh: Product of capacity and encoding factors
 */
int wm_substrate_update(wm_substrate_bridge_t* bridge);

/**
 * Get current capacity factor
 *
 * WHAT: Retrieve WM capacity reduction factor
 * WHY: Query how much capacity is reduced due to ATP
 * HOW: Return effects.capacity_factor under mutex
 *
 * @param bridge Bridge to query
 * @return Capacity factor [0-1], or -1.0 on error
 *
 * INTERPRETATION:
 * - 1.0: Full capacity (7 items)
 * - 0.57: Reduced capacity (4 items)
 * - 0.29: Minimal capacity (2 items)
 */
float wm_substrate_get_capacity_factor(wm_substrate_bridge_t* bridge);

/**
 * Get current decay rate multiplier
 *
 * WHAT: Retrieve decay rate modulation factor
 * WHY: Query how temperature affects forgetting rate
 * HOW: Return effects.decay_rate_mod under mutex
 *
 * @param bridge Bridge to query
 * @return Decay rate multiplier [0.5-2.0], or -1.0 on error
 *
 * INTERPRETATION:
 * - 1.0: Normal decay
 * - 1.5: Faster decay (fever)
 * - 0.7: Slower decay (hypothermia)
 */
float wm_substrate_get_decay_mod(wm_substrate_bridge_t* bridge);

/**
 * Get current refresh efficiency
 *
 * WHAT: Retrieve attention refresh effectiveness
 * WHY: Query how metabolic state affects rehearsal
 * HOW: Return effects.refresh_efficiency under mutex
 *
 * @param bridge Bridge to query
 * @return Refresh efficiency [0-1], or -1.0 on error
 *
 * INTERPRETATION:
 * - 1.0: Full refresh effectiveness
 * - 0.5: Half effectiveness (moderate deficit)
 * - 0.0: No refresh possible (critical state)
 */
float wm_substrate_get_refresh_efficiency(wm_substrate_bridge_t* bridge);

/**
 * Get current encoding strength
 *
 * WHAT: Retrieve new item encoding effectiveness
 * WHY: Query how substrate health affects WM encoding
 * HOW: Return effects.encoding_strength under mutex
 *
 * @param bridge Bridge to query
 * @return Encoding strength [0-1], or -1.0 on error
 *
 * INTERPRETATION:
 * - 1.0: Full encoding strength
 * - 0.5: Weakened encoding (moderate deficit)
 * - 0.0: Failed encoding (critical state)
 */
float wm_substrate_get_encoding_strength(wm_substrate_bridge_t* bridge);

/**
 * Get all substrate effects
 *
 * WHAT: Retrieve complete effects structure
 * WHY: Atomic access to all effects at once
 * HOW: Copy effects structure under mutex
 *
 * @param bridge Bridge to query
 * @param effects Output parameter for effects (must be non-NULL)
 * @return 0 on success, negative on error
 *
 * USAGE:
 * Prefer this over individual getters when needing multiple values,
 * as it provides atomic snapshot of all effects
 */
int wm_substrate_get_effects(
    const wm_substrate_bridge_t* bridge,
    wm_substrate_effects_t* effects
);

/**
 * Check if WM is metabolically impaired
 *
 * WHAT: Query whether WM is in critical metabolic state
 * WHY: Determine if WM function is severely compromised
 * HOW: Return effects.is_impaired flag
 *
 * @param bridge Bridge to query
 * @return true if impaired (ATP < threshold), false otherwise
 *
 * IMPAIRMENT CRITERIA:
 * - ATP below WM_ATP_THRESHOLD_MINIMAL (0.3)
 * - Capacity reduced to 2 items or fewer
 * - Encoding likely to fail
 */
bool wm_substrate_is_impaired(wm_substrate_bridge_t* bridge);

/**
 * Get bridge statistics
 *
 * WHAT: Retrieve runtime statistics
 * WHY: Monitor metabolic impact on WM over time
 * HOW: Copy stats structure under mutex
 *
 * @param bridge Bridge to query
 * @param stats Output parameter for statistics (must be non-NULL)
 * @return 0 on success, negative on error
 *
 * STATISTICS:
 * - total_updates: Number of wm_substrate_update() calls
 * - capacity_limited_events: Times capacity was reduced
 * - encoding_failures: Encoding attempts while impaired
 * - min_capacity_factor: Worst capacity seen
 * - avg_decay_rate_mod: Average decay multiplier
 */
int wm_substrate_get_stats(
    const wm_substrate_bridge_t* bridge,
    wm_substrate_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORKING_MEMORY_SUBSTRATE_BRIDGE_H */
