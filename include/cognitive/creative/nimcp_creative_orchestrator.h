//=============================================================================
// nimcp_creative_orchestrator.h - Creative Cognitive Orchestrator
//=============================================================================
/**
 * @file nimcp_creative_orchestrator.h
 * @brief Master orchestrator for all creative subsystems
 *
 * WHAT: Coordinates appreciation, inspiration, and generation subsystems
 * WHY:  Unified management of complex creative pipeline
 * HOW:  Component composition with update cycling and resource management
 *
 * ARCHITECTURE:
 * +===========================================================================+
 * |                     Creative Cognitive Orchestrator                        |
 * +===========================================================================+
 * |  +-----------------------------------------------------------------------+
 * |  |                     Core Creative Modules                              |
 * |  +-----------------------------------------------------------------------+
 * |  | Aesthetic Eval | Style Repr | Influence Blend | Pattern Extract       |
 * |  +-----------------------------------------------------------------------+
 * |  +------------------------+  +------------------------+
 * |  |    Appreciation        |  |    Generation          |
 * |  +------------------------+  +------------------------+
 * |  | Emotion Bridge         |  | Text Generator         |
 * |  | Memory Bridge          |  | Music Generator        |
 * |  | Style Perception       |  | Visual Generator       |
 * |  +------------------------+  | Multimodal Director    |
 * |                              +------------------------+
 * |  +------------------------+  +------------------------+
 * |  |    External Models     |  |    Bridges             |
 * |  +------------------------+  +------------------------+
 * |  | ONNX Runtime           |  | Creative Bridge        |
 * |  | Diffusion Bridge       |  | Neural Bridge          |
 * |  | GAN Bridge             |  | Ethics Bridge          |
 * |  | API Client             |  | Training Bridge        |
 * |  +------------------------+  +------------------------+
 * +===========================================================================+
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_ORCHESTRATOR_H
#define NIMCP_CREATIVE_ORCHESTRATOR_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Submodules
//=============================================================================

/* Appreciation */
typedef struct aesthetic_evaluator aesthetic_evaluator_t;
typedef struct creative_emotion_bridge creative_emotion_bridge_t;
typedef struct creative_memory_bridge creative_memory_bridge_t;
typedef struct style_perception style_perception_t;

/* Inspiration */
typedef struct style_representer style_representer_t;
typedef struct influence_blender influence_blender_t;
typedef struct creative_pattern_extractor creative_pattern_extractor_t;
typedef struct creative_knowledge_bridge creative_knowledge_bridge_t;

/* Generation */
typedef struct text_generator text_generator_t;
typedef struct music_generator music_generator_t;
typedef struct visual_generator visual_generator_t;
typedef struct video_generator video_generator_t;
typedef struct multimodal_director multimodal_director_t;

/* External */
typedef struct creative_onnx_runtime creative_onnx_runtime_t;
typedef struct creative_api_client creative_api_client_t;
typedef struct diffusion_bridge diffusion_bridge_t;
typedef struct gan_bridge gan_bridge_t;

/* Bridges */
typedef struct creative_bridge creative_bridge_t;
typedef struct creative_neural_bridge creative_neural_bridge_t;
typedef struct creative_ethics_bridge creative_ethics_bridge_t;
typedef struct creative_training_bridge creative_training_bridge_t;

//=============================================================================
// Orchestrator Statistics
//=============================================================================

/**
 * @brief Statistics for creative orchestrator
 */
typedef struct {
    /* Appreciation statistics */
    uint64_t text_evaluations;          /**< Number of text evaluations */
    uint64_t music_evaluations;         /**< Number of music evaluations */
    uint64_t visual_evaluations;        /**< Number of visual evaluations */
    float avg_evaluation_time_ms;       /**< Average evaluation time */

    /* Inspiration statistics */
    uint64_t style_extractions;         /**< Number of style extractions */
    uint64_t influence_blends;          /**< Number of influence blends */
    float avg_blend_coherence;          /**< Average blend coherence */

    /* Generation statistics */
    uint64_t texts_generated;           /**< Number of texts generated */
    uint64_t music_pieces_generated;    /**< Number of music pieces generated */
    uint64_t images_generated;          /**< Number of images generated */
    uint64_t videos_generated;          /**< Number of videos generated */
    float avg_text_gen_time_ms;         /**< Average text generation time */
    float avg_music_gen_time_ms;        /**< Average music generation time */
    float avg_visual_gen_time_ms;       /**< Average visual generation time */
    float avg_video_gen_time_ms;        /**< Average video generation time */

    /* Quality statistics */
    float avg_generated_quality;        /**< Average quality of generations */
    uint64_t validations_passed;        /**< Validations passed */
    uint64_t validations_failed;        /**< Validations failed */
    uint64_t copyright_denials;         /**< Copyright-related denials */
    uint64_t ethics_denials;            /**< Ethics-related denials */

    /* Resource usage */
    uint64_t peak_memory_bytes;         /**< Peak memory usage */
    uint64_t total_gpu_time_ms;         /**< Total GPU time used */
    uint64_t api_calls;                 /**< Number of external API calls */

    /* System health */
    uint64_t update_cycles;             /**< Number of update cycles */
    uint64_t last_update_us;            /**< Last update timestamp */
    float system_load;                  /**< [0-1] Current system load */
    bool is_healthy;                    /**< System health status */
} creative_orchestrator_stats_t;

//=============================================================================
// Orchestrator State
//=============================================================================

/**
 * @brief Creative orchestrator state
 */
typedef enum {
    CREATIVE_STATE_UNINITIALIZED = 0,  /**< Not yet initialized */
    CREATIVE_STATE_INITIALIZING,       /**< Currently initializing */
    CREATIVE_STATE_READY,              /**< Ready for operations */
    CREATIVE_STATE_GENERATING,         /**< Actively generating content */
    CREATIVE_STATE_EVALUATING,         /**< Evaluating content */
    CREATIVE_STATE_ERROR,              /**< Error state */
    CREATIVE_STATE_SHUTDOWN            /**< Shutting down */
} creative_orchestrator_state_t;

//=============================================================================
// Orchestrator Structure
//=============================================================================

/**
 * @brief Creative cognitive orchestrator
 *
 * Master coordinator for all creative subsystems
 */
struct creative_orchestrator {
    /* Configuration */
    creative_config_t config;           /**< System configuration */
    creative_orchestrator_state_t state; /**< Current state */

    /* === Appreciation Subsystem === */
    aesthetic_evaluator_t* aesthetic_eval;      /**< Aesthetic quality evaluator */
    creative_emotion_bridge_t* emotion_bridge;  /**< Emotion-aesthetic bridge */
    creative_memory_bridge_t* memory_bridge;    /**< Artistic memory bridge */
    style_perception_t* style_perception;       /**< Style recognition */

    /* === Inspiration Subsystem === */
    style_representer_t* style_repr;            /**< Style embedding system */
    influence_blender_t* influence_blend;       /**< Multi-influence blending */
    creative_pattern_extractor_t* pattern_ext;  /**< Pattern extraction */
    creative_knowledge_bridge_t* knowledge_bridge; /**< KG integration */

    /* === Generation Subsystem === */
    text_generator_t* text_gen;                 /**< Text generation */
    music_generator_t* music_gen;               /**< Music generation */
    visual_generator_t* visual_gen;             /**< Visual generation */
    video_generator_t* video_gen;               /**< Video generation */
    multimodal_director_t* mm_director;         /**< Full-length coordination */

    /* === External Model Integration === */
    creative_onnx_runtime_t* onnx_runtime;      /**< ONNX Runtime wrapper */
    creative_api_client_t* api_client;          /**< Cloud API client */
    diffusion_bridge_t* diffusion_bridge;       /**< Diffusion model interface */
    gan_bridge_t* gan_bridge;                   /**< GAN model interface */

    /* === Integration Bridges === */
    creative_bridge_t* creative_bridge;         /**< Main validation bridge */
    creative_neural_bridge_t* neural_bridge;    /**< Neural generation interface */
    creative_ethics_bridge_t* ethics_bridge;    /**< Copyright/ethics validation */
    creative_training_bridge_t* training_bridge; /**< Training/fine-tuning */

    /* === Brain Integration (set externally) === */
    void* brain;                                /**< brain_t pointer */
    void* emotion_system;                       /**< emotional_system_t pointer */
    void* hippocampus;                          /**< hippocampus pointer */
    void* ethics_engine;                        /**< ethics_engine_t pointer */
    void* immune_system;                        /**< brain_immune_system_t pointer */
    void* temporal_lobe;                        /**< temporal_adapter pointer */
    void* vae_system;                           /**< VAE system for latent spaces */

    /* === Statistics === */
    creative_orchestrator_stats_t stats;        /**< Runtime statistics */

    /* === Threading/Synchronization === */
    void* mutex;                                /**< Thread safety mutex */
    bool owns_mutex;                            /**< Whether we own the mutex */

    /* === Resource Management === */
    void* memory_pool;                          /**< Memory pool for allocations */
    uint64_t allocated_bytes;                   /**< Current allocation total */
};

//=============================================================================
// Orchestrator Lifecycle API
//=============================================================================

/**
 * @brief Create creative orchestrator
 *
 * @param config Configuration (NULL for defaults)
 * @return Orchestrator handle or NULL on error
 */
creative_orchestrator_t* creative_orchestrator_create(const creative_config_t* config);

/**
 * @brief Destroy creative orchestrator
 *
 * @param orch Orchestrator to destroy
 */
void creative_orchestrator_destroy(creative_orchestrator_t* orch);

/**
 * @brief Initialize subsystems
 *
 * Called internally by create, but can be called separately for lazy init
 *
 * @param orch Orchestrator
 * @return 0 on success, -1 on error
 */
int creative_orchestrator_init_subsystems(creative_orchestrator_t* orch);

/**
 * @brief Shutdown all subsystems
 *
 * @param orch Orchestrator
 */
void creative_orchestrator_shutdown(creative_orchestrator_t* orch);

//=============================================================================
// Orchestrator Update API
//=============================================================================

/**
 * @brief Update orchestrator (periodic tick)
 *
 * @param orch Orchestrator
 * @param dt_us Time delta in microseconds
 * @return 0 on success, -1 on error
 */
int creative_orchestrator_update(creative_orchestrator_t* orch, uint64_t dt_us);

/**
 * @brief Get orchestrator state
 *
 * @param orch Orchestrator
 * @return Current state
 */
creative_orchestrator_state_t creative_orchestrator_get_state(
    const creative_orchestrator_t* orch);

/**
 * @brief Get orchestrator statistics
 *
 * @param orch Orchestrator
 * @param out Output statistics
 * @return 0 on success, -1 on error
 */
int creative_orchestrator_get_stats(creative_orchestrator_t* orch,
                                     creative_orchestrator_stats_t* out);

/**
 * @brief Reset statistics
 *
 * @param orch Orchestrator
 */
void creative_orchestrator_reset_stats(creative_orchestrator_t* orch);

//=============================================================================
// Brain Integration API
//=============================================================================

/**
 * @brief Set brain reference for integration
 *
 * @param orch Orchestrator
 * @param brain Brain pointer (brain_t)
 */
void creative_orchestrator_set_brain(creative_orchestrator_t* orch, void* brain);

/**
 * @brief Set emotion system reference
 *
 * @param orch Orchestrator
 * @param emotion Emotion system pointer
 */
void creative_orchestrator_set_emotion_system(creative_orchestrator_t* orch,
                                               void* emotion);

/**
 * @brief Set hippocampus reference
 *
 * @param orch Orchestrator
 * @param hippo Hippocampus pointer
 */
void creative_orchestrator_set_hippocampus(creative_orchestrator_t* orch,
                                            void* hippo);

/**
 * @brief Set ethics engine reference
 *
 * @param orch Orchestrator
 * @param ethics Ethics engine pointer
 */
void creative_orchestrator_set_ethics_engine(creative_orchestrator_t* orch,
                                              void* ethics);

/**
 * @brief Set immune system reference
 *
 * @param orch Orchestrator
 * @param immune Immune system pointer
 */
void creative_orchestrator_set_immune_system(creative_orchestrator_t* orch,
                                              void* immune);

/**
 * @brief Set temporal lobe reference
 *
 * @param orch Orchestrator
 * @param temporal Temporal adapter pointer
 */
void creative_orchestrator_set_temporal_lobe(creative_orchestrator_t* orch,
                                              void* temporal);

/**
 * @brief Set VAE system reference
 *
 * @param orch Orchestrator
 * @param vae VAE system pointer
 */
void creative_orchestrator_set_vae_system(creative_orchestrator_t* orch,
                                           void* vae);

//=============================================================================
// Component Accessors
//=============================================================================

/**
 * @brief Get aesthetic evaluator
 *
 * @param orch Orchestrator
 * @return Aesthetic evaluator or NULL
 */
aesthetic_evaluator_t* creative_orchestrator_get_aesthetic_eval(
    creative_orchestrator_t* orch);

/**
 * @brief Get style representer
 *
 * @param orch Orchestrator
 * @return Style representer or NULL
 */
style_representer_t* creative_orchestrator_get_style_repr(
    creative_orchestrator_t* orch);

/**
 * @brief Get text generator
 *
 * @param orch Orchestrator
 * @return Text generator or NULL
 */
text_generator_t* creative_orchestrator_get_text_gen(
    creative_orchestrator_t* orch);

/**
 * @brief Get music generator
 *
 * @param orch Orchestrator
 * @return Music generator or NULL
 */
music_generator_t* creative_orchestrator_get_music_gen(
    creative_orchestrator_t* orch);

/**
 * @brief Get visual generator
 *
 * @param orch Orchestrator
 * @return Visual generator or NULL
 */
visual_generator_t* creative_orchestrator_get_visual_gen(
    creative_orchestrator_t* orch);

/**
 * @brief Get diffusion bridge
 *
 * @param orch Orchestrator
 * @return Diffusion bridge or NULL
 */
diffusion_bridge_t* creative_orchestrator_get_diffusion_bridge(
    creative_orchestrator_t* orch);

/**
 * @brief Get creative validation bridge
 *
 * @param orch Orchestrator
 * @return Creative bridge or NULL
 */
creative_bridge_t* creative_orchestrator_get_creative_bridge(
    creative_orchestrator_t* orch);

/**
 * @brief Get multimodal director
 *
 * @param orch Orchestrator
 * @return Multimodal director or NULL
 */
multimodal_director_t* creative_orchestrator_get_director(
    creative_orchestrator_t* orch);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Update configuration at runtime
 *
 * @param orch Orchestrator
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int creative_orchestrator_update_config(creative_orchestrator_t* orch,
                                         const creative_config_t* config);

/**
 * @brief Get current configuration
 *
 * @param orch Orchestrator
 * @return Current configuration pointer
 */
const creative_config_t* creative_orchestrator_get_config(
    const creative_orchestrator_t* orch);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last error message
 *
 * @param orch Orchestrator
 * @return Error message or NULL
 */
const char* creative_orchestrator_get_last_error(
    const creative_orchestrator_t* orch);

/**
 * @brief Clear error state
 *
 * @param orch Orchestrator
 */
void creative_orchestrator_clear_error(creative_orchestrator_t* orch);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_ORCHESTRATOR_H */
