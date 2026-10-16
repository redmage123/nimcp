//=============================================================================
// nimcp_fuzzy_bridge.h - Fuzzy Logic System Bridge
//=============================================================================
/**
 * @file nimcp_fuzzy_bridge.h
 * @brief System bridge connecting fuzzy logic to 18 NIMCP subsystems
 *
 * WHAT: Bidirectional integration between fuzzy logic and SNN, STDP,
 *       plasticity, LNN, training, quantum, symbolic logic, immune,
 *       BBB, health, KG, logging, security, cycle, bio-router, ethics, LGSS
 * WHY:  Enables fuzzy membership to modulate neural firing, training rates,
 *       plasticity thresholds, and ethical decision scoring throughout NIMCP
 * HOW:  Subsystem pointers set via defense-in-depth setters; bridge provides
 *       domain-specific conversion functions (spike<->fuzzy, LNN state
 *       classification, training convergence, etc.)
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_FUZZY_BRIDGE_H
#define NIMCP_FUZZY_BRIDGE_H

#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/fuzzy/nimcp_fuzzy_operators.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations (KG Wiring Integration)
//=============================================================================

struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

struct kg_module_wiring;
typedef struct kg_module_wiring kg_module_wiring_t;

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_FUZZY_BRIDGE         0x0283

/** Maximum spike channels for SNN conversion */
#define FUZZY_BRIDGE_MAX_SPIKE_CHANNELS 128

/** Maximum LNN state dimensions */
#define FUZZY_BRIDGE_MAX_LNN_DIM        64

//=============================================================================
// Error Codes
//=============================================================================

#define FUZZY_BRIDGE_ERROR_BASE         28200

#define FUZZY_BRIDGE_ERR_OK             0
#define FUZZY_BRIDGE_ERR_NULL           (FUZZY_BRIDGE_ERROR_BASE + 1)
#define FUZZY_BRIDGE_ERR_NOT_CONNECTED  (FUZZY_BRIDGE_ERROR_BASE + 2)
#define FUZZY_BRIDGE_ERR_SUBSYSTEM      (FUZZY_BRIDGE_ERROR_BASE + 3)
#define FUZZY_BRIDGE_ERR_STATE          (FUZZY_BRIDGE_ERROR_BASE + 4)
#define FUZZY_BRIDGE_ERR_CONVERSION     (FUZZY_BRIDGE_ERROR_BASE + 5)
#define FUZZY_BRIDGE_ERR_INVALID_DIM    (FUZZY_BRIDGE_ERROR_BASE + 6)

//=============================================================================
// Bridge State
//=============================================================================

typedef enum {
    FUZZY_BRIDGE_STATE_IDLE,
    FUZZY_BRIDGE_STATE_INITIALIZING,
    FUZZY_BRIDGE_STATE_ACTIVE,
    FUZZY_BRIDGE_STATE_DEGRADED,
    FUZZY_BRIDGE_STATE_ERROR,
    FUZZY_BRIDGE_STATE_SHUTDOWN,
    FUZZY_BRIDGE_STATE_COUNT
} fuzzy_bridge_state_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    bool enable_snn_integration;
    bool enable_stdp_integration;
    bool enable_plasticity_integration;
    bool enable_lnn_integration;
    bool enable_training_integration;
    bool enable_quantum_integration;
    bool enable_symbolic_integration;
    bool enable_immune_integration;
    bool enable_bbb_validation;
    bool enable_kg_wiring;
    bool enable_logging;
    bool enable_security;
    bool enable_ethics;
    bool enable_lgss;
    float spike_rate_min;               /**< Min spike rate for fuzzy->spike (Hz) */
    float spike_rate_max;               /**< Max spike rate for fuzzy->spike (Hz) */
    float stdp_window_ms;               /**< STDP temporal window for fuzzy weighting */
    float plasticity_rate_min;          /**< Min plasticity rate */
    float plasticity_rate_max;          /**< Max plasticity rate */
    float lnn_time_step;                /**< LNN integration step */
    float training_lr_min;              /**< Min learning rate */
    float training_lr_max;              /**< Max learning rate */
    float convergence_threshold;        /**< Fuzzy convergence membership threshold */
    float inflammation_sensitivity;
    float fatigue_sensitivity;
    uint32_t health_check_interval_ms;
} fuzzy_bridge_config_t;

//=============================================================================
// Statistics
//=============================================================================

typedef struct {
    uint64_t spike_conversions;
    uint64_t stdp_modulations;
    uint64_t plasticity_rate_computations;
    uint64_t lnn_classifications;
    uint64_t training_lr_schedules;
    uint64_t convergence_checks;
    uint64_t quantum_inferences;
    uint64_t symbolic_matches;
    uint64_t immune_checks;
    uint64_t bbb_validations;
    uint64_t ethics_checks;
    uint64_t lgss_checks;
    uint64_t kg_sync_events;
    uint64_t kg_messages_sent;
    uint64_t kg_messages_received;
    uint64_t health_heartbeats;
    float avg_conversion_time_us;
} fuzzy_bridge_stats_t;

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct fuzzy_bridge fuzzy_bridge_t;

//=============================================================================
// Lifecycle
//=============================================================================

fuzzy_bridge_t* fuzzy_bridge_create(const fuzzy_bridge_config_t* config);
void fuzzy_bridge_destroy(fuzzy_bridge_t* bridge);
fuzzy_bridge_config_t fuzzy_bridge_default_config(void);
fuzzy_bridge_state_t fuzzy_bridge_get_state(const fuzzy_bridge_t* bridge);

//=============================================================================
// Subsystem Setters (Defense-in-Depth)
//=============================================================================

int fuzzy_bridge_set_immune(fuzzy_bridge_t* bridge, void* immune);
int fuzzy_bridge_set_bbb(fuzzy_bridge_t* bridge, void* bbb);
int fuzzy_bridge_set_health_agent(fuzzy_bridge_t* bridge, void* health_agent);
int fuzzy_bridge_set_kg_wiring(fuzzy_bridge_t* bridge, kg_wiring_t* kg_wiring);
kg_module_wiring_t* fuzzy_bridge_create_kg_wiring(void);
int fuzzy_bridge_set_kg_registry(fuzzy_bridge_t* bridge, void* kg_registry);
int fuzzy_bridge_set_logger(fuzzy_bridge_t* bridge, void* logger);
int fuzzy_bridge_set_security(fuzzy_bridge_t* bridge, void* security);
int fuzzy_bridge_set_cycle_coordinator(fuzzy_bridge_t* bridge, void* coordinator);
int fuzzy_bridge_set_bio_router(fuzzy_bridge_t* bridge, void* router);
int fuzzy_bridge_set_ethics(fuzzy_bridge_t* bridge, void* ethics);
int fuzzy_bridge_set_lgss(fuzzy_bridge_t* bridge, const void* lgss_kb);
int fuzzy_bridge_set_snn(fuzzy_bridge_t* bridge, void* snn);
int fuzzy_bridge_set_stdp(fuzzy_bridge_t* bridge, void* stdp);
int fuzzy_bridge_set_plasticity(fuzzy_bridge_t* bridge, void* plasticity);
int fuzzy_bridge_set_lnn(fuzzy_bridge_t* bridge, void* lnn);
int fuzzy_bridge_set_training(fuzzy_bridge_t* bridge, void* training);
int fuzzy_bridge_set_quantum(fuzzy_bridge_t* bridge, void* quantum);
int fuzzy_bridge_set_symbolic(fuzzy_bridge_t* bridge, void* symbolic);

//=============================================================================
// SNN Integration: Fuzzy <-> Spike Conversion
//=============================================================================

/**
 * @brief Convert fuzzy membership values to spike population rates
 *
 * Each membership degree maps to a spike rate in [rate_min, rate_max].
 * Used by financial neural bridge for regime->spike encoding.
 *
 * @param bridge        Fuzzy bridge
 * @param memberships   Array of membership degrees [0,1]
 * @param count         Number of values (channels)
 * @param out_rates     Output spike rates (Hz)
 * @return 0 on success
 */
int fuzzy_bridge_to_spike_population(fuzzy_bridge_t* bridge,
                                      const float* memberships, uint32_t count,
                                      float* out_rates);

/**
 * @brief Convert spike rates to fuzzy membership values
 *
 * Inverse: spike rates map to membership degrees via sigmoid normalization.
 *
 * @param bridge      Fuzzy bridge
 * @param rates       Spike rates (Hz)
 * @param count       Number of channels
 * @param out_memberships Output membership degrees [0,1]
 * @return 0 on success
 */
int fuzzy_bridge_from_spike_population(fuzzy_bridge_t* bridge,
                                        const float* rates, uint32_t count,
                                        float* out_memberships);

//=============================================================================
// STDP Integration: Fuzzy Temporal Windows
//=============================================================================

/**
 * @brief Compute fuzzy temporal membership for STDP learning
 *
 * Maps spike time difference to membership in [potentiation, depression]
 * fuzzy sets. dt > 0 -> potentiation, dt < 0 -> depression.
 *
 * @param bridge        Fuzzy bridge
 * @param dt_ms         Spike time difference (post - pre) in ms
 * @param out_potentiation Membership in potentiation set
 * @param out_depression   Membership in depression set
 * @return 0 on success
 */
int fuzzy_bridge_stdp_temporal_membership(fuzzy_bridge_t* bridge, float dt_ms,
                                           float* out_potentiation,
                                           float* out_depression);

//=============================================================================
// Plasticity Integration
//=============================================================================

/**
 * @brief Compute fuzzy plasticity rate from performance and stability
 *
 * @param bridge            Fuzzy bridge
 * @param performance_score Performance metric [0,1]
 * @param stability_score   Stability metric [0,1]
 * @param out_rate          Output plasticity rate
 * @return 0 on success
 */
int fuzzy_bridge_plasticity_rate(fuzzy_bridge_t* bridge,
                                  float performance_score, float stability_score,
                                  float* out_rate);

//=============================================================================
// LNN Integration
//=============================================================================

/**
 * @brief Classify LNN state vector into fuzzy categories
 *
 * @param bridge      Fuzzy bridge
 * @param state       LNN state vector
 * @param state_dim   State dimensionality
 * @param out_value   Output fuzzy value with category memberships
 * @return 0 on success
 */
int fuzzy_bridge_lnn_classify_state(fuzzy_bridge_t* bridge,
                                     const float* state, uint32_t state_dim,
                                     fuzzy_value_t* out_value);

//=============================================================================
// Training Integration
//=============================================================================

/**
 * @brief Schedule learning rate via fuzzy inference
 *
 * Maps (epoch_progress, loss_trend) to learning rate adjustment.
 *
 * @param bridge          Fuzzy bridge
 * @param epoch_progress  Fraction of training complete [0,1]
 * @param loss_trend      Recent loss trend (-1=decreasing, +1=increasing)
 * @param base_lr         Base learning rate
 * @param out_lr          Output adjusted learning rate
 * @return 0 on success
 */
int fuzzy_bridge_training_lr_schedule(fuzzy_bridge_t* bridge,
                                       float epoch_progress, float loss_trend,
                                       float base_lr, float* out_lr);

/**
 * @brief Assess convergence using fuzzy membership
 *
 * @param bridge          Fuzzy bridge
 * @param loss_delta      Change in loss between epochs
 * @param gradient_norm   Gradient norm
 * @param out_convergence Output convergence degree [0,1]
 * @return 0 on success
 */
int fuzzy_bridge_training_convergence(fuzzy_bridge_t* bridge,
                                       float loss_delta, float gradient_norm,
                                       float* out_convergence);

//=============================================================================
// Quantum Integration
//=============================================================================

/**
 * @brief Perform fuzzy inference with quantum-accelerated fuzzification
 *
 * Falls back to classical if quantum backend not available.
 *
 * @param bridge      Fuzzy bridge
 * @param inputs      Crisp inputs
 * @param num_inputs  Input count
 * @param engine      Inference engine to use
 * @param out_result  Output result
 * @return 0 on success
 */
int fuzzy_bridge_quantum_inference(fuzzy_bridge_t* bridge,
                                    const float* inputs, uint32_t num_inputs,
                                    fuzzy_inference_engine_t* engine,
                                    fuzzy_inference_result_t* out_result);

//=============================================================================
// Symbolic Logic Integration
//=============================================================================

/**
 * @brief Match fuzzy action scores against symbolic rules
 *
 * @param bridge          Fuzzy bridge
 * @param action_type     Action type string
 * @param fuzzy_score     Fuzzy score for the action [0,1]
 * @param out_match       Output: true if symbolic rule matched
 * @param out_action_score Adjusted action score after symbolic rules
 * @return 0 on success
 */
int fuzzy_bridge_symbolic_match(fuzzy_bridge_t* bridge,
                                 const char* action_type, float fuzzy_score,
                                 bool* out_match, float* out_action_score);

//=============================================================================
// Health & Monitoring
//=============================================================================

int fuzzy_bridge_heartbeat(fuzzy_bridge_t* bridge, const char* operation, float progress);
int fuzzy_bridge_check_health(const fuzzy_bridge_t* bridge);

//=============================================================================
// Modulation & Statistics
//=============================================================================

int fuzzy_bridge_set_inflammation(fuzzy_bridge_t* bridge, float level);
int fuzzy_bridge_set_fatigue(fuzzy_bridge_t* bridge, float level);
int fuzzy_bridge_get_stats(const fuzzy_bridge_t* bridge, fuzzy_bridge_stats_t* stats);
void fuzzy_bridge_reset_stats(fuzzy_bridge_t* bridge);
const char* fuzzy_bridge_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FUZZY_BRIDGE_H */
