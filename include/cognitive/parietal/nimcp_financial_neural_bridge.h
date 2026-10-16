//=============================================================================
// nimcp_financial_neural_bridge.h - Financial Neural Integration Bridge
//=============================================================================
/**
 * @file nimcp_financial_neural_bridge.h
 * @brief SNN spike encoding, STDP reward learning, LNN prediction,
 *        plasticity adaptation, quantum optimization for financial data
 *
 * WHAT: Bridges financial market data to neural computation substrates
 *       (SNN, STDP, LNN, plasticity) for learning-based prediction
 *       and adaptive risk management.
 *
 * WHY:  Traditional financial models use fixed parameters. Neural
 *       integration enables adaptive parameter tuning through:
 *       - Spike-coded market event representation (SNN)
 *       - Reward-modulated learning from trade outcomes (STDP)
 *       - Continuous state prediction (LNN)
 *       - Performance-driven parameter adaptation (plasticity)
 *
 * HOW:  Market events are encoded as spike populations via fuzzy-to-spike
 *       conversion. STDP learns from trade returns with fuzzy reward
 *       magnitude. LNN predicts future states with fuzzy post-processing.
 *       Plasticity adapts risk parameters based on Sharpe/volatility.
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_FINANCIAL_NEURAL_BRIDGE_H
#define NIMCP_FINANCIAL_NEURAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/parietal/nimcp_financial_market.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_FINANCIAL_NEURAL      0x0396
#define FIN_NEURAL_MAX_SPIKE_CHANNELS   64
#define FIN_NEURAL_MAX_LNN_STATE_DIM    32
#define FIN_NEURAL_MAX_PREDICTION_HORIZON 252
#define FIN_NEURAL_MAX_MEMORY_PATTERNS  128

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_NEURAL_ERROR_BASE           34000
#define FIN_NEURAL_ERR_OK               0
#define FIN_NEURAL_ERR_NULL             (FIN_NEURAL_ERROR_BASE + 1)
#define FIN_NEURAL_ERR_NOT_CONNECTED    (FIN_NEURAL_ERROR_BASE + 2)
#define FIN_NEURAL_ERR_SNN              (FIN_NEURAL_ERROR_BASE + 3)
#define FIN_NEURAL_ERR_STDP             (FIN_NEURAL_ERROR_BASE + 4)
#define FIN_NEURAL_ERR_LNN              (FIN_NEURAL_ERROR_BASE + 5)
#define FIN_NEURAL_ERR_PLASTICITY       (FIN_NEURAL_ERROR_BASE + 6)
#define FIN_NEURAL_ERR_QUANTUM          (FIN_NEURAL_ERROR_BASE + 7)
#define FIN_NEURAL_ERR_ENCODING         (FIN_NEURAL_ERROR_BASE + 8)
#define FIN_NEURAL_ERR_PREDICTION       (FIN_NEURAL_ERROR_BASE + 9)
#define FIN_NEURAL_ERR_CONVERGENCE      (FIN_NEURAL_ERROR_BASE + 10)
#define FIN_NEURAL_ERR_SUBSYSTEM        (FIN_NEURAL_ERROR_BASE + 11)
#define FIN_NEURAL_ERR_VALIDATION       (FIN_NEURAL_ERROR_BASE + 12)

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    FIN_NEURAL_STATE_UNINITIALIZED = 0,
    FIN_NEURAL_STATE_IDLE,
    FIN_NEURAL_STATE_ENCODING,
    FIN_NEURAL_STATE_PREDICTING,
    FIN_NEURAL_STATE_LEARNING,
    FIN_NEURAL_STATE_ADAPTING,
    FIN_NEURAL_STATE_ERROR
} fin_neural_state_t;

/** Market event types for spike encoding (guarded) */
#ifndef FIN_MARKET_EVENT_TYPE_DEFINED
#define FIN_MARKET_EVENT_TYPE_DEFINED
typedef enum {
    FIN_EVENT_PRICE_CHANGE, FIN_EVENT_VOLUME_SPIKE,
    FIN_EVENT_VOLATILITY_SHIFT, FIN_EVENT_REGIME_CHANGE,
    FIN_EVENT_SENTIMENT_SHIFT, FIN_EVENT_INDICATOR_SIGNAL,
    FIN_EVENT_EARNINGS, FIN_EVENT_MACRO_RELEASE,
    FIN_EVENT_TYPE_COUNT
} fin_market_event_type_t;
#endif /* FIN_MARKET_EVENT_TYPE_DEFINED */

/** Spike encoding schemes */
typedef enum {
    FIN_ENCODING_RATE,
    FIN_ENCODING_TEMPORAL,
    FIN_ENCODING_POPULATION,
    FIN_ENCODING_FUZZY_POPULATION,
    FIN_ENCODING_TYPE_COUNT
} fin_spike_encoding_t;

/** Memory consolidation phases */
typedef enum {
    FIN_MEMORY_ENCODING_PHASE,
    FIN_MEMORY_CONSOLIDATION_PHASE,
    FIN_MEMORY_RETRIEVAL_PHASE,
    FIN_MEMORY_PHASE_COUNT
} fin_memory_phase_t;

//=============================================================================
// Data Structures
//=============================================================================

/** Market event for neural encoding (guarded) */
#ifndef FIN_MARKET_EVENT_DEFINED
#define FIN_MARKET_EVENT_DEFINED
typedef struct {
    fin_market_event_type_t type;
    float magnitude;
    float direction;
    uint64_t timestamp_us;
    float context[8];
    uint32_t context_count;
} fin_market_event_t;
#endif /* FIN_MARKET_EVENT_DEFINED */

/** Spike train from encoding */
typedef struct {
    float spike_rates[FIN_NEURAL_MAX_SPIKE_CHANNELS];
    uint64_t spike_times[FIN_NEURAL_MAX_SPIKE_CHANNELS];
    uint32_t active_channels;
    fin_spike_encoding_t encoding;
    float total_activity;
} fin_spike_train_t;

/** STDP reward signal */
typedef struct {
    float reward_magnitude;
    float temporal_discount;
    float fuzzy_profitable_degree;
    float fuzzy_neutral_degree;
    float fuzzy_loss_degree;
    uint64_t reward_timestamp_us;
} fin_stdp_reward_t;

/** LNN prediction output */
typedef struct {
    float predicted_return;
    float predicted_volatility;
    float predicted_direction;
    float confidence;
    float state_vector[FIN_NEURAL_MAX_LNN_STATE_DIM];
    uint32_t state_dim;
    uint32_t horizon_steps;
    /* Fuzzy post-processing */
    fin_fuzzy_market_condition_t fuzzy_regime;
    float prediction_quality;
} fin_neural_prediction_t;

/** Plasticity adaptation parameters */
typedef struct {
    float current_plasticity_rate;
    float performance_score;
    float stability_score;
    float adapted_risk_tolerance;
    float adapted_position_size_scale;
    float adapted_stop_loss_distance;
    float fuzzy_adaptation_degree;
    uint32_t adaptation_epoch;
} fin_plasticity_params_t;

/** Memory pattern for consolidation */
typedef struct {
    float pattern_vector[FIN_NEURAL_MAX_LNN_STATE_DIM];
    float outcome;
    float importance;
    uint64_t creation_time_us;
    uint32_t retrieval_count;
    float retrieval_strength;
} fin_memory_pattern_t;

/** Quantum optimization result for portfolio */
typedef struct {
    float optimal_weights[256];
    uint32_t asset_count;
    float objective_value;
    float fuzzy_constraint_satisfaction;
    uint32_t qubits_used;
    float quantum_advantage_estimate;
    bool classical_fallback_used;
} fin_quantum_result_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    /* Encoding */
    fin_spike_encoding_t default_encoding;
    uint32_t spike_channels;
    float encoding_gain;
    float encoding_threshold;
    bool enable_fuzzy_encoding;

    /* STDP */
    float stdp_learning_rate;
    float stdp_temporal_window_ms;
    float reward_decay_rate;

    /* LNN */
    uint32_t lnn_state_dim;
    float lnn_time_constant;
    uint32_t prediction_horizon;
    bool enable_fuzzy_prediction;

    /* Plasticity */
    float plasticity_base_rate;
    float performance_window_days;
    float stability_window_days;

    /* Quantum */
    bool enable_quantum;
    uint32_t quantum_max_qubits;

    /* Memory */
    uint32_t max_memory_patterns;
    float consolidation_threshold;

    /* Modulation */
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} fin_neural_config_t;

//=============================================================================
// Statistics
//=============================================================================

typedef struct {
    uint64_t events_encoded;
    uint64_t predictions_made;
    uint64_t stdp_updates;
    uint64_t plasticity_adaptations;
    uint64_t quantum_optimizations;
    uint64_t memory_consolidations;
    uint64_t memory_retrievals;
    float avg_prediction_error;
    float avg_encoding_time_us;
    float avg_prediction_time_us;
    float cumulative_reward;
    float current_plasticity_rate;
} fin_neural_stats_t;

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct financial_neural_bridge financial_neural_bridge_t;

//=============================================================================
// Lifecycle
//=============================================================================

financial_neural_bridge_t* financial_neural_bridge_create(
    const fin_neural_config_t* config);
void financial_neural_bridge_destroy(financial_neural_bridge_t* bridge);
fin_neural_config_t financial_neural_bridge_default_config(void);
fin_neural_state_t financial_neural_bridge_get_state(
    const financial_neural_bridge_t* bridge);
int financial_neural_bridge_reset(financial_neural_bridge_t* bridge);

//=============================================================================
// Subsystem Setters
//=============================================================================

int financial_neural_bridge_set_snn(financial_neural_bridge_t* bridge, void* snn);
int financial_neural_bridge_set_stdp(financial_neural_bridge_t* bridge, void* stdp);
int financial_neural_bridge_set_lnn(financial_neural_bridge_t* bridge, void* lnn);
int financial_neural_bridge_set_plasticity(financial_neural_bridge_t* bridge,
                                            void* plasticity);
int financial_neural_bridge_set_quantum(financial_neural_bridge_t* bridge,
                                         void* quantum);
int financial_neural_bridge_set_immune(financial_neural_bridge_t* bridge,
                                        void* immune);
int financial_neural_bridge_set_health_agent(financial_neural_bridge_t* bridge,
                                              void* health_agent);
int financial_neural_bridge_set_logger(financial_neural_bridge_t* bridge,
                                        void* logger);  /* Phase 8: Change Set 2/3 */
int financial_neural_bridge_set_fuzzy_bridge(financial_neural_bridge_t* bridge,
                                              void* fuzzy_bridge);

/** Set instance-level BBB for validation */
int financial_neural_bridge_set_instance_bbb(financial_neural_bridge_t* bridge, void* bbb);

/** Enable/disable instance-level BBB validation */
int financial_neural_bridge_enable_bbb_validation(financial_neural_bridge_t* bridge,
                                                   bool enable);

/** Enable/disable instance-level immune validation */
int financial_neural_bridge_enable_immune_validation(financial_neural_bridge_t* bridge,
                                                      bool enable);

//=============================================================================
// KG Wiring Integration (Change Set 1)
//=============================================================================

/* Forward declaration for KG wiring */
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/** Set KG wiring for financial neural bridge */
int financial_neural_bridge_set_kg_wiring(financial_neural_bridge_t* bridge, kg_wiring_t* kg);

//=============================================================================
// Spike Encoding
//=============================================================================

/** Encode a market event as spike trains */
int financial_neural_bridge_encode_market_event(
    financial_neural_bridge_t* bridge,
    const fin_market_event_t* event,
    fin_spike_train_t* out_spikes);

/** Encode fuzzy market condition as population-coded spikes */
int financial_neural_bridge_encode_fuzzy_regime(
    financial_neural_bridge_t* bridge,
    const fin_fuzzy_market_condition_t* condition,
    fin_spike_train_t* out_spikes);

/** Decode spike train back to market signal */
int financial_neural_bridge_decode_spikes(
    financial_neural_bridge_t* bridge,
    const fin_spike_train_t* spikes,
    float* out_signal, float* out_confidence);

//=============================================================================
// STDP Reward Learning
//=============================================================================

/** Apply STDP reward from trade outcome */
int financial_neural_bridge_stdp_reward(
    financial_neural_bridge_t* bridge,
    float trade_return,
    uint64_t trade_duration_us,
    fin_stdp_reward_t* out_reward);

/** Compute fuzzy reward components for a trade return */
int financial_neural_bridge_compute_fuzzy_reward(
    financial_neural_bridge_t* bridge,
    float trade_return,
    fin_stdp_reward_t* out_reward);

//=============================================================================
// LNN Prediction
//=============================================================================

/** Generate prediction from current neural state */
int financial_neural_bridge_lnn_predict(
    financial_neural_bridge_t* bridge,
    const fin_time_series_t* recent_data,
    uint32_t horizon_steps,
    fin_neural_prediction_t* out_prediction);

/** Update LNN state with new observation */
int financial_neural_bridge_lnn_update(
    financial_neural_bridge_t* bridge,
    const float* observation, uint32_t obs_dim);

//=============================================================================
// Plasticity & Adaptation
//=============================================================================

/** Adapt risk parameters based on recent performance */
int financial_neural_bridge_adapt_risk_params(
    financial_neural_bridge_t* bridge,
    float sharpe_ratio,
    float volatility_trend,
    fin_plasticity_params_t* out_params);

/** Get current plasticity parameters */
int financial_neural_bridge_get_plasticity(
    const financial_neural_bridge_t* bridge,
    fin_plasticity_params_t* out_params);

//=============================================================================
// Quantum Optimization
//=============================================================================

/** Run quantum-accelerated portfolio optimization */
int financial_neural_bridge_quantum_optimize(
    financial_neural_bridge_t* bridge,
    const float* expected_returns, uint32_t asset_count,
    const float* covariance_matrix,
    const float* constraints, uint32_t num_constraints,
    fin_quantum_result_t* out_result);

//=============================================================================
// Memory Consolidation
//=============================================================================

/** Store a market pattern for consolidation */
int financial_neural_bridge_store_pattern(
    financial_neural_bridge_t* bridge,
    const float* pattern, uint32_t dim,
    float outcome, float importance);

/** Retrieve similar patterns from memory */
int financial_neural_bridge_retrieve_patterns(
    financial_neural_bridge_t* bridge,
    const float* query, uint32_t dim,
    fin_memory_pattern_t* out_patterns,
    uint32_t max_patterns, uint32_t* out_count);

/** Trigger memory consolidation cycle */
int financial_neural_bridge_consolidate(
    financial_neural_bridge_t* bridge);

//=============================================================================
// Training
//=============================================================================

/** Run one training step on historical data */
int financial_neural_bridge_train_step(
    financial_neural_bridge_t* bridge,
    const float* input, const float* target,
    uint32_t dim, float learning_rate);

/** Get training convergence status */
int financial_neural_bridge_get_convergence(
    const financial_neural_bridge_t* bridge,
    float* out_loss, float* out_convergence_degree);

//=============================================================================
// Health & Modulation
//=============================================================================

int financial_neural_bridge_set_inflammation(financial_neural_bridge_t* bridge,
                                              float level);
int financial_neural_bridge_set_fatigue(financial_neural_bridge_t* bridge,
                                         float level);
int financial_neural_bridge_get_stats(const financial_neural_bridge_t* bridge,
                                       fin_neural_stats_t* stats);
void financial_neural_bridge_reset_stats(financial_neural_bridge_t* bridge);
const char* financial_neural_bridge_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_NEURAL_BRIDGE_H */
