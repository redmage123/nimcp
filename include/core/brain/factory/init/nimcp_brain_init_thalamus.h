//=============================================================================
// nimcp_brain_init_thalamus.h - Thalamus Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_thalamus.h
 * @brief Thalamus subsystem initialization for brain factory
 *
 * WHAT: Initialization of the thalamus as the central relay gateway
 * WHY:  All sensory and motor signals (except olfaction) route through thalamus
 * HOW:  Creates thalamus with all nuclei, configures relay pathways
 *
 * BIOLOGICAL RATIONALE:
 * The thalamus is a bilateral diencephalic structure that acts as the
 * "gateway to cortex." It relays and modulates signals between subcortical
 * structures and cortical areas. This module integrates:
 *
 * 1. First-Order Nuclei (Subcortical -> Cortex):
 *    - LGN (Lateral Geniculate): Visual relay from retina to V1
 *    - MGN (Medial Geniculate): Auditory relay from IC to A1
 *    - VPL (Ventral Posterior Lateral): Body somatosensory to S1
 *    - VPM (Ventral Posterior Medial): Face somatosensory to S1
 *    - VA (Ventral Anterior): Motor from BG to motor cortex
 *    - VL (Ventral Lateral): Motor from cerebellum to motor cortex
 *
 * 2. Higher-Order Nuclei (Cortex <-> Cortex relay):
 *    - Pulvinar: Attention modulation, visual association
 *    - MD (Mediodorsal): Executive functions, prefrontal
 *    - Anterior: Limbic/memory (cingulate, hippocampus)
 *
 * 3. Thalamic Reticular Nucleus (TRN):
 *    - GABAergic shell surrounding thalamus
 *    - Attention-based gating of thalamic channels
 *    - Sleep spindle generation
 *
 * 4. Firing Modes:
 *    - Tonic mode: Awake, attentive - faithful linear relay
 *    - Burst mode: Drowsy, inattentive - burst responses
 *    - Inhibited: TRN suppression - no output
 *
 * INTEGRATION POINTS:
 * - Visual cortex: LGN provides input to V1
 * - Audio cortex: MGN provides input to A1
 * - Basal ganglia: VA receives BG output for motor relay
 * - Cerebellum: VL receives cerebellar output for motor timing
 * - Executive: MD relays to prefrontal cortex
 * - Medulla: Arousal modulates tonic/burst mode switching
 * - Attention: Pulvinar coordinates attentional gating
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_THALAMUS_H
#define NIMCP_BRAIN_INIT_THALAMUS_H

#include "core/brain/nimcp_brain.h"
#include "core/brain/subcortical/nimcp_thalamus.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Get default thalamus configuration for brain integration
 *
 * WHAT: Returns a configuration optimized for brain-level integration
 * WHY:  Different from standalone thalamus config - considers brain context
 * HOW:  Enables appropriate nuclei based on brain's enabled subsystems
 *
 * CONFIGURATION LOGIC:
 * - Visual cortex enabled -> LGN with larger channel count
 * - Audio cortex enabled -> MGN with larger channel count
 * - Basal ganglia enabled -> VA with motor channel mapping
 * - Cerebellum present -> VL with timing coordination
 * - Executive enabled -> MD with prefrontal relay
 * - Attention enabled -> Pulvinar with gating support
 * - TRN always enabled for proper gating
 *
 * @param brain The brain to configure for
 * @param config Output configuration structure
 */
void nimcp_brain_thal_default_config(brain_t brain, thalamus_config_t* config);

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Initialize thalamus subsystem for brain
 *
 * WHAT: Creates and integrates thalamus with the brain
 * WHY:  Enables cortical relay and attentional gating
 * HOW:  Creates thalamus, connects to brain subsystems, registers pathways
 *
 * INITIALIZATION ORDER:
 * 1. Check prerequisites (medulla for arousal, attention for gating)
 * 2. Create thalamus with brain-appropriate config
 * 3. Connect arousal from medulla (controls tonic/burst mode)
 * 4. Register relay pathways with perception cortices
 * 5. Connect to BG-thalamus bridge if BG enabled
 * 6. Initialize TRN gating based on attention system
 * 7. Set initial arousal state from medulla
 *
 * NUCLEI INITIALIZATION:
 * - LGN: Visual channels matching visual cortex inputs
 * - MGN: Audio channels matching audio cortex inputs
 * - VPL/VPM: Somatosensory channels for touch/proprioception
 * - VA: Motor channels matching BG action outputs
 * - VL: Cerebellar timing channels
 * - Pulvinar: Attention coordination channels
 * - MD: Executive relay to prefrontal
 * - Anterior: Limbic relay for memory
 * - TRN: Inhibitory gating for all nuclei
 *
 * @param brain The brain to initialize thalamus for
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_thalamus_subsystem(brain_t brain);

/**
 * @brief Destroy thalamus subsystem
 *
 * WHAT: Cleans up thalamus resources
 * WHY:  Proper resource management
 * HOW:  Disconnects from subsystems, destroys thalamus
 *
 * @param brain The brain containing the thalamus
 */
void nimcp_brain_thal_destroy(brain_t brain);

//=============================================================================
// Processing
//=============================================================================

/**
 * @brief Step thalamus subsystem
 *
 * WHAT: Advances thalamus simulation by one timestep
 * WHY:  Updates firing modes, attention gating, relay dynamics
 * HOW:  Calls thalamus_step with appropriate dt
 *
 * @param brain The brain containing the thalamus
 * @param dt_ms Timestep in milliseconds
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_step(brain_t brain, float dt_ms);

//=============================================================================
// Relay Functions
//=============================================================================

/**
 * @brief Relay visual signal through thalamus (LGN)
 *
 * WHAT: Routes retinal input through LGN to visual cortex
 * WHY:  Biologically-accurate visual pathway with attention gating
 * HOW:  Applies TRN inhibition, attention modulation, firing mode
 *
 * @param brain The brain containing the thalamus
 * @param retinal_input Retinal ganglion cell output
 * @param input_size Size of input
 * @param v1_output Buffer for V1 input
 * @param output_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_relay_visual(brain_t brain,
                                   const float* retinal_input,
                                   uint32_t input_size,
                                   float* v1_output,
                                   uint32_t output_size);

/**
 * @brief Relay auditory signal through thalamus (MGN)
 *
 * WHAT: Routes inferior colliculus input through MGN to auditory cortex
 * WHY:  Biologically-accurate auditory pathway with attention gating
 * HOW:  Applies TRN inhibition, attention modulation, firing mode
 *
 * @param brain The brain containing the thalamus
 * @param ic_input Inferior colliculus output
 * @param input_size Size of input
 * @param a1_output Buffer for A1 input
 * @param output_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_relay_auditory(brain_t brain,
                                     const float* ic_input,
                                     uint32_t input_size,
                                     float* a1_output,
                                     uint32_t output_size);

/**
 * @brief Relay motor signal through thalamus (VA/VL)
 *
 * WHAT: Routes BG/cerebellar output through VA/VL to motor cortex
 * WHY:  Required pathway for voluntary motor control
 * HOW:  BG disinhibition signals through VA, cerebellar timing through VL
 *
 * @param brain The brain containing the thalamus
 * @param bg_input Basal ganglia GPi/SNr disinhibition
 * @param bg_size Size of BG input
 * @param motor_output Buffer for motor cortex input
 * @param output_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_relay_motor(brain_t brain,
                                  const float* bg_input,
                                  uint32_t bg_size,
                                  float* motor_output,
                                  uint32_t output_size);

/**
 * @brief Relay executive signal through thalamus (MD)
 *
 * WHAT: Routes signals to/from prefrontal cortex through MD
 * WHY:  Executive functions require thalamic relay
 * HOW:  Higher-order relay with bidirectional cortical connections
 *
 * @param brain The brain containing the thalamus
 * @param input Input signal (from various sources)
 * @param input_size Size of input
 * @param pfc_output Buffer for prefrontal cortex input
 * @param output_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_relay_executive(brain_t brain,
                                      const float* input,
                                      uint32_t input_size,
                                      float* pfc_output,
                                      uint32_t output_size);

/**
 * @brief Relay through specific nucleus
 *
 * WHAT: Generic relay through any thalamic nucleus
 * WHY:  Allows custom relay paths for special use cases
 * HOW:  Selects nucleus, applies modulation, performs relay
 *
 * @param brain The brain containing the thalamus
 * @param nucleus_type Target nucleus
 * @param input Input signal
 * @param input_size Size of input
 * @param output Buffer for output
 * @param output_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_relay(brain_t brain,
                            thal_nucleus_type_t nucleus_type,
                            const float* input,
                            uint32_t input_size,
                            float* output,
                            uint32_t output_size);

//=============================================================================
// Attention and Gating
//=============================================================================

/**
 * @brief Set global attention level
 *
 * WHAT: Modulates thalamic relay gain based on attention
 * WHY:  Attention gates information flow to cortex
 * HOW:  Higher attention -> stronger relay, lower TRN inhibition
 *
 * @param brain The brain containing the thalamus
 * @param attention Attention level [0-1]
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_set_attention(brain_t brain, float attention);

/**
 * @brief Set attention for specific nucleus
 *
 * @param brain The brain containing the thalamus
 * @param nucleus_type Target nucleus
 * @param attention Attention level [0-1]
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_set_nucleus_attention(brain_t brain,
                                            thal_nucleus_type_t nucleus_type,
                                            float attention);

/**
 * @brief Set arousal level (affects tonic/burst mode)
 *
 * WHAT: Modulates thalamic firing mode based on arousal
 * WHY:  Arousal state affects relay fidelity
 * HOW:  High arousal -> tonic mode, low arousal -> burst mode
 *
 * @param brain The brain containing the thalamus
 * @param arousal Arousal level [0-1]
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_set_arousal(brain_t brain, float arousal);

/**
 * @brief Apply TRN inhibition to specific nucleus
 *
 * @param brain The brain containing the thalamus
 * @param nucleus_type Target nucleus
 * @param inhibition Inhibition strength [0-1]
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_apply_trn_inhibition(brain_t brain,
                                           thal_nucleus_type_t nucleus_type,
                                           float inhibition);

//=============================================================================
// Firing Mode Control
//=============================================================================

/**
 * @brief Set firing mode for nucleus
 *
 * @param brain The brain containing the thalamus
 * @param nucleus_type Target nucleus
 * @param mode Firing mode (tonic, burst, inhibited)
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_set_mode(brain_t brain,
                               thal_nucleus_type_t nucleus_type,
                               thal_firing_mode_t mode);

/**
 * @brief Get firing mode for nucleus
 *
 * @param brain The brain containing the thalamus
 * @param nucleus_type Target nucleus
 * @return Current firing mode
 */
thal_firing_mode_t nimcp_brain_thal_get_mode(brain_t brain,
                                              thal_nucleus_type_t nucleus_type);

/**
 * @brief Trigger burst in nucleus
 *
 * WHAT: Forces burst firing in specified nucleus
 * WHY:  Simulates alerting response or sleep spindle
 * HOW:  Activates T-type Ca2+ channels for burst
 *
 * @param brain The brain containing the thalamus
 * @param nucleus_type Target nucleus
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_trigger_burst(brain_t brain,
                                    thal_nucleus_type_t nucleus_type);

//=============================================================================
// Integration Callbacks
//=============================================================================

/**
 * @brief Callback for medulla arousal changes
 *
 * Called by medulla when arousal state changes.
 * Updates thalamic firing modes based on arousal.
 *
 * @param brain The brain containing the thalamus
 * @param arousal_level New arousal level [0-1]
 */
void nimcp_brain_thal_on_arousal_change(brain_t brain, float arousal_level);

/**
 * @brief Callback for attention system updates
 *
 * Called by attention system when focus changes.
 * Updates TRN gating based on attended channels.
 *
 * @param brain The brain containing the thalamus
 * @param channel_id Attended channel
 * @param attention_weight Attention weight [0-1]
 */
void nimcp_brain_thal_on_attention_change(brain_t brain,
                                           uint32_t channel_id,
                                           float attention_weight);

/**
 * @brief Callback for sleep/wake state changes
 *
 * Called when sleep/wake state transitions.
 * Adjusts firing modes (awake -> tonic, sleep -> burst).
 *
 * @param brain The brain containing the thalamus
 * @param is_awake true if awake, false if sleeping
 */
void nimcp_brain_thal_on_sleep_wake_change(brain_t brain, bool is_awake);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get thalamus statistics
 *
 * @param brain The brain containing the thalamus
 * @param stats Output: statistics structure
 * @return 0 on success, -1 on error
 */
int nimcp_brain_thal_get_stats(brain_t brain, thalamus_stats_t* stats);

/**
 * @brief Check if thalamus is enabled
 *
 * @param brain The brain to check
 * @return true if thalamus is enabled and initialized
 */
bool nimcp_brain_thal_is_enabled(brain_t brain);

/**
 * @brief Get current global attention level
 *
 * @param brain The brain containing the thalamus
 * @return Current attention level [0-1], or -1 on error
 */
float nimcp_brain_thal_get_attention(brain_t brain);

/**
 * @brief Get current arousal level
 *
 * @param brain The brain containing the thalamus
 * @return Current arousal level [0-1], or -1 on error
 */
float nimcp_brain_thal_get_arousal(brain_t brain);

/**
 * @brief Get average firing rate for nucleus
 *
 * @param brain The brain containing the thalamus
 * @param nucleus_type Target nucleus
 * @return Average firing rate (Hz), or -1 on error
 */
float nimcp_brain_thal_get_firing_rate(brain_t brain,
                                        thal_nucleus_type_t nucleus_type);

/**
 * @brief Get fraction of cells in tonic mode
 *
 * @param brain The brain containing the thalamus
 * @param nucleus_type Target nucleus
 * @return Fraction [0-1] in tonic mode, or -1 on error
 */
float nimcp_brain_thal_get_tonic_fraction(brain_t brain,
                                           thal_nucleus_type_t nucleus_type);

//=============================================================================
// Direct Access (for advanced integration)
//=============================================================================

/**
 * @brief Get direct pointer to thalamus (advanced use only)
 *
 * WARNING: Direct manipulation of thalamus bypasses brain-level coordination.
 * Use brain-level API functions when possible.
 *
 * @param brain The brain containing the thalamus
 * @return Pointer to thalamus, or NULL if not enabled
 */
thalamus_t* nimcp_brain_thal_get_handle(brain_t brain);

/**
 * @brief Get pointer to specific nucleus (advanced use only)
 *
 * @param brain The brain containing the thalamus
 * @param nucleus_type Target nucleus
 * @return Pointer to nucleus, or NULL if not available
 */
thal_nucleus_t* nimcp_brain_thal_get_nucleus(brain_t brain,
                                              thal_nucleus_type_t nucleus_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_THALAMUS_H */
