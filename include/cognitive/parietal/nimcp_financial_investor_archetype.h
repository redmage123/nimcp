//=============================================================================
// nimcp_financial_investor_archetype.h - Investor Archetype Module
//=============================================================================
/**
 * @file nimcp_financial_investor_archetype.h
 * @brief 10 investor archetypes with deep fuzzy heuristic integration
 *
 * WHAT: Models 10 legendary investor archetypes (Graham, Buffett, Lynch,
 *       Fisher, Soros, Templeton, Dalio, Simons, Munger, Livermore) as
 *       cognitive decision templates with domain-specific heuristics.
 *
 * WHY:  Investment decisions benefit from multiple perspectives. Each
 *       archetype encodes decades of proven wisdom as computable heuristics.
 *       Fuzzy logic enables graded scoring (e.g., "60% margin of safety"
 *       rather than binary "pass/fail"), archetype blending, and adaptive
 *       selection based on market conditions.
 *
 * HOW:  Each archetype defines heuristic evaluation functions that return
 *       fuzzy membership degrees. Archetypes can be blended using fuzzy
 *       weighted average. Adaptive selection uses Sugeno FIS to match
 *       market regime + sentiment to archetype suitability. Emotional
 *       modulation uses fuzzy operators (fear reduces position, stress
 *       increases conservatism).
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_FINANCIAL_INVESTOR_ARCHETYPE_H
#define NIMCP_FINANCIAL_INVESTOR_ARCHETYPE_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/parietal/nimcp_financial_market.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_FINANCIAL_ARCHETYPE   0x0397
#define FIN_ARCH_MAX_ARCHETYPES         16
#define FIN_ARCH_MAX_HEURISTICS         20
#define FIN_ARCH_MAX_BLEND_SIZE         10
#define FIN_ARCH_MAX_MENTAL_MODELS      16
#define FIN_ARCH_MAX_SECTORS            32
#define FIN_ARCH_FISHER_CHECKLIST_SIZE  15

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_ARCH_ERROR_BASE             35000
#define FIN_ARCH_ERR_OK                 0
#define FIN_ARCH_ERR_NULL               (FIN_ARCH_ERROR_BASE + 1)
#define FIN_ARCH_ERR_INVALID_ARCHETYPE  (FIN_ARCH_ERROR_BASE + 2)
#define FIN_ARCH_ERR_INVALID_HEURISTIC  (FIN_ARCH_ERROR_BASE + 3)
#define FIN_ARCH_ERR_BLEND              (FIN_ARCH_ERROR_BASE + 4)
#define FIN_ARCH_ERR_ETHICS             (FIN_ARCH_ERROR_BASE + 5)
#define FIN_ARCH_ERR_LGSS               (FIN_ARCH_ERROR_BASE + 6)
#define FIN_ARCH_ERR_FUZZY              (FIN_ARCH_ERROR_BASE + 7)
#define FIN_ARCH_ERR_CONFIG             (FIN_ARCH_ERROR_BASE + 8)
#define FIN_ARCH_ERR_MIRROR             (FIN_ARCH_ERROR_BASE + 9)
#define FIN_ARCH_ERR_SUBSYSTEM          (FIN_ARCH_ERROR_BASE + 10)
#define FIN_ARCH_ERR_VALIDATION         (FIN_ARCH_ERROR_BASE + 11)

//=============================================================================
// Enumerations
//=============================================================================

/** The 10 investor archetypes */
typedef enum {
    FIN_ARCH_GRAHAM = 0,
    FIN_ARCH_BUFFETT,
    FIN_ARCH_LYNCH,
    FIN_ARCH_FISHER,
    FIN_ARCH_SOROS,
    FIN_ARCH_TEMPLETON,
    FIN_ARCH_DALIO,
    FIN_ARCH_SIMONS,
    FIN_ARCH_MUNGER,
    FIN_ARCH_LIVERMORE,
    FIN_ARCH_COUNT
} fin_archetype_id_t;

/** Cognitive style of the archetype */
typedef enum {
    FIN_COGNITIVE_ANALYTICAL,
    FIN_COGNITIVE_INTUITIVE,
    FIN_COGNITIVE_SYSTEMATIC,
    FIN_COGNITIVE_CONTRARIAN,
    FIN_COGNITIVE_ADAPTIVE,
    FIN_COGNITIVE_STYLE_COUNT
} fin_cognitive_style_t;

/** Heuristic types used across archetypes */
typedef enum {
    FIN_HEURISTIC_MARGIN_OF_SAFETY,
    FIN_HEURISTIC_ECONOMIC_MOAT,
    FIN_HEURISTIC_CIRCLE_OF_COMPETENCE,
    FIN_HEURISTIC_PEG_RATIO,
    FIN_HEURISTIC_REFLEXIVITY,
    FIN_HEURISTIC_MAXIMUM_PESSIMISM,
    FIN_HEURISTIC_RISK_PARITY,
    FIN_HEURISTIC_STATISTICAL_EDGE,
    FIN_HEURISTIC_SCUTTLEBUTT,
    FIN_HEURISTIC_FIFTEEN_POINT_CHECKLIST,
    FIN_HEURISTIC_PIVOTAL_POINT,
    FIN_HEURISTIC_PYRAMIDING,
    FIN_HEURISTIC_INVERSION,
    FIN_HEURISTIC_MENTAL_MODEL_CONVERGENCE,
    FIN_HEURISTIC_CONTRARIAN_SENTIMENT,
    FIN_HEURISTIC_EARNINGS_GROWTH,
    FIN_HEURISTIC_TYPE_COUNT
} fin_heuristic_type_t;

/** Decision types (guarded to avoid redefinition) */
#ifndef FIN_DECISION_TYPE_DEFINED
#define FIN_DECISION_TYPE_DEFINED
typedef enum {
    FIN_DECISION_STRONG_BUY,
    FIN_DECISION_BUY,
    FIN_DECISION_HOLD,
    FIN_DECISION_REDUCE,
    FIN_DECISION_SELL,
    FIN_DECISION_STRONG_SELL,
    FIN_DECISION_NO_ACTION,
    FIN_DECISION_TYPE_COUNT
} fin_decision_type_t;
#endif /* FIN_DECISION_TYPE_DEFINED */

/** Time horizon preference */
typedef enum {
    FIN_HORIZON_INTRADAY,
    FIN_HORIZON_SWING,
    FIN_HORIZON_MEDIUM_TERM,
    FIN_HORIZON_LONG_TERM,
    FIN_HORIZON_VERY_LONG_TERM,
    FIN_HORIZON_COUNT
} fin_time_horizon_t;

//=============================================================================
// Data Structures
//=============================================================================

/** Archetype profile descriptor */
typedef struct {
    fin_archetype_id_t id;
    char name[64];
    char philosophy[256];
    fin_cognitive_style_t cognitive_style;
    fin_time_horizon_t preferred_horizon;
    float risk_tolerance;
    float concentration_preference;
    float turnover_preference;
    float contrarian_tendency;
    /* Heuristics this archetype uses */
    fin_heuristic_type_t heuristics[FIN_ARCH_MAX_HEURISTICS];
    uint32_t heuristic_count;
    /* Sectors of competence (Buffett circle) */
    uint32_t competence_sectors[FIN_ARCH_MAX_SECTORS];
    uint32_t competence_sector_count;
} fin_archetype_profile_t;

/** Input data for heuristic evaluation */
typedef struct {
    /* Price/value data */
    float current_price;
    float intrinsic_value;
    float book_value;
    float earnings_per_share;
    float earnings_growth_rate;
    float dividend_yield;
    float peg_ratio;

    /* Market context */
    float fear_greed_index;
    float market_consensus_strength;
    float sector_distance;

    /* Moat inputs */
    float market_share_stability;
    float pricing_power;
    float switching_cost;
    float brand_strength;

    /* Technical inputs */
    float rsi;
    float pivot_price;
    float pivot_tolerance;
    float breakout_confirmation;
    float unrealized_profit_pct;
    float z_score;

    /* Qualitative inputs (Fisher scuttlebutt) */
    float management_quality;
    float rd_effectiveness;
    float competitive_position;
    float fisher_checklist_scores[FIN_ARCH_FISHER_CHECKLIST_SIZE];

    /* Risk inputs */
    float risk_contributions[8];
    uint32_t risk_contribution_count;
    float leverage_ratio;
    float position_concentration;

    /* Mental model scores (Munger) */
    float mental_model_activations[FIN_ARCH_MAX_MENTAL_MODELS];
    uint32_t mental_model_count;

    /* Reflexivity inputs (Soros) */
    float price_momentum;
    float sentiment_divergence;
    float volume_trend;

    /* Market regime */
    fin_fuzzy_market_condition_t market_condition;
    fin_fuzzy_sentiment_t market_sentiment;
} fin_heuristic_input_t;

/** Result from a single heuristic evaluation */
typedef struct {
    fin_heuristic_type_t type;
    float crisp_score;
    float fuzzy_membership;
    float confidence;
    char explanation[128];
} fin_heuristic_result_t;

/** Fuzzy decision with multi-membership */
typedef struct {
    float strong_buy_degree;
    float buy_degree;
    float hold_degree;
    float reduce_degree;
    float sell_degree;
    float strong_sell_degree;
    float no_action_degree;
    fin_decision_type_t dominant;
    float decision_entropy;
} fin_fuzzy_decision_t;

/** Emotional state affecting decisions */
typedef struct {
    float fear_level;
    float greed_level;
    float stress_level;
    float arousal_level;
    float confidence_level;
    /* Fuzzy emotional modulation outputs */
    float fuzzy_position_scale;
    float fuzzy_conservatism;
    float fuzzy_horizon_adjustment;
    float emotional_bias;
} fin_emotional_state_t;

/** Complete archetype decision output */
typedef struct {
    fin_archetype_id_t archetype;
    fin_decision_type_t decision;
    float conviction;
    float position_size_pct;
    float stop_loss_pct;
    float take_profit_pct;
    fin_time_horizon_t horizon;
    /* Heuristic breakdown */
    fin_heuristic_result_t heuristics[FIN_ARCH_MAX_HEURISTICS];
    uint32_t heuristic_count;
    /* Fuzzy decision */
    fin_fuzzy_decision_t fuzzy_decision;
    /* Emotional modulation applied */
    fin_emotional_state_t emotional_state;
    /* Validation */
    bool ethics_validated;
    bool lgss_validated;
    float processing_time_us;
} fin_archetype_decision_t;

/** Blend of multiple archetype decisions */
typedef struct {
    fin_archetype_id_t archetypes[FIN_ARCH_MAX_BLEND_SIZE];
    float weights[FIN_ARCH_MAX_BLEND_SIZE];
    uint32_t archetype_count;
    /* Blended output */
    fin_decision_type_t blended_decision;
    float blended_conviction;
    float blend_entropy;
    fin_fuzzy_decision_t blended_fuzzy;
    /* Per-archetype contributions */
    fin_archetype_decision_t decisions[FIN_ARCH_MAX_BLEND_SIZE];
} fin_blend_result_t;

/** Archetype suitability scores for adaptive selection */
typedef struct {
    float suitability[FIN_ARCH_COUNT];
    fin_archetype_id_t best_archetype;
    float best_suitability;
    float selection_confidence;
} fin_archetype_suitability_t;

/** Mirror learning record */
typedef struct {
    fin_archetype_id_t archetype;
    fin_decision_type_t decision_made;
    float outcome_return;
    float prediction_error;
    uint64_t timestamp_us;
    bool was_correct;
} fin_mirror_record_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    /* Fuzzy integration */
    bool enable_fuzzy_heuristics;
    bool enable_fuzzy_decision_scoring;
    bool enable_fuzzy_emotional_blend;

    /* Validation */
    bool enable_ethics_validation;
    bool enable_lgss_validation;

    /* Adaptation */
    bool enable_adaptive_selection;
    bool enable_mirror_learning;
    bool enable_self_reflection;
    float mirror_learning_rate;

    /* Emotional modulation */
    bool enable_emotional_modulation;
    float fear_sensitivity;
    float greed_sensitivity;
    float stress_sensitivity;

    /* Blending */
    uint32_t max_blend_archetypes;
    float min_blend_weight;

    /* Modulation */
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} fin_archetype_config_t;

//=============================================================================
// Statistics
//=============================================================================

typedef struct {
    uint64_t total_evaluations;
    uint64_t total_blends;
    uint64_t adaptive_selections;
    uint64_t mirror_learning_steps;
    uint64_t ethics_checks;
    uint64_t lgss_checks;
    uint64_t fuzzy_heuristic_evals;
    float avg_conviction;
    float avg_decision_entropy;
    float avg_processing_time_us;
    /* Per-archetype usage counts */
    uint64_t archetype_usage[FIN_ARCH_COUNT];
    float archetype_accuracy[FIN_ARCH_COUNT];
    uint64_t async_operations;  /**< Bio-async operations count (Change Set 4) */
} fin_archetype_stats_t;

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct financial_investor_archetype financial_investor_archetype_t;

//=============================================================================
// Lifecycle
//=============================================================================

financial_investor_archetype_t* financial_investor_archetype_create(
    const fin_archetype_config_t* config);
void financial_investor_archetype_destroy(financial_investor_archetype_t* arch);
fin_archetype_config_t financial_investor_archetype_default_config(void);

//=============================================================================
// Subsystem Setters
//=============================================================================

int financial_investor_archetype_set_ethics(
    financial_investor_archetype_t* arch, void* ethics);
int financial_investor_archetype_set_lgss(
    financial_investor_archetype_t* arch, void* lgss);
int financial_investor_archetype_set_immune(
    financial_investor_archetype_t* arch, void* immune);
int financial_investor_archetype_set_health_agent(
    financial_investor_archetype_t* arch, void* health_agent);
int financial_investor_archetype_set_logger(
    financial_investor_archetype_t* arch, void* logger);  /* Phase 8: Change Set 2/3 */
int financial_investor_archetype_set_fuzzy(
    financial_investor_archetype_t* arch, void* fuzzy_bridge);
int financial_investor_archetype_set_bbb(
    financial_investor_archetype_t* arch, void* bbb);

/** Enable/disable instance-level BBB validation */
int financial_investor_archetype_enable_bbb_validation(
    financial_investor_archetype_t* arch, bool enable);

/** Enable/disable instance-level immune validation */
int financial_investor_archetype_enable_immune_validation(
    financial_investor_archetype_t* arch, bool enable);

//=============================================================================
// Bio-Async Integration (Change Set 4)
//=============================================================================

/* Forward declarations for bio-async types (guarded to avoid redefinition) */
#ifndef NIMCP_BIO_ASYNC_FWD_DECL
#define NIMCP_BIO_ASYNC_FWD_DECL
struct bio_async_context;
typedef struct bio_async_context bio_async_context_t;
struct bio_router_struct;
typedef struct bio_router_struct* bio_router_t;
#endif

/** Set bio-async context for asynchronous processing */
int financial_investor_archetype_set_bio_async(
    financial_investor_archetype_t* arch, bio_async_context_t* ctx);
/** Set bio-router for message routing */
int financial_investor_archetype_set_bio_router(
    financial_investor_archetype_t* arch, bio_router_t* router);

//=============================================================================
// KG Wiring Integration (Change Set 1)
//=============================================================================

/* Forward declaration for KG wiring */
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/** Set KG wiring for financial investor archetype */
int financial_investor_archetype_set_kg_wiring(
    financial_investor_archetype_t* arch, kg_wiring_t* kg);

//=============================================================================
// Archetype Profile Access
//=============================================================================

/** Get the built-in profile for an archetype */
int financial_investor_archetype_get_profile(
    fin_archetype_id_t id,
    fin_archetype_profile_t* out_profile);

/** Get archetype name string */
const char* financial_investor_archetype_name(fin_archetype_id_t id);

//=============================================================================
// Single Archetype Evaluation
//=============================================================================

/** Evaluate a single archetype on given inputs */
int financial_investor_archetype_evaluate(
    financial_investor_archetype_t* arch,
    fin_archetype_id_t archetype,
    const fin_heuristic_input_t* input,
    fin_archetype_decision_t* out_decision);

/** Evaluate a specific heuristic */
int financial_investor_archetype_evaluate_heuristic(
    financial_investor_archetype_t* arch,
    fin_heuristic_type_t heuristic,
    const fin_heuristic_input_t* input,
    fin_heuristic_result_t* out_result);

//=============================================================================
// Archetype Blending
//=============================================================================

/** Evaluate multiple archetypes and blend their decisions */
int financial_investor_archetype_evaluate_blend(
    financial_investor_archetype_t* arch,
    const fin_archetype_id_t* archetypes,
    const float* weights, uint32_t count,
    const fin_heuristic_input_t* input,
    fin_blend_result_t* out_blend);

//=============================================================================
// Adaptive Selection
//=============================================================================

/** Select best archetype(s) for current market conditions */
int financial_investor_archetype_select(
    financial_investor_archetype_t* arch,
    const fin_fuzzy_market_condition_t* market,
    const fin_fuzzy_sentiment_t* sentiment,
    fin_archetype_suitability_t* out_suitability);

//=============================================================================
// Emotional Modulation
//=============================================================================

/** Apply emotional modulation to a decision */
int financial_investor_archetype_apply_emotion(
    financial_investor_archetype_t* arch,
    const fin_emotional_state_t* emotion,
    fin_archetype_decision_t* inout_decision);

/** Compute emotional state from market conditions */
int financial_investor_archetype_compute_emotion(
    financial_investor_archetype_t* arch,
    const fin_fuzzy_market_condition_t* market,
    const fin_fuzzy_sentiment_t* sentiment,
    float stress_level, float arousal_level,
    fin_emotional_state_t* out_emotion);

//=============================================================================
// Mirror Learning & Self-Reflection
//=============================================================================

/** Record a decision outcome for mirror learning */
int financial_investor_archetype_mirror_record(
    financial_investor_archetype_t* arch,
    const fin_mirror_record_t* record);

/** Trigger self-reflection on recent performance */
int financial_investor_archetype_self_reflect(
    financial_investor_archetype_t* arch,
    float* out_accuracy, float* out_calibration);

//=============================================================================
// Health & Modulation
//=============================================================================

int financial_investor_archetype_set_inflammation(
    financial_investor_archetype_t* arch, float level);
int financial_investor_archetype_set_fatigue(
    financial_investor_archetype_t* arch, float level);
int financial_investor_archetype_get_stats(
    const financial_investor_archetype_t* arch,
    fin_archetype_stats_t* stats);
void financial_investor_archetype_reset_stats(
    financial_investor_archetype_t* arch);
const char* financial_investor_archetype_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_INVESTOR_ARCHETYPE_H */
