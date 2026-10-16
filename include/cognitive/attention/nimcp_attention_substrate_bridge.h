/**
 * @file nimcp_attention_substrate_bridge.h
 * @brief Bridge between neural substrate and attention system
 *
 * WHAT: Bidirectional integration layer connecting metabolic/thermal substrate
 *       with attention mechanisms (focus, shifting, filtering, vigilance).
 *
 * WHY: Attention is metabolically expensive. The frontoparietal attention
 *      networks require sustained ATP for focus maintenance, rapid attention
 *      switching, distractor filtering, and vigilance. ATP depletion,
 *      hyperthermia, and metabolic stress impair these capabilities.
 *
 * HOW: Monitors substrate state (ATP, temperature, metabolic stress) and
 *      modulates attention parameters:
 *      - Focus capacity: Ability to sustain attention on target
 *      - Shifting efficiency: Speed of attention reorienting
 *      - Filter strength: Ability to suppress distractors
 *      - Vigilance: Sustained attention over time
 *
 * BIOLOGICAL BASIS:
 * - Frontoparietal attention networks are metabolically costly
 * - ATP depletion reduces sustained attention span
 * - Hyperthermia (fever) impairs executive attention
 * - Metabolic stress increases distractibility
 * - Vigilance tasks show performance decline with fatigue
 *
 * Integration Points:
 * ┌──────────────────┐
 * │ Neural Substrate │
 * │  - ATP levels    │──┐
 * │  - Temperature   │  │
 * │  - Metabolic Q10 │  │
 * └──────────────────┘  │
 *                       │
 *                       ▼
 *           ┌──────────────────────┐
 *           │ Attention Substrate  │
 *           │      Bridge          │
 *           └──────────────────────┘
 *                       │
 *                       ▼
 * ┌──────────────────────────────────┐
 * │    Attention System              │
 * │  - Focus capacity                │
 * │  - Shifting efficiency           │
 * │  - Filter strength               │
 * │  - Vigilance factor              │
 * └──────────────────────────────────┘
 */

#ifndef NIMCP_ATTENTION_SUBSTRATE_BRIDGE_H
#define NIMCP_ATTENTION_SUBSTRATE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include <stdbool.h>
#include <stdint.h>

/* Forward declaration for attention system (opaque pointer) */
typedef struct nimcp_attention_system nimcp_attention_system_t;

/**
 * Bio-async module ID for attention substrate bridge
 * Range: 0x1200-0x12FF (substrate bridges)
 */
#define BIO_MODULE_SUBSTRATE_ATTENTION 0x1201

/**
 * ATP threshold constants
 * WHAT: Critical ATP levels that trigger attention impairment
 * WHY: Attention networks require high metabolic support
 */
#define ATTENTION_SUBSTRATE_ATP_FULL 0.8f      /* Full attention capacity */
#define ATTENTION_SUBSTRATE_ATP_REDUCED 0.5f   /* Reduced attention */
#define ATTENTION_SUBSTRATE_ATP_CRITICAL 0.3f  /* Severely impaired */

/**
 * Temperature Q10 coefficient for attention
 * WHAT: Temperature sensitivity of attention processes
 * WHY: Attention is sensitive to fever/hyperthermia
 * HOW: Q10 = 2.5 means 2.5x metabolic rate per 10°C increase
 */
#define ATTENTION_SUBSTRATE_Q10_FOCUS 2.5f      /* Focus maintenance */
#define ATTENTION_SUBSTRATE_Q10_SHIFTING 2.8f   /* Attention switching */
#define ATTENTION_SUBSTRATE_Q10_FILTER 2.3f     /* Distractor filtering */
#define ATTENTION_SUBSTRATE_Q10_VIGILANCE 2.6f  /* Sustained vigilance */

/**
 * Attention substrate effects
 * WHAT: Computed metabolic/thermal effects on attention capabilities
 * WHY: Provides quantified impact of substrate state on attention
 * HOW: Values in [0-1] range, where 1.0 = optimal, 0.0 = fully impaired
 */
typedef struct {
    float focus_capacity;       /* Ability to sustain focus [0-1] */
    float shifting_efficiency;  /* Speed of attention switching [0-1] */
    float filter_strength;      /* Distractor suppression [0-1] */
    float vigilance_factor;     /* Sustained attention capability [0-1] */
    bool is_impaired;           /* Overall impairment flag */
} attention_substrate_effects_t;

/**
 * Attention substrate configuration
 * WHAT: Bridge behavior configuration
 * WHY: Allows tuning of substrate-attention coupling
 * HOW: Enable/disable specific modulations and set sensitivities
 */
typedef struct {
    bool enable_focus_modulation;      /* Modulate focus capacity */
    bool enable_shifting_modulation;   /* Modulate shifting speed */
    bool enable_filter_modulation;     /* Modulate filter strength */
    bool enable_bio_async;             /* Enable bio-async messaging */
    float atp_sensitivity;             /* ATP impact scaling [0-1] */
    float temperature_sensitivity;     /* Temperature impact scaling [0-1] */
} attention_substrate_config_t;

/**
 * Attention substrate statistics
 * WHAT: Runtime monitoring metrics
 * WHY: Track bridge performance and attention state over time
 * HOW: Accumulated counters and averages
 */
typedef struct {
    uint64_t update_count;              /* Number of updates performed */
    uint64_t impairment_events;         /* Times attention became impaired */
    float avg_focus_capacity;           /* Running average focus */
    float avg_shifting_efficiency;      /* Running average shifting */
    float avg_filter_strength;          /* Running average filtering */
    float avg_vigilance;                /* Running average vigilance */
    float min_focus_observed;           /* Lowest focus observed */
    float max_focus_observed;           /* Highest focus observed */
} attention_substrate_stats_t;

/**
 * Attention substrate bridge
 * WHAT: Main integration structure connecting substrate and attention
 * WHY: Encapsulates bidirectional substrate-attention coupling
 * HOW: Holds pointers to both systems, computed effects, and state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    neural_substrate_t* substrate;             /* Neural substrate system */
    nimcp_attention_system_t* attention;       /* Attention system */
    attention_substrate_effects_t effects;     /* Current effects */
    attention_substrate_config_t config;       /* Configuration */
    attention_substrate_stats_t stats;         /* Statistics */} attention_substrate_bridge_t;

/**
 * Get default configuration
 * WHAT: Provides sensible default bridge settings
 * WHY: Ensures safe initialization with validated parameters
 * HOW: Sets all modulations enabled, moderate sensitivities
 *
 * @param config Output configuration structure
 */
void attention_substrate_default_config(attention_substrate_config_t* config);

/**
 * Create attention substrate bridge
 * WHAT: Allocates and initializes bridge between substrate and attention
 * WHY: Establishes bidirectional integration layer
 * HOW: Validates inputs, allocates structure, initializes mutex
 *
 * @param config Bridge configuration
 * @param substrate Neural substrate system (must be non-NULL)
 * @param attention Attention system (must be non-NULL)
 * @return Bridge pointer on success, NULL on failure
 *
 * BIOLOGICAL: Models the metabolic dependency of attention networks
 */
attention_substrate_bridge_t* attention_substrate_bridge_create(
    const attention_substrate_config_t* config,
    neural_substrate_t* substrate,
    nimcp_attention_system_t* attention
);

/**
 * Destroy attention substrate bridge
 * WHAT: Cleans up bridge resources
 * WHY: Prevents memory leaks, disconnects systems
 * HOW: Disconnects bio-async, destroys mutex, frees structure
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void attention_substrate_bridge_destroy(attention_substrate_bridge_t* bridge);

/**
 * Connect to bio-async router
 * WHAT: Registers bridge as bio-async module
 * WHY: Enables inter-module messaging for coordination
 * HOW: Registers with BIO_MODULE_SUBSTRATE_ATTENTION ID
 *
 * @param bridge Attention substrate bridge
 * @return 0 on success, negative on error
 */
int attention_substrate_connect_bio_async(attention_substrate_bridge_t* bridge);

/**
 * Disconnect from bio-async router
 * WHAT: Unregisters bridge from bio-async system
 * WHY: Clean shutdown of messaging
 * HOW: Unregisters module context
 *
 * @param bridge Attention substrate bridge
 * @return 0 on success, negative on error
 */
int attention_substrate_disconnect_bio_async(attention_substrate_bridge_t* bridge);

/**
 * Check bio-async connection status
 * WHAT: Queries whether bridge is connected to bio-async
 * WHY: Allows conditional messaging logic
 * HOW: Returns bio_async_enabled flag
 *
 * @param bridge Attention substrate bridge
 * @return true if connected, false otherwise
 */
bool attention_substrate_is_bio_async_connected(const attention_substrate_bridge_t* bridge);

/**
 * Update attention substrate effects
 * WHAT: Recomputes substrate effects on attention based on current state
 * WHY: Keeps attention modulation synchronized with substrate changes
 * HOW: Reads substrate ATP/temperature, computes effects, updates attention
 *
 * Flow:
 * 1. Read substrate state (ATP, temperature, metabolic stress)
 * 2. Compute focus capacity (ATP-dependent)
 * 3. Compute shifting efficiency (temperature-sensitive)
 * 4. Compute filter strength (metabolic stress impact)
 * 5. Compute vigilance factor (sustained attention)
 * 6. Detect impairment (any factor < 0.3)
 * 7. Update statistics
 * 8. Apply modulation to attention system
 *
 * @param bridge Attention substrate bridge
 * @return 0 on success, negative on error
 *
 * BIOLOGICAL: Models metabolic fatigue effects on attention:
 * - ATP depletion → reduced focus capacity
 * - Hyperthermia → impaired attention switching
 * - Metabolic stress → weakened distractor filtering
 * - Sustained vigilance → vigilance decrement
 */
int attention_substrate_update(attention_substrate_bridge_t* bridge);

/**
 * Get current focus capacity
 * WHAT: Returns computed focus maintenance capability
 * WHY: Allows querying attention sustainability
 * HOW: Returns effects.focus_capacity [0-1]
 *
 * @param bridge Attention substrate bridge
 * @return Focus capacity [0-1], or -1 on error
 *
 * BIOLOGICAL: High-ATP states support sustained focus, low-ATP impairs it
 */
float attention_substrate_get_focus_capacity(attention_substrate_bridge_t* bridge);

/**
 * Get current shifting efficiency
 * WHAT: Returns computed attention switching speed
 * WHY: Indicates ability to reorient attention
 * HOW: Returns effects.shifting_efficiency [0-1]
 *
 * @param bridge Attention substrate bridge
 * @return Shifting efficiency [0-1], or -1 on error
 *
 * BIOLOGICAL: Temperature-sensitive, fever impairs rapid attention shifts
 */
float attention_substrate_get_shifting_efficiency(attention_substrate_bridge_t* bridge);

/**
 * Get current filter strength
 * WHAT: Returns computed distractor suppression capability
 * WHY: Indicates selective attention quality
 * HOW: Returns effects.filter_strength [0-1]
 *
 * @param bridge Attention substrate bridge
 * @return Filter strength [0-1], or -1 on error
 *
 * BIOLOGICAL: Metabolic stress reduces top-down filtering, increases distractibility
 */
float attention_substrate_get_filter_strength(attention_substrate_bridge_t* bridge);

/**
 * Get current vigilance factor
 * WHAT: Returns computed sustained attention capability
 * WHY: Indicates resistance to vigilance decrement
 * HOW: Returns effects.vigilance_factor [0-1]
 *
 * @param bridge Attention substrate bridge
 * @return Vigilance factor [0-1], or -1 on error
 *
 * BIOLOGICAL: Fatigue and metabolic depletion cause vigilance decrement
 */
float attention_substrate_get_vigilance(attention_substrate_bridge_t* bridge);

/**
 * Get all attention substrate effects
 * WHAT: Returns complete effects structure
 * WHY: Allows bulk querying of all attention modulations
 * HOW: Copies effects structure to output parameter
 *
 * @param bridge Attention substrate bridge
 * @param effects Output effects structure
 * @return 0 on success, negative on error
 */
int attention_substrate_get_effects(
    const attention_substrate_bridge_t* bridge,
    attention_substrate_effects_t* effects
);

/**
 * Check if attention is impaired
 * WHAT: Returns overall impairment status
 * WHY: Quick check for critical attention degradation
 * HOW: Returns true if any effect < 0.3 (critical threshold)
 *
 * @param bridge Attention substrate bridge
 * @return true if impaired, false otherwise
 *
 * BIOLOGICAL: Severe ATP depletion or hyperthermia critically impairs attention
 */
bool attention_substrate_is_impaired(attention_substrate_bridge_t* bridge);

/**
 * Get bridge statistics
 * WHAT: Returns accumulated runtime statistics
 * WHY: Monitoring and debugging of bridge behavior
 * HOW: Copies stats structure to output parameter
 *
 * @param bridge Attention substrate bridge
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
int attention_substrate_get_stats(
    const attention_substrate_bridge_t* bridge,
    attention_substrate_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ATTENTION_SUBSTRATE_BRIDGE_H */
