/**
 * @file nimcp_knowledge_distillation.h
 * @brief Knowledge Distillation for NIMCP Training
 *
 * WHAT: Transfer knowledge from teacher model(s) to student model
 * WHY:  Compress models, improve student accuracy, enable ensemble distillation
 * HOW:  Soft labels, feature matching, attention transfer, relation-based distillation
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: Manual implementation with nn.KLDivLoss, teacher.eval()
 * - JAX: Optax distillation losses
 * - TensorFlow: tf.keras.losses with teacher predictions
 *
 * NIMCP APPROACH:
 * - Integrates with loss_functions module for distillation losses
 * - Connects to brain factory for multi-region distillation
 * - Bio-async for distributed teacher-student training
 *
 * BIOLOGICAL GROUNDING:
 * - Models developmental learning from mature brain regions
 * - Prefrontal guidance of posterior cortex (top-down attention)
 * - Expert systems teaching novice circuits
 * - Synaptic consolidation during sleep (memory distillation)
 *
 * INTEGRATION POINTS:
 * - loss_functions: Custom distillation losses
 * - brain_factory: Multi-region knowledge transfer
 * - cognitive_modules: Transfer reasoning patterns
 * - thalamic_router: Route teacher signals to student
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_KNOWLEDGE_DISTILLATION_H
#define NIMCP_KNOWLEDGE_DISTILLATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"
#include "middleware/training/nimcp_loss_functions.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define KD_DEFAULT_TEMPERATURE        4.0f   /**< Default distillation temperature */
#define KD_DEFAULT_ALPHA              0.7f   /**< Default soft label weight */
#define KD_MAX_TEACHERS               16     /**< Maximum ensemble teachers */
#define KD_MAX_FEATURE_LAYERS         32     /**< Maximum feature matching layers */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Knowledge distillation method
 *
 * COMPARISON:
 * - Hinton (2015): Response-based with temperature
 * - FitNets (2014): Feature-based with hints
 * - Attention Transfer (2017): Attention map matching
 * - Relational KD (2019): Instance relation matching
 */
typedef enum {
    KD_METHOD_RESPONSE = 0,          /**< Response-based (soft labels) */
    KD_METHOD_FEATURE,               /**< Feature-based (FitNets) */
    KD_METHOD_ATTENTION,             /**< Attention transfer */
    KD_METHOD_RELATIONAL,            /**< Relational knowledge */
    KD_METHOD_SELF_DISTILLATION,     /**< Self-distillation (Born-Again) */
    KD_METHOD_MUTUAL,                /**< Mutual distillation (DML) */
    KD_METHOD_ENSEMBLE,              /**< Multi-teacher ensemble */
    KD_METHOD_PROGRESSIVE,           /**< Progressive distillation */
    KD_METHOD_ONLINE,                /**< Online distillation */
    KD_METHOD_HYBRID,                /**< Combine multiple methods */
    KD_METHOD_COUNT
} kd_method_t;

/**
 * @brief Distillation loss type
 */
typedef enum {
    KD_LOSS_KL_DIV = 0,              /**< KL divergence */
    KD_LOSS_MSE,                     /**< Mean squared error */
    KD_LOSS_COSINE,                  /**< Cosine similarity loss */
    KD_LOSS_L1,                      /**< L1 / MAE loss */
    KD_LOSS_FOCAL,                   /**< Focal loss (hard samples) */
    KD_LOSS_PEARSON,                 /**< Pearson correlation loss */
    KD_LOSS_COUNT
} kd_loss_type_t;

/**
 * @brief Feature matching strategy
 */
typedef enum {
    KD_FEATURE_MATCH_DIRECT = 0,     /**< Direct feature matching */
    KD_FEATURE_MATCH_REGRESSOR,      /**< Learnable regressor layer */
    KD_FEATURE_MATCH_PROJECTOR,      /**< Projection to common space */
    KD_FEATURE_MATCH_GRAM,           /**< Gram matrix matching */
    KD_FEATURE_MATCH_COUNT
} kd_feature_match_t;

/**
 * @brief Teacher ensemble weighting
 */
typedef enum {
    KD_ENSEMBLE_UNIFORM = 0,         /**< Equal weights */
    KD_ENSEMBLE_CONFIDENCE,          /**< Weight by confidence */
    KD_ENSEMBLE_PERFORMANCE,         /**< Weight by performance */
    KD_ENSEMBLE_LEARNED,             /**< Learnable weights */
    KD_ENSEMBLE_COUNT
} kd_ensemble_weight_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Response-based distillation config (Hinton 2015)
 *
 * BIOLOGICAL BASIS:
 * - Temperature models neural noise / uncertainty
 * - Soft labels provide richer supervision than hard targets
 * - Similar to probabilistic population coding
 */
typedef struct {
    float temperature;               /**< Softmax temperature (T > 1 softens) */
    float alpha;                     /**< Weight for soft labels [0, 1] */
    kd_loss_type_t loss_type;        /**< Loss for soft label matching */
    bool use_teacher_logits;         /**< Match logits (pre-softmax) */
} kd_response_config_t;

/**
 * @brief Feature-based distillation config (FitNets)
 *
 * BIOLOGICAL BASIS:
 * - Models intermediate representation matching
 * - Similar to aligning feature maps between brain regions
 */
typedef struct {
    kd_feature_match_t match_method; /**< Feature matching method */
    uint32_t* teacher_layers;        /**< Teacher layer indices to match */
    uint32_t* student_layers;        /**< Corresponding student layers */
    uint32_t num_layers;             /**< Number of layer pairs */
    float feature_weight;            /**< Weight for feature loss */
    bool normalize_features;         /**< L2 normalize features */
} kd_feature_config_t;

/**
 * @brief Attention transfer config (Zagoruyko 2017)
 *
 * BIOLOGICAL BASIS:
 * - Models attention pattern transfer between regions
 * - Prefrontal → posterior attention guidance
 */
typedef struct {
    uint32_t* attention_layers;      /**< Layers to transfer attention */
    uint32_t num_layers;             /**< Number of attention layers */
    float attention_weight;          /**< Weight for attention loss */
    bool use_spatial_attention;      /**< Spatial attention maps */
    bool use_channel_attention;      /**< Channel attention maps */
    float p_norm;                    /**< Lp norm for attention (default 2) */
} kd_attention_config_t;

/**
 * @brief Relational distillation config (RKD)
 *
 * BIOLOGICAL BASIS:
 * - Models relational knowledge (analogical reasoning)
 * - Instance relationships in semantic space
 */
typedef struct {
    bool use_distance_wise;          /**< Distance-wise relations */
    bool use_angle_wise;             /**< Angle-wise relations */
    float distance_weight;           /**< Weight for distance loss */
    float angle_weight;              /**< Weight for angle loss */
    bool use_attention_pooling;      /**< Attention-weighted pooling */
} kd_relational_config_t;

/**
 * @brief Self-distillation config (Born-Again Networks)
 *
 * BIOLOGICAL BASIS:
 * - Models iterative refinement during sleep/consolidation
 * - Self-teaching through memory replay
 */
typedef struct {
    uint32_t num_generations;        /**< Number of self-distillation generations */
    bool reinitialize_student;       /**< Reinitialize student each generation */
    float temperature_decay;         /**< Temperature decay per generation */
    bool save_all_generations;       /**< Save intermediate models */
} kd_self_distill_config_t;

/**
 * @brief Online/mutual distillation config (DML)
 *
 * BIOLOGICAL BASIS:
 * - Models peer learning between brain regions
 * - Bidirectional knowledge exchange
 */
typedef struct {
    uint32_t num_peers;              /**< Number of peer models */
    bool symmetric_kl;               /**< Symmetric KL (average) */
    float mutual_weight;             /**< Weight for mutual loss */
    bool share_classifier;           /**< Share final classifier */
} kd_mutual_config_t;

/**
 * @brief Ensemble distillation config
 */
typedef struct {
    kd_ensemble_weight_t weighting;  /**< Teacher weighting strategy */
    float* teacher_weights;          /**< Manual teacher weights */
    uint32_t num_teachers;           /**< Number of teachers */
    bool uncertainty_weighting;      /**< Weight by prediction uncertainty */
} kd_ensemble_config_t;

/**
 * @brief Complete knowledge distillation configuration
 */
typedef struct {
    kd_method_t method;              /**< Distillation method */

    /* Method-specific configs */
    kd_response_config_t response;
    kd_feature_config_t feature;
    kd_attention_config_t attention;
    kd_relational_config_t relational;
    kd_self_distill_config_t self_distill;
    kd_mutual_config_t mutual;
    kd_ensemble_config_t ensemble;

    /* General settings */
    float hard_label_weight;         /**< Weight for hard label loss [0, 1] */
    bool teacher_eval_mode;          /**< Teacher in eval mode (no dropout) */
    bool detach_teacher;             /**< No gradients through teacher */

    /* Integration */
    bool integrate_loss_functions;   /**< Use NIMCP loss_functions module */
    bool integrate_brain_factory;    /**< Enable brain region distillation */

    /* Debugging */
    bool verbose;
    bool track_statistics;
} kd_config_t;

//=============================================================================
// Teacher Model Structure
//=============================================================================

/**
 * @brief Teacher model wrapper
 *
 * Wraps any model (SNN, LNN, CNN, or brain module) as teacher
 */
typedef struct {
    void* model;                     /**< Opaque model pointer */
    const char* name;                /**< Teacher name */

    /* Forward function signature */
    int (*forward)(void* model, const nimcp_tensor_t* input,
                   nimcp_tensor_t* output, nimcp_tensor_t** features,
                   uint32_t* num_features);

    /* Optional attention extraction */
    int (*get_attention)(void* model, nimcp_tensor_t** attention_maps,
                         uint32_t* num_maps);

    /* Optional eval mode control */
    void (*set_eval_mode)(void* model, bool eval);  /**< Set model eval mode */
    bool (*is_eval_mode)(void* model);              /**< Check if in eval mode */

    /* Performance metrics */
    float accuracy;                  /**< Teacher accuracy (for weighting) */
    float confidence;                /**< Average prediction confidence */
    bool force_eval_mode;            /**< Force teacher to eval mode during distillation */
} kd_teacher_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Knowledge distillation statistics
 */
typedef struct {
    uint64_t total_steps;            /**< Total training steps */
    uint64_t teacher_forward_count;  /**< Teacher forward passes */

    /* Loss components */
    double total_soft_loss;          /**< Cumulative soft label loss */
    double total_hard_loss;          /**< Cumulative hard label loss */
    double total_feature_loss;       /**< Cumulative feature loss */
    double total_attention_loss;     /**< Cumulative attention loss */

    /* Metrics */
    float student_accuracy;          /**< Student accuracy */
    float teacher_student_agreement; /**< Agreement rate */
    float knowledge_transfer_ratio;  /**< KTR metric */

    /* Performance */
    double teacher_time_ms;          /**< Time in teacher forward */
    double student_time_ms;          /**< Time in student forward */
    double distill_time_ms;          /**< Time computing distillation loss */
} kd_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Knowledge distillation context (opaque)
 */
typedef struct kd_ctx_s kd_ctx_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default KD configuration
 *
 * DEFAULTS:
 * - Response-based distillation
 * - Temperature: 4.0
 * - Alpha: 0.7 (70% soft, 30% hard)
 * - KL divergence loss
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int kd_default_config(kd_config_t* config);

/**
 * @brief Create KD context
 *
 * @param config KD configuration
 * @return KD context or NULL on failure
 */
kd_ctx_t* kd_create(const kd_config_t* config);

/**
 * @brief Destroy KD context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void kd_destroy(kd_ctx_t* ctx);

//=============================================================================
// Teacher Management API
//=============================================================================

/**
 * @brief Register teacher model
 *
 * WHAT: Add teacher model for distillation
 * WHY:  Define knowledge source
 * HOW:  Wrap model with forward function
 *
 * @param ctx KD context
 * @param teacher Teacher model wrapper
 * @return Teacher index or negative on error
 */
int kd_register_teacher(kd_ctx_t* ctx, const kd_teacher_t* teacher);

/**
 * @brief Unregister teacher
 *
 * @param ctx KD context
 * @param teacher_idx Teacher index
 * @return 0 on success, negative on error
 */
int kd_unregister_teacher(kd_ctx_t* ctx, uint32_t teacher_idx);

/**
 * @brief Get teacher count
 *
 * @param ctx KD context
 * @return Number of registered teachers
 */
uint32_t kd_get_teacher_count(const kd_ctx_t* ctx);

/**
 * @brief Set teacher weights for ensemble
 *
 * @param ctx KD context
 * @param weights Weight array [num_teachers]
 * @return 0 on success, negative on error
 */
int kd_set_teacher_weights(kd_ctx_t* ctx, const float* weights);

//=============================================================================
// Distillation Loss API
//=============================================================================

/**
 * @brief Compute distillation loss
 *
 * WHAT: Compute combined distillation + task loss
 * WHY:  Main training objective with knowledge transfer
 * HOW:  Forward through teacher, compute soft label loss
 *
 * COMPARISON (PyTorch equivalent):
 * ```python
 * with torch.no_grad():
 *     teacher_logits = teacher(input)
 * soft_loss = F.kl_div(
 *     F.log_softmax(student_logits / T, dim=1),
 *     F.softmax(teacher_logits / T, dim=1),
 *     reduction='batchmean'
 * ) * (T * T)
 * hard_loss = F.cross_entropy(student_logits, targets)
 * loss = alpha * soft_loss + (1 - alpha) * hard_loss
 * ```
 *
 * @param ctx KD context
 * @param student_logits Student model output
 * @param input Input for teacher forward
 * @param targets Hard labels
 * @param loss Output total loss
 * @return 0 on success, negative on error
 */
int kd_compute_loss(
    kd_ctx_t* ctx,
    const nimcp_tensor_t* student_logits,
    const nimcp_tensor_t* input,
    const nimcp_tensor_t* targets,
    float* loss
);

/**
 * @brief Compute distillation loss with feature matching
 *
 * @param ctx KD context
 * @param student_logits Student logits
 * @param student_features Student intermediate features
 * @param num_student_features Number of student features
 * @param input Input for teacher
 * @param targets Hard labels
 * @param loss Output total loss
 * @return 0 on success, negative on error
 */
int kd_compute_loss_with_features(
    kd_ctx_t* ctx,
    const nimcp_tensor_t* student_logits,
    nimcp_tensor_t** student_features,
    uint32_t num_student_features,
    const nimcp_tensor_t* input,
    const nimcp_tensor_t* targets,
    float* loss
);

/**
 * @brief Compute response-based loss only
 *
 * @param ctx KD context
 * @param student_logits Student logits
 * @param teacher_logits Teacher logits
 * @param temperature Temperature
 * @return Soft label loss
 */
float kd_response_loss(
    kd_ctx_t* ctx,
    const nimcp_tensor_t* student_logits,
    const nimcp_tensor_t* teacher_logits,
    float temperature
);

/**
 * @brief Compute feature matching loss
 *
 * @param ctx KD context
 * @param student_features Student features
 * @param teacher_features Teacher features
 * @param num_features Number of feature pairs
 * @return Feature matching loss
 */
float kd_feature_loss(
    kd_ctx_t* ctx,
    nimcp_tensor_t** student_features,
    nimcp_tensor_t** teacher_features,
    uint32_t num_features
);

/**
 * @brief Compute attention transfer loss
 *
 * @param ctx KD context
 * @param student_attention Student attention maps
 * @param teacher_attention Teacher attention maps
 * @param num_layers Number of attention layers
 * @return Attention loss
 */
float kd_attention_loss(
    kd_ctx_t* ctx,
    nimcp_tensor_t** student_attention,
    nimcp_tensor_t** teacher_attention,
    uint32_t num_layers
);

/**
 * @brief Compute relational distillation loss
 *
 * @param ctx KD context
 * @param student_embeddings Student embeddings [batch, dim]
 * @param teacher_embeddings Teacher embeddings [batch, dim]
 * @return Relational loss
 */
float kd_relational_loss(
    kd_ctx_t* ctx,
    const nimcp_tensor_t* student_embeddings,
    const nimcp_tensor_t* teacher_embeddings
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to brain factory for region distillation
 *
 * WHAT: Enable knowledge transfer between brain regions
 * WHY:  Mature regions guide developing regions
 * HOW:  Prefrontal → posterior, expert → novice
 *
 * @param ctx KD context
 * @param brain_factory Brain factory
 * @return 0 on success, negative on error
 */
int kd_connect_brain_factory(kd_ctx_t* ctx, void* brain_factory);

/**
 * @brief Connect to loss functions module
 *
 * @param ctx KD context
 * @param loss_ctx Loss function context
 * @return 0 on success, negative on error
 */
int kd_connect_loss_functions(kd_ctx_t* ctx, nimcp_loss_context_t* loss_ctx);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get KD statistics
 *
 * @param ctx KD context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int kd_get_stats(const kd_ctx_t* ctx, kd_stats_t* stats);

/**
 * @brief Reset KD statistics
 *
 * @param ctx KD context
 */
void kd_reset_stats(kd_ctx_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get method name
 */
const char* kd_method_name(kd_method_t method);

/**
 * @brief Get loss type name
 */
const char* kd_loss_type_name(kd_loss_type_t type);

/**
 * @brief Validate KD configuration
 */
int kd_validate_config(const kd_config_t* config);

/**
 * @brief Compute softmax with temperature
 *
 * @param logits Input logits
 * @param output Output probabilities
 * @param count Number of classes
 * @param temperature Temperature (T > 1 softens)
 */
void kd_softmax_temperature(
    const float* logits,
    float* output,
    size_t count,
    float temperature
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KNOWLEDGE_DISTILLATION_H */
