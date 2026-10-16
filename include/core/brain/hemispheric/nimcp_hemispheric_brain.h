//=============================================================================
// nimcp_hemispheric_brain.h - Bilateral Brain with Hemispheric Architecture
//=============================================================================
/**
 * @file nimcp_hemispheric_brain.h
 * @brief Two-hemisphere brain architecture with inter-hemispheric coordination
 *
 * WHAT: Complete bilateral brain with left/right hemispheres and corpus callosum
 * WHY:  Models biological brain lateralization, enables specialized processing
 * HOW:  Wraps two brain_t instances with coordinated updates and communication
 *
 * BIOLOGICAL BASIS:
 * - ~86 billion neurons split between hemispheres (~43B each)
 * - Lateralized processing (language left, spatial right for most people)
 * - Corpus callosum (~200M axons) enables inter-hemispheric transfer
 * - Contralateral motor/sensory mapping (left brain controls right body)
 * - Shared structures: brainstem, cerebellum (partial), thalamus
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                      HEMISPHERIC BRAIN                                   │
 * │                                                                          │
 * │  ┌──────────────────────────┐     ┌──────────────────────────┐         │
 * │  │    LEFT HEMISPHERE       │     │    RIGHT HEMISPHERE      │         │
 * │  │  - Language (Broca's)    │     │  - Spatial processing    │         │
 * │  │  - Logic/sequential      │     │  - Pattern recognition   │         │
 * │  │  - Fine motor (R hand)   │     │  - Emotion processing    │         │
 * │  │  - Local attention       │     │  - Global attention      │         │
 * │  └────────────┬─────────────┘     └─────────────┬────────────┘         │
 * │               │                                  │                       │
 * │               └──────────┬───────────────────────┘                       │
 * │                          │                                               │
 * │               ┌──────────▼──────────┐                                   │
 * │               │   CORPUS CALLOSUM   │                                   │
 * │               │  - Motor channel    │                                   │
 * │               │  - Sensory channel  │                                   │
 * │               │  - Cognitive channel│                                   │
 * │               │  - Emotional channel│                                   │
 * │               └─────────────────────┘                                   │
 * │                                                                          │
 * │  ┌────────────────────────────────────────────────────────────────┐    │
 * │  │                    SHARED STRUCTURES                            │    │
 * │  │  - Thalamus (routing)    - Brainstem (autonomic)               │    │
 * │  │  - Brain Immune System   - Hippocampus (bilateral)             │    │
 * │  └────────────────────────────────────────────────────────────────┘    │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * PROCESSING MODES:
 * - Lateralized: Route to dominant hemisphere for domain
 * - Parallel: Both hemispheres process simultaneously
 * - Competitive: Race, winner provides output
 * - Cooperative: Combine outputs based on specialization
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_HEMISPHERIC_BRAIN_H
#define NIMCP_HEMISPHERIC_BRAIN_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"
#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"
#include "async/nimcp_bio_async.h"
#include "utils/platform/nimcp_platform_tier.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct hemispheric_brain_struct hemispheric_brain_t;
typedef struct brain_immune_system brain_immune_system_t;

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Processing mode for hemispheric operations
 */
typedef enum {
    HEMISPHERIC_MODE_LATERALIZED,    // Route to dominant hemisphere
    HEMISPHERIC_MODE_PARALLEL,       // Both hemispheres simultaneously
    HEMISPHERIC_MODE_COMPETITIVE,    // Race, faster/stronger wins
    HEMISPHERIC_MODE_COOPERATIVE     // Combine weighted outputs
} hemispheric_mode_t;

/**
 * @brief Cooperation strategy for combining outputs
 */
typedef enum {
    COOPERATION_AVERAGE,             // Simple average
    COOPERATION_WEIGHTED,            // By specialization weights
    COOPERATION_DOMINANT,            // Prefer dominant hemisphere
    COOPERATION_ATTENTION_GATED      // Based on attention signals
} cooperation_strategy_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for hemispheric brain
 */
typedef struct {
    // Task identification
    const char* task_name;
    brain_task_t task;

    // Network size
    brain_size_t size;
    uint32_t num_inputs;
    uint32_t num_outputs;

    // Hemisphere configuration
    hemisphere_config_t left_config;
    hemisphere_config_t right_config;

    // Lateralization
    lateralization_profile_t lateralization;

    // Corpus callosum
    callosum_config_t callosum_config;

    // Processing
    hemispheric_mode_t default_mode;
    cooperation_strategy_t cooperation_strategy;

    // Resource management
    platform_tier_t initial_tier;
    bool asymmetric_resources;        // Allow different tiers per hemisphere
    float left_resource_fraction;     // 0.0-1.0 (right = 1.0 - left)

    // Shared structures
    bool enable_shared_thalamus;
    bool enable_shared_immune;

    // Bio-async
    bool enable_bio_async;
} hemispheric_brain_config_t;

/**
 * @brief Statistics for hemispheric brain
 */
typedef struct {
    // Hemisphere activity
    float left_activity;
    float right_activity;
    hemisphere_id_t current_dominant;

    // Processing stats
    uint64_t lateralized_operations;
    uint64_t parallel_operations;
    uint64_t competitive_operations;
    uint64_t cooperative_operations;

    // Competition results
    uint64_t left_wins;
    uint64_t right_wins;

    // Resource usage
    float total_energy;
    float left_energy;
    float right_energy;

    // Timing
    float avg_update_time_ms;
    float avg_inference_time_ms;

    // Callosum stats
    callosum_stats_t callosum_stats;

    // Per-hemisphere stats
    hemisphere_stats_t left_stats;
    hemisphere_stats_t right_stats;
} hemispheric_brain_stats_t;

//=============================================================================
// Main Structure
//=============================================================================

/**
 * @brief Hemispheric brain with bilateral architecture
 *
 * WHAT: Complete brain with two hemispheres and inter-hemispheric bridge
 * WHY:  Biologically-inspired lateralized processing
 * HOW:  Wraps two brain_hemisphere_t with corpus callosum coordination
 */
struct hemispheric_brain_struct {
    // === Hemispheres ===
    brain_hemisphere_t* left;
    brain_hemisphere_t* right;

    // === Inter-Hemispheric Connection ===
    corpus_callosum_t* callosum;

    // === Shared Structures ===
    brain_t* thalamus;                   // Optional shared routing
    brain_immune_system_t* immune;       // Unified immune system

    // === Lateralization ===
    lateralization_profile_t lateralization;
    hemisphere_id_t dominant_hemisphere; // Current dominant (can shift)

    // === Processing Configuration ===
    hemispheric_mode_t default_mode;
    cooperation_strategy_t cooperation_strategy;

    // === Resource Management ===
    bool asymmetric_resources;
    float left_resource_fraction;

    // === State ===
    bool callosum_intact;                // false = split-brain
    bool is_active;
    bool bilateral_mode;                 // Emergency bilateral processing
    uint64_t creation_time;
    uint64_t update_count;

    // === Bio-Async ===
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // === Statistics ===
    hemispheric_brain_stats_t stats;

    // === Thread Safety ===
    void* mutex;                         // nimcp_mutex_t*
};

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default hemispheric brain configuration
 *
 * @return Default configuration (right-handed lateralization)
 */
hemispheric_brain_config_t hemispheric_brain_default_config(void);

/**
 * @brief Create hemispheric brain
 *
 * WHAT: Initialize bilateral brain with hemispheres and callosum
 * WHY:  Entry point for hemispheric processing
 * HOW:  Create hemispheres, callosum, connect systems
 *
 * @param config Configuration (NULL for defaults)
 * @return Hemispheric brain instance or NULL on error
 */
hemispheric_brain_t* hemispheric_brain_create(
    const hemispheric_brain_config_t* config
);

/**
 * @brief Destroy hemispheric brain
 *
 * @param brain Brain to destroy
 */
void hemispheric_brain_destroy(hemispheric_brain_t* brain);

//=============================================================================
// Update and Processing
//=============================================================================

/**
 * @brief Update hemispheric brain state
 *
 * WHAT: Process one simulation timestep
 * WHY:  Advance neural dynamics in both hemispheres
 * HOW:  Update hemispheres, process callosum, sync state
 *
 * @param brain Hemispheric brain
 * @param dt Time delta in seconds
 * @return 0 on success
 */
int hemispheric_brain_update(hemispheric_brain_t* brain, float dt);

/**
 * @brief Process input with lateralized routing
 *
 * WHAT: Route input to appropriate hemisphere based on domain
 * WHY:  Leverage hemisphere specialization
 * HOW:  Check lateralization, send to dominant hemisphere
 *
 * @param brain Hemispheric brain
 * @param input Input array
 * @param input_size Number of inputs
 * @param domain Cognitive domain for routing
 * @param output Output array (pre-allocated)
 * @param output_size Number of outputs
 * @return 0 on success
 */
int hemispheric_brain_process_lateralized(
    hemispheric_brain_t* brain,
    const float* input,
    uint32_t input_size,
    cognitive_domain_t domain,
    float* output,
    uint32_t output_size
);

/**
 * @brief Process input in parallel (both hemispheres)
 *
 * WHAT: Send input to both hemispheres simultaneously
 * WHY:  Compare hemisphere responses, enable cooperation
 * HOW:  Fork input, parallel update, gather outputs
 *
 * @param brain Hemispheric brain
 * @param input Input array
 * @param input_size Number of inputs
 * @param left_output Left hemisphere output
 * @param right_output Right hemisphere output
 * @param output_size Number of outputs
 * @return 0 on success
 */
int hemispheric_brain_process_parallel(
    hemispheric_brain_t* brain,
    const float* input,
    uint32_t input_size,
    float* left_output,
    float* right_output,
    uint32_t output_size
);

/**
 * @brief Process input competitively
 *
 * WHAT: Both hemispheres race, winner provides output
 * WHY:  Model competitive processing (e.g., attention)
 * HOW:  Parallel process, select stronger/faster response
 *
 * @param brain Hemispheric brain
 * @param input Input array
 * @param input_size Number of inputs
 * @param output Winner's output
 * @param output_size Number of outputs
 * @param winner Which hemisphere won
 * @return 0 on success
 */
int hemispheric_brain_process_competitive(
    hemispheric_brain_t* brain,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size,
    hemisphere_id_t* winner
);

/**
 * @brief Process input cooperatively
 *
 * WHAT: Combine outputs from both hemispheres
 * WHY:  Integrate bilateral processing
 * HOW:  Parallel process, combine by strategy
 *
 * @param brain Hemispheric brain
 * @param input Input array
 * @param input_size Number of inputs
 * @param output Combined output
 * @param output_size Number of outputs
 * @return 0 on success
 */
int hemispheric_brain_process_cooperative(
    hemispheric_brain_t* brain,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
);

/**
 * @brief Run inference using default processing mode
 *
 * @param brain Hemispheric brain
 * @param input Input array
 * @param input_size Number of inputs
 * @param output Output array
 * @param output_size Number of outputs
 * @return 0 on success
 */
int hemispheric_brain_infer(
    hemispheric_brain_t* brain,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
);

/**
 * @brief Train hemispheric brain
 *
 * @param brain Hemispheric brain
 * @param input Training input
 * @param target Training target
 * @param size Input/target size
 * @return Loss value
 */
float hemispheric_brain_train(
    hemispheric_brain_t* brain,
    const float* input,
    const float* target,
    uint32_t size
);

//=============================================================================
// Hemisphere Access
//=============================================================================

/**
 * @brief Get left hemisphere
 */
brain_hemisphere_t* hemispheric_brain_get_left(hemispheric_brain_t* brain);

/**
 * @brief Get right hemisphere
 */
brain_hemisphere_t* hemispheric_brain_get_right(hemispheric_brain_t* brain);

/**
 * @brief Get hemisphere by ID
 */
brain_hemisphere_t* hemispheric_brain_get_hemisphere(
    hemispheric_brain_t* brain,
    hemisphere_id_t id
);

/**
 * @brief Get currently dominant hemisphere
 */
brain_hemisphere_t* hemispheric_brain_get_dominant(hemispheric_brain_t* brain);

/**
 * @brief Get underlying brain_t (for legacy API compatibility)
 */
brain_t hemispheric_brain_get_brain(
    hemispheric_brain_t* brain,
    hemisphere_id_t id
);

//=============================================================================
// Lateralization Control
//=============================================================================

/**
 * @brief Get dominant hemisphere for cognitive domain
 *
 * @param brain Hemispheric brain
 * @param domain Cognitive domain
 * @return Dominant hemisphere ID
 */
hemisphere_id_t hemispheric_brain_get_dominant_for(
    const hemispheric_brain_t* brain,
    cognitive_domain_t domain
);

/**
 * @brief Get dominance value for cognitive domain
 *
 * @param brain Hemispheric brain
 * @param domain Cognitive domain
 * @return Dominance 0.0-1.0 (0=right, 1=left)
 */
float hemispheric_brain_get_dominance(
    const hemispheric_brain_t* brain,
    cognitive_domain_t domain
);

/**
 * @brief Shift dominance for cognitive domain (plasticity)
 *
 * @param brain Hemispheric brain
 * @param domain Cognitive domain
 * @param shift Shift amount (+ve = more left, -ve = more right)
 * @return 0 on success
 */
int hemispheric_brain_shift_dominance(
    hemispheric_brain_t* brain,
    cognitive_domain_t domain,
    float shift
);

/**
 * @brief Set lateralization profile
 *
 * @param brain Hemispheric brain
 * @param profile New lateralization profile
 * @return 0 on success
 */
int hemispheric_brain_set_lateralization(
    hemispheric_brain_t* brain,
    const lateralization_profile_t* profile
);

/**
 * @brief Get lateralization profile
 *
 * @param brain Hemispheric brain
 * @param profile Output profile
 * @return 0 on success
 */
int hemispheric_brain_get_lateralization(
    const hemispheric_brain_t* brain,
    lateralization_profile_t* profile
);

//=============================================================================
// Callosum Control
//=============================================================================

/**
 * @brief Get corpus callosum
 */
corpus_callosum_t* hemispheric_brain_get_callosum(hemispheric_brain_t* brain);

/**
 * @brief Disconnect callosum (split-brain)
 *
 * WHAT: Sever inter-hemispheric connection
 * WHY:  Simulate split-brain experiments
 * HOW:  Block callosum message transfer
 *
 * @param brain Hemispheric brain
 * @return 0 on success
 */
int hemispheric_brain_disconnect_callosum(hemispheric_brain_t* brain);

/**
 * @brief Reconnect callosum
 *
 * @param brain Hemispheric brain
 * @return 0 on success
 */
int hemispheric_brain_reconnect_callosum(hemispheric_brain_t* brain);

/**
 * @brief Check if callosum is intact
 *
 * @param brain Hemispheric brain
 * @return true if connected
 */
bool hemispheric_brain_is_callosum_intact(const hemispheric_brain_t* brain);

/**
 * @brief Set callosum bandwidth mode
 *
 * @param brain Hemispheric brain
 * @param mode Bandwidth mode
 * @return 0 on success
 */
int hemispheric_brain_set_callosum_bandwidth(
    hemispheric_brain_t* brain,
    callosum_bandwidth_mode_t mode
);

//=============================================================================
// Resource Management
//=============================================================================

/**
 * @brief Set tier for specific hemisphere
 *
 * @param brain Hemispheric brain
 * @param hemisphere Hemisphere ID
 * @param tier New tier
 * @return 0 on success
 */
int hemispheric_brain_set_tier(
    hemispheric_brain_t* brain,
    hemisphere_id_t hemisphere,
    platform_tier_t tier
);

/**
 * @brief Get tier for specific hemisphere
 *
 * @param brain Hemispheric brain
 * @param hemisphere Hemisphere ID
 * @return Current tier
 */
platform_tier_t hemispheric_brain_get_tier(
    const hemispheric_brain_t* brain,
    hemisphere_id_t hemisphere
);

/**
 * @brief Set asymmetric resource allocation
 *
 * WHAT: Allocate different resources to each hemisphere
 * WHY:  Prioritize one hemisphere for specialized tasks
 * HOW:  Adjust tier and processing allocation
 *
 * @param brain Hemispheric brain
 * @param left_fraction Fraction for left (0.0-1.0)
 * @param rebalance_immediately Apply immediately
 * @return 0 on success
 */
int hemispheric_brain_set_asymmetric_resources(
    hemispheric_brain_t* brain,
    float left_fraction,
    bool rebalance_immediately
);

/**
 * @brief Enable/disable asymmetric resources
 *
 * @param brain Hemispheric brain
 * @param enable true to enable
 * @return 0 on success
 */
int hemispheric_brain_enable_asymmetric_resources(
    hemispheric_brain_t* brain,
    bool enable
);

//=============================================================================
// Processing Mode
//=============================================================================

/**
 * @brief Set default processing mode
 *
 * @param brain Hemispheric brain
 * @param mode Processing mode
 * @return 0 on success
 */
int hemispheric_brain_set_mode(
    hemispheric_brain_t* brain,
    hemispheric_mode_t mode
);

/**
 * @brief Get current processing mode
 *
 * @param brain Hemispheric brain
 * @return Current mode
 */
hemispheric_mode_t hemispheric_brain_get_mode(const hemispheric_brain_t* brain);

/**
 * @brief Set cooperation strategy
 *
 * @param brain Hemispheric brain
 * @param strategy Cooperation strategy
 * @return 0 on success
 */
int hemispheric_brain_set_cooperation_strategy(
    hemispheric_brain_t* brain,
    cooperation_strategy_t strategy
);

//=============================================================================
// State Query
//=============================================================================

/**
 * @brief Get hemispheric brain statistics
 *
 * @param brain Hemispheric brain
 * @param stats Output statistics
 * @return 0 on success
 */
int hemispheric_brain_get_stats(
    const hemispheric_brain_t* brain,
    hemispheric_brain_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param brain Hemispheric brain
 * @return 0 on success
 */
int hemispheric_brain_reset_stats(hemispheric_brain_t* brain);

/**
 * @brief Get total energy consumption
 *
 * @param brain Hemispheric brain
 * @return Total energy across both hemispheres
 */
float hemispheric_brain_get_energy(const hemispheric_brain_t* brain);

/**
 * @brief Check if hemispheric brain is active
 *
 * @param brain Hemispheric brain
 * @return true if active
 */
bool hemispheric_brain_is_active(const hemispheric_brain_t* brain);

/**
 * @brief Enable/disable hemispheric brain
 *
 * @param brain Hemispheric brain
 * @param active true to enable
 * @return 0 on success
 */
int hemispheric_brain_set_active(
    hemispheric_brain_t* brain,
    bool active
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * @param brain Hemispheric brain
 * @return 0 on success
 */
int hemispheric_brain_connect_bio_async(hemispheric_brain_t* brain);

/**
 * @brief Disconnect from bio-async router
 *
 * @param brain Hemispheric brain
 * @return 0 on success
 */
int hemispheric_brain_disconnect_bio_async(hemispheric_brain_t* brain);

/**
 * @brief Check bio-async connection
 *
 * @param brain Hemispheric brain
 * @return true if connected
 */
bool hemispheric_brain_is_bio_async_connected(const hemispheric_brain_t* brain);

//=============================================================================
// Immune System Integration
//=============================================================================

/**
 * @brief Connect brain immune system
 *
 * @param brain Hemispheric brain
 * @param immune Brain immune system
 * @return 0 on success
 */
int hemispheric_brain_connect_immune(
    hemispheric_brain_t* brain,
    brain_immune_system_t* immune
);

/**
 * @brief Get connected immune system
 *
 * @param brain Hemispheric brain
 * @return Immune system or NULL
 */
brain_immune_system_t* hemispheric_brain_get_immune(
    const hemispheric_brain_t* brain
);

//=============================================================================
// Immune Bridge Integration
//=============================================================================

/**
 * @brief Apply lateralization shift from immune modulation
 *
 * WHAT: Shift lateralization weights in response to immune signals
 * WHY:  Inflammation can cause compensatory shifts between hemispheres
 * HOW:  Adjust domain dominance weights based on shift magnitude
 *
 * @param brain Hemispheric brain
 * @param shift Shift magnitude (+ve = toward right, -ve = toward left)
 * @return 0 on success
 */
int hemispheric_brain_apply_lateralization_shift(
    hemispheric_brain_t* brain,
    float shift
);

/**
 * @brief Set bilateral processing mode
 *
 * WHAT: Force both hemispheres to process all domains
 * WHY:  Emergency response when lateralization fails
 * HOW:  Override lateralization routing to use parallel processing
 *
 * @param brain Hemispheric brain
 * @param bilateral true to enable bilateral mode, false for normal
 * @return 0 on success
 */
int hemispheric_brain_set_bilateral_mode(
    hemispheric_brain_t* brain,
    bool bilateral
);

/**
 * @brief Check if bilateral mode is active
 *
 * @param brain Hemispheric brain
 * @return true if bilateral mode is active
 */
bool hemispheric_brain_is_bilateral_mode(const hemispheric_brain_t* brain);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get processing mode name string
 *
 * @param mode Processing mode
 * @return Human-readable name
 */
const char* hemispheric_mode_name(hemispheric_mode_t mode);

/**
 * @brief Get cooperation strategy name string
 *
 * @param strategy Cooperation strategy
 * @return Human-readable name
 */
const char* cooperation_strategy_name(cooperation_strategy_t strategy);

/**
 * @brief Validate hemispheric brain configuration
 *
 * @param config Configuration to validate
 * @return true if valid
 */
bool hemispheric_brain_validate_config(const hemispheric_brain_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_HEMISPHERIC_BRAIN_H
