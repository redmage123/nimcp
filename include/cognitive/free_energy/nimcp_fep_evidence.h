/**
 * @file nimcp_fep_evidence.h
 * @brief Model Evidence Computation for Free Energy Principle
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bayesian model comparison and evidence computation for FEP generative models
 * WHY:  Model selection is fundamental to hierarchical inference - the brain must select
 *       the best generative model from alternatives. Model evidence balances complexity
 *       and accuracy, implementing Occam's razor at the computational level.
 * HOW:  Compute log evidence via ELBO, importance sampling, annealed sampling, and bridge
 *       sampling. Compare models via Bayes factors and posterior model probabilities.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * BAYESIAN BRAIN HYPOTHESIS (Knill & Pouget, 2004):
 * -------------------------------------------------
 * The brain performs Bayesian inference by maintaining and comparing multiple generative
 * models. Model selection occurs implicitly through free energy minimization.
 *
 * MODEL EVIDENCE IN PREDICTIVE CODING:
 * ------------------------------------
 * Each cortical hierarchy level implicitly computes model evidence:
 *
 *   log p(o) = log ∫ p(o|s)p(s) ds
 *
 * This marginal likelihood is approximated via variational free energy:
 *
 *   log p(o) ≥ -F = ELBO = E_q[log p(o,s)] - E_q[log q(s)]
 *
 * OCCAM'S RAZOR IN NEURAL SYSTEMS:
 * --------------------------------
 * Model evidence naturally implements Occam's razor:
 *
 *   log p(o) = Accuracy - Complexity
 *
 * Where:
 *   Accuracy = E_q[log p(o|s)]  = How well model predicts data
 *   Complexity = KL[q(s)||p(s)] = Deviation from prior (parameter cost)
 *
 * Simpler models (closer to prior) have lower complexity cost. Complex models
 * must provide better accuracy to be selected.
 *
 * HIERARCHICAL MODEL SELECTION:
 * -----------------------------
 * The brain maintains multiple hypotheses simultaneously across hierarchy:
 *
 *   Level N:   M₁ᴺ    M₂ᴺ    M₃ᴺ  ← Abstract concepts
 *              ↓       ↓      ↓
 *   Level N-1: M₁ᴺ⁻¹  M₂ᴺ⁻¹  M₃ᴺ⁻¹ ← Features
 *              ↓       ↓      ↓
 *   Level 0:   Observations
 *
 * Each model competes based on evidence. Selection happens via precision-weighted
 * prediction error - models with lower error gain precision (confidence).
 *
 * NEURAL IMPLEMENTATION:
 * ----------------------
 * 1. **Prediction Error Units**: Encode model accuracy
 *    - Lower error → Higher evidence for that model
 *
 * 2. **Precision Neurons**: Encode model confidence
 *    - Higher precision → Model more likely selected
 *
 * 3. **Model Neurons**: Encode model probabilities
 *    - Softmax over evidence → Posterior model probabilities
 *
 * BAYES FACTORS (Model Comparison):
 * ---------------------------------
 * Comparing two models M₁ and M₂:
 *
 *   BF₁₂ = p(o|M₁) / p(o|M₂)
 *
 * Interpretation:
 *   BF₁₂ > 3    → Positive evidence for M₁
 *   BF₁₂ > 10   → Strong evidence for M₁
 *   BF₁₂ > 100  → Decisive evidence for M₁
 *
 * BAYESIAN MODEL AVERAGING:
 * -------------------------
 * Rather than selecting a single model, the brain may average:
 *
 *   p(o'|o) = ∑ᵢ p(o'|Mᵢ,o) p(Mᵢ|o)
 *
 * Where p(Mᵢ|o) ∝ p(o|Mᵢ)p(Mᵢ) is the posterior model probability.
 *
 * EVIDENCE COMPUTATION METHODS:
 * -----------------------------
 * 1. **ELBO (Evidence Lower Bound)**:
 *    Fast approximation, always lower bounds true evidence
 *
 * 2. **Importance Sampling**:
 *    Monte Carlo estimate using proposal distribution
 *
 * 3. **Annealed Importance Sampling (AIS)**:
 *    Temperature schedule from prior to posterior
 *    More accurate than plain importance sampling
 *
 * 4. **Bridge Sampling**:
 *    Uses intermediate "bridge" distributions
 *    Gold standard for evidence estimation
 *
 * REFERENCES:
 * - Friston, K. (2010) "The free-energy principle: a unified brain theory?"
 * - Penny, W.D. (2012) "Comparing dynamic causal models using AIC, BIC and free energy"
 * - Stephan, K.E. et al. (2009) "Bayesian model selection for group studies"
 * - Grosse, R.B. et al. (2013) "Annealing between distributions by averaging moments"
 * - Knill, D.C., Pouget, A. (2004) "The Bayesian brain: the role of uncertainty"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    MODEL EVIDENCE SYSTEM                                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 EVIDENCE COMPUTATION                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   Model M + Data o                                                 │  ║
 * ║   │         ↓                                                           │  ║
 * ║   │   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │  ║
 * ║   │   │    ELBO     │  │ Importance  │  │  Annealed   │              │  ║
 * ║   │   │ (Fast bound)│  │  Sampling   │  │  Sampling   │              │  ║
 * ║   │   └─────────────┘  └─────────────┘  └─────────────┘              │  ║
 * ║   │         ↓                 ↓                 ↓                       │  ║
 * ║   │   log p(o|M) ≈ ELBO ≈ IS estimate ≈ AIS estimate                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              DECOMPOSITION (Occam's Razor)                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   log p(o|M) = Accuracy - Complexity                              │  ║
 * ║   │                    ↓           ↓                                   │  ║
 * ║   │             E_q[log p(o|s)]  KL[q||p]                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   Simple model: Low accuracy, Low complexity                       │  ║
 * ║   │   Complex model: High accuracy, High complexity                    │  ║
 * ║   │   → Occam's razor: Prefer simpler unless accuracy justifies cost   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    MODEL COMPARISON                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   Models: M₁, M₂, ..., Mₙ                                         │  ║
 * ║   │       ↓                                                            │  ║
 * ║   │   Compute evidence for each: log p(o|Mᵢ)                          │  ║
 * ║   │       ↓                                                            │  ║
 * ║   │   Bayes factors: BFᵢⱼ = p(o|Mᵢ) / p(o|Mⱼ)                        │  ║
 * ║   │       ↓                                                            │  ║
 * ║   │   Posterior probabilities: p(Mᵢ|o) ∝ p(o|Mᵢ)p(Mᵢ)                │  ║
 * ║   │       ↓                                                            │  ║
 * ║   │   Select best: argmax p(Mᵢ|o)                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   BAYESIAN MODEL AVERAGING                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   Prediction = ∑ᵢ p(o'|Mᵢ,o) * p(Mᵢ|o)                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   Each model contributes weighted by its evidence                  │  ║
 * ║   │   More robust than single model selection                          │  ║
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

#ifndef NIMCP_FEP_EVIDENCE_H
#define NIMCP_FEP_EVIDENCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Evidence computation */
#define FEP_EVIDENCE_MIN_SAMPLES         100      /**< Minimum MC samples */
#define FEP_EVIDENCE_DEFAULT_SAMPLES     1000     /**< Default MC samples */
#define FEP_EVIDENCE_MAX_SAMPLES         100000   /**< Maximum MC samples */

/* Annealing schedule */
#define FEP_EVIDENCE_DEFAULT_TEMP_START  0.0f     /**< Start at prior */
#define FEP_EVIDENCE_DEFAULT_TEMP_END    1.0f     /**< End at posterior */
#define FEP_EVIDENCE_DEFAULT_ANNEAL_STEPS 10      /**< Annealing steps */

/* Bayes factor interpretation thresholds */
#define FEP_EVIDENCE_BF_WEAK             1.0f     /**< No evidence */
#define FEP_EVIDENCE_BF_POSITIVE         3.0f     /**< Positive evidence */
#define FEP_EVIDENCE_BF_STRONG           10.0f    /**< Strong evidence */
#define FEP_EVIDENCE_BF_VERY_STRONG      30.0f    /**< Very strong evidence */
#define FEP_EVIDENCE_BF_DECISIVE         100.0f   /**< Decisive evidence */

/* Numerical stability */
#define FEP_EVIDENCE_LOG_MIN             -1e10f   /**< Minimum log value */
#define FEP_EVIDENCE_LOG_MAX             1e10f    /**< Maximum log value */
#define FEP_EVIDENCE_EPSILON             1e-8f    /**< Small constant */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Evidence computation methods
 *
 * WHAT: Different algorithms for computing model evidence
 * WHY:  Trade-off between speed and accuracy
 * HOW:  ELBO (fast), importance sampling, annealing, bridge sampling
 */
typedef enum {
    EVIDENCE_ELBO = 0,           /**< Evidence Lower Bound (fastest) */
    EVIDENCE_IMPORTANCE,         /**< Importance sampling estimate */
    EVIDENCE_ANNEALED,           /**< Annealed importance sampling (accurate) */
    EVIDENCE_BRIDGE              /**< Bridge sampling (gold standard) */
} fep_evidence_method_t;

/**
 * @brief Bayes factor strength interpretation
 */
typedef enum {
    BF_STRENGTH_NONE = 0,        /**< BF < 1: No evidence */
    BF_STRENGTH_WEAK,            /**< 1 ≤ BF < 3: Weak evidence */
    BF_STRENGTH_POSITIVE,        /**< 3 ≤ BF < 10: Positive evidence */
    BF_STRENGTH_STRONG,          /**< 10 ≤ BF < 30: Strong evidence */
    BF_STRENGTH_VERY_STRONG,     /**< 30 ≤ BF < 100: Very strong evidence */
    BF_STRENGTH_DECISIVE         /**< BF ≥ 100: Decisive evidence */
} fep_bf_strength_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Evidence computation configuration
 */
typedef struct {
    /* Method selection */
    fep_evidence_method_t method;            /**< Computation method */

    /* Sampling parameters */
    uint32_t num_samples;                    /**< Number of Monte Carlo samples */

    /* Annealing schedule (for EVIDENCE_ANNEALED) */
    float temperature_schedule_start;        /**< Starting temperature (prior) */
    float temperature_schedule_end;          /**< Ending temperature (posterior) */
    uint32_t annealing_steps;                /**< Number of annealing steps */

    /* Model averaging */
    bool enable_model_averaging;             /**< Use Bayesian model averaging */
} fep_evidence_config_t;

/**
 * @brief Evidence computation result
 *
 * WHAT: Complete evidence decomposition
 * WHY:  Understand model selection in terms of accuracy vs complexity
 * HOW:  Store all components of evidence calculation
 */
typedef struct {
    /* Evidence estimates */
    float log_evidence;                      /**< log p(o|M) */
    float evidence_lower_bound;              /**< ELBO (lower bound) */

    /* Decomposition (Occam's razor) */
    float model_complexity;                  /**< KL[q(s)||p(s)] */
    float model_accuracy;                    /**< E_q[log p(o|s)] */

    /* Comparison */
    float bayes_factor;                      /**< BF relative to reference */
    fep_bf_strength_t bf_strength;           /**< Interpretation of BF */

    /* Diagnostics */
    float effective_sample_size;             /**< ESS for IS/AIS */
    float monte_carlo_error;                 /**< MC standard error */
} fep_evidence_result_t;

/**
 * @brief Model score for comparison
 *
 * WHAT: Evidence and posterior probability for a single model
 * WHY:  Rank models and compute model averaging weights
 * HOW:  Store model ID, evidence, and posterior probability
 */
typedef struct {
    uint32_t model_id;                       /**< Model identifier */
    float log_evidence;                      /**< log p(o|M) */
    float posterior_probability;             /**< p(M|o) */
    float prior_probability;                 /**< p(M) */
} fep_model_score_t;

/**
 * @brief Evidence system statistics
 */
typedef struct {
    uint64_t total_computations;             /**< Total evidence computations */
    uint64_t elbo_computations;              /**< ELBO calls */
    uint64_t importance_computations;        /**< IS calls */
    uint64_t annealed_computations;          /**< AIS calls */
    uint64_t bridge_computations;            /**< Bridge sampling calls */
    uint64_t model_comparisons;              /**< Model comparison calls */

    float avg_log_evidence;                  /**< Average log evidence */
    float avg_complexity;                    /**< Average complexity */
    float avg_accuracy;                      /**< Average accuracy */
} fep_evidence_stats_t;

/**
 * @brief Evidence system state
 */
typedef struct {
    /* Configuration */
    fep_evidence_config_t config;

    /* FEP integration */
    fep_system_t* fep_system;                /**< Connected FEP system */

    /* Reference model (for Bayes factors) */
    fep_system_t* reference_model;           /**< Reference for BF computation */
    float reference_log_evidence;            /**< Cached reference evidence */
    bool reference_valid;                    /**< Reference cache valid */

    /* Statistics */
    fep_evidence_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;            /**< Bio-async module context */
    bool bio_async_enabled;                  /**< Bio-async active flag */

    /* Thread safety */
    nimcp_mutex_t* mutex;                    /**< Thread synchronization */
} fep_evidence_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default evidence configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with standard parameters
 * HOW:  Set method to ELBO, 1000 samples, standard annealing
 *
 * @param config Output configuration
 */
void fep_evidence_default_config(fep_evidence_config_t* config);

/**
 * @brief Create evidence system
 *
 * WHAT: Initialize model evidence computation system
 * WHY:  Enable Bayesian model comparison and selection
 * HOW:  Allocate system, initialize config, create mutex
 *
 * @param config Configuration
 * @return New evidence system or NULL on failure
 */
fep_evidence_system_t* fep_evidence_create(const fep_evidence_config_t* config);

/**
 * @brief Destroy evidence system
 *
 * WHAT: Clean up evidence system
 * WHY:  Release all allocated resources
 * HOW:  Free memory, destroy mutex, NULL-safe
 *
 * @param sys Evidence system (NULL safe)
 */
void fep_evidence_destroy(fep_evidence_system_t* sys);

/* ============================================================================
 * Evidence Computation API
 * ============================================================================ */

/**
 * @brief Compute log evidence for model
 *
 * WHAT: Estimate log p(o|M) for a generative model
 * WHY:  Core of model selection - evidence quantifies model quality
 * HOW:  Use configured method (ELBO, IS, AIS, Bridge) to estimate
 *
 * @param sys Evidence system
 * @param fep FEP generative model
 * @param observations Observation matrix (n_obs x obs_dim)
 * @param n_obs Number of observations
 * @param obs_dim Observation dimensionality
 * @param result Output evidence result
 * @return 0 on success, negative on error
 */
int fep_compute_log_evidence(
    fep_evidence_system_t* sys,
    fep_system_t* fep,
    const float* observations,
    size_t n_obs,
    size_t obs_dim,
    fep_evidence_result_t* result
);

/**
 * @brief Compute Evidence Lower Bound (ELBO)
 *
 * WHAT: Fast lower bound approximation of log evidence
 * WHY:  ELBO = E_q[log p(o,s)] - E_q[log q(s)] is tractable
 * HOW:  Compute accuracy (expected log likelihood) minus complexity (KL)
 *
 * @param sys Evidence system
 * @param fep FEP model
 * @param observation Single observation
 * @param obs_dim Observation dimensionality
 * @param elbo Output ELBO value
 * @return 0 on success
 */
int fep_compute_elbo(
    fep_evidence_system_t* sys,
    fep_system_t* fep,
    const float* observation,
    size_t obs_dim,
    float* elbo
);

/**
 * @brief Compute model complexity
 *
 * WHAT: KL divergence between posterior and prior
 * WHY:  Complexity = KL[q(s)||p(s)] measures parameter cost
 * HOW:  Compute KL between variational posterior and prior
 *
 * @param sys Evidence system
 * @param fep FEP model
 * @param complexity Output complexity
 * @return 0 on success
 */
int fep_compute_model_complexity(
    fep_evidence_system_t* sys,
    fep_system_t* fep,
    float* complexity
);

/**
 * @brief Compute model accuracy
 *
 * WHAT: Expected log likelihood E_q[log p(o|s)]
 * WHY:  Accuracy measures how well model predicts data
 * HOW:  Sample from q(s), compute average log p(o|s)
 *
 * @param sys Evidence system
 * @param fep FEP model
 * @param observation Observation
 * @param obs_dim Observation dimensionality
 * @param accuracy Output accuracy
 * @return 0 on success
 */
int fep_compute_model_accuracy(
    fep_evidence_system_t* sys,
    fep_system_t* fep,
    const float* observation,
    size_t obs_dim,
    float* accuracy
);

/* ============================================================================
 * Model Comparison API
 * ============================================================================ */

/**
 * @brief Compute Bayes factor between two models
 *
 * WHAT: Compute BF₁₂ = p(o|M₁) / p(o|M₂)
 * WHY:  Quantify relative evidence for model1 vs model2
 * HOW:  Compute log evidence for both, take difference
 *
 * @param sys Evidence system
 * @param model1 First model
 * @param model2 Second model (reference)
 * @param observations Observation matrix
 * @param n_obs Number of observations
 * @param obs_dim Observation dimensionality
 * @param log_bf Output log Bayes factor
 * @return 0 on success
 */
int fep_compute_bayes_factor(
    fep_evidence_system_t* sys,
    fep_system_t* model1,
    fep_system_t* model2,
    const float* observations,
    size_t n_obs,
    size_t obs_dim,
    float* log_bf
);

/**
 * @brief Compare multiple models
 *
 * WHAT: Compute evidence for all models and rank
 * WHY:  Model selection requires comparing all alternatives
 * HOW:  Compute log evidence for each, convert to posteriors
 *
 * @param sys Evidence system
 * @param models Array of FEP models
 * @param n_models Number of models
 * @param observations Observation matrix
 * @param n_obs Number of observations
 * @param obs_dim Observation dimensionality
 * @param scores Output model scores (must be size n_models)
 * @return 0 on success
 */
int fep_compare_models(
    fep_evidence_system_t* sys,
    fep_system_t** models,
    size_t n_models,
    const float* observations,
    size_t n_obs,
    size_t obs_dim,
    fep_model_score_t* scores
);

/**
 * @brief Select best model from scores
 *
 * WHAT: Find model with highest posterior probability
 * WHY:  MAP (Maximum A Posteriori) model selection
 * HOW:  argmax_i p(Mi|o)
 *
 * @param sys Evidence system
 * @param scores Model scores
 * @param n_models Number of models
 * @param best_model_id Output best model ID
 * @return 0 on success, -1 on error
 */
int fep_select_best_model(
    fep_evidence_system_t* sys,
    const fep_model_score_t* scores,
    size_t n_models,
    uint32_t* best_model_id
);

/* ============================================================================
 * Model Averaging API
 * ============================================================================ */

/**
 * @brief Bayesian model averaging for prediction
 *
 * WHAT: Average predictions weighted by model evidence
 * WHY:  More robust than selecting single model
 * HOW:  prediction = ∑ᵢ p(o'|Mᵢ) * p(Mᵢ|o)
 *
 * @param sys Evidence system
 * @param models Array of FEP models
 * @param n_models Number of models
 * @param scores Model scores (from fep_compare_models)
 * @param averaged_prediction Output averaged prediction
 * @param pred_dim Prediction dimensionality
 * @return 0 on success
 */
int fep_model_average_prediction(
    fep_evidence_system_t* sys,
    fep_system_t** models,
    size_t n_models,
    const fep_model_score_t* scores,
    float* averaged_prediction,
    size_t pred_dim
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect evidence system to FEP system
 *
 * WHAT: Link evidence computation to FEP generative model
 * WHY:  Evidence system needs access to FEP beliefs and predictions
 * HOW:  Store FEP pointer for evidence computation
 *
 * @param evidence Evidence system
 * @param fep FEP system
 * @return 0 on success
 */
int fep_evidence_connect(fep_evidence_system_t* evidence, fep_system_t* fep);

/**
 * @brief Set reference model for Bayes factors
 *
 * WHAT: Designate a reference model for BF computation
 * WHY:  Bayes factors are always relative to a reference
 * HOW:  Compute and cache reference model evidence
 *
 * @param sys Evidence system
 * @param reference Reference FEP model
 * @param observations Observation data
 * @param n_obs Number of observations
 * @param obs_dim Observation dimensionality
 * @return 0 on success
 */
int fep_evidence_set_reference(
    fep_evidence_system_t* sys,
    fep_system_t* reference,
    const float* observations,
    size_t n_obs,
    size_t obs_dim
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY:  Enable inter-module communication about model evidence
 * HOW:  Register as BIO_MODULE_FEP_EVIDENCE module
 *
 * @param sys Evidence system
 * @return 0 on success
 */
int fep_evidence_connect_bio_async(fep_evidence_system_t* sys);

/**
 * @brief Disconnect from bio-async router
 *
 * @param sys Evidence system
 * @return 0 on success
 */
int fep_evidence_disconnect_bio_async(fep_evidence_system_t* sys);

/**
 * @brief Check if bio-async is connected
 *
 * @param sys Evidence system
 * @return true if connected
 */
bool fep_evidence_is_bio_async_connected(const fep_evidence_system_t* sys);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param sys Evidence system
 * @param stats Output statistics
 * @return 0 on success
 */
int fep_evidence_get_stats(const fep_evidence_system_t* sys, fep_evidence_stats_t* stats);

/**
 * @brief Interpret Bayes factor strength
 *
 * WHAT: Convert numerical BF to qualitative interpretation
 * WHY:  Aid interpretation (Kass & Raftery, 1995)
 * HOW:  Threshold-based classification
 *
 * @param log_bf Log Bayes factor
 * @return BF strength category
 */
fep_bf_strength_t fep_interpret_bayes_factor(float log_bf);

/**
 * @brief Convert BF strength to string
 *
 * @param strength BF strength
 * @return Human-readable string
 */
const char* fep_bf_strength_to_string(fep_bf_strength_t strength);

/**
 * @brief Convert evidence method to string
 *
 * @param method Evidence method
 * @return Human-readable string
 */
const char* fep_evidence_method_to_string(fep_evidence_method_t method);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_EVIDENCE_H */
