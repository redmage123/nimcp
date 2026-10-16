/**
 * @file nimcp_insight_discovery.h
 * @brief Insight and discovery engine for "aha moment" generation
 *
 * WHAT: Engine for generating breakthrough insights and discoveries
 * WHY:  Enable creative problem solving through restructuring
 * HOW:  Incubation, constraint relaxation, restructuring, eureka detection
 *
 * BIOLOGICAL BASIS:
 * Insight moments involve sudden reorganization of neural representations,
 * often preceded by impasse and incubation. The parietal cortex integrates
 * disparate information to enable these sudden reconfigurations.
 *
 * CAPABILITIES:
 * - Incubation: Background processing of unsolved problems
 * - Constraint Relaxation: Identify and relax limiting assumptions
 * - Restructuring: Reframe problems to reveal hidden solutions
 * - Eureka Detection: Recognize when breakthrough has occurred
 * - Impasse Resolution: Break through mental blocks
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_INSIGHT_DISCOVERY_H
#define NIMCP_INSIGHT_DISCOVERY_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define INSIGHT_MAX_DESCRIPTION         512
#define INSIGHT_MAX_CONSTRAINTS         32
#define INSIGHT_MAX_PERSPECTIVES        16
#define INSIGHT_MAX_INCUBATION_QUEUE    64

#define BIO_MODULE_INSIGHT_DISCOVERY    0x03A2

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

typedef struct insight_engine insight_engine_t;

/* ============================================================================
 * CORE TYPES
 * ============================================================================ */

/**
 * @brief Constraint that may limit problem solving
 */
typedef struct {
    uint32_t id;
    char description[256];
    float binding_strength;         /**< How strongly held [0,1] */
    bool is_explicit;               /**< Explicitly stated vs assumed */
    bool is_relaxable;              /**< Can be relaxed? */
    float relaxation_cost;          /**< Cost to relax [0,1] */
} insight_constraint_t;

/**
 * @brief Problem perspective/framing
 */
typedef struct {
    uint32_t id;
    char description[256];
    float* representation;          /**< Problem in this frame */
    uint32_t repr_dim;
    float novelty;                  /**< How different from default [0,1] */
    float utility;                  /**< How useful is this framing [0,1] */
} insight_perspective_t;

/**
 * @brief Problem state for insight processing
 */
typedef struct {
    uint32_t id;
    char description[INSIGHT_MAX_DESCRIPTION];

    float* state;                   /**< Current problem state */
    float* goal;                    /**< Goal state (if known) */
    uint32_t state_dim;

    insight_constraint_t* constraints;
    uint32_t num_constraints;

    insight_perspective_t* perspectives;
    uint32_t num_perspectives;
    uint32_t current_perspective;

    float impasse_level;            /**< How stuck [0,1] */
    float incubation_progress;      /**< Background processing [0,1] */
    bool is_solved;
} insight_problem_t;

/**
 * @brief Restructuring operation
 */
typedef struct {
    char description[256];
    uint32_t constraint_relaxed;    /**< Which constraint was relaxed */
    uint32_t new_perspective;       /**< New perspective adopted */
    float transformation[16];       /**< State transformation matrix */
    float novelty;                  /**< How novel is restructuring */
} insight_restructuring_t;

/**
 * @brief Eureka/insight result
 */
typedef struct {
    uint32_t id;
    char description[INSIGHT_MAX_DESCRIPTION];

    insight_problem_t* problem;     /**< Original problem */
    insight_restructuring_t restructuring;  /**< How problem was restructured */

    float* solution;                /**< The insight solution */
    uint32_t solution_dim;

    float surprise_magnitude;       /**< How surprising [0,1] */
    float elegance;                 /**< Solution elegance [0,1] */
    float confidence;               /**< Confidence in insight [0,1] */

    uint64_t incubation_time_us;    /**< Time spent incubating */
    uint32_t restructuring_attempts; /**< Attempts before success */

    bool verified;
} insight_eureka_t;

/**
 * @brief Impasse state
 */
typedef struct {
    float stuckness_level;          /**< How stuck [0,1] */
    uint32_t failed_attempts;       /**< Number of failed attempts */
    uint64_t time_at_impasse_us;    /**< Time stuck */
    insight_constraint_t* blocking_constraints;  /**< What's blocking */
    uint32_t num_blocking;
    bool ready_for_incubation;      /**< Should incubate? */
} insight_impasse_t;

/**
 * @brief Configuration
 */
typedef struct {
    float impasse_threshold;        /**< When to declare impasse */
    float incubation_rate;          /**< Background processing rate */
    float constraint_relaxation_rate;  /**< How fast to relax constraints */
    float restructuring_threshold;  /**< Min restructuring novelty */

    bool enable_incubation;
    bool enable_constraint_relaxation;
    bool enable_perspective_shifting;

    uint32_t max_restructuring_attempts;
    uint32_t incubation_queue_size;

    float inflammation_sensitivity;
    float fatigue_sensitivity;
} insight_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t problems_processed;
    uint64_t insights_generated;
    uint64_t impasses_detected;
    uint64_t impasses_resolved;
    uint64_t constraints_relaxed;
    uint64_t perspectives_shifted;
    float avg_incubation_time_us;
    float avg_surprise_magnitude;
    float insight_success_rate;
} insight_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

insight_engine_t* insight_engine_create(void);
insight_engine_t* insight_engine_create_custom(const insight_config_t* config);
void insight_engine_destroy(insight_engine_t* engine);
insight_config_t insight_engine_default_config(void);

/* ============================================================================
 * PROBLEM MANAGEMENT API
 * ============================================================================ */

insight_problem_t* insight_create_problem(const char* description);
int insight_add_constraint(insight_problem_t* problem, const char* description,
                          float binding_strength, bool is_explicit);
int insight_add_perspective(insight_problem_t* problem, const char* description,
                           const float* representation, uint32_t dim);
void insight_free_problem(insight_problem_t* problem);

/* ============================================================================
 * INCUBATION API
 * ============================================================================ */

uint32_t insight_incubate(insight_engine_t* engine, insight_problem_t* problem);
int insight_check_incubation(insight_engine_t* engine, uint32_t problem_id,
                            insight_eureka_t** result);
int insight_process_incubation_step(insight_engine_t* engine);
int insight_cancel_incubation(insight_engine_t* engine, uint32_t problem_id);

/* ============================================================================
 * CONSTRAINT RELAXATION API
 * ============================================================================ */

int insight_identify_blocking_constraints(insight_engine_t* engine,
                                         const insight_problem_t* problem,
                                         insight_constraint_t* blocking,
                                         uint32_t max_constraints,
                                         uint32_t* num_found);
int insight_relax_constraint(insight_engine_t* engine,
                            insight_problem_t* problem,
                            uint32_t constraint_id);
int insight_find_relaxable_constraints(insight_engine_t* engine,
                                       const insight_problem_t* problem,
                                       uint32_t* constraint_ids,
                                       uint32_t max_constraints,
                                       uint32_t* num_found);

/* ============================================================================
 * RESTRUCTURING API
 * ============================================================================ */

insight_restructuring_t* insight_attempt_restructure(insight_engine_t* engine,
                                                    insight_problem_t* problem);
int insight_shift_perspective(insight_engine_t* engine,
                             insight_problem_t* problem,
                             uint32_t new_perspective);
int insight_generate_perspectives(insight_engine_t* engine,
                                 insight_problem_t* problem,
                                 uint32_t max_perspectives);
void insight_free_restructuring(insight_restructuring_t* restructuring);

/* ============================================================================
 * EUREKA DETECTION API
 * ============================================================================ */

bool insight_check_eureka(insight_engine_t* engine,
                         const insight_problem_t* problem,
                         insight_eureka_t** result);
float insight_estimate_surprise(insight_engine_t* engine,
                               const insight_eureka_t* eureka);
int insight_verify_eureka(insight_engine_t* engine, insight_eureka_t* eureka);
void insight_free_eureka(insight_eureka_t* eureka);

/* ============================================================================
 * IMPASSE API
 * ============================================================================ */

int insight_detect_impasse(insight_engine_t* engine,
                          const insight_problem_t* problem,
                          insight_impasse_t* impasse);
int insight_resolve_impasse(insight_engine_t* engine,
                           insight_problem_t* problem,
                           const insight_impasse_t* impasse);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int insight_set_inflammation(insight_engine_t* engine, float level);
int insight_set_fatigue(insight_engine_t* engine, float level);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int insight_get_stats(const insight_engine_t* engine, insight_stats_t* stats);
void insight_reset_stats(insight_engine_t* engine);
const char* insight_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INSIGHT_DISCOVERY_H */
