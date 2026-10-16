/**
 * @file nimcp_mirror_multimodal.h
 * @brief Multi-Modal Action Features for Mirror Neurons
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Multi-modal action feature representation for enhanced mirror neuron processing
 * WHY:  Actions are perceived through multiple sensory modalities - visual, motor, auditory, semantic
 * HOW:  Unified feature structure with modality-specific vectors and cross-modal fusion
 *
 * BIOLOGICAL BASIS:
 * ==============================================================================
 * Mirror neurons in the premotor cortex (F5) and inferior parietal lobule (IPL)
 * integrate information from multiple sensory modalities:
 *
 * 1. Visual Features (V5/MT, STS)
 *    - Motion direction, velocity, trajectory
 *    - Biological motion patterns
 *    - Body part configuration
 *
 * 2. Motor Features (F5, primary motor cortex)
 *    - Effector type (hand, mouth, foot)
 *    - Grip type, force profile
 *    - Movement kinematics
 *
 * 3. Auditory Features (auditory cortex, STS)
 *    - Action sounds (grasping, walking)
 *    - Vocalization patterns
 *    - Audio-motor associations
 *
 * 4. Semantic Features (temporal cortex, STS)
 *    - Action category (transitive/intransitive)
 *    - Goal representation
 *    - Object affordances
 *
 * ARCHITECTURE:
 * ==============================================================================
 *        Visual Input    Motor Feedback   Audio Input    Semantic Context
 *             |               |               |                |
 *             v               v               v                v
 *        +---------+     +---------+     +---------+     +---------+
 *        | Visual  |     |  Motor  |     | Auditory|     | Semantic|
 *        | Features|     | Features|     | Features|     | Features|
 *        +---------+     +---------+     +---------+     +---------+
 *             |               |               |                |
 *             +-------+-------+-------+-------+
 *                     |
 *                     v
 *            +------------------+
 *            | Feature Fusion   |
 *            | (weighted combo) |
 *            +------------------+
 *                     |
 *                     v
 *            +------------------+
 *            | Mirror Neuron    |
 *            | Action Matching  |
 *            +------------------+
 *
 * KEY FEATURES:
 * ==============================================================================
 * 1. Modality-Specific Feature Vectors
 *    - Each modality has configurable dimension
 *    - Supports sparse or dense representations
 *
 * 2. Cross-Modal Fusion
 *    - Weighted combination of modality features
 *    - Configurable fusion weights based on context
 *
 * 3. Action <-> Multimodal Conversion
 *    - Convert simple action_t to rich multimodal representation
 *    - Extract compact action from multimodal features
 *
 * 4. Feature Comparison
 *    - Modality-specific similarity metrics
 *    - Cross-modal coherence checking
 *
 * NIMCP CODING STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT/WHY/HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free
 *
 * @see nimcp_mirror_neurons.h
 * @see Phase 10.11.8 - Multi-Modal Mirror Neuron Processing
 */

#ifndef NIMCP_MIRROR_MULTIMODAL_H
#define NIMCP_MIRROR_MULTIMODAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Defaults
//=============================================================================

/** @brief Maximum visual feature dimensions */
#define NIMCP_MULTIMODAL_VISUAL_DIM         16

/** @brief Maximum motor feature dimensions */
#define NIMCP_MULTIMODAL_MOTOR_DIM          16

/** @brief Maximum auditory feature dimensions */
#define NIMCP_MULTIMODAL_AUDITORY_DIM       8

/** @brief Maximum semantic feature dimensions */
#define NIMCP_MULTIMODAL_SEMANTIC_DIM       8

/** @brief Total fused feature dimensions */
#define NIMCP_MULTIMODAL_FUSED_DIM          32

/** @brief Default visual weight in fusion */
#define NIMCP_MULTIMODAL_VISUAL_WEIGHT      0.35f

/** @brief Default motor weight in fusion */
#define NIMCP_MULTIMODAL_MOTOR_WEIGHT       0.35f

/** @brief Default auditory weight in fusion */
#define NIMCP_MULTIMODAL_AUDITORY_WEIGHT    0.15f

/** @brief Default semantic weight in fusion */
#define NIMCP_MULTIMODAL_SEMANTIC_WEIGHT    0.15f

//=============================================================================
// Feature Types
//=============================================================================

/**
 * @brief Visual feature modality type
 *
 * WHAT: Types of visual features for action recognition
 * WHY:  Different visual cues carry different information about actions
 */
typedef enum {
    VISUAL_FEATURE_MOTION = 0,       /**< Motion direction/velocity */
    VISUAL_FEATURE_POSTURE,          /**< Body configuration */
    VISUAL_FEATURE_TRAJECTORY,       /**< Movement path */
    VISUAL_FEATURE_BIOLOGICAL,       /**< Biological motion pattern */
    VISUAL_FEATURE_OBJECT,           /**< Manipulated object features */
    VISUAL_FEATURE_COUNT
} visual_feature_type_t;

/**
 * @brief Motor feature modality type
 *
 * WHAT: Types of motor features for action execution/planning
 * WHY:  Motor features encode how an action is performed
 */
typedef enum {
    MOTOR_FEATURE_EFFECTOR = 0,      /**< Which body part (hand/foot/mouth) */
    MOTOR_FEATURE_GRIP,              /**< Grip type (precision/power/etc) */
    MOTOR_FEATURE_FORCE,             /**< Force profile */
    MOTOR_FEATURE_KINEMATICS,        /**< Joint angles/velocities */
    MOTOR_FEATURE_TIMING,            /**< Temporal pattern */
    MOTOR_FEATURE_COUNT
} motor_feature_type_t;

/**
 * @brief Auditory feature modality type
 *
 * WHAT: Types of auditory features associated with actions
 * WHY:  Many actions produce characteristic sounds
 */
typedef enum {
    AUDITORY_FEATURE_ACTION_SOUND = 0, /**< Sound produced by action */
    AUDITORY_FEATURE_VOCALIZATION,     /**< Speech/vocal patterns */
    AUDITORY_FEATURE_RHYTHM,           /**< Rhythmic patterns */
    AUDITORY_FEATURE_COUNT
} auditory_feature_type_t;

/**
 * @brief Semantic feature modality type
 *
 * WHAT: Types of semantic/conceptual features
 * WHY:  Actions have meaning beyond physical movements
 */
typedef enum {
    SEMANTIC_FEATURE_CATEGORY = 0,   /**< Action category (grasp/locomote/etc) */
    SEMANTIC_FEATURE_TRANSITIVITY,   /**< Transitive vs intransitive */
    SEMANTIC_FEATURE_GOAL,           /**< Goal/intention encoding */
    SEMANTIC_FEATURE_AFFORDANCE,     /**< Object affordances */
    SEMANTIC_FEATURE_COUNT
} semantic_feature_type_t;

//=============================================================================
// Feature Structures
//=============================================================================

/**
 * @brief Visual feature vector
 *
 * WHAT: Visual features extracted from action observation
 * WHY:  Visual cortex (V5/MT, STS) processing of biological motion
 */
typedef struct {
    float motion[3];                 /**< Motion direction (x,y,z) + velocity */
    float posture[4];                /**< Body part positions (normalized) */
    float trajectory[4];             /**< Trajectory shape parameters */
    float biological_motion;         /**< Biological motion confidence (0-1) */
    float object_features[4];        /**< Object being manipulated */
    float confidence;                /**< Overall visual confidence (0-1) */
    uint32_t valid_mask;             /**< Bitmask of valid feature types */
} visual_features_t;

/**
 * @brief Motor feature vector
 *
 * WHAT: Motor features encoding action execution parameters
 * WHY:  Premotor cortex (F5) representation of motor plans
 */
typedef struct {
    float effector[4];               /**< Effector encoding (hand/foot/mouth/eye) */
    float grip_type[4];              /**< Grip type encoding */
    float force_profile[4];          /**< Force magnitude and pattern */
    float kinematics[4];             /**< Joint angle derivatives */
    float timing;                    /**< Action duration/rhythm */
    float confidence;                /**< Motor feature confidence (0-1) */
    uint32_t valid_mask;             /**< Bitmask of valid feature types */
} motor_features_t;

/**
 * @brief Auditory feature vector
 *
 * WHAT: Auditory features associated with actions
 * WHY:  Audio-motor neurons respond to action sounds
 */
typedef struct {
    float action_sound[4];           /**< Characteristic action sound */
    float vocalization[2];           /**< Associated speech/vocals */
    float rhythm[2];                 /**< Rhythmic pattern features */
    float confidence;                /**< Auditory confidence (0-1) */
    uint32_t valid_mask;             /**< Bitmask of valid feature types */
} auditory_features_t;

/**
 * @brief Semantic feature vector
 *
 * WHAT: Semantic/conceptual features of actions
 * WHY:  High-level action understanding beyond kinematics
 */
typedef struct {
    float category[4];               /**< Action category embedding */
    float transitivity;              /**< Transitive (1) vs intransitive (0) */
    float goal[2];                   /**< Goal representation */
    float affordance;                /**< Object affordance strength */
    float confidence;                /**< Semantic confidence (0-1) */
    uint32_t valid_mask;             /**< Bitmask of valid feature types */
} semantic_features_t;

/**
 * @brief Complete multi-modal action features
 *
 * WHAT: Unified structure containing all modality features
 * WHY:  Enable rich action representation for mirror neuron matching
 */
typedef struct {
    // Individual modality features
    visual_features_t visual;        /**< Visual feature vector */
    motor_features_t motor;          /**< Motor feature vector */
    auditory_features_t auditory;    /**< Auditory feature vector */
    semantic_features_t semantic;    /**< Semantic feature vector */

    // Fusion weights (can be context-dependent)
    float visual_weight;             /**< Weight for visual fusion */
    float motor_weight;              /**< Weight for motor fusion */
    float auditory_weight;           /**< Weight for auditory fusion */
    float semantic_weight;           /**< Weight for semantic fusion */

    // Fused representation
    float fused[NIMCP_MULTIMODAL_FUSED_DIM]; /**< Combined feature vector */
    bool fused_valid;                /**< Whether fused features are computed */

    // Metadata
    uint32_t action_id;              /**< Associated action ID */
    uint64_t timestamp;              /**< Feature extraction timestamp */
    float overall_confidence;        /**< Combined confidence score */
} multimodal_action_features_t;

/**
 * @brief Fusion configuration
 *
 * WHAT: Parameters controlling cross-modal feature fusion
 * WHY:  Allow context-dependent weighting of modalities
 */
typedef struct {
    float visual_weight;             /**< Visual modality weight (0-1) */
    float motor_weight;              /**< Motor modality weight (0-1) */
    float auditory_weight;           /**< Auditory modality weight (0-1) */
    float semantic_weight;           /**< Semantic modality weight (0-1) */
    bool normalize_weights;          /**< Auto-normalize to sum=1 */
    bool require_min_confidence;     /**< Require minimum confidence per modality */
    float min_confidence;            /**< Minimum confidence threshold */
} fusion_config_t;

//=============================================================================
// Forward Declarations
//=============================================================================

/**
 * @brief Forward declaration for action_t from mirror neurons
 */
typedef struct {
    uint32_t action_id;
    char action_name[64];
    float features[32];
    uint32_t num_features;
    uint64_t timestamp;
    uint32_t agent_id;
    float confidence;
} action_t;

//=============================================================================
// Core API - Feature Initialization
//=============================================================================

/**
 * @brief Initialize multimodal features with defaults
 *
 * WHAT: Create empty multimodal features structure
 * WHY:  Safe initialization before populating features
 * HOW:  Zero all features, set default weights
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (no shared state)
 *
 * @return Initialized multimodal features structure
 */
multimodal_action_features_t multimodal_features_init(void);

/**
 * @brief Get default fusion configuration
 *
 * WHAT: Return sensible default fusion parameters
 * WHY:  Provide good starting point for feature fusion
 * HOW:  Return pre-configured fusion_config_t
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (no shared state)
 *
 * @return Default fusion configuration
 */
fusion_config_t multimodal_get_default_fusion_config(void);

//=============================================================================
// Core API - Feature Conversion
//=============================================================================

/**
 * @brief Convert action_t to multimodal features
 *
 * WHAT: Expand simple action representation to multi-modal features
 * WHY:  Enable richer action matching using multiple modalities
 * HOW:  Map action.features to visual/motor components, estimate others
 *
 * The conversion maps the 32-float action.features array as:
 * - features[0-7]   -> visual features
 * - features[8-15]  -> motor features
 * - features[16-23] -> auditory features
 * - features[24-31] -> semantic features
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (no shared state)
 *
 * @param action Source action structure
 * @param out_features Output multimodal features
 * @return true on success, false on error
 */
bool multimodal_from_action(const action_t* action,
                            multimodal_action_features_t* out_features);

/**
 * @brief Convert multimodal features to action_t
 *
 * WHAT: Compact multimodal features back to simple action
 * WHY:  Interface with systems expecting action_t
 * HOW:  Use fused features or concatenate modality features
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (no shared state)
 *
 * @param features Source multimodal features
 * @param out_action Output action structure
 * @return true on success, false on error
 */
bool multimodal_to_action(const multimodal_action_features_t* features,
                          action_t* out_action);

//=============================================================================
// Core API - Feature Setting
//=============================================================================

/**
 * @brief Set visual features from observation
 *
 * WHAT: Populate visual feature vector
 * WHY:  Update from visual processing pipeline
 * HOW:  Copy features and set validity mask
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param features Multimodal features to update
 * @param visual Visual features to set
 * @return true on success, false on error
 */
bool multimodal_set_visual(multimodal_action_features_t* features,
                           const visual_features_t* visual);

/**
 * @brief Set motor features from execution/planning
 *
 * WHAT: Populate motor feature vector
 * WHY:  Update from motor cortex or execution feedback
 * HOW:  Copy features and set validity mask
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param features Multimodal features to update
 * @param motor Motor features to set
 * @return true on success, false on error
 */
bool multimodal_set_motor(multimodal_action_features_t* features,
                          const motor_features_t* motor);

/**
 * @brief Set auditory features from audio processing
 *
 * WHAT: Populate auditory feature vector
 * WHY:  Update from auditory processing pipeline
 * HOW:  Copy features and set validity mask
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param features Multimodal features to update
 * @param auditory Auditory features to set
 * @return true on success, false on error
 */
bool multimodal_set_auditory(multimodal_action_features_t* features,
                             const auditory_features_t* auditory);

/**
 * @brief Set semantic features from high-level processing
 *
 * WHAT: Populate semantic feature vector
 * WHY:  Update from conceptual/semantic processing
 * HOW:  Copy features and set validity mask
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param features Multimodal features to update
 * @param semantic Semantic features to set
 * @return true on success, false on error
 */
bool multimodal_set_semantic(multimodal_action_features_t* features,
                             const semantic_features_t* semantic);

//=============================================================================
// Core API - Feature Fusion
//=============================================================================

/**
 * @brief Compute fused feature vector
 *
 * WHAT: Combine modality features into single vector
 * WHY:  Enable efficient action matching with combined representation
 * HOW:  Weighted combination of modality features
 *
 * COMPLEXITY: O(FUSED_DIM)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param features Multimodal features to fuse
 * @param config Fusion configuration (NULL = use defaults)
 * @return true on success, false on error
 */
bool multimodal_compute_fusion(multimodal_action_features_t* features,
                               const fusion_config_t* config);

/**
 * @brief Update fusion weights
 *
 * WHAT: Change the modality weights used for fusion
 * WHY:  Allow context-dependent emphasis on different modalities
 * HOW:  Update weight values, optionally normalize
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param features Multimodal features to update
 * @param visual_weight New visual weight (0-1)
 * @param motor_weight New motor weight (0-1)
 * @param auditory_weight New auditory weight (0-1)
 * @param semantic_weight New semantic weight (0-1)
 * @param normalize Whether to normalize weights to sum=1
 * @return true on success, false on error
 */
bool multimodal_set_weights(multimodal_action_features_t* features,
                            float visual_weight,
                            float motor_weight,
                            float auditory_weight,
                            float semantic_weight,
                            bool normalize);

//=============================================================================
// Core API - Feature Comparison
//=============================================================================

/**
 * @brief Compare multimodal features (overall similarity)
 *
 * WHAT: Compute similarity between two multimodal feature sets
 * WHY:  Action matching using rich multi-modal representation
 * HOW:  Weighted combination of per-modality similarities
 *
 * COMPLEXITY: O(FUSED_DIM) if fused, else O(all modality dims)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param features_a First feature set
 * @param features_b Second feature set
 * @return Similarity score (0-1), or -1.0 on error
 */
float multimodal_compare(const multimodal_action_features_t* features_a,
                         const multimodal_action_features_t* features_b);

/**
 * @brief Compare visual features only
 *
 * WHAT: Compute visual-only similarity
 * WHY:  Assess visual match independent of other modalities
 * HOW:  Cosine similarity of visual feature vectors
 *
 * COMPLEXITY: O(VISUAL_DIM)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param features_a First feature set
 * @param features_b Second feature set
 * @return Visual similarity (0-1), or -1.0 on error
 */
float multimodal_compare_visual(const multimodal_action_features_t* features_a,
                                const multimodal_action_features_t* features_b);

/**
 * @brief Compare motor features only
 *
 * WHAT: Compute motor-only similarity
 * WHY:  Assess motor match independent of other modalities
 * HOW:  Cosine similarity of motor feature vectors
 *
 * COMPLEXITY: O(MOTOR_DIM)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param features_a First feature set
 * @param features_b Second feature set
 * @return Motor similarity (0-1), or -1.0 on error
 */
float multimodal_compare_motor(const multimodal_action_features_t* features_a,
                               const multimodal_action_features_t* features_b);

/**
 * @brief Compare auditory features only
 *
 * WHAT: Compute auditory-only similarity
 * WHY:  Assess audio match independent of other modalities
 * HOW:  Cosine similarity of auditory feature vectors
 *
 * COMPLEXITY: O(AUDITORY_DIM)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param features_a First feature set
 * @param features_b Second feature set
 * @return Auditory similarity (0-1), or -1.0 on error
 */
float multimodal_compare_auditory(const multimodal_action_features_t* features_a,
                                  const multimodal_action_features_t* features_b);

/**
 * @brief Compare semantic features only
 *
 * WHAT: Compute semantic-only similarity
 * WHY:  Assess conceptual match independent of other modalities
 * HOW:  Cosine similarity of semantic feature vectors
 *
 * COMPLEXITY: O(SEMANTIC_DIM)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param features_a First feature set
 * @param features_b Second feature set
 * @return Semantic similarity (0-1), or -1.0 on error
 */
float multimodal_compare_semantic(const multimodal_action_features_t* features_a,
                                  const multimodal_action_features_t* features_b);

/**
 * @brief Check cross-modal coherence
 *
 * WHAT: Verify that modalities are consistent with each other
 * WHY:  Detect conflicting or unreliable feature sets
 * HOW:  Check learned cross-modal correlations
 *
 * Example: Visual features showing "grasp" should be consistent with
 * motor features showing hand/grip activity
 *
 * COMPLEXITY: O(num_modality_pairs)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param features Multimodal features to check
 * @param out_coherence Output coherence score (0-1, can be NULL)
 * @return true if coherent (above threshold), false if incoherent
 */
bool multimodal_check_coherence(const multimodal_action_features_t* features,
                                float* out_coherence);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get string name for visual feature type
 *
 * WHAT: Human-readable name for visual feature enum
 * WHY:  Debugging and logging
 *
 * @param type Visual feature type
 * @return String name (static, do not free)
 */
const char* multimodal_visual_type_name(visual_feature_type_t type);

/**
 * @brief Get string name for motor feature type
 *
 * WHAT: Human-readable name for motor feature enum
 * WHY:  Debugging and logging
 *
 * @param type Motor feature type
 * @return String name (static, do not free)
 */
const char* multimodal_motor_type_name(motor_feature_type_t type);

/**
 * @brief Get string name for auditory feature type
 *
 * WHAT: Human-readable name for auditory feature enum
 * WHY:  Debugging and logging
 *
 * @param type Auditory feature type
 * @return String name (static, do not free)
 */
const char* multimodal_auditory_type_name(auditory_feature_type_t type);

/**
 * @brief Get string name for semantic feature type
 *
 * WHAT: Human-readable name for semantic feature enum
 * WHY:  Debugging and logging
 *
 * @param type Semantic feature type
 * @return String name (static, do not free)
 */
const char* multimodal_semantic_type_name(semantic_feature_type_t type);

/**
 * @brief Print multimodal features for debugging
 *
 * WHAT: Output human-readable feature dump
 * WHY:  Debugging and analysis
 *
 * @param features Features to print
 * @param prefix Line prefix (can be NULL)
 */
void multimodal_print(const multimodal_action_features_t* features,
                      const char* prefix);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIRROR_MULTIMODAL_H
