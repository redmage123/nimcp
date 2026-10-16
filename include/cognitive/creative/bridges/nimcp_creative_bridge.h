//=============================================================================
// nimcp_creative_bridge.h - Creative Content Validation Bridge
//=============================================================================
/**
 * @file nimcp_creative_bridge.h
 * @brief Main validation bridge for creative content
 *
 * WHAT: Defense-in-depth validation for generated creative content
 * WHY:  Ensure quality, originality, and safety of AI-generated art
 * HOW:  Multi-stage validation pipeline with configurable thresholds
 *
 * VALIDATION STAGES:
 * 1. Quality Check: Minimum aesthetic quality threshold
 * 2. Coherence Check: Internal consistency
 * 3. Copyright Check: Similarity to known works
 * 4. Ethics Check: Harmful content detection
 * 5. Originality Check: Novelty assessment
 *
 * DESIGN:
 * Follows defense-in-depth pattern from financial_bridge.h
 * Each stage can PASS, WARN, ESCALATE, or DENY
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_BRIDGE_H
#define NIMCP_CREATIVE_BRIDGE_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Validation Stage Types
//=============================================================================

/**
 * @brief Validation stages
 */
typedef enum {
    VALIDATION_STAGE_QUALITY = 0,  /**< Quality check */
    VALIDATION_STAGE_COHERENCE,    /**< Coherence check */
    VALIDATION_STAGE_COPYRIGHT,    /**< Copyright check */
    VALIDATION_STAGE_ETHICS,       /**< Ethics check */
    VALIDATION_STAGE_ORIGINALITY,  /**< Originality check */
    VALIDATION_STAGE_BIAS,         /**< Bias detection */
    VALIDATION_STAGE_COUNT
} validation_stage_t;

/**
 * @brief Stage result
 */
typedef struct {
    validation_stage_t stage;      /**< Which stage */
    creative_validation_result_t result; /**< Stage result */
    float score;                   /**< Stage score */
    char message[256];             /**< Stage message */
    float time_ms;                 /**< Stage execution time */
} stage_result_t;

/**
 * @brief Full validation pipeline result
 */
typedef struct {
    stage_result_t stages[VALIDATION_STAGE_COUNT];
    uint32_t num_stages_run;       /**< Number of stages executed */
    creative_validation_result_t overall_result;
    creative_deny_reason_t deny_reason;
    bool short_circuited;          /**< Did we stop early due to failure? */
    uint32_t failed_stage;         /**< Which stage failed (if any) */
    float total_time_ms;           /**< Total validation time */
} validation_pipeline_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Per-stage configuration
 */
typedef struct {
    bool enabled;                  /**< Is this stage enabled? */
    float pass_threshold;          /**< Score to pass (0-1) */
    float warn_threshold;          /**< Score to warn (0-1) */
    float escalate_threshold;      /**< Score to escalate (0-1) */
    bool can_deny;                 /**< Can this stage deny? */
    bool continue_on_warn;         /**< Continue pipeline on warning? */
} stage_config_t;

/**
 * @brief Creative bridge configuration
 */
typedef struct {
    /* Stage configurations */
    stage_config_t stages[VALIDATION_STAGE_COUNT];

    /* Pipeline behavior */
    bool short_circuit_on_deny;    /**< Stop on first denial? */
    bool collect_all_warnings;     /**< Run all stages even after warning? */

    /* Copyright settings */
    float copyright_similarity_threshold; /**< Max similarity before denial */
    char* copyright_db_path;       /**< Path to copyright database */
    bool check_style_copyright;    /**< Check style similarity too */

    /* Ethics settings */
    bool detect_nsfw;              /**< Detect NSFW content */
    bool detect_violence;          /**< Detect violent content */
    bool detect_hate;              /**< Detect hate content */
    bool detect_deception;         /**< Detect deceptive content */

    /* Quality settings */
    float min_quality_score;       /**< Minimum quality score */

    /* Bias settings */
    bool detect_gender_bias;       /**< Detect gender bias */
    bool detect_racial_bias;       /**< Detect racial bias */
    bool detect_cultural_bias;     /**< Detect cultural bias */

    /* Escalation */
    bool enable_human_review;      /**< Allow human review escalation */
    void (*escalation_callback)(const validation_pipeline_result_t*, void*);
    void* escalation_context;
} creative_bridge_config_t;

/**
 * @brief Initialize config with defaults
 */
void creative_bridge_config_defaults(creative_bridge_config_t* config);

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Creative validation bridge
 */
struct creative_bridge {
    creative_bridge_config_t config;

    /* Validators */
    void* quality_validator;       /**< Quality assessment */
    void* coherence_validator;     /**< Coherence checking */
    void* copyright_validator;     /**< Copyright checking */
    void* ethics_validator;        /**< Ethics checking */
    void* originality_validator;   /**< Originality checking */
    void* bias_validator;          /**< Bias detection */

    /* Copyright database */
    void* copyright_db;            /**< Known works database */

    /* Integration */
    void* aesthetic_evaluator;     /**< For quality checking */
    void* ethics_engine;           /**< External ethics engine */

    /* Statistics */
    uint64_t validations_performed;
    uint64_t passed;
    uint64_t warned;
    uint64_t escalated;
    uint64_t denied;
    float avg_validation_time_ms;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create creative bridge
 *
 * @param config Configuration
 * @return Bridge or NULL on error
 */
creative_bridge_t* creative_bridge_create(const creative_bridge_config_t* config);

/**
 * @brief Destroy creative bridge
 *
 * @param bridge Bridge to destroy
 */
void creative_bridge_destroy(creative_bridge_t* bridge);

//=============================================================================
// Validation API
//=============================================================================

/**
 * @brief Validate content through full pipeline
 *
 * @param bridge Bridge
 * @param content Content to validate (type depends on modality)
 * @param modality Content modality
 * @param result Output pipeline result
 * @return 0 on success, -1 on error
 */
int creative_bridge_validate(creative_bridge_t* bridge,
                              const void* content,
                              art_modality_t modality,
                              validation_pipeline_result_t* result);

/**
 * @brief Validate text content
 *
 * @param bridge Bridge
 * @param text Text content
 * @param len Text length
 * @param modality Text modality
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_bridge_validate_text(creative_bridge_t* bridge,
                                   const char* text, size_t len,
                                   art_modality_t modality,
                                   validation_pipeline_result_t* result);

/**
 * @brief Validate image content
 *
 * @param bridge Bridge
 * @param image Image to validate
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_bridge_validate_image(creative_bridge_t* bridge,
                                    const visual_image_t* image,
                                    validation_pipeline_result_t* result);

/**
 * @brief Validate music content
 *
 * @param bridge Bridge
 * @param tracks Music tracks
 * @param num_tracks Number of tracks
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_bridge_validate_music(creative_bridge_t* bridge,
                                    const music_track_t* tracks,
                                    uint32_t num_tracks,
                                    validation_pipeline_result_t* result);

//=============================================================================
// Individual Stage API
//=============================================================================

/**
 * @brief Run quality check only
 *
 * @param bridge Bridge
 * @param content Content
 * @param modality Modality
 * @param result Output stage result
 * @return 0 on success, -1 on error
 */
int creative_bridge_check_quality(creative_bridge_t* bridge,
                                   const void* content,
                                   art_modality_t modality,
                                   stage_result_t* result);

/**
 * @brief Run copyright check only
 *
 * @param bridge Bridge
 * @param content Content
 * @param modality Modality
 * @param result Output stage result
 * @return 0 on success, -1 on error
 */
int creative_bridge_check_copyright(creative_bridge_t* bridge,
                                     const void* content,
                                     art_modality_t modality,
                                     stage_result_t* result);

/**
 * @brief Run ethics check only
 *
 * @param bridge Bridge
 * @param content Content
 * @param modality Modality
 * @param result Output stage result
 * @return 0 on success, -1 on error
 */
int creative_bridge_check_ethics(creative_bridge_t* bridge,
                                  const void* content,
                                  art_modality_t modality,
                                  stage_result_t* result);

/**
 * @brief Run originality check only
 *
 * @param bridge Bridge
 * @param content Content
 * @param modality Modality
 * @param result Output stage result
 * @return 0 on success, -1 on error
 */
int creative_bridge_check_originality(creative_bridge_t* bridge,
                                       const void* content,
                                       art_modality_t modality,
                                       stage_result_t* result);

//=============================================================================
// Copyright Database API
//=============================================================================

/**
 * @brief Add work to copyright database
 *
 * @param bridge Bridge
 * @param title Work title
 * @param creator Creator name
 * @param embedding Style embedding
 * @param content_hash Content hash
 * @return 0 on success, -1 on error
 */
int creative_bridge_add_copyrighted_work(creative_bridge_t* bridge,
                                          const char* title,
                                          const char* creator,
                                          const style_embedding_t* embedding,
                                          uint64_t content_hash);

/**
 * @brief Find similar copyrighted works
 *
 * @param bridge Bridge
 * @param embedding Style embedding to compare
 * @param max_results Maximum results
 * @param titles Output titles (caller allocated)
 * @param similarities Output similarities (caller allocated)
 * @return Number of similar works found
 */
uint32_t creative_bridge_find_similar_works(creative_bridge_t* bridge,
                                             const style_embedding_t* embedding,
                                             uint32_t max_results,
                                             char** titles,
                                             float* similarities);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Enable/disable validation stage
 *
 * @param bridge Bridge
 * @param stage Stage to configure
 * @param enabled Enable or disable
 */
void creative_bridge_set_stage_enabled(creative_bridge_t* bridge,
                                        validation_stage_t stage,
                                        bool enabled);

/**
 * @brief Set stage thresholds
 *
 * @param bridge Bridge
 * @param stage Stage to configure
 * @param pass Pass threshold
 * @param warn Warn threshold
 * @param escalate Escalate threshold
 */
void creative_bridge_set_thresholds(creative_bridge_t* bridge,
                                     validation_stage_t stage,
                                     float pass, float warn, float escalate);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Set aesthetic evaluator
 *
 * @param bridge Bridge
 * @param evaluator Aesthetic evaluator
 */
void creative_bridge_set_evaluator(creative_bridge_t* bridge, void* evaluator);

/**
 * @brief Set ethics engine
 *
 * @param bridge Bridge
 * @param ethics Ethics engine
 */
void creative_bridge_set_ethics_engine(creative_bridge_t* bridge, void* ethics);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get validation statistics
 *
 * @param bridge Bridge
 * @param passed Output: passed count
 * @param warned Output: warned count
 * @param escalated Output: escalated count
 * @param denied Output: denied count
 */
void creative_bridge_get_stats(const creative_bridge_t* bridge,
                                uint64_t* passed,
                                uint64_t* warned,
                                uint64_t* escalated,
                                uint64_t* denied);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 */
void creative_bridge_reset_stats(creative_bridge_t* bridge);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get stage name
 *
 * @param stage Stage
 * @return Stage name string
 */
const char* creative_bridge_stage_name(validation_stage_t stage);

/**
 * @brief Get result name
 *
 * @param result Result
 * @return Result name string
 */
const char* creative_bridge_result_name(creative_validation_result_t result);

/**
 * @brief Get deny reason name
 *
 * @param reason Deny reason
 * @return Reason name string
 */
const char* creative_bridge_deny_reason_name(creative_deny_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_BRIDGE_H */
