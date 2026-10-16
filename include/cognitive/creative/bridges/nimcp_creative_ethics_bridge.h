//=============================================================================
// nimcp_creative_ethics_bridge.h - Creative Ethics Integration
//=============================================================================
/**
 * @file nimcp_creative_ethics_bridge.h
 * @brief Bridge connecting creative system to ethics engine
 *
 * WHAT: Ethical validation and guidance for creative content
 * WHY:  Ensure AI-generated art respects ethical boundaries
 * HOW:  Interface to brain's ethics engine with creative-specific checks
 *
 * ETHICAL CONCERNS:
 * - Copyright/plagiarism
 * - Harmful content (violence, hate, NSFW)
 * - Deception (deepfakes, misinformation)
 * - Bias (stereotypes, underrepresentation)
 * - Privacy (unauthorized likenesses)
 * - Cultural sensitivity
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_ETHICS_BRIDGE_H
#define NIMCP_CREATIVE_ETHICS_BRIDGE_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Ethics Categories
//=============================================================================

/**
 * @brief Creative ethics concern types
 */
typedef enum {
    ETHICS_CONCERN_NONE = 0,
    ETHICS_CONCERN_COPYRIGHT,          /**< Copyright/IP violation */
    ETHICS_CONCERN_PLAGIARISM,         /**< Direct plagiarism */
    ETHICS_CONCERN_VIOLENCE,           /**< Violent content */
    ETHICS_CONCERN_HATE,               /**< Hate speech/imagery */
    ETHICS_CONCERN_NSFW,               /**< Adult/sexual content */
    ETHICS_CONCERN_DECEPTION,          /**< Deepfakes, misinformation */
    ETHICS_CONCERN_PRIVACY,            /**< Privacy violation */
    ETHICS_CONCERN_BIAS,               /**< Harmful bias/stereotypes */
    ETHICS_CONCERN_CULTURAL,           /**< Cultural insensitivity */
    ETHICS_CONCERN_CHILD_SAFETY,       /**< Child exploitation */
    ETHICS_CONCERN_SELF_HARM,          /**< Self-harm promotion */
    ETHICS_CONCERN_TERRORISM,          /**< Terrorism promotion */
    ETHICS_CONCERN_COUNT
} ethics_concern_t;

/**
 * @brief Ethics severity levels
 */
typedef enum {
    ETHICS_SEVERITY_NONE = 0,          /**< No concern */
    ETHICS_SEVERITY_MINOR,             /**< Minor concern, allow with note */
    ETHICS_SEVERITY_MODERATE,          /**< Moderate, may need review */
    ETHICS_SEVERITY_SEVERE,            /**< Severe, should block */
    ETHICS_SEVERITY_CRITICAL           /**< Critical, must block */
} ethics_severity_t;

/**
 * @brief Ethics check result
 */
typedef struct {
    ethics_concern_t concern;          /**< Type of concern */
    ethics_severity_t severity;        /**< Severity level */
    float confidence;                  /**< [0-1] Confidence in detection */
    char description[256];             /**< Human-readable description */
    char location[128];                /**< Where in content (if applicable) */
} ethics_concern_result_t;

/**
 * @brief Full ethics evaluation
 */
typedef struct {
    bool is_ethical;                   /**< Overall ethical judgment */
    ethics_concern_result_t* concerns; /**< Array of concerns */
    uint32_t num_concerns;             /**< Number of concerns */
    ethics_severity_t max_severity;    /**< Maximum severity found */
    float overall_risk_score;          /**< [0-1] Overall risk */
    bool requires_human_review;        /**< Needs human review? */
    char recommendation[512];          /**< Recommended action */
} creative_ethics_evaluation_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Per-concern configuration
 */
typedef struct {
    bool detect;                       /**< Enable detection */
    ethics_severity_t min_severity_to_block; /**< Min severity to block */
    float detection_threshold;         /**< Detection confidence threshold */
} concern_config_t;

/**
 * @brief Ethics bridge configuration
 */
typedef struct {
    /* Concern configurations */
    concern_config_t concerns[ETHICS_CONCERN_COUNT];

    /* Overall settings */
    float global_risk_threshold;       /**< Block above this risk score */
    bool strict_mode;                  /**< Stricter interpretations */

    /* Human review */
    bool enable_human_review;          /**< Allow escalation to humans */
    ethics_severity_t min_severity_for_review;

    /* Integration */
    bool use_external_ethics_engine;   /**< Use brain's ethics engine */

    /* Content-specific */
    bool allow_artistic_violence;      /**< Allow artistic depictions */
    bool allow_historical_context;     /**< Allow in historical context */
    bool allow_educational_context;    /**< Allow for education */

    /* Copyright specific */
    float style_similarity_threshold;  /**< Max style similarity */
    float content_similarity_threshold; /**< Max content similarity */
} creative_ethics_bridge_config_t;

/**
 * @brief Initialize config with defaults
 */
void creative_ethics_bridge_config_defaults(creative_ethics_bridge_config_t* config);

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Creative ethics bridge
 */
struct creative_ethics_bridge {
    creative_ethics_bridge_config_t config;

    /* Detection models */
    void* nsfw_detector;               /**< NSFW content detector */
    void* violence_detector;           /**< Violence detector */
    void* hate_detector;               /**< Hate content detector */
    void* face_detector;               /**< Face detection for privacy */
    void* deepfake_detector;           /**< Deepfake detector */
    void* bias_detector;               /**< Bias detector */

    /* External integration */
    void* ethics_engine;               /**< Brain's ethics engine */
    void* copyright_db;                /**< Copyright database */

    /* Human review queue */
    void* review_queue;                /**< Items pending review */

    /* Statistics */
    uint64_t evaluations;
    uint64_t blocked;
    uint64_t flagged_for_review;
    uint32_t concerns_by_type[ETHICS_CONCERN_COUNT];
};

/** @brief Typedef for creative_ethics_bridge */
typedef struct creative_ethics_bridge creative_ethics_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create ethics bridge
 *
 * @param config Configuration
 * @return Bridge or NULL on error
 */
creative_ethics_bridge_t* creative_ethics_bridge_create(
    const creative_ethics_bridge_config_t* config);

/**
 * @brief Destroy ethics bridge
 *
 * @param bridge Bridge to destroy
 */
void creative_ethics_bridge_destroy(creative_ethics_bridge_t* bridge);

//=============================================================================
// Evaluation API
//=============================================================================

/**
 * @brief Evaluate content ethics
 *
 * @param bridge Bridge
 * @param content Content
 * @param modality Content modality
 * @param context Additional context
 * @param evaluation Output evaluation
 * @return 0 on success, -1 on error
 */
int creative_ethics_evaluate(creative_ethics_bridge_t* bridge,
                              const void* content,
                              art_modality_t modality,
                              const char* context,
                              creative_ethics_evaluation_t* evaluation);

/**
 * @brief Evaluate text ethics
 *
 * @param bridge Bridge
 * @param text Text content
 * @param len Text length
 * @param context Context
 * @param evaluation Output evaluation
 * @return 0 on success, -1 on error
 */
int creative_ethics_evaluate_text(creative_ethics_bridge_t* bridge,
                                   const char* text, size_t len,
                                   const char* context,
                                   creative_ethics_evaluation_t* evaluation);

/**
 * @brief Evaluate image ethics
 *
 * @param bridge Bridge
 * @param image Image
 * @param context Context
 * @param evaluation Output evaluation
 * @return 0 on success, -1 on error
 */
int creative_ethics_evaluate_image(creative_ethics_bridge_t* bridge,
                                    const visual_image_t* image,
                                    const char* context,
                                    creative_ethics_evaluation_t* evaluation);

//=============================================================================
// Specific Check API
//=============================================================================

/**
 * @brief Check for NSFW content
 *
 * @param bridge Bridge
 * @param image Image
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_ethics_check_nsfw(creative_ethics_bridge_t* bridge,
                                const visual_image_t* image,
                                ethics_concern_result_t* result);

/**
 * @brief Check for violence
 *
 * @param bridge Bridge
 * @param content Content
 * @param modality Modality
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_ethics_check_violence(creative_ethics_bridge_t* bridge,
                                    const void* content,
                                    art_modality_t modality,
                                    ethics_concern_result_t* result);

/**
 * @brief Check for hate content
 *
 * @param bridge Bridge
 * @param content Content
 * @param modality Modality
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_ethics_check_hate(creative_ethics_bridge_t* bridge,
                                const void* content,
                                art_modality_t modality,
                                ethics_concern_result_t* result);

/**
 * @brief Check for deepfake/deception
 *
 * @param bridge Bridge
 * @param image Image
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_ethics_check_deepfake(creative_ethics_bridge_t* bridge,
                                    const visual_image_t* image,
                                    ethics_concern_result_t* result);

/**
 * @brief Check for bias
 *
 * @param bridge Bridge
 * @param content Content
 * @param modality Modality
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_ethics_check_bias(creative_ethics_bridge_t* bridge,
                                const void* content,
                                art_modality_t modality,
                                ethics_concern_result_t* result);

/**
 * @brief Check for privacy concerns
 *
 * @param bridge Bridge
 * @param image Image
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int creative_ethics_check_privacy(creative_ethics_bridge_t* bridge,
                                   const visual_image_t* image,
                                   ethics_concern_result_t* result);

//=============================================================================
// Prompt Evaluation API
//=============================================================================

/**
 * @brief Evaluate generation prompt ethics
 *
 * Check if a prompt is ethical before generation
 *
 * @param bridge Bridge
 * @param prompt Prompt text
 * @param evaluation Output evaluation
 * @return 0 on success, -1 on error
 */
int creative_ethics_evaluate_prompt(creative_ethics_bridge_t* bridge,
                                     const char* prompt,
                                     creative_ethics_evaluation_t* evaluation);

/**
 * @brief Filter/modify unethical prompt
 *
 * @param bridge Bridge
 * @param prompt Original prompt
 * @param filtered Output filtered prompt
 * @param max_len Max output length
 * @return 0 on success (prompt may be modified), -1 on error
 */
int creative_ethics_filter_prompt(creative_ethics_bridge_t* bridge,
                                   const char* prompt,
                                   char* filtered,
                                   size_t max_len);

//=============================================================================
// Human Review API
//=============================================================================

/**
 * @brief Submit for human review
 *
 * @param bridge Bridge
 * @param content Content
 * @param modality Modality
 * @param evaluation Initial evaluation
 * @return Review ticket ID, 0 on error
 */
uint64_t creative_ethics_submit_review(creative_ethics_bridge_t* bridge,
                                        const void* content,
                                        art_modality_t modality,
                                        const creative_ethics_evaluation_t* evaluation);

/**
 * @brief Check review status
 *
 * @param bridge Bridge
 * @param ticket_id Ticket ID
 * @param approved Output: whether approved
 * @return 0 on complete, 1 on pending, -1 on error
 */
int creative_ethics_check_review(creative_ethics_bridge_t* bridge,
                                  uint64_t ticket_id,
                                  bool* approved);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Set external ethics engine
 *
 * @param bridge Bridge
 * @param engine Ethics engine pointer
 */
void creative_ethics_set_engine(creative_ethics_bridge_t* bridge,
                                 void* engine);

/**
 * @brief Set copyright database
 *
 * @param bridge Bridge
 * @param db Copyright database
 */
void creative_ethics_set_copyright_db(creative_ethics_bridge_t* bridge,
                                       void* db);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get concern name
 *
 * @param concern Concern type
 * @return Name string
 */
const char* creative_ethics_concern_name(ethics_concern_t concern);

/**
 * @brief Get severity name
 *
 * @param severity Severity level
 * @return Name string
 */
const char* creative_ethics_severity_name(ethics_severity_t severity);

/**
 * @brief Free evaluation
 *
 * @param evaluation Evaluation to free
 */
void ethics_evaluation_free(creative_ethics_evaluation_t* evaluation);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_ETHICS_BRIDGE_H */
