//=============================================================================
// nimcp_bg_striosome_matrix.h - Striosome-Matrix Compartmentalization
//=============================================================================
/**
 * @file nimcp_bg_striosome_matrix.h
 * @brief Striosome and matrix compartments of the striatum
 *
 * WHAT: Models the two interdigitated compartments of the striatum
 * WHY:  Enables meta-control of dopamine signaling and distinct processing streams
 * HOW:  Striosomes receive limbic/PFC input and project to SNc (dopamine control),
 *       Matrix receives sensorimotor input and projects to GP/SNr (action output)
 *
 * BIOLOGICAL BASIS:
 * - Striatum is organized into striosomes (patches, ~15%) and matrix (~85%)
 * - Striosomes:
 *   - Receive input from limbic cortex, mPFC, amygdala
 *   - Project to dopamine neurons in SNc
 *   - Involved in cost/benefit evaluation, motivation
 *   - Enable "wanting to want" - control dopamine release itself
 * - Matrix:
 *   - Receive input from sensorimotor cortex
 *   - Project to GPi/GPe/SNr (standard BG output)
 *   - Involved in action execution
 *   - Standard direct/indirect pathway
 *
 * FUNCTIONAL IMPLICATIONS:
 * - Striosomes: Value and motivation computation
 * - Matrix: Motor execution and habit expression
 * - Striosome dysfunction: Addiction, compulsive behaviors
 * - Matrix dysfunction: Movement disorders
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_BG_STRIOSOME_MATRIX_H
#define NIMCP_BG_STRIOSOME_MATRIX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BGSM_MAX_STRIOSOMES 32          /**< Maximum striosome compartments */
#define BGSM_MAX_MATRIX_ZONES 64        /**< Maximum matrix zones */
#define BGSM_STRIOSOME_RATIO 0.15f      /**< Striosome percentage (~15%) */
#define BGSM_DEFAULT_SNc_WEIGHT 0.5f    /**< Default striosome→SNc weight */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Compartment type
 */
typedef enum {
    BGSM_COMPARTMENT_STRIOSOME = 0,     /**< Striosome (patch) */
    BGSM_COMPARTMENT_MATRIX             /**< Matrix */
} bgsm_compartment_type_t;

/**
 * @brief Striosome input source
 */
typedef enum {
    BGSM_INPUT_LIMBIC = 0,              /**< Limbic cortex (emotion) */
    BGSM_INPUT_MPFC,                    /**< Medial prefrontal cortex */
    BGSM_INPUT_AMYGDALA,                /**< Amygdala (fear/reward) */
    BGSM_INPUT_HIPPOCAMPUS              /**< Hippocampus (context) */
} bgsm_striosome_input_t;

/**
 * @brief Matrix input source
 */
typedef enum {
    BGSM_INPUT_MOTOR = 0,               /**< Motor cortex */
    BGSM_INPUT_PREMOTOR,                /**< Premotor cortex */
    BGSM_INPUT_SMA,                     /**< Supplementary motor area */
    BGSM_INPUT_SOMATOSENSORY            /**< Somatosensory cortex */
} bgsm_matrix_input_t;

/**
 * @brief Striosome state
 */
typedef enum {
    BGSM_STATE_BASELINE = 0,            /**< Normal baseline activity */
    BGSM_STATE_ACTIVATED,               /**< Increased activity */
    BGSM_STATE_SUPPRESSED,              /**< Decreased activity */
    BGSM_STATE_BURST                    /**< Phasic burst (reward-related) */
} bgsm_striosome_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Individual striosome compartment
 */
typedef struct {
    uint32_t id;                        /**< Striosome identifier */
    float activation;                   /**< Current activation [0-1] */
    float baseline;                     /**< Baseline activation */
    bgsm_striosome_state_t state;       /**< Current state */

    /* Input weights from limbic sources */
    float limbic_weight;                /**< Weight from limbic cortex */
    float mpfc_weight;                  /**< Weight from mPFC */
    float amygdala_weight;              /**< Weight from amygdala */
    float hippocampus_weight;           /**< Weight from hippocampus */

    /* Output to SNc */
    float snc_output;                   /**< Output to dopamine neurons */
    float snc_weight;                   /**< Weight to SNc */

    /* Value-related */
    float value_estimate;               /**< Cost/benefit value estimate */
    float motivation_signal;            /**< Motivation modulation */
} bgsm_striosome_t;

/**
 * @brief Matrix zone (action-related)
 */
typedef struct {
    uint32_t id;                        /**< Zone identifier */
    uint32_t action_id;                 /**< Associated action */
    float d1_activation;                /**< D1 MSN activation */
    float d2_activation;                /**< D2 MSN activation */
    float baseline;                     /**< Baseline activation */

    /* Input weights from motor sources */
    float motor_weight;                 /**< Weight from motor cortex */
    float premotor_weight;              /**< Weight from premotor */
    float sma_weight;                   /**< Weight from SMA */

    /* Output to GP/SNr */
    float gpi_output;                   /**< Output to GPi */
    float gpe_output;                   /**< Output to GPe */
    float snr_output;                   /**< Output to SNr */
} bgsm_matrix_zone_t;

/**
 * @brief Striosome-matrix system configuration
 */
typedef struct {
    uint32_t num_striosomes;            /**< Number of striosomes */
    uint32_t num_matrix_zones;          /**< Number of matrix zones */
    float striosome_ratio;              /**< Striosome proportion [0-1] */
    float snc_modulation_gain;          /**< Striosome→SNc gain */
    float matrix_da_sensitivity;        /**< Matrix dopamine sensitivity */
    bool enable_lateral_inhibition;     /**< Striosome-matrix interaction */
    float boundary_interaction;         /**< Boundary zone interaction strength */
} bgsm_config_t;

/**
 * @brief Striosome-matrix system statistics
 */
typedef struct {
    float avg_striosome_activation;     /**< Average striosome activation */
    float avg_matrix_activation;        /**< Average matrix activation */
    float snc_modulation_strength;      /**< Current SNc modulation */
    uint32_t striosome_bursts;          /**< Striosome burst count */
    float boundary_spillover;           /**< Cross-compartment activity */
} bgsm_stats_t;

/**
 * @brief Striosome-matrix system
 */
typedef struct bgsm_system bgsm_system_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
void bgsm_default_config(bgsm_config_t* config);

/**
 * @brief Create striosome-matrix system
 */
bgsm_system_t* bgsm_create(const bgsm_config_t* config);

/**
 * @brief Destroy striosome-matrix system
 */
void bgsm_destroy(bgsm_system_t* system);

/**
 * @brief Reset to initial state
 */
int bgsm_reset(bgsm_system_t* system);

//=============================================================================
// Striosome Functions
//=============================================================================

/**
 * @brief Set limbic/value input to striosomes
 * @param system Striosome-matrix system
 * @param source Input source type
 * @param values Input values (size num_striosomes)
 * @return 0 on success
 */
int bgsm_set_striosome_input(bgsm_system_t* system,
                              bgsm_striosome_input_t source,
                              const float* values);

/**
 * @brief Process striosomes and compute SNc modulation
 * @return 0 on success
 */
int bgsm_process_striosomes(bgsm_system_t* system);

/**
 * @brief Get striosome output to SNc
 * @param system Striosome-matrix system
 * @return SNc modulation signal [-1, 1]
 */
float bgsm_get_snc_modulation(const bgsm_system_t* system);

/**
 * @brief Get striosome activation
 * @param system Striosome-matrix system
 * @param striosome_id Striosome to query
 * @return Activation level [0-1]
 */
float bgsm_get_striosome_activation(const bgsm_system_t* system,
                                     uint32_t striosome_id);

/**
 * @brief Get motivation signal from striosomes
 * @return Aggregate motivation [0-1]
 */
float bgsm_get_motivation(const bgsm_system_t* system);

//=============================================================================
// Matrix Functions
//=============================================================================

/**
 * @brief Set motor/sensory input to matrix
 * @param system Striosome-matrix system
 * @param source Input source type
 * @param values Input values (size num_matrix_zones)
 * @return 0 on success
 */
int bgsm_set_matrix_input(bgsm_system_t* system,
                           bgsm_matrix_input_t source,
                           const float* values);

/**
 * @brief Set dopamine level for matrix
 * @param system Striosome-matrix system
 * @param dopamine Dopamine level [0-1]
 * @return 0 on success
 */
int bgsm_set_matrix_dopamine(bgsm_system_t* system, float dopamine);

/**
 * @brief Process matrix through D1/D2 pathways
 * @return 0 on success
 */
int bgsm_process_matrix(bgsm_system_t* system);

/**
 * @brief Get D1 pathway output for action
 * @param system Striosome-matrix system
 * @param action_id Action to query
 * @return D1 activation [0-1]
 */
float bgsm_get_d1_output(const bgsm_system_t* system, uint32_t action_id);

/**
 * @brief Get D2 pathway output for action
 * @param system Striosome-matrix system
 * @param action_id Action to query
 * @return D2 activation [0-1]
 */
float bgsm_get_d2_output(const bgsm_system_t* system, uint32_t action_id);

/**
 * @brief Get all D1 outputs
 * @param system Striosome-matrix system
 * @param output Output buffer (size num_actions)
 * @return 0 on success
 */
int bgsm_get_all_d1_output(const bgsm_system_t* system, float* output);

/**
 * @brief Get all D2 outputs
 * @param system Striosome-matrix system
 * @param output Output buffer (size num_actions)
 * @return 0 on success
 */
int bgsm_get_all_d2_output(const bgsm_system_t* system, float* output);

//=============================================================================
// Interaction Functions
//=============================================================================

/**
 * @brief Apply striosome modulation to matrix
 *
 * Striosome activation can gate matrix output, implementing
 * motivational modulation of action.
 *
 * @param system Striosome-matrix system
 * @return 0 on success
 */
int bgsm_apply_striosome_modulation(bgsm_system_t* system);

/**
 * @brief Process boundary interaction
 *
 * Activity can "spill over" at striosome-matrix boundaries.
 *
 * @param system Striosome-matrix system
 * @return 0 on success
 */
int bgsm_process_boundary(bgsm_system_t* system);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Step the system
 * @param system Striosome-matrix system
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int bgsm_step(bgsm_system_t* system, float dt_ms);

/**
 * @brief Full processing cycle
 * @param system Striosome-matrix system
 * @return 0 on success
 */
int bgsm_process(bgsm_system_t* system);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get system statistics
 */
int bgsm_get_stats(const bgsm_system_t* system, bgsm_stats_t* stats);

/**
 * @brief Get compartment type name
 */
const char* bgsm_compartment_name(bgsm_compartment_type_t type);

/**
 * @brief Get striosome state name
 */
const char* bgsm_state_name(bgsm_striosome_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BG_STRIOSOME_MATRIX_H */
