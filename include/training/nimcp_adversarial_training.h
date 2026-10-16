/**
 * @file nimcp_adversarial_training.h
 * @brief Adversarial Training for NIMCP
 *
 * WHAT: Train models to be robust against adversarial perturbations
 * WHY:  Improve robustness, safety, and reliability of neural networks
 * HOW:  PGD training, TRADES, adversarial augmentation, certified defense
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: Advertorch, CleverHans, RobustBench
 * - JAX: Neural Oblivion, jax-verify
 * - TensorFlow: CleverHans, tf-robustness
 *
 * NIMCP APPROACH:
 * - Integrates with brain immune system for intrusion detection
 * - Bio-inspired via neural noise tolerance mechanisms
 * - Leverages predictive immune system
 *
 * BIOLOGICAL GROUNDING:
 * - Brain is robust to neural noise and dropout
 * - Immune system: Detect and respond to adversarial patterns
 * - Error correction: Redundancy and lateral inhibition
 * - Predictive coding: Detect anomalous inputs
 *
 * INTEGRATION POINTS:
 * - brain_immune: Adversarial detection
 * - predictive_immune: Anomaly detection
 * - gradient_manager: Gradient-based attacks
 * - tensor: Perturbation operations
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_ADVERSARIAL_TRAINING_H
#define NIMCP_ADVERSARIAL_TRAINING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_loss_functions.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define ADV_DEFAULT_EPSILON           0.031f   /**< Default L-inf epsilon (8/255) */
#define ADV_DEFAULT_STEP_SIZE         0.007f   /**< Default PGD step size */
#define ADV_DEFAULT_NUM_STEPS         7        /**< Default PGD steps */
#define ADV_DEFAULT_TRADES_BETA       6.0f     /**< Default TRADES beta */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Adversarial attack type
 *
 * COMPARISON:
 * - FGSM: Fast, single-step
 * - PGD: Multi-step, stronger
 * - C&W: Optimization-based, strong
 * - AutoAttack: Ensemble, state-of-art
 */
typedef enum {
    ADV_ATTACK_FGSM = 0,             /**< Fast Gradient Sign Method */
    ADV_ATTACK_PGD,                  /**< Projected Gradient Descent */
    ADV_ATTACK_CW,                   /**< Carlini & Wagner */
    ADV_ATTACK_DEEPFOOL,             /**< DeepFool minimal perturbation */
    ADV_ATTACK_AUTOATTACK,           /**< AutoAttack ensemble */
    ADV_ATTACK_SQUARE,               /**< Square attack (black-box) */
    ADV_ATTACK_TRADES_INNER,         /**< TRADES inner maximization */
    ADV_ATTACK_FREE,                 /**< Free adversarial training */
    ADV_ATTACK_CUSTOM,               /**< Custom attack function */
    ADV_ATTACK_COUNT
} adv_attack_t;

/**
 * @brief Adversarial training method
 *
 * COMPARISON:
 * - Standard AT: Train on adversarial examples
 * - TRADES: Trade-off clean vs robust accuracy
 * - MART: Misclassification-aware robust training
 * - AWP: Adversarial weight perturbation
 */
typedef enum {
    ADV_TRAIN_STANDARD = 0,          /**< Standard adversarial training */
    ADV_TRAIN_TRADES,                /**< TRADES (clean + KL on adv) */
    ADV_TRAIN_MART,                  /**< MART (boosted cross-entropy) */
    ADV_TRAIN_FREE,                  /**< Free adversarial training */
    ADV_TRAIN_FAST,                  /**< FGSM + random start */
    ADV_TRAIN_AWP,                   /**< Adversarial weight perturbation */
    ADV_TRAIN_ADVERSARIAL_AUGMENT,   /**< Adversarial as augmentation */
    ADV_TRAIN_CERTIFIED,             /**< Certified defense (IBP/CROWN) */
    ADV_TRAIN_COUNT
} adv_train_method_t;

/**
 * @brief Norm type for perturbation
 */
typedef enum {
    ADV_NORM_LINF = 0,               /**< L-infinity norm */
    ADV_NORM_L2,                     /**< L2 norm */
    ADV_NORM_L1,                     /**< L1 norm */
    ADV_NORM_L0,                     /**< L0 (sparsity) */
    ADV_NORM_COUNT
} adv_norm_t;

/**
 * @brief Detection method for adversarial examples
 *
 * BIOLOGICAL BASIS:
 * - Anomaly detection ≈ immune threat detection
 * - Statistical tests ≈ homeostatic regulation
 */
typedef enum {
    ADV_DETECT_NONE = 0,             /**< No detection */
    ADV_DETECT_STATISTICAL,          /**< Statistical anomaly detection */
    ADV_DETECT_LAYER_ACTIVATION,     /**< Unusual layer activations */
    ADV_DETECT_INPUT_TRANSFORM,      /**< Feature squeezing */
    ADV_DETECT_ENSEMBLE,             /**< Ensemble disagreement */
    ADV_DETECT_CERTIFIED,            /**< Certified bounds */
    ADV_DETECT_COUNT
} adv_detection_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Attack configuration
 */
typedef struct {
    adv_attack_t type;               /**< Attack type */
    adv_norm_t norm;                 /**< Perturbation norm */
    float epsilon;                   /**< Maximum perturbation */
    float step_size;                 /**< Per-step perturbation */
    uint32_t num_steps;              /**< Number of attack steps */
    bool random_start;               /**< Random initialization */
    float random_init_scale;         /**< Scale for random init */

    /* C&W specific */
    float confidence;                /**< C&W confidence parameter */
    float c_init;                    /**< C&W c initialization */
    uint32_t binary_search_steps;    /**< C&W binary search steps */

    /* Targeted attack */
    bool targeted;                   /**< Targeted attack */
    int target_class;                /**< Target class (-1 = least likely) */
} adv_attack_config_t;

/**
 * @brief TRADES configuration
 *
 * REFERENCE: Zhang et al. 2019 "Theoretically Principled Trade-off"
 */
typedef struct {
    float beta;                      /**< Trade-off parameter */
    bool use_kl_loss;                /**< Use KL for robustness loss */
    float clean_weight;              /**< Weight for clean loss */
} adv_trades_config_t;

/**
 * @brief AWP configuration (Adversarial Weight Perturbation)
 *
 * REFERENCE: Wu et al. 2020 "Adversarial Weight Perturbation"
 */
typedef struct {
    float gamma;                     /**< Weight perturbation scale */
    uint32_t awp_warmup;             /**< Warmup epochs before AWP */
    bool perturb_running_stats;      /**< Perturb BN running stats */
} adv_awp_config_t;

/**
 * @brief Certified defense configuration
 */
typedef struct {
    float certified_epsilon;         /**< Certified perturbation radius */
    bool use_ibp;                    /**< Use Interval Bound Propagation */
    bool use_crown;                  /**< Use CROWN relaxation */
    float kappa_schedule_start;      /**< IBP schedule start */
    float kappa_schedule_end;        /**< IBP schedule end */
    uint32_t schedule_length;        /**< Schedule length (epochs) */
} adv_certified_config_t;

/**
 * @brief Detection configuration
 */
typedef struct {
    adv_detection_t method;          /**< Detection method */
    float threshold;                 /**< Detection threshold */
    bool log_detections;             /**< Log detected adversarials */
    bool reject_detected;            /**< Reject detected examples */
} adv_detection_config_t;

/**
 * @brief Complete adversarial training configuration
 */
typedef struct {
    adv_train_method_t method;       /**< Training method */
    adv_attack_config_t attack;      /**< Attack configuration */

    /* Method-specific configs */
    adv_trades_config_t trades;
    adv_awp_config_t awp;
    adv_certified_config_t certified;
    adv_detection_config_t detection;

    /* Training schedule */
    float adversarial_ratio;         /**< Ratio of adv examples */
    uint32_t warmup_epochs;          /**< Clean training warmup */
    bool curriculum;                 /**< Curriculum (easy to hard) */
    float curriculum_epsilon_init;   /**< Initial curriculum epsilon */

    /* Regularization */
    float smoothness_reg;            /**< Smoothness regularization */
    float lipschitz_reg;             /**< Lipschitz regularization */

    /* Integration */
    bool integrate_gradient_manager; /**< Use gradient_manager */
    bool integrate_brain_immune;     /**< Use immune detection */
    bool integrate_predictive_immune;/**< Use predictive immune */

    /* Debugging */
    bool verbose;
    bool track_statistics;
    bool save_adversarial_examples;  /**< Save generated examples */
} adv_config_t;

//=============================================================================
// Adversarial Example Structure
//=============================================================================

/**
 * @brief Adversarial example
 */
typedef struct {
    nimcp_tensor_t* clean_input;     /**< Original input */
    nimcp_tensor_t* perturbation;    /**< Adversarial perturbation */
    nimcp_tensor_t* adv_input;       /**< Perturbed input */
    uint32_t original_class;         /**< Original predicted class */
    uint32_t adversarial_class;      /**< Adversarial predicted class */
    float perturbation_norm;         /**< Norm of perturbation */
    bool attack_success;             /**< Whether attack succeeded */
} adv_example_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Adversarial training statistics
 */
typedef struct {
    uint64_t total_steps;            /**< Total training steps */
    uint64_t adversarial_steps;      /**< Steps with adversarial examples */

    /* Accuracy */
    float clean_accuracy;            /**< Accuracy on clean examples */
    float robust_accuracy;           /**< Accuracy on adversarial examples */
    float accuracy_gap;              /**< Clean - robust accuracy */

    /* Attack statistics */
    float avg_perturbation_norm;     /**< Average perturbation size */
    float attack_success_rate;       /**< Rate of successful attacks */
    uint64_t total_attacks;          /**< Total attacks performed */
    uint64_t successful_attacks;     /**< Successful attacks */

    /* Detection statistics */
    float detection_rate;            /**< Adversarial detection rate */
    float false_positive_rate;       /**< Clean examples detected as adv */

    /* Loss components */
    float avg_clean_loss;            /**< Average clean loss */
    float avg_adv_loss;              /**< Average adversarial loss */
    float avg_trades_loss;           /**< Average TRADES KL loss */

    /* Timing */
    double attack_time_ms;           /**< Time generating attacks */
    double training_time_ms;         /**< Total training time */
} adv_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Adversarial training context (opaque)
 */
typedef struct adv_ctx_s adv_ctx_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default adversarial training configuration
 *
 * DEFAULTS:
 * - PGD-7 attack
 * - L-inf norm, epsilon = 8/255
 * - Standard adversarial training
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int adv_default_config(adv_config_t* config);

/**
 * @brief Get TRADES configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int adv_trades_config(adv_config_t* config);

/**
 * @brief Create adversarial training context
 *
 * @param config Adversarial training configuration
 * @return Context or NULL on failure
 */
adv_ctx_t* adv_create(const adv_config_t* config);

/**
 * @brief Destroy adversarial training context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void adv_destroy(adv_ctx_t* ctx);

//=============================================================================
// Attack Generation API
//=============================================================================

/**
 * @brief Generate adversarial example using FGSM
 *
 * WHAT: Single-step gradient attack
 * WHY:  Fast adversarial example generation
 * HOW:  x_adv = x + epsilon * sign(grad_x(L))
 *
 * @param ctx Adversarial context
 * @param input Clean input tensor
 * @param label True label
 * @param forward_fn Model forward function
 * @param model Model
 * @param adv_example Output adversarial example
 * @return 0 on success, negative on error
 */
int adv_fgsm(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* input,
    const nimcp_tensor_t* label,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    adv_example_t* adv_example
);

/**
 * @brief Generate adversarial example using PGD
 *
 * WHAT: Multi-step projected gradient attack
 * WHY:  Stronger adversarial examples
 * HOW:  Iterated FGSM with projection to epsilon ball
 *
 * @param ctx Adversarial context
 * @param input Clean input tensor
 * @param label True label
 * @param forward_fn Model forward function
 * @param model Model
 * @param adv_example Output adversarial example
 * @return 0 on success, negative on error
 */
int adv_pgd(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* input,
    const nimcp_tensor_t* label,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    adv_example_t* adv_example
);

/**
 * @brief Generate adversarial batch
 *
 * @param ctx Adversarial context
 * @param inputs Input batch
 * @param labels Label batch
 * @param batch_size Batch size
 * @param forward_fn Model forward function
 * @param model Model
 * @param adv_inputs Output adversarial inputs
 * @return 0 on success, negative on error
 */
int adv_generate_batch(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* labels,
    uint32_t batch_size,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    nimcp_tensor_t** adv_inputs
);

//=============================================================================
// Adversarial Training API
//=============================================================================

/**
 * @brief Compute adversarial training loss
 *
 * WHAT: Combined loss for adversarial training
 * WHY:  Train robust model
 * HOW:  Generate adversarial examples, compute loss
 *
 * For TRADES:
 *   L = CE(f(x), y) + beta * KL(f(x) || f(x_adv))
 *
 * @param ctx Adversarial context
 * @param inputs Clean inputs
 * @param labels True labels
 * @param forward_fn Model forward function
 * @param model Model
 * @param total_loss Output total loss
 * @param clean_loss Output clean loss (optional)
 * @param adv_loss Output adversarial loss (optional)
 * @return 0 on success, negative on error
 */
int adv_compute_loss(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* labels,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* total_loss,
    float* clean_loss,
    float* adv_loss
);

/**
 * @brief Apply AWP weight perturbation
 *
 * @param ctx Adversarial context
 * @param params Model parameters
 * @param num_params Number of parameters
 * @param gradients Gradients
 * @return 0 on success, negative on error
 */
int adv_awp_perturb(
    adv_ctx_t* ctx,
    float* params,
    size_t num_params,
    const float* gradients
);

/**
 * @brief Restore AWP weight perturbation
 *
 * @param ctx Adversarial context
 * @param params Model parameters
 * @param num_params Number of parameters
 * @return 0 on success, negative on error
 */
int adv_awp_restore(
    adv_ctx_t* ctx,
    float* params,
    size_t num_params
);

//=============================================================================
// Detection API
//=============================================================================

/**
 * @brief Detect adversarial example
 *
 * WHAT: Detect if input is adversarial
 * WHY:  Reject or handle adversarial inputs
 *
 * BIOLOGICAL BASIS:
 * - Immune detection of foreign patterns
 * - Anomaly detection in neural activity
 *
 * @param ctx Adversarial context
 * @param input Input to check
 * @param forward_fn Model forward function
 * @param model Model
 * @param is_adversarial Output detection result
 * @param confidence Output detection confidence
 * @return 0 on success, negative on error
 */
int adv_detect(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    bool* is_adversarial,
    float* confidence
);

//=============================================================================
// Evaluation API
//=============================================================================

/**
 * @brief Evaluate robust accuracy
 *
 * @param ctx Adversarial context
 * @param inputs Test inputs
 * @param labels Test labels
 * @param forward_fn Model forward function
 * @param model Model
 * @param clean_accuracy Output clean accuracy
 * @param robust_accuracy Output robust accuracy
 * @return 0 on success, negative on error
 */
int adv_evaluate(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* labels,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* clean_accuracy,
    float* robust_accuracy
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to gradient manager
 *
 * @param ctx Adversarial context
 * @param grad_manager Gradient manager
 * @return 0 on success, negative on error
 */
int adv_connect_gradient_manager(
    adv_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
);

/**
 * @brief Connect to brain immune system
 *
 * BIOLOGICAL BASIS:
 * - Immune system detects foreign/abnormal patterns
 * - Adversarial examples are "pathogens"
 *
 * @param ctx Adversarial context
 * @param brain_immune Brain immune system
 * @return 0 on success, negative on error
 */
int adv_connect_brain_immune(adv_ctx_t* ctx, void* brain_immune);

/**
 * @brief Connect to predictive immune system
 *
 * @param ctx Adversarial context
 * @param predictive_immune Predictive immune system
 * @return 0 on success, negative on error
 */
int adv_connect_predictive_immune(adv_ctx_t* ctx, void* predictive_immune);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get adversarial training statistics
 *
 * @param ctx Adversarial context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int adv_get_stats(const adv_ctx_t* ctx, adv_stats_t* stats);

/**
 * @brief Reset adversarial training statistics
 *
 * @param ctx Adversarial context
 */
void adv_reset_stats(adv_ctx_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get attack name
 */
const char* adv_attack_name(adv_attack_t attack);

/**
 * @brief Get training method name
 */
const char* adv_train_method_name(adv_train_method_t method);

/**
 * @brief Get norm name
 */
const char* adv_norm_name(adv_norm_t norm);

/**
 * @brief Validate adversarial configuration
 */
int adv_validate_config(const adv_config_t* config);

/**
 * @brief Free adversarial example
 */
void adv_free_example(adv_example_t* example);

/**
 * @brief Project perturbation to epsilon ball
 */
void adv_project_perturbation(
    float* perturbation,
    size_t size,
    float epsilon,
    adv_norm_t norm
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ADVERSARIAL_TRAINING_H */
