//=============================================================================
// nimcp_thalamus.h - Thalamic Nuclei System
//=============================================================================
/**
 * @file nimcp_thalamus.h
 * @brief Thalamic nuclei implementation for sensory relay and cortical gating
 *
 * WHAT: Biologically-inspired thalamic nuclei for signal relay and attention gating
 * WHY:  Gateway to cortex - all sensory info (except olfaction) routes through thalamus
 * HOW:  Implements LGN, MGN, VPL/VPM, VA/VL, Pulvinar, and MD nuclei with firing modes
 *
 * BIOLOGICAL BASIS:
 * - The thalamus is a bilateral diencephalic structure that relays signals to cortex
 * - Two major cell types: relay cells (thalamocortical) and inhibitory interneurons
 * - Two firing modes:
 *   1. Tonic mode: Faithful relay, associated with awake attention
 *   2. Burst mode: Low-frequency bursts, associated with sleep or reduced attention
 * - Thalamic Reticular Nucleus (TRN) provides inhibitory gating
 * - First-order nuclei: Receive subcortical input (sensory)
 * - Higher-order nuclei: Receive cortical input (cortico-cortical relay)
 *
 * NUCLEI IMPLEMENTED:
 * - LGN (Lateral Geniculate): Visual relay from retina to V1
 * - MGN (Medial Geniculate): Auditory relay from inferior colliculus to A1
 * - VPL/VPM (Ventral Posterior): Somatosensory relay to S1
 * - VA/VL (Ventral Anterior/Lateral): Motor relay from BG/cerebellum to motor cortex
 * - Pulvinar: Attention modulation, visual attention gating
 * - MD (Mediodorsal): Prefrontal relay for executive functions
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#ifndef NIMCP_THALAMUS_H
#define NIMCP_THALAMUS_H

#include <stdint.h>
#include <stdbool.h>

#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define THAL_MAX_CHANNELS 256           /**< Maximum channels per nucleus */
#define THAL_DEFAULT_NEURONS 64         /**< Default neurons per nucleus */
#define THAL_BURST_THRESHOLD 0.3f       /**< Threshold for burst mode transition */
#define THAL_ATTENTION_BASELINE 0.5f    /**< Baseline attention level */
#define THAL_TRN_INHIBITION_STRENGTH 0.7f /**< TRN inhibition strength */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Thalamic nucleus type
 */
typedef enum {
    THAL_NUCLEUS_LGN = 0,       /**< Lateral Geniculate - visual */
    THAL_NUCLEUS_MGN,           /**< Medial Geniculate - auditory */
    THAL_NUCLEUS_VPL,           /**< Ventral Posterior Lateral - somatosensory (body) */
    THAL_NUCLEUS_VPM,           /**< Ventral Posterior Medial - somatosensory (face) */
    THAL_NUCLEUS_VA,            /**< Ventral Anterior - motor (BG input) */
    THAL_NUCLEUS_VL,            /**< Ventral Lateral - motor (cerebellar input) */
    THAL_NUCLEUS_PULVINAR,      /**< Pulvinar - attention, visual association */
    THAL_NUCLEUS_MD,            /**< Mediodorsal - prefrontal, executive */
    THAL_NUCLEUS_ANTERIOR,      /**< Anterior - limbic, memory */
    THAL_NUCLEUS_TRN,           /**< Thalamic Reticular Nucleus - inhibitory gating */
    THAL_NUCLEUS_COUNT
} thal_nucleus_type_t;

/**
 * @brief Thalamic firing mode
 *
 * BIOLOGICAL BASIS:
 * - Tonic mode: T-type Ca2+ channels inactivated, faithful linear relay
 * - Burst mode: T-type Ca2+ channels activated, 2-7 spike bursts, occurs during
 *   drowsiness or when attention is withdrawn
 */
typedef enum {
    THAL_MODE_TONIC = 0,        /**< Awake, attentive - linear relay */
    THAL_MODE_BURST,            /**< Drowsy, inattentive - burst responses */
    THAL_MODE_INHIBITED         /**< TRN suppression - no output */
} thal_firing_mode_t;

/**
 * @brief Relay order (first-order vs higher-order)
 */
typedef enum {
    THAL_ORDER_FIRST = 0,       /**< First-order: subcortical input (LGN, MGN, VPL) */
    THAL_ORDER_HIGHER           /**< Higher-order: cortical input (Pulvinar, MD) */
} thal_relay_order_t;

/**
 * @brief Input source type
 */
typedef enum {
    THAL_INPUT_SUBCORTICAL = 0, /**< From subcortical structures (retina, IC, etc.) */
    THAL_INPUT_CORTICAL,        /**< From cortex (layer 5/6) */
    THAL_INPUT_BASAL_GANGLIA,   /**< From basal ganglia (GPi/SNr) */
    THAL_INPUT_CEREBELLUM,      /**< From deep cerebellar nuclei */
    THAL_INPUT_BRAINSTEM,       /**< From brainstem nuclei */
    THAL_INPUT_TRN              /**< From TRN (inhibitory) */
} thal_input_source_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Single thalamic relay cell
 */
typedef struct {
    uint32_t cell_id;               /**< Unique cell identifier */
    float membrane_potential;       /**< Membrane potential (mV) */
    float firing_rate;              /**< Current firing rate (Hz) */
    thal_firing_mode_t mode;        /**< Current firing mode */
    float t_channel_state;          /**< T-type Ca2+ channel state [0-1] */
    float refractory_time;          /**< Refractory period remaining (ms) */
    bool is_bursting;               /**< Currently in burst */
    uint32_t burst_spike_count;     /**< Spikes in current burst */
} thal_relay_cell_t;

/**
 * @brief Individual thalamic nucleus configuration
 */
typedef struct {
    thal_nucleus_type_t type;       /**< Nucleus type */
    uint32_t num_neurons;           /**< Number of relay neurons */
    uint32_t num_channels;          /**< Input/output channels */
    thal_relay_order_t order;       /**< First or higher order */
    float burst_threshold;          /**< Threshold for burst mode */
    float attention_weight;         /**< Baseline attention modulation */
    float trn_inhibition;           /**< TRN inhibition strength */
    bool enable_adaptation;         /**< Enable firing rate adaptation */
} thal_nucleus_config_t;

/**
 * @brief Individual thalamic nucleus
 */
typedef struct {
    thal_nucleus_type_t type;           /**< Nucleus type */
    thal_relay_order_t order;           /**< Relay order */

    /* Neurons */
    thal_relay_cell_t* cells;           /**< Relay cells */
    uint32_t num_cells;                 /**< Number of cells */

    /* I/O channels */
    uint32_t num_input_channels;        /**< Input channel count */
    uint32_t num_output_channels;       /**< Output channel count */
    float* input_buffer;                /**< Current inputs */
    float* output_buffer;               /**< Current outputs */

    /* Firing mode */
    thal_firing_mode_t dominant_mode;   /**< Dominant firing mode */
    float tonic_fraction;               /**< Fraction of cells in tonic mode */

    /* Attention modulation */
    float attention_level;              /**< Current attention [0-1] */
    float* channel_attention;           /**< Per-channel attention weights */

    /* TRN inhibition */
    float trn_inhibition;               /**< TRN inhibition strength */
    float* channel_inhibition;          /**< Per-channel TRN inhibition */

    /* Activity */
    float avg_firing_rate;              /**< Average firing rate (Hz) */
    float output_gain;                  /**< Output scaling factor */

    /* Configuration */
    thal_nucleus_config_t config;       /**< Configuration */

    /* Thread safety */
    nimcp_mutex_t* mutex;               /**< Mutex for thread safety */
} thal_nucleus_t;

/**
 * @brief Thalamic Reticular Nucleus (TRN)
 *
 * BIOLOGICAL BASIS:
 * - Shell of GABAergic neurons surrounding thalamus
 * - Receives collaterals from both thalamocortical and corticothalamic fibers
 * - Provides inhibitory feedback to thalamic nuclei
 * - Implements attentional gating and sleep spindles
 */
typedef struct {
    float* inhibition_map;              /**< Inhibition per thalamic channel */
    uint32_t num_channels;              /**< Number of channels */
    float* cortical_drive;              /**< Cortical attention input */
    float* collateral_input;            /**< Thalamocortical collaterals */
    float global_inhibition;            /**< Global inhibition level */
    float attention_gain;               /**< Attention modulation gain */
    bool is_active;                     /**< TRN processing active */
    nimcp_mutex_t* mutex;               /**< Thread safety */
} thalamic_reticular_nucleus_t;

/**
 * @brief Thalamus configuration
 */
typedef struct {
    uint32_t neurons_per_nucleus;       /**< Default neurons per nucleus */
    uint32_t channels_per_nucleus;      /**< Default channels per nucleus */
    float attention_baseline;           /**< Baseline attention level */
    float burst_threshold;              /**< Global burst threshold */
    float trn_strength;                 /**< TRN inhibition strength */
    bool enable_trn;                    /**< Enable TRN gating */
    bool enable_mode_switching;         /**< Enable tonic/burst switching */
    bool enable_attention_gating;       /**< Enable attention modulation */

    /* Individual nucleus configs (optional overrides) */
    thal_nucleus_config_t lgn_config;
    thal_nucleus_config_t mgn_config;
    thal_nucleus_config_t vpl_config;
    thal_nucleus_config_t vpm_config;
    thal_nucleus_config_t va_config;
    thal_nucleus_config_t vl_config;
    thal_nucleus_config_t pulvinar_config;
    thal_nucleus_config_t md_config;
} thalamus_config_t;

/**
 * @brief Thalamus statistics
 */
typedef struct {
    uint64_t total_signals_relayed;     /**< Total signals processed */
    uint64_t signals_per_nucleus[THAL_NUCLEUS_COUNT]; /**< Per-nucleus counts */
    float avg_relay_latency_ms;         /**< Average relay latency */
    float tonic_mode_fraction;          /**< Fraction of time in tonic mode */
    float avg_attention_level;          /**< Average attention level */
    float avg_trn_inhibition;           /**< Average TRN inhibition */
    uint32_t burst_count;               /**< Number of burst events */
} thalamus_stats_t;

/**
 * @brief Main thalamus system
 */
typedef struct {
    /* Individual nuclei */
    thal_nucleus_t* lgn;                /**< Lateral Geniculate (visual) */
    thal_nucleus_t* mgn;                /**< Medial Geniculate (auditory) */
    thal_nucleus_t* vpl;                /**< Ventral Posterior Lateral (body somatosensory) */
    thal_nucleus_t* vpm;                /**< Ventral Posterior Medial (face somatosensory) */
    thal_nucleus_t* va;                 /**< Ventral Anterior (BG → motor) */
    thal_nucleus_t* vl;                 /**< Ventral Lateral (cerebellar → motor) */
    thal_nucleus_t* pulvinar;           /**< Pulvinar (attention, visual assoc) */
    thal_nucleus_t* md;                 /**< Mediodorsal (prefrontal) */

    /* TRN for gating */
    thalamic_reticular_nucleus_t* trn;  /**< Thalamic Reticular Nucleus */

    /* Global state */
    float global_arousal;               /**< Global arousal/vigilance [0-1] */
    float global_attention;             /**< Global attention level [0-1] */
    thal_firing_mode_t dominant_mode;   /**< Dominant firing mode across nuclei */

    /* Configuration */
    thalamus_config_t config;           /**< Configuration */

    /* Statistics */
    thalamus_stats_t stats;             /**< Runtime statistics */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;       /**< Bio-async context */
    bool bio_async_enabled;             /**< Bio-async connected */

    /* Thread safety */
    nimcp_mutex_t* mutex;               /**< Mutex for thread safety */
} thalamus_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize default nucleus configuration
 * @param config Configuration to initialize
 * @param type Nucleus type
 */
void thal_nucleus_default_config(thal_nucleus_config_t* config, thal_nucleus_type_t type);

/**
 * @brief Initialize default thalamus configuration
 * @param config Configuration to initialize
 */
void thalamus_default_config(thalamus_config_t* config);

/**
 * @brief Create individual nucleus
 * @param config Nucleus configuration
 * @return New nucleus or NULL on failure
 */
thal_nucleus_t* thal_nucleus_create(const thal_nucleus_config_t* config);

/**
 * @brief Destroy individual nucleus
 * @param nucleus Nucleus to destroy
 */
void thal_nucleus_destroy(thal_nucleus_t* nucleus);

/**
 * @brief Create thalamus system
 * @param config Configuration (NULL for defaults)
 * @return New thalamus instance or NULL on failure
 */
thalamus_t* thalamus_create(const thalamus_config_t* config);

/**
 * @brief Destroy thalamus system
 * @param thal Thalamus to destroy
 */
void thalamus_destroy(thalamus_t* thal);

/**
 * @brief Reset thalamus to initial state
 * @param thal Thalamus to reset
 * @return 0 on success, negative on error
 */
int thalamus_reset(thalamus_t* thal);

//=============================================================================
// Signal Relay Functions
//=============================================================================

/**
 * @brief Relay signal through specific nucleus
 *
 * Applies attention gating, TRN inhibition, and firing mode modulation
 *
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @param input Input signal array
 * @param input_size Input size
 * @param output Output buffer
 * @param output_size Output buffer size
 * @return 0 on success, negative on error
 */
int thalamus_relay(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
);

/**
 * @brief Relay visual signal through LGN
 * @param thal Thalamus system
 * @param retinal_input Input from retina
 * @param input_size Input size
 * @param v1_output Output to V1
 * @param output_size Output size
 * @return 0 on success, negative on error
 */
int thalamus_relay_visual(
    thalamus_t* thal,
    const float* retinal_input,
    uint32_t input_size,
    float* v1_output,
    uint32_t output_size
);

/**
 * @brief Relay auditory signal through MGN
 * @param thal Thalamus system
 * @param ic_input Input from inferior colliculus
 * @param input_size Input size
 * @param a1_output Output to A1
 * @param output_size Output size
 * @return 0 on success, negative on error
 */
int thalamus_relay_auditory(
    thalamus_t* thal,
    const float* ic_input,
    uint32_t input_size,
    float* a1_output,
    uint32_t output_size
);

/**
 * @brief Relay motor signal through VA/VL
 *
 * Routes basal ganglia/cerebellar input to motor cortex
 *
 * @param thal Thalamus system
 * @param bg_input Input from basal ganglia (GPi disinhibition)
 * @param bg_size BG input size
 * @param motor_output Output to motor cortex
 * @param output_size Output size
 * @return 0 on success, negative on error
 */
int thalamus_relay_motor(
    thalamus_t* thal,
    const float* bg_input,
    uint32_t bg_size,
    float* motor_output,
    uint32_t output_size
);

/**
 * @brief Relay executive signal through MD
 * @param thal Thalamus system
 * @param input Input signal
 * @param input_size Input size
 * @param pfc_output Output to prefrontal cortex
 * @param output_size Output size
 * @return 0 on success, negative on error
 */
int thalamus_relay_executive(
    thalamus_t* thal,
    const float* input,
    uint32_t input_size,
    float* pfc_output,
    uint32_t output_size
);

/**
 * @brief Get nucleus output
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @param output Output buffer
 * @param size Buffer size
 * @return Actual output size, negative on error
 */
int thalamus_get_output(
    const thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    float* output,
    uint32_t size
);

//=============================================================================
// Attention and Gating Functions
//=============================================================================

/**
 * @brief Set attention level for nucleus
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @param attention Attention level [0-1]
 * @return 0 on success, negative on error
 */
int thalamus_set_attention(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    float attention
);

/**
 * @brief Set per-channel attention
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @param channel Channel index
 * @param attention Attention level [0-1]
 * @return 0 on success, negative on error
 */
int thalamus_set_channel_attention(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    uint32_t channel,
    float attention
);

/**
 * @brief Set global arousal level
 *
 * High arousal → tonic mode (faithful relay)
 * Low arousal → burst mode (sleep, drowsiness)
 *
 * @param thal Thalamus system
 * @param arousal Arousal level [0-1]
 * @return 0 on success, negative on error
 */
int thalamus_set_arousal(thalamus_t* thal, float arousal);

/**
 * @brief Get current attention level for nucleus
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @return Attention level [0-1], or -1 on error
 */
float thalamus_get_attention(const thalamus_t* thal, thal_nucleus_type_t nucleus_type);

/**
 * @brief Apply TRN inhibition to nucleus
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @param inhibition Inhibition strength [0-1]
 * @return 0 on success, negative on error
 */
int thalamus_apply_trn_inhibition(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    float inhibition
);

/**
 * @brief Apply per-channel TRN inhibition
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @param channel Channel index
 * @param inhibition Inhibition strength [0-1]
 * @return 0 on success, negative on error
 */
int thalamus_apply_channel_inhibition(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    uint32_t channel,
    float inhibition
);

//=============================================================================
// Firing Mode Functions
//=============================================================================

/**
 * @brief Set firing mode for nucleus
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @param mode Firing mode
 * @return 0 on success, negative on error
 */
int thalamus_set_mode(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    thal_firing_mode_t mode
);

/**
 * @brief Get firing mode for nucleus
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @return Current firing mode
 */
thal_firing_mode_t thalamus_get_mode(
    const thalamus_t* thal,
    thal_nucleus_type_t nucleus_type
);

/**
 * @brief Trigger burst in nucleus
 *
 * Simulates T-type Ca2+ channel activation for burst response
 *
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @return 0 on success, negative on error
 */
int thalamus_trigger_burst(thalamus_t* thal, thal_nucleus_type_t nucleus_type);

/**
 * @brief Get fraction of cells in tonic mode
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @return Fraction [0-1] of cells in tonic mode
 */
float thalamus_get_tonic_fraction(
    const thalamus_t* thal,
    thal_nucleus_type_t nucleus_type
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Step thalamus simulation
 * @param thal Thalamus system
 * @param dt Time step in milliseconds
 * @return 0 on success, negative on error
 */
int thalamus_step(thalamus_t* thal, float dt);

/**
 * @brief Step individual nucleus
 * @param nucleus Nucleus to step
 * @param dt Time step in milliseconds
 * @return 0 on success, negative on error
 */
int thal_nucleus_step(thal_nucleus_t* nucleus, float dt);

/**
 * @brief Update TRN gating
 * @param thal Thalamus system
 * @return 0 on success, negative on error
 */
int thalamus_update_trn(thalamus_t* thal);

/**
 * @brief Process input for nucleus
 * @param nucleus Target nucleus
 * @param input Input signal
 * @param size Input size
 * @return 0 on success, negative on error
 */
int thal_nucleus_process_input(
    thal_nucleus_t* nucleus,
    const float* input,
    uint32_t size
);

//=============================================================================
// Integration Functions
//=============================================================================

/**
 * @brief Connect to basal ganglia for motor relay
 * @param thal Thalamus system
 * @param bg_output Basal ganglia thalamic output
 * @param size Output size
 * @return 0 on success, negative on error
 */
int thalamus_connect_basal_ganglia(
    thalamus_t* thal,
    const float* bg_output,
    uint32_t size
);

/**
 * @brief Receive pulvinar attention signal
 *
 * Pulvinar coordinates visual attention across cortex
 *
 * @param thal Thalamus system
 * @param attention_signal Attention weights
 * @param size Signal size
 * @return 0 on success, negative on error
 */
int thalamus_pulvinar_attention(
    thalamus_t* thal,
    const float* attention_signal,
    uint32_t size
);

/**
 * @brief Get nucleus pointer by type
 * @param thal Thalamus system
 * @param type Nucleus type
 * @return Nucleus pointer or NULL
 */
thal_nucleus_t* thalamus_get_nucleus(thalamus_t* thal, thal_nucleus_type_t type);

/**
 * @brief Get nucleus pointer (const)
 * @param thal Thalamus system
 * @param type Nucleus type
 * @return Nucleus pointer or NULL
 */
const thal_nucleus_t* thalamus_get_nucleus_const(
    const thalamus_t* thal,
    thal_nucleus_type_t type
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 * @param thal Thalamus system
 * @return 0 on success, negative on error
 */
int thalamus_connect_bio_async(thalamus_t* thal);

/**
 * @brief Disconnect from bio-async router
 * @param thal Thalamus system
 * @return 0 on success, negative on error
 */
int thalamus_disconnect_bio_async(thalamus_t* thal);

/**
 * @brief Check if connected to bio-async
 * @param thal Thalamus system
 * @return true if connected
 */
bool thalamus_is_bio_async_connected(const thalamus_t* thal);

//=============================================================================
// Statistics and Debugging
//=============================================================================

/**
 * @brief Get thalamus statistics
 * @param thal Thalamus system
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int thalamus_get_stats(const thalamus_t* thal, thalamus_stats_t* stats);

/**
 * @brief Get nucleus type name
 * @param type Nucleus type
 * @return Nucleus name string
 */
const char* thal_nucleus_name(thal_nucleus_type_t type);

/**
 * @brief Get firing mode name
 * @param mode Firing mode
 * @return Mode name string
 */
const char* thal_mode_name(thal_firing_mode_t mode);

/**
 * @brief Get average firing rate for nucleus
 * @param thal Thalamus system
 * @param nucleus_type Target nucleus
 * @return Average firing rate (Hz)
 */
float thalamus_get_firing_rate(
    const thalamus_t* thal,
    thal_nucleus_type_t nucleus_type
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THALAMUS_H */
