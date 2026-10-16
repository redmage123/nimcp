//=============================================================================
// nimcp_brain_init_basal_ganglia.h - Basal Ganglia Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_basal_ganglia.h
 * @brief Basal ganglia subsystem initialization for brain factory
 *
 * WHAT: Initialization of the enhanced basal ganglia subsystem
 * WHY:  Provides biologically-complete action selection and motor control
 * HOW:  Creates and configures enhanced BG with all enhancement modules
 *
 * BIOLOGICAL RATIONALE:
 * The basal ganglia are a group of subcortical nuclei that play a crucial
 * role in action selection, motor control, reinforcement learning, and
 * habit formation. This module integrates:
 *
 * 1. Core BG Circuit:
 *    - Striatum: D1 (direct) and D2 (indirect) pathway MSNs
 *    - GPe/GPi: Globus pallidus external and internal segments
 *    - STN: Subthalamic nucleus (hyperdirect pathway)
 *    - SNc/SNr: Substantia nigra pars compacta/reticulata
 *
 * 2. Enhancement Modules:
 *    - Beta oscillations (13-30 Hz): Movement suppression, pathological states
 *    - Multi-neuromodulators: DA, 5HT, ACh, NE, adenosine interactions
 *    - Hierarchical RL: Options framework with temporal abstraction
 *    - Model-based planning: World model with MB/MF arbitration
 *    - Nucleus accumbens: Wanting/liking, PIT effects
 *    - Superior colliculus: Saccade generation and orienting
 *    - Striatal interneurons: FSI, TAN, LTS timing and modulation
 *    - Cerebellar coordination: Timing and error sharing
 *    - Outcome devaluation: Goal-directed vs habitual behavior
 *    - Temporal credit assignment: TD-lambda eligibility traces
 *
 * INTEGRATION POINTS:
 * - Executive controller: Goal-directed action selection
 * - Dragonfly system: Motor output for pursuit behaviors
 * - Medulla: Arousal modulation of action thresholds
 * - Emotional system: Reward/aversion signals to NAc
 * - FEP orchestrator: Action as free energy minimization
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_BASAL_GANGLIA_H
#define NIMCP_BRAIN_INIT_BASAL_GANGLIA_H

#include "core/brain/nimcp_brain.h"
#include "core/brain/subcortical/nimcp_basal_ganglia_enhanced.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Get default enhanced BG configuration for brain integration
 *
 * WHAT: Returns a configuration optimized for brain-level integration
 * WHY:  Different from standalone BG config - considers brain context
 * HOW:  Enables appropriate modules based on brain's subsystem state
 *
 * @param brain The brain to configure for
 * @param config Output configuration structure
 */
void nimcp_brain_bg_default_config(brain_t brain, bg_enhanced_config_t* config);

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Initialize basal ganglia subsystem for brain
 *
 * WHAT: Creates and integrates enhanced BG with the brain
 * WHY:  Enables biologically-complete action selection
 * HOW:  Creates BG, connects to brain subsystems, registers with orchestrators
 *
 * INITIALIZATION ORDER:
 * 1. Check prerequisites (executive, emotional system, medulla)
 * 2. Create enhanced BG with brain-appropriate config
 * 3. Connect to brain's neuromodulator system
 * 4. Register with FEP orchestrator if enabled
 * 5. Connect to dragonfly if enabled
 * 6. Set initial state based on brain's arousal level
 *
 * @param brain The brain to initialize BG for
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_basal_ganglia_subsystem(brain_t brain);

/**
 * @brief Destroy basal ganglia subsystem
 *
 * WHAT: Cleans up BG resources
 * WHY:  Proper resource management
 * HOW:  Disconnects from subsystems, destroys BG
 *
 * @param brain The brain containing the BG
 */
void nimcp_brain_bg_destroy(brain_t brain);

//=============================================================================
// Processing
//=============================================================================

/**
 * @brief Step basal ganglia subsystem
 *
 * WHAT: Advances BG simulation by one timestep
 * WHY:  Updates all BG dynamics (oscillations, neuromodulators, etc.)
 * HOW:  Calls bg_enhanced_step with appropriate dt
 *
 * @param brain The brain containing the BG
 * @param dt_ms Timestep in milliseconds
 * @return 0 on success, -1 on error
 */
int nimcp_brain_bg_step(brain_t brain, float dt_ms);

/**
 * @brief Select action using basal ganglia
 *
 * WHAT: Performs action selection through enhanced BG
 * WHY:  Unified interface for brain-level action selection
 * HOW:  Gathers cortical input, runs BG selection, returns action
 *
 * @param brain The brain containing the BG
 * @param cortical_input Cortical input activations
 * @param selected_action Output: selected action ID
 * @return 0 on success, -1 on error
 */
int nimcp_brain_bg_select_action(brain_t brain,
                                  const float* cortical_input,
                                  uint32_t* selected_action);

/**
 * @brief Process reward through basal ganglia
 *
 * WHAT: Updates BG based on received reward
 * WHY:  Enables reinforcement learning in the brain
 * HOW:  Routes reward to BG's reward processing systems
 *
 * @param brain The brain containing the BG
 * @param reward Received reward value
 * @param predicted_reward Expected reward value
 * @return 0 on success, -1 on error
 */
int nimcp_brain_bg_process_reward(brain_t brain,
                                   float reward,
                                   float predicted_reward);

//=============================================================================
// Integration Callbacks
//=============================================================================

/**
 * @brief Callback for emotional system reward signals
 *
 * Called by emotional system when reward/aversion signals change.
 * Routes to nucleus accumbens for wanting/liking updates.
 */
void nimcp_brain_bg_on_emotional_signal(brain_t brain,
                                         float valence,
                                         float arousal);

/**
 * @brief Callback for executive goal updates
 *
 * Called by executive controller when active goal changes.
 * Updates BG's goal-directed behavior mode.
 */
void nimcp_brain_bg_on_goal_change(brain_t brain,
                                    uint32_t goal_id,
                                    bool is_active);

/**
 * @brief Callback for medulla arousal changes
 *
 * Called by medulla when arousal state changes.
 * Modulates BG action thresholds and neuromodulator levels.
 */
void nimcp_brain_bg_on_arousal_change(brain_t brain,
                                       float arousal_level);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get basal ganglia statistics
 */
int nimcp_brain_bg_get_stats(brain_t brain, bg_enhanced_stats_t* stats);

/**
 * @brief Check if basal ganglia is enabled
 */
bool nimcp_brain_bg_is_enabled(brain_t brain);

/**
 * @brief Get current behavior type (goal-directed vs habitual)
 */
bgod_behavior_type_t nimcp_brain_bg_get_behavior_type(brain_t brain);

/**
 * @brief Get motivation level from nucleus accumbens
 */
float nimcp_brain_bg_get_motivation(brain_t brain);

//=============================================================================
// Training Integration API
//=============================================================================

/**
 * @brief Get the training bridge from basal ganglia
 *
 * WHAT: Returns the BG-training integration bridge
 * WHY:  Allows direct access to training plasticity system
 * HOW:  Retrieves bridge from enhanced BG subsystem
 *
 * @param brain The brain containing the BG
 * @return Training bridge pointer, or NULL if not available
 */
bgtr_bridge_t* nimcp_brain_bg_get_training_bridge(brain_t brain);

/**
 * @brief Get training plasticity statistics
 *
 * WHAT: Returns statistics about BG training/learning
 * WHY:  Allows monitoring of reinforcement learning progress
 * HOW:  Queries training bridge for accumulated stats
 *
 * @param brain The brain containing the BG
 * @param stats Output: training statistics
 * @return 0 on success, -1 on error
 */
int nimcp_brain_bg_get_training_stats(brain_t brain, bgtr_bridge_stats_t* stats);

/**
 * @brief Connect external training context to BG training bridge
 *
 * WHAT: Links training module to BG plasticity
 * WHY:  Enables training module control of BG learning
 * HOW:  Registers training context with training bridge
 *
 * @param brain The brain containing the BG
 * @param training Training context to connect
 * @return 0 on success, -1 on error
 */
int nimcp_brain_bg_connect_training_context(brain_t brain,
                                             nimcp_training_context_t* training);

/**
 * @brief Get last reward prediction error
 *
 * WHAT: Returns the most recent RPE from training bridge
 * WHY:  Useful for monitoring learning signals
 * HOW:  Queries training bridge's last_rpe field
 *
 * @param brain The brain containing the BG
 * @return Last RPE value, or 0.0 if unavailable
 */
float nimcp_brain_bg_get_last_rpe(brain_t brain);

/**
 * @brief Get count of active eligibility traces
 *
 * WHAT: Returns number of active eligibility traces
 * WHY:  Indicates how many actions are being tracked for credit assignment
 * HOW:  Queries training bridge's trace count
 *
 * @param brain The brain containing the BG
 * @return Number of active traces
 */
uint32_t nimcp_brain_bg_get_active_traces(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_BASAL_GANGLIA_H */
