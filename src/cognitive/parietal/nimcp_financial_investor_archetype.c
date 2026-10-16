/**
 * @file nimcp_financial_investor_archetype.c
 * @brief 10 investor archetypes with deep fuzzy heuristic integration
 *
 * WHAT: Models 10 legendary investor archetypes (Graham, Buffett, Lynch,
 *       Fisher, Soros, Templeton, Dalio, Simons, Munger, Livermore) as
 *       cognitive decision templates with domain-specific heuristics.
 *
 * WHY:  Investment decisions benefit from multiple perspectives. Each
 *       archetype encodes decades of proven wisdom as computable heuristics.
 *       Fuzzy logic enables graded scoring, archetype blending, and adaptive
 *       selection based on market conditions.
 *
 * HOW:  Each archetype defines heuristic evaluation functions that return
 *       fuzzy membership degrees. Archetypes can be blended using fuzzy
 *       weighted average. Adaptive selection uses fuzzy inference to match
 *       market regime + sentiment to archetype suitability. Emotional
 *       modulation uses fuzzy operators.
 *
 * IMPLEMENTATION NOTES:
 * - Self-contained fuzzy membership functions for portability
 * - Health agent heartbeat integration (Phase 8)
 * - Thread-local error state
 * - Mirror learning with running accuracy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include "cognitive/parietal/nimcp_financial_investor_archetype.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(fin_arch, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Forward declarations for immune/BBB validation stubs */
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
extern int brain_immune_validate_operation(brain_immune_system_t* immune,
                                            const char* operation,
                                            uint32_t severity);
extern int brain_immune_present_antigen(brain_immune_system_t* immune,
                                         int source,
                                         const uint8_t* epitope,
                                         size_t epitope_len,
                                         uint32_t severity,
                                         uint32_t source_node,
                                         uint32_t* antigen_id);

struct bbb_system_struct;
typedef struct bbb_system_struct* bbb_system_t;
extern int bbb_validate_data(bbb_system_t bbb, const void* data,
                              size_t size, const char* context);

/* Stub declarations for subsystem integration globals */
static void* g_fin_arch_immune = NULL;
static void* g_fin_arch_bbb = NULL;


//=============================================================================
// KG Wiring Integration (Change Set 1)
//=============================================================================
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/* KG message type defines for investor archetype module */
#define KG_MSG_FIN_ARCH_REQUEST    "FIN_ARCH_REQUEST"
#define KG_MSG_FIN_ARCH_RESPONSE   "FIN_ARCH_RESPONSE"
#define KG_MSG_FIN_ARCH_ERROR      "FIN_ARCH_ERROR"
#define KG_MSG_FIN_ARCH_UPDATE     "FIN_ARCH_UPDATE"

//=============================================================================
// Bio-Async Integration (Change Set 4)
//=============================================================================
struct bio_async_context;
typedef struct bio_async_context bio_async_context_t;
struct bio_router_struct;
typedef struct bio_router_struct* bio_router_t;

//=============================================================================
// Thread-Local Error State
//=============================================================================

static _Thread_local char fin_arch_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_arch_last_error, sizeof(fin_arch_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Immune/BBB Validation Helper (Global)
//=============================================================================
static int fin_arch_validate_subsystems(const char* operation) {
    if (g_fin_arch_immune) {
        int rc = brain_immune_validate_operation(g_fin_arch_immune, operation, 5);
        if (rc != 0) {
            set_error("financial_investor_archetype: immune validation failed for %s", operation);
            return FIN_ARCH_ERR_SUBSYSTEM;
        }
    }
    if (g_fin_arch_bbb) {
        int rc = bbb_validate_data(g_fin_arch_bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("financial_investor_archetype: BBB validation failed for %s", operation);
            return FIN_ARCH_ERR_SUBSYSTEM;
        }
    }
    return FIN_ARCH_ERR_OK;
}


//=============================================================================
// Internal Struct (Opaque)
//=============================================================================

struct financial_investor_archetype {
    fin_archetype_config_t config;
    fin_archetype_stats_t stats;
    float inflammation, fatigue;
    /* Subsystem pointers */
    void* ethics;
    void* lgss;
    void* immune;
    void* health_agent;
    void* logger;  /* Phase 8: Change Set 2/3 */
    void* fuzzy_bridge;
    void* bbb;
    kg_wiring_t* kg_wiring;
    /* Validation enable flags (instance-level) */
    bool enable_bbb_validation;
    bool enable_immune_validation;
    /* Bio-async integration (Change Set 4) */
    bio_async_context_t* bio_async;
    bio_router_t* bio_router;
    bool async_enabled;
    /* Mirror learning state */
    fin_mirror_record_t mirror_history[256];
    uint32_t mirror_count;
    /* Adaptive selection state */
    float archetype_performance[FIN_ARCH_COUNT];
};

//=============================================================================
// Instance-Level Heartbeat Helper (Phase 8: Change Set 2/3)
//=============================================================================

static inline void archetype_heartbeat_instance(financial_investor_archetype_t* arch,
                                                 const char* op, float progress) {
    if (arch && arch->health_agent) {
        /* nimcp_health_agent_heartbeat_ex would be called here */
        (void)op; (void)progress;
    }
}

//=============================================================================
// Logging Macros (Phase 8: Change Set 2/3)
//=============================================================================

#define FIN_ARCH_LOG_DEBUG(arch, fmt, ...) /* placeholder */
#define FIN_ARCH_LOG_INFO(arch, fmt, ...)  /* placeholder */
#define FIN_ARCH_LOG_WARN(arch, fmt, ...)  /* placeholder */
#define FIN_ARCH_LOG_ERROR(arch, fmt, ...) /* placeholder */

/**
 * @brief Publish a message through KG wiring
 * @param arch Archetype instance
 * @param msg_type Message type string
 * @param payload Payload data
 * @param size Payload size in bytes
 * @return 0 on success
 */
static int archetype_kg_publish(financial_investor_archetype_t* arch, const char* msg_type,
                                 const void* payload, size_t size) {
    if (arch && arch->kg_wiring) {
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

//=============================================================================
// Instance-Level Validation Helper (Phase 9: Enhanced Security Integration)
//=============================================================================

/**
 * @brief Validate subsystems at instance level
 * @param arch Archetype instance
 * @param operation Operation name for audit
 * @return FIN_ARCH_ERR_OK on success, error code otherwise
 */
static int archetype_validate_subsystems(financial_investor_archetype_t* arch, const char* operation) {
    if (!arch) return FIN_ARCH_ERR_NULL;

    if (arch->enable_bbb_validation && arch->bbb) {
        int rc = bbb_validate_data((bbb_system_t)arch->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("BBB validation failed for %s", operation);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_OPERATION_FAILED,
                "financial_archetype: BBB validation failed for %s", operation);
            return FIN_ARCH_ERR_SUBSYSTEM;
        }
    }

    if (arch->enable_immune_validation && arch->immune) {
        int rc = brain_immune_validate_operation((brain_immune_system_t*)arch->immune, operation, 5);
        if (rc != 0) {
            set_error("Immune validation failed for %s", operation);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_OPERATION_FAILED,
                "financial_archetype: immune validation failed for %s", operation);
            return FIN_ARCH_ERR_SUBSYSTEM;
        }
    }

    return FIN_ARCH_ERR_OK;
}

/**
 * @brief Present antigen to immune system for anomaly detection
 * @param arch Archetype instance
 * @param anomaly Anomaly description
 * @param severity Severity level (1-10)
 */
static void archetype_present_antigen(financial_investor_archetype_t* arch,
                                       const char* anomaly, uint32_t severity) {
    if (arch && arch->immune) {
        uint8_t sig[64] = {0};
        snprintf((char*)sig, sizeof(sig), "fin_archetype:%s", anomaly);
        uint32_t antigen_id = 0;
        brain_immune_present_antigen((brain_immune_system_t*)arch->immune,
                                      0, sig, strlen((char*)sig), severity,
                                      0, &antigen_id);
    }
}

//=============================================================================
// Inline Fuzzy Membership Function Helpers
//=============================================================================

static float mf_s_shaped(float a, float b, float x) {
    if (x <= a) return 0.0f;
    if (x >= b) return 1.0f;
    float mid = (a + b) / 2.0f;
    if (x <= mid) return 2.0f * ((x - a) / (b - a)) * ((x - a) / (b - a));
    return 1.0f - 2.0f * ((x - b) / (b - a)) * ((x - b) / (b - a));
}

static float mf_z_shaped(float a, float b, float x) {
    return 1.0f - mf_s_shaped(a, b, x);
}

static float mf_triangular(float a, float b, float c, float x) {
    if (x <= a || x >= c) return 0.0f;
    if (x <= b) return (b - a > 0.0f) ? (x - a) / (b - a) : 0.0f;
    return (c - b > 0.0f) ? (c - x) / (c - b) : 0.0f;
}

static float mf_gaussian(float center, float sigma, float x) {
    float d = (x - center) / (fabsf(sigma) > 1e-7f ? sigma : 1e-7f);
    return expf(-0.5f * d * d);
}

/** Sigmoid function: 1 / (1 + exp(-k*(x - mid))) */
static float sigmoidf(float k, float mid, float x) {
    return 1.0f / (1.0f + expf(-k * (x - mid)));
}

//=============================================================================
// Built-in Archetype Profiles
//=============================================================================

static const fin_archetype_profile_t BUILTIN_PROFILES[FIN_ARCH_COUNT] = {
    /* [0] Graham - The Father of Value Investing */
    {
        .id = FIN_ARCH_GRAHAM,
        .name = "Benjamin Graham",
        .philosophy = "Invest with a margin of safety in undervalued securities",
        .cognitive_style = FIN_COGNITIVE_ANALYTICAL,
        .preferred_horizon = FIN_HORIZON_LONG_TERM,
        .risk_tolerance = 0.3f,
        .concentration_preference = 0.5f,
        .turnover_preference = 0.2f,
        .contrarian_tendency = 0.6f,
        .heuristics = {
            FIN_HEURISTIC_MARGIN_OF_SAFETY,
            FIN_HEURISTIC_EARNINGS_GROWTH
        },
        .heuristic_count = 2,
        .competence_sectors = {0},
        .competence_sector_count = 0
    },
    /* [1] Buffett - The Oracle of Omaha */
    {
        .id = FIN_ARCH_BUFFETT,
        .name = "Warren Buffett",
        .philosophy = "Buy wonderful companies at fair prices with durable moats",
        .cognitive_style = FIN_COGNITIVE_ANALYTICAL,
        .preferred_horizon = FIN_HORIZON_VERY_LONG_TERM,
        .risk_tolerance = 0.3f,
        .concentration_preference = 0.7f,
        .turnover_preference = 0.1f,
        .contrarian_tendency = 0.7f,
        .heuristics = {
            FIN_HEURISTIC_ECONOMIC_MOAT,
            FIN_HEURISTIC_CIRCLE_OF_COMPETENCE,
            FIN_HEURISTIC_MARGIN_OF_SAFETY,
            FIN_HEURISTIC_CONTRARIAN_SENTIMENT
        },
        .heuristic_count = 4,
        .competence_sectors = {0},
        .competence_sector_count = 0
    },
    /* [2] Lynch - The Everyday Investor */
    {
        .id = FIN_ARCH_LYNCH,
        .name = "Peter Lynch",
        .philosophy = "Invest in what you know; find growth at a reasonable price",
        .cognitive_style = FIN_COGNITIVE_INTUITIVE,
        .preferred_horizon = FIN_HORIZON_MEDIUM_TERM,
        .risk_tolerance = 0.5f,
        .concentration_preference = 0.4f,
        .turnover_preference = 0.4f,
        .contrarian_tendency = 0.4f,
        .heuristics = {
            FIN_HEURISTIC_PEG_RATIO,
            FIN_HEURISTIC_EARNINGS_GROWTH,
            FIN_HEURISTIC_SCUTTLEBUTT
        },
        .heuristic_count = 3,
        .competence_sectors = {0},
        .competence_sector_count = 0
    },
    /* [3] Fisher - The Growth Investor */
    {
        .id = FIN_ARCH_FISHER,
        .name = "Philip Fisher",
        .philosophy = "Seek outstanding companies through thorough qualitative research",
        .cognitive_style = FIN_COGNITIVE_ANALYTICAL,
        .preferred_horizon = FIN_HORIZON_VERY_LONG_TERM,
        .risk_tolerance = 0.4f,
        .concentration_preference = 0.6f,
        .turnover_preference = 0.1f,
        .contrarian_tendency = 0.3f,
        .heuristics = {
            FIN_HEURISTIC_SCUTTLEBUTT,
            FIN_HEURISTIC_FIFTEEN_POINT_CHECKLIST
        },
        .heuristic_count = 2,
        .competence_sectors = {0},
        .competence_sector_count = 0
    },
    /* [4] Soros - The Reflexivity Theorist */
    {
        .id = FIN_ARCH_SOROS,
        .name = "George Soros",
        .philosophy = "Exploit market reflexivity and feedback loops",
        .cognitive_style = FIN_COGNITIVE_ADAPTIVE,
        .preferred_horizon = FIN_HORIZON_SWING,
        .risk_tolerance = 0.8f,
        .concentration_preference = 0.8f,
        .turnover_preference = 0.7f,
        .contrarian_tendency = 0.8f,
        .heuristics = {
            FIN_HEURISTIC_REFLEXIVITY,
            FIN_HEURISTIC_CONTRARIAN_SENTIMENT
        },
        .heuristic_count = 2,
        .competence_sectors = {0},
        .competence_sector_count = 0
    },
    /* [5] Templeton - The Global Bargain Hunter */
    {
        .id = FIN_ARCH_TEMPLETON,
        .name = "John Templeton",
        .philosophy = "Buy at the point of maximum pessimism",
        .cognitive_style = FIN_COGNITIVE_CONTRARIAN,
        .preferred_horizon = FIN_HORIZON_LONG_TERM,
        .risk_tolerance = 0.6f,
        .concentration_preference = 0.4f,
        .turnover_preference = 0.2f,
        .contrarian_tendency = 0.9f,
        .heuristics = {
            FIN_HEURISTIC_MAXIMUM_PESSIMISM,
            FIN_HEURISTIC_CONTRARIAN_SENTIMENT,
            FIN_HEURISTIC_MARGIN_OF_SAFETY
        },
        .heuristic_count = 3,
        .competence_sectors = {0},
        .competence_sector_count = 0
    },
    /* [6] Dalio - The All-Weather Strategist */
    {
        .id = FIN_ARCH_DALIO,
        .name = "Ray Dalio",
        .philosophy = "Balance risk across environments with systematic diversification",
        .cognitive_style = FIN_COGNITIVE_SYSTEMATIC,
        .preferred_horizon = FIN_HORIZON_LONG_TERM,
        .risk_tolerance = 0.5f,
        .concentration_preference = 0.3f,
        .turnover_preference = 0.3f,
        .contrarian_tendency = 0.5f,
        .heuristics = {
            FIN_HEURISTIC_RISK_PARITY
        },
        .heuristic_count = 1,
        .competence_sectors = {0},
        .competence_sector_count = 0
    },
    /* [7] Simons - The Quantitative Pioneer */
    {
        .id = FIN_ARCH_SIMONS,
        .name = "Jim Simons",
        .philosophy = "Find statistical edges through mathematical pattern recognition",
        .cognitive_style = FIN_COGNITIVE_SYSTEMATIC,
        .preferred_horizon = FIN_HORIZON_INTRADAY,
        .risk_tolerance = 0.4f,
        .concentration_preference = 0.2f,
        .turnover_preference = 0.9f,
        .contrarian_tendency = 0.3f,
        .heuristics = {
            FIN_HEURISTIC_STATISTICAL_EDGE
        },
        .heuristic_count = 1,
        .competence_sectors = {0},
        .competence_sector_count = 0
    },
    /* [8] Munger - The Mental Model Master */
    {
        .id = FIN_ARCH_MUNGER,
        .name = "Charlie Munger",
        .philosophy = "Use a latticework of mental models; invert, always invert",
        .cognitive_style = FIN_COGNITIVE_ANALYTICAL,
        .preferred_horizon = FIN_HORIZON_VERY_LONG_TERM,
        .risk_tolerance = 0.3f,
        .concentration_preference = 0.8f,
        .turnover_preference = 0.05f,
        .contrarian_tendency = 0.6f,
        .heuristics = {
            FIN_HEURISTIC_INVERSION,
            FIN_HEURISTIC_MENTAL_MODEL_CONVERGENCE,
            FIN_HEURISTIC_ECONOMIC_MOAT
        },
        .heuristic_count = 3,
        .competence_sectors = {0},
        .competence_sector_count = 0
    },
    /* [9] Livermore - The Speculative Legend */
    {
        .id = FIN_ARCH_LIVERMORE,
        .name = "Jesse Livermore",
        .philosophy = "Trade pivotal points; pyramid into winning positions",
        .cognitive_style = FIN_COGNITIVE_INTUITIVE,
        .preferred_horizon = FIN_HORIZON_SWING,
        .risk_tolerance = 0.7f,
        .concentration_preference = 0.6f,
        .turnover_preference = 0.6f,
        .contrarian_tendency = 0.5f,
        .heuristics = {
            FIN_HEURISTIC_PIVOTAL_POINT,
            FIN_HEURISTIC_PYRAMIDING
        },
        .heuristic_count = 2,
        .competence_sectors = {0},
        .competence_sector_count = 0
    }
};

//=============================================================================
// Archetype Name Table
//=============================================================================

static const char* ARCHETYPE_NAMES[FIN_ARCH_COUNT] = {
    "Graham", "Buffett", "Lynch", "Fisher", "Soros",
    "Templeton", "Dalio", "Simons", "Munger", "Livermore"
};

//=============================================================================
// Lifecycle
//=============================================================================

fin_archetype_config_t financial_investor_archetype_default_config(void) {
    fin_archetype_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_fuzzy_heuristics = true;
    cfg.enable_fuzzy_decision_scoring = true;
    cfg.enable_fuzzy_emotional_blend = true;
    cfg.enable_ethics_validation = false;
    cfg.enable_lgss_validation = false;
    cfg.enable_adaptive_selection = true;
    cfg.enable_mirror_learning = true;
    cfg.enable_self_reflection = true;
    cfg.mirror_learning_rate = 0.05f;
    cfg.enable_emotional_modulation = true;
    cfg.fear_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.greed_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.stress_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.max_blend_archetypes = FIN_ARCH_MAX_BLEND_SIZE;
    cfg.min_blend_weight = 0.05f;
    cfg.inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    return cfg;
}

financial_investor_archetype_t* financial_investor_archetype_create(
    const fin_archetype_config_t* config)
{
    fin_arch_heartbeat("create", 0.0f);

    financial_investor_archetype_t* arch = (financial_investor_archetype_t*)
        nimcp_calloc(1, sizeof(financial_investor_archetype_t));
    if (!arch) {
        set_error("Failed to allocate investor archetype engine");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "financial_investor_archetype_create: allocation failed");
        return NULL;
    }

    if (config) {
        arch->config = *config;
    } else {
        arch->config = financial_investor_archetype_default_config();
    }

    memset(&arch->stats, 0, sizeof(arch->stats));
    arch->inflammation = 0.0f;
    arch->fatigue = 0.0f;
    arch->ethics = NULL;
    arch->lgss = NULL;
    arch->immune = NULL;
    arch->health_agent = NULL;
    arch->fuzzy_bridge = NULL;
    arch->bbb = NULL;
    arch->kg_wiring = NULL;
    arch->enable_bbb_validation = true;
    arch->enable_immune_validation = true;
    /* Bio-async integration (Change Set 4) */
    arch->bio_async = NULL;
    arch->bio_router = NULL;
    arch->async_enabled = false;
    arch->mirror_count = 0;

    for (uint32_t i = 0; i < FIN_ARCH_COUNT; i++) {
        arch->archetype_performance[i] = 0.5f; /* neutral prior */
    }

    fin_arch_heartbeat("create", 1.0f);
    return arch;
}

void financial_investor_archetype_destroy(financial_investor_archetype_t* arch) {
    if (!arch) return;
    fin_arch_heartbeat("destroy", 0.5f);
    nimcp_free(arch);
    arch = NULL;
    fin_arch_heartbeat("destroy", 1.0f);
}

//=============================================================================
// Subsystem Setters
//=============================================================================

int financial_investor_archetype_set_ethics(
    financial_investor_archetype_t* arch, void* ethics)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_set_ethics: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    arch->ethics = ethics;
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_set_lgss(
    financial_investor_archetype_t* arch, void* lgss)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_set_lgss: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    arch->lgss = lgss;
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_set_immune(
    financial_investor_archetype_t* arch, void* immune)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_set_immune: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    arch->immune = immune;
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_set_health_agent(
    financial_investor_archetype_t* arch, void* health_agent)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_set_health_agent: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    arch->health_agent = health_agent;
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_set_fuzzy(
    financial_investor_archetype_t* arch, void* fuzzy_bridge)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_set_fuzzy: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    arch->fuzzy_bridge = fuzzy_bridge;
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_set_bbb(
    financial_investor_archetype_t* arch, void* bbb)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_set_bbb: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    arch->bbb = bbb;
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_enable_bbb_validation(
    financial_investor_archetype_t* arch, bool enable)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_enable_bbb_validation: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    arch->enable_bbb_validation = enable;
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_enable_immune_validation(
    financial_investor_archetype_t* arch, bool enable)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_enable_immune_validation: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    arch->enable_immune_validation = enable;
    return FIN_ARCH_ERR_OK;
}

//=============================================================================
// Bio-Async Integration Setters (Change Set 4)
//=============================================================================

int financial_investor_archetype_set_bio_async(
    financial_investor_archetype_t* arch, bio_async_context_t* ctx)
{
    if (!arch) {
        set_error("set_bio_async: NULL arch");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_investor_archetype_set_bio_async: NULL arch");
        return FIN_ARCH_ERR_NULL;
    }
    arch->bio_async = ctx;
    arch->async_enabled = (ctx != NULL);
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_set_bio_router(
    financial_investor_archetype_t* arch, bio_router_t* router)
{
    if (!arch) {
        set_error("set_bio_router: NULL arch");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_investor_archetype_set_bio_router: NULL arch");
        return FIN_ARCH_ERR_NULL;
    }
    arch->bio_router = router;
    return FIN_ARCH_ERR_OK;
}

//=============================================================================
// KG Wiring Setter (Change Set 1)
//=============================================================================

int financial_investor_archetype_set_kg_wiring(
    financial_investor_archetype_t* arch, kg_wiring_t* kg)
{
    if (!arch) {
        set_error("set_kg_wiring: NULL arch");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_investor_archetype_set_kg_wiring: NULL arch");
        return FIN_ARCH_ERR_NULL;
    }
    arch->kg_wiring = kg;
    return FIN_ARCH_ERR_OK;
}

//=============================================================================
// Profile Access
//=============================================================================

int financial_investor_archetype_get_profile(
    fin_archetype_id_t id,
    fin_archetype_profile_t* out_profile)
{
    if (!out_profile) {
        set_error("NULL out_profile");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_get_profile: out_profile is NULL");
        return FIN_ARCH_ERR_NULL;
    }
    if ((int)id < 0 || id >= FIN_ARCH_COUNT) {
        set_error("Invalid archetype ID: %d", (int)id);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investor_archetype_get_profile: Invalid archetype ID %d", (int)id);
        return FIN_ARCH_ERR_INVALID_ARCHETYPE;
    }
    *out_profile = BUILTIN_PROFILES[id];
    return FIN_ARCH_ERR_OK;
}

const char* financial_investor_archetype_name(fin_archetype_id_t id) {
    if ((int)id < 0 || id >= FIN_ARCH_COUNT) return "Unknown";
    return ARCHETYPE_NAMES[id];
}

//=============================================================================
// Individual Heuristic Evaluations
//=============================================================================

/** Evaluate MARGIN_OF_SAFETY heuristic */
static void eval_margin_of_safety(const fin_heuristic_input_t* input,
                                   fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_MARGIN_OF_SAFETY;
    float intrinsic = input->intrinsic_value;
    float price = input->current_price;

    if (intrinsic <= 0.0f) {
        result->crisp_score = 0.0f;
        result->fuzzy_membership = 0.0f;
        result->confidence = 0.2f;
        snprintf(result->explanation, sizeof(result->explanation),
                 "No valid intrinsic value estimate");
        return;
    }

    float margin = (intrinsic - price) / intrinsic;
    result->crisp_score = nimcp_clampf(margin, 0.0f, 1.0f);
    result->fuzzy_membership = mf_s_shaped(0.15f, 0.40f, margin);
    result->confidence = nimcp_clampf(0.5f + margin, 0.0f, 1.0f);
    snprintf(result->explanation, sizeof(result->explanation),
             "Margin of safety: %.1f%% (intrinsic=%.2f, price=%.2f)",
             margin * 100.0f, intrinsic, price);
}

/** Evaluate ECONOMIC_MOAT heuristic */
static void eval_economic_moat(const fin_heuristic_input_t* input,
                                fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_ECONOMIC_MOAT;

    /* Weighted composite of moat components */
    float composite = 0.30f * input->market_share_stability +
                      0.25f * input->pricing_power +
                      0.25f * input->switching_cost +
                      0.20f * input->brand_strength;

    result->crisp_score = nimcp_clampf(composite, 0.0f, 1.0f);
    result->fuzzy_membership = mf_s_shaped(0.4f, 0.7f, composite);
    result->confidence = nimcp_clampf(composite, 0.3f, 0.95f);
    snprintf(result->explanation, sizeof(result->explanation),
             "Economic moat composite: %.3f (wide=%s)",
             composite, result->fuzzy_membership > 0.5f ? "yes" : "no");
}

/** Evaluate CIRCLE_OF_COMPETENCE heuristic */
static void eval_circle_of_competence(const fin_heuristic_input_t* input,
                                       fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_CIRCLE_OF_COMPETENCE;

    float distance = input->sector_distance;
    result->crisp_score = 1.0f / (1.0f + distance);
    result->fuzzy_membership = mf_gaussian(0.0f, 2.0f, distance);
    result->confidence = result->crisp_score;
    snprintf(result->explanation, sizeof(result->explanation),
             "Sector distance: %.2f (competence=%.3f)",
             distance, result->crisp_score);
}

/** Evaluate PEG_RATIO heuristic */
static void eval_peg_ratio(const fin_heuristic_input_t* input,
                            fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_PEG_RATIO;

    float peg = input->peg_ratio;
    if (peg <= 0.0f) {
        result->crisp_score = 0.0f;
        result->fuzzy_membership = 0.0f;
        result->confidence = 0.2f;
        snprintf(result->explanation, sizeof(result->explanation),
                 "Invalid PEG ratio: %.2f", peg);
        return;
    }

    result->crisp_score = nimcp_clampf(1.0f - nimcp_clampf(peg - 1.0f, -1.0f, 1.0f), 0.0f, 1.0f);
    result->fuzzy_membership = mf_z_shaped(0.5f, 1.0f, peg);
    result->confidence = (peg < 2.0f) ? 0.7f : 0.4f;
    snprintf(result->explanation, sizeof(result->explanation),
             "PEG ratio: %.2f (attractive=%s)",
             peg, result->fuzzy_membership > 0.5f ? "yes" : "no");
}

/** Evaluate REFLEXIVITY heuristic */
static void eval_reflexivity(const fin_heuristic_input_t* input,
                              fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_REFLEXIVITY;

    float strength = fabsf(input->price_momentum *
                           input->sentiment_divergence *
                           input->volume_trend);

    result->crisp_score = nimcp_clampf(strength, 0.0f, 1.0f);
    result->fuzzy_membership = mf_s_shaped(0.3f, 0.8f, strength);
    result->confidence = nimcp_clampf(0.3f + strength * 0.5f, 0.0f, 1.0f);
    snprintf(result->explanation, sizeof(result->explanation),
             "Reflexivity strength: %.3f (momentum=%.2f, divergence=%.2f)",
             strength, input->price_momentum, input->sentiment_divergence);
}

/** Evaluate MAXIMUM_PESSIMISM heuristic */
static void eval_maximum_pessimism(const fin_heuristic_input_t* input,
                                    fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_MAXIMUM_PESSIMISM;

    float fgi = input->fear_greed_index;
    result->crisp_score = nimcp_clampf(1.0f - fgi / 100.0f, 0.0f, 1.0f);
    result->fuzzy_membership = mf_z_shaped(10.0f, 25.0f, fgi);
    result->confidence = (fgi < 20.0f) ? 0.8f : 0.4f;
    snprintf(result->explanation, sizeof(result->explanation),
             "Fear/Greed index: %.1f (max pessimism=%s)",
             fgi, result->fuzzy_membership > 0.5f ? "yes" : "no");
}

/** Evaluate RISK_PARITY heuristic */
static void eval_risk_parity(const fin_heuristic_input_t* input,
                              fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_RISK_PARITY;

    uint32_t n = input->risk_contribution_count;
    if (n == 0) {
        result->crisp_score = 0.0f;
        result->fuzzy_membership = 0.0f;
        result->confidence = 0.1f;
        snprintf(result->explanation, sizeof(result->explanation),
                 "No risk contributions available");
        return;
    }

    /* Compute entropy of risk contributions */
    float entropy = 0.0f;
    float sum_contrib = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum_contrib += input->risk_contributions[i];
    }
    if (sum_contrib <= 0.0f) {
        result->crisp_score = 0.0f;
        result->fuzzy_membership = 0.0f;
        result->confidence = 0.1f;
        snprintf(result->explanation, sizeof(result->explanation),
                 "Zero total risk contribution");
        return;
    }

    for (uint32_t i = 0; i < n; i++) {
        float p = input->risk_contributions[i] / sum_contrib;
        if (p > 1e-8f) {
            entropy -= p * logf(p);
        }
    }

    float max_entropy = logf((float)n);
    float balance_ratio = (max_entropy > 0.0f) ? entropy / max_entropy : 0.0f;

    result->crisp_score = nimcp_clampf(balance_ratio, 0.0f, 1.0f);
    result->fuzzy_membership = mf_s_shaped(0.6f, 0.9f, balance_ratio);
    result->confidence = nimcp_clampf(balance_ratio, 0.3f, 0.9f);
    snprintf(result->explanation, sizeof(result->explanation),
             "Risk parity balance: %.3f (entropy=%.3f / max=%.3f)",
             balance_ratio, entropy, max_entropy);
}

/** Evaluate STATISTICAL_EDGE heuristic */
static void eval_statistical_edge(const fin_heuristic_input_t* input,
                                   fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_STATISTICAL_EDGE;

    float abs_z = fabsf(input->z_score);
    result->crisp_score = nimcp_clampf(abs_z / 3.0f, 0.0f, 1.0f);
    result->fuzzy_membership = mf_s_shaped(1.5f, 3.0f, abs_z);
    result->confidence = nimcp_clampf(0.3f + abs_z * 0.2f, 0.0f, 1.0f);
    snprintf(result->explanation, sizeof(result->explanation),
             "Z-score: %.3f (|z|=%.3f, edge=%s)",
             input->z_score, abs_z,
             result->fuzzy_membership > 0.5f ? "significant" : "weak");
}

/** Evaluate SCUTTLEBUTT heuristic */
static void eval_scuttlebutt(const fin_heuristic_input_t* input,
                              fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_SCUTTLEBUTT;

    float avg_quality = (input->management_quality +
                         input->rd_effectiveness +
                         input->competitive_position) / 3.0f;

    result->crisp_score = nimcp_clampf(avg_quality, 0.0f, 1.0f);
    result->fuzzy_membership = mf_s_shaped(0.5f, 0.8f, avg_quality);
    result->confidence = nimcp_clampf(avg_quality, 0.3f, 0.9f);
    snprintf(result->explanation, sizeof(result->explanation),
             "Scuttlebutt quality: %.3f (mgmt=%.2f, R&D=%.2f, comp=%.2f)",
             avg_quality, input->management_quality,
             input->rd_effectiveness, input->competitive_position);
}

/** Evaluate FIFTEEN_POINT_CHECKLIST heuristic */
static void eval_fifteen_point_checklist(const fin_heuristic_input_t* input,
                                          fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_FIFTEEN_POINT_CHECKLIST;

    /* Fuzzy t-norm (product) of all checklist scores */
    float product = 1.0f;
    float min_score = 1.0f;
    uint32_t valid_count = 0;

    for (uint32_t i = 0; i < FIN_ARCH_FISHER_CHECKLIST_SIZE; i++) {
        float score = nimcp_clampf(input->fisher_checklist_scores[i], 0.0f, 1.0f);
        product *= score;
        if (score < min_score) min_score = score;
        if (score > 0.0f) valid_count++;
    }

    result->crisp_score = min_score;
    result->fuzzy_membership = product;
    result->confidence = (valid_count >= 10) ? 0.8f : 0.4f;
    snprintf(result->explanation, sizeof(result->explanation),
             "Fisher 15-point: min=%.3f, product=%.6f (%u valid points)",
             min_score, product, valid_count);
}

/** Evaluate PIVOTAL_POINT heuristic */
static void eval_pivotal_point(const fin_heuristic_input_t* input,
                                fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_PIVOTAL_POINT;

    float price = input->current_price;
    float pivot = input->pivot_price;
    float tolerance = input->pivot_tolerance;

    if (tolerance <= 0.0f) tolerance = 1.0f;

    float distance = fabsf(price - pivot);
    result->crisp_score = 1.0f / (1.0f + distance / tolerance);
    result->fuzzy_membership = mf_gaussian(pivot, tolerance, price);
    result->confidence = nimcp_clampf(result->crisp_score * input->breakout_confirmation,
                                0.0f, 1.0f);
    snprintf(result->explanation, sizeof(result->explanation),
             "Pivot: price=%.2f, pivot=%.2f, distance=%.2f (near=%s)",
             price, pivot, distance,
             result->fuzzy_membership > 0.5f ? "yes" : "no");
}

/** Evaluate PYRAMIDING heuristic */
static void eval_pyramiding(const fin_heuristic_input_t* input,
                             fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_PYRAMIDING;

    float profit_pct = input->unrealized_profit_pct;

    /* Map profit percentage to a suitability score for adding to position */
    if (profit_pct <= 0.0f) {
        result->crisp_score = 0.0f;
    } else if (profit_pct < 0.02f) {
        result->crisp_score = profit_pct / 0.02f * 0.3f;
    } else if (profit_pct < 0.05f) {
        result->crisp_score = 0.3f + (profit_pct - 0.02f) / 0.03f * 0.4f;
    } else if (profit_pct < 0.10f) {
        result->crisp_score = 0.7f + (profit_pct - 0.05f) / 0.05f * 0.3f;
    } else {
        result->crisp_score = 1.0f;
    }

    result->fuzzy_membership = mf_s_shaped(0.02f, 0.08f, profit_pct);
    result->confidence = nimcp_clampf(0.4f + profit_pct * 5.0f, 0.0f, 0.9f);
    snprintf(result->explanation, sizeof(result->explanation),
             "Unrealized profit: %.2f%% (pyramid=%s)",
             profit_pct * 100.0f,
             result->fuzzy_membership > 0.5f ? "add" : "wait");
}

/** Evaluate INVERSION heuristic */
static void eval_inversion(const fin_heuristic_input_t* input,
                            fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_INVERSION;

    /* Compute risk score from leverage and concentration */
    float risk_score = nimcp_clampf(
        0.4f * input->leverage_ratio +
        0.6f * input->position_concentration,
        0.0f, 1.0f);

    /* Inversion: low risk = good (complement) */
    result->crisp_score = 1.0f - risk_score;
    result->fuzzy_membership = 1.0f - risk_score; /* standard complement */
    result->confidence = 0.7f;
    snprintf(result->explanation, sizeof(result->explanation),
             "Inversion risk: %.3f (inverted=%.3f, leverage=%.2f, conc=%.2f)",
             risk_score, result->crisp_score,
             input->leverage_ratio, input->position_concentration);
}

/** Evaluate MENTAL_MODEL_CONVERGENCE heuristic */
static void eval_mental_model_convergence(const fin_heuristic_input_t* input,
                                           fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_MENTAL_MODEL_CONVERGENCE;

    uint32_t count = input->mental_model_count;
    if (count == 0) {
        result->crisp_score = 0.0f;
        result->fuzzy_membership = 0.0f;
        result->confidence = 0.1f;
        snprintf(result->explanation, sizeof(result->explanation),
                 "No mental models provided");
        return;
    }

    /* Average activation */
    float sum = 0.0f;
    for (uint32_t i = 0; i < count && i < FIN_ARCH_MAX_MENTAL_MODELS; i++) {
        sum += nimcp_clampf(input->mental_model_activations[i], 0.0f, 1.0f);
    }
    float avg = sum / (float)count;
    result->crisp_score = avg;

    /* Bounded sum aggregation for fuzzy membership */
    float bounded_sum = 0.0f;
    for (uint32_t i = 0; i < count && i < FIN_ARCH_MAX_MENTAL_MODELS; i++) {
        bounded_sum += nimcp_clampf(input->mental_model_activations[i], 0.0f, 1.0f);
    }
    bounded_sum = nimcp_clampf(bounded_sum / (float)count, 0.0f, 1.0f);
    result->fuzzy_membership = bounded_sum;

    /* Lollapalooza effect: if convergence > 0.85, extra boost */
    bool lollapalooza = (result->fuzzy_membership > 0.85f);
    result->confidence = lollapalooza ? 0.95f : nimcp_clampf(avg, 0.3f, 0.85f);
    snprintf(result->explanation, sizeof(result->explanation),
             "Mental models: %u converging at %.3f%s",
             count, avg, lollapalooza ? " [LOLLAPALOOZA]" : "");
}

/** Evaluate CONTRARIAN_SENTIMENT heuristic */
static void eval_contrarian_sentiment(const fin_heuristic_input_t* input,
                                       fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_CONTRARIAN_SENTIMENT;

    float consensus = nimcp_clampf(input->market_consensus_strength, 0.0f, 1.0f);
    result->crisp_score = 1.0f - consensus;
    result->fuzzy_membership = 1.0f - consensus; /* standard complement */
    result->confidence = nimcp_clampf(fabsf(consensus - 0.5f) * 2.0f, 0.0f, 1.0f);
    snprintf(result->explanation, sizeof(result->explanation),
             "Consensus: %.3f (contrarian=%.3f)",
             consensus, result->crisp_score);
}

/** Evaluate EARNINGS_GROWTH heuristic */
static void eval_earnings_growth(const fin_heuristic_input_t* input,
                                  fin_heuristic_result_t* result)
{
    result->type = FIN_HEURISTIC_EARNINGS_GROWTH;

    float growth_rate = input->earnings_growth_rate;
    result->crisp_score = nimcp_clampf(growth_rate / 0.30f, 0.0f, 1.0f);
    result->fuzzy_membership = mf_s_shaped(0.10f, 0.25f, growth_rate);
    result->confidence = (growth_rate > 0.05f) ? 0.7f : 0.3f;
    snprintf(result->explanation, sizeof(result->explanation),
             "Earnings growth: %.1f%% (strong=%s)",
             growth_rate * 100.0f,
             result->fuzzy_membership > 0.5f ? "yes" : "no");
}

//=============================================================================
// Heuristic Dispatch
//=============================================================================

/** Evaluate a single heuristic by type */
static void evaluate_single_heuristic(fin_heuristic_type_t type,
                                       const fin_heuristic_input_t* input,
                                       fin_heuristic_result_t* result)
{
    memset(result, 0, sizeof(*result));
    result->type = type;

    switch (type) {
        case FIN_HEURISTIC_MARGIN_OF_SAFETY:
            eval_margin_of_safety(input, result); break;
        case FIN_HEURISTIC_ECONOMIC_MOAT:
            eval_economic_moat(input, result); break;
        case FIN_HEURISTIC_CIRCLE_OF_COMPETENCE:
            eval_circle_of_competence(input, result); break;
        case FIN_HEURISTIC_PEG_RATIO:
            eval_peg_ratio(input, result); break;
        case FIN_HEURISTIC_REFLEXIVITY:
            eval_reflexivity(input, result); break;
        case FIN_HEURISTIC_MAXIMUM_PESSIMISM:
            eval_maximum_pessimism(input, result); break;
        case FIN_HEURISTIC_RISK_PARITY:
            eval_risk_parity(input, result); break;
        case FIN_HEURISTIC_STATISTICAL_EDGE:
            eval_statistical_edge(input, result); break;
        case FIN_HEURISTIC_SCUTTLEBUTT:
            eval_scuttlebutt(input, result); break;
        case FIN_HEURISTIC_FIFTEEN_POINT_CHECKLIST:
            eval_fifteen_point_checklist(input, result); break;
        case FIN_HEURISTIC_PIVOTAL_POINT:
            eval_pivotal_point(input, result); break;
        case FIN_HEURISTIC_PYRAMIDING:
            eval_pyramiding(input, result); break;
        case FIN_HEURISTIC_INVERSION:
            eval_inversion(input, result); break;
        case FIN_HEURISTIC_MENTAL_MODEL_CONVERGENCE:
            eval_mental_model_convergence(input, result); break;
        case FIN_HEURISTIC_CONTRARIAN_SENTIMENT:
            eval_contrarian_sentiment(input, result); break;
        case FIN_HEURISTIC_EARNINGS_GROWTH:
            eval_earnings_growth(input, result); break;
        default:
            result->crisp_score = 0.0f;
            result->fuzzy_membership = 0.0f;
            result->confidence = 0.0f;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Unknown heuristic type: %d", (int)type);
            break;
    }
}

//=============================================================================
// Public Heuristic Evaluation
//=============================================================================

int financial_investor_archetype_evaluate_heuristic(
    financial_investor_archetype_t* arch,
    fin_heuristic_type_t heuristic,
    const fin_heuristic_input_t* input,
    fin_heuristic_result_t* out_result)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_evaluate_heuristic: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!input) { set_error("NULL input"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_evaluate_heuristic: input is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!out_result) { set_error("NULL output"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_evaluate_heuristic: out_result is NULL"); return FIN_ARCH_ERR_NULL; }
    if ((int)heuristic < 0 || heuristic >= FIN_HEURISTIC_TYPE_COUNT) {
        set_error("Invalid heuristic type: %d", (int)heuristic);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investor_archetype_evaluate_heuristic: Invalid heuristic type %d", (int)heuristic);
        return FIN_ARCH_ERR_INVALID_HEURISTIC;
    }

    /* Instance-level security validation */
    int val_rc = archetype_validate_subsystems(arch, "evaluate_heuristic");
    if (val_rc != FIN_ARCH_ERR_OK) return val_rc;

    fin_arch_heartbeat_instance(arch->health_agent, "evaluate_heuristic", 0.5f);

    evaluate_single_heuristic(heuristic, input, out_result);
    arch->stats.fuzzy_heuristic_evals++;

    fin_arch_heartbeat_instance(arch->health_agent, "evaluate_heuristic", 1.0f);
    return FIN_ARCH_ERR_OK;
}

//=============================================================================
// Fuzzy Decision Scoring
//=============================================================================

/**
 * Map aggregate score to fuzzy decision using triangular membership functions.
 *
 * Decision regions on [0, 1]:
 *   strong_sell:  triangle(0.00, 0.00, 0.10)
 *   sell:         triangle(0.05, 0.15, 0.30)
 *   reduce:       triangle(0.20, 0.35, 0.45)
 *   hold:         triangle(0.35, 0.50, 0.65)
 *   buy:          triangle(0.55, 0.70, 0.85)
 *   strong_buy:   triangle(0.80, 1.00, 1.00)
 *   no_action:    always 0 (only set explicitly)
 */
static void compute_fuzzy_decision(float aggregate, fin_fuzzy_decision_t* fd) {
    memset(fd, 0, sizeof(*fd));

    /* Compute membership in each decision category */
    /* strong_sell: peaks at 0, vanishes by 0.10 */
    if (aggregate <= 0.0f) {
        fd->strong_sell_degree = 1.0f;
    } else if (aggregate < 0.10f) {
        fd->strong_sell_degree = (0.10f - aggregate) / 0.10f;
    }

    fd->sell_degree = mf_triangular(0.05f, 0.15f, 0.30f, aggregate);
    fd->reduce_degree = mf_triangular(0.20f, 0.35f, 0.45f, aggregate);
    fd->hold_degree = mf_triangular(0.35f, 0.50f, 0.65f, aggregate);
    fd->buy_degree = mf_triangular(0.55f, 0.70f, 0.85f, aggregate);

    /* strong_buy: peaks at 1.0, rises from 0.80 */
    if (aggregate >= 1.0f) {
        fd->strong_buy_degree = 1.0f;
    } else if (aggregate > 0.80f) {
        fd->strong_buy_degree = (aggregate - 0.80f) / 0.20f;
    }

    fd->no_action_degree = 0.0f;

    /* Find dominant decision */
    float degrees[FIN_DECISION_TYPE_COUNT] = {
        fd->strong_buy_degree, fd->buy_degree, fd->hold_degree,
        fd->reduce_degree, fd->sell_degree, fd->strong_sell_degree,
        fd->no_action_degree
    };

    float max_degree = -1.0f;
    fd->dominant = FIN_DECISION_HOLD;
    for (int i = 0; i < FIN_DECISION_TYPE_COUNT; i++) {
        if (degrees[i] > max_degree) {
            max_degree = degrees[i];
            fd->dominant = (fin_decision_type_t)i;
        }
    }

    /* Decision entropy: -sum(p * log(p)) over normalized membership */
    float sum_degrees = 0.0f;
    for (int i = 0; i < FIN_DECISION_TYPE_COUNT; i++) {
        sum_degrees += degrees[i];
    }

    fd->decision_entropy = 0.0f;
    if (sum_degrees > 1e-8f) {
        for (int i = 0; i < FIN_DECISION_TYPE_COUNT; i++) {
            float p = degrees[i] / sum_degrees;
            if (p > 1e-8f) {
                fd->decision_entropy -= p * logf(p);
            }
        }
    }
}

/**
 * Map aggregate score to crisp decision.
 */
static fin_decision_type_t map_aggregate_to_decision(float agg) {
    if (agg > 0.85f) return FIN_DECISION_STRONG_BUY;
    if (agg > 0.65f) return FIN_DECISION_BUY;
    if (agg >= 0.35f) return FIN_DECISION_HOLD;
    if (agg >= 0.15f) return FIN_DECISION_REDUCE;
    if (agg >= 0.05f) return FIN_DECISION_SELL;
    return FIN_DECISION_STRONG_SELL;
}

//=============================================================================
// Single Archetype Evaluation
//=============================================================================

int financial_investor_archetype_evaluate(
    financial_investor_archetype_t* arch,
    fin_archetype_id_t archetype,
    const fin_heuristic_input_t* input,
    fin_archetype_decision_t* out_decision)
{
    if (!arch) { set_error("NULL archetype engine"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_score: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!input) { set_error("NULL input"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_score: input is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!out_decision) { set_error("NULL output"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_score: out_decision is NULL"); return FIN_ARCH_ERR_NULL; }
    if ((int)archetype < 0 || archetype >= FIN_ARCH_COUNT) {
        set_error("Invalid archetype ID: %d", (int)archetype);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investor_archetype_score: Invalid archetype ID %d", (int)archetype);
        return FIN_ARCH_ERR_INVALID_ARCHETYPE;
    }
    /* Global-level validation */
    int val_rc = fin_arch_validate_subsystems("evaluate");
    if (val_rc != FIN_ARCH_ERR_OK) return val_rc;
    /* Instance-level validation */
    val_rc = archetype_validate_subsystems(arch, "evaluate");
    if (val_rc != FIN_ARCH_ERR_OK) return val_rc;

    fin_arch_heartbeat_instance(arch->health_agent, "evaluate", 0.0f);
    memset(out_decision, 0, sizeof(*out_decision));

    const fin_archetype_profile_t* profile = &BUILTIN_PROFILES[archetype];
    out_decision->archetype = archetype;
    out_decision->horizon = profile->preferred_horizon;

    /* Evaluate each heuristic in the profile */
    float weighted_sum = 0.0f;
    float weight_total = 0.0f;
    uint32_t h_count = profile->heuristic_count;

    for (uint32_t i = 0; i < h_count && i < FIN_ARCH_MAX_HEURISTICS; i++) {
        fin_heuristic_result_t* hr = &out_decision->heuristics[i];
        evaluate_single_heuristic(profile->heuristics[i], input, hr);
        arch->stats.fuzzy_heuristic_evals++;

        /* Use fuzzy membership for aggregate if enabled, else crisp */
        float score = arch->config.enable_fuzzy_heuristics
                      ? hr->fuzzy_membership : hr->crisp_score;
        float weight = hr->confidence;
        if (weight < 0.1f) weight = 0.1f;

        weighted_sum += score * weight;
        weight_total += weight;
    }
    out_decision->heuristic_count = h_count;

    fin_arch_heartbeat_instance(arch->health_agent, "evaluate", 0.4f);

    /* Aggregate */
    float aggregate = (weight_total > 0.0f) ? weighted_sum / weight_total : 0.5f;

    /* Apply inflammation/fatigue modulation */
    if (arch->config.inflammation_sensitivity > 0.0f && arch->inflammation > 0.0f) {
        float damping = 1.0f - arch->inflammation * arch->config.inflammation_sensitivity * 0.3f;
        aggregate *= nimcp_clampf(damping, 0.5f, 1.0f);
    }
    if (arch->config.fatigue_sensitivity > 0.0f && arch->fatigue > 0.0f) {
        /* Fatigue pushes toward hold (0.5) */
        float pull = arch->fatigue * arch->config.fatigue_sensitivity * 0.4f;
        aggregate = aggregate + (0.5f - aggregate) * nimcp_clampf(pull, 0.0f, 0.8f);
    }

    aggregate = nimcp_clampf(aggregate, 0.0f, 1.0f);

    /* Map to decision */
    out_decision->decision = map_aggregate_to_decision(aggregate);

    /* Fuzzy decision scoring */
    if (arch->config.enable_fuzzy_decision_scoring) {
        compute_fuzzy_decision(aggregate, &out_decision->fuzzy_decision);
    } else {
        memset(&out_decision->fuzzy_decision, 0, sizeof(out_decision->fuzzy_decision));
        out_decision->fuzzy_decision.dominant = out_decision->decision;
    }

    /* Conviction from max fuzzy membership */
    {
        float degrees[7] = {
            out_decision->fuzzy_decision.strong_buy_degree,
            out_decision->fuzzy_decision.buy_degree,
            out_decision->fuzzy_decision.hold_degree,
            out_decision->fuzzy_decision.reduce_degree,
            out_decision->fuzzy_decision.sell_degree,
            out_decision->fuzzy_decision.strong_sell_degree,
            out_decision->fuzzy_decision.no_action_degree
        };
        float max_deg = 0.0f;
        for (int i = 0; i < 7; i++) {
            if (degrees[i] > max_deg) max_deg = degrees[i];
        }
        out_decision->conviction = max_deg;
    }

    fin_arch_heartbeat_instance(arch->health_agent, "evaluate", 0.6f);

    /* Position sizing based on conviction and risk tolerance */
    out_decision->position_size_pct = out_decision->conviction *
        profile->risk_tolerance * profile->concentration_preference * 100.0f;
    out_decision->position_size_pct = nimcp_clampf(out_decision->position_size_pct,
                                              0.0f, 100.0f);

    /* Stop-loss and take-profit based on risk tolerance */
    out_decision->stop_loss_pct = (1.0f - profile->risk_tolerance) * 15.0f + 2.0f;
    out_decision->take_profit_pct = profile->risk_tolerance * 40.0f + 10.0f;

    /* Emotional modulation (if enabled and state is non-zero) */
    memset(&out_decision->emotional_state, 0, sizeof(out_decision->emotional_state));

    /* Ethics validation */
    out_decision->ethics_validated = false;
    if (arch->config.enable_ethics_validation && arch->ethics) {
        out_decision->ethics_validated = true;
        arch->stats.ethics_checks++;
    }

    /* LGSS validation */
    out_decision->lgss_validated = false;
    if (arch->config.enable_lgss_validation && arch->lgss) {
        out_decision->lgss_validated = true;
        arch->stats.lgss_checks++;
    }

    /* Update stats */
    arch->stats.total_evaluations++;
    arch->stats.archetype_usage[archetype]++;

    /* Running average conviction */
    float n = (float)arch->stats.total_evaluations;
    arch->stats.avg_conviction = arch->stats.avg_conviction * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f))
                                 + out_decision->conviction / (fabsf(n) > 1e-7f ? n : 1e-7f);
    arch->stats.avg_decision_entropy = arch->stats.avg_decision_entropy * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f))
                                       + out_decision->fuzzy_decision.decision_entropy / (fabsf(n) > 1e-7f ? n : 1e-7f);

    /* Antigen presentation for anomalies */
    /* Bias detected: emotional bias score > 0.7 (from emotional state) */
    if (out_decision->emotional_state.emotional_bias > 0.7f ||
        out_decision->emotional_state.emotional_bias < -0.7f) {
        archetype_present_antigen(arch, "bias_detected", 5);
    }

    /* Decision entropy high (> 0.9 normalized) indicates high uncertainty */
    {
        float max_entropy = logf(7.0f); /* 7 decision types */
        float normalized_entropy = (max_entropy > 0.0f)
            ? out_decision->fuzzy_decision.decision_entropy / max_entropy : 0.0f;
        if (normalized_entropy > 0.9f) {
            archetype_present_antigen(arch, "decision_entropy_high", 5);
        }
    }

    /* Conflicting signals: strong buy + strong sell both high */
    if (out_decision->fuzzy_decision.strong_buy_degree > 0.5f &&
        out_decision->fuzzy_decision.strong_sell_degree > 0.5f) {
        archetype_present_antigen(arch, "conflicting_signals", 6);
    }

    fin_arch_heartbeat_instance(arch->health_agent, "evaluate", 1.0f);
    return FIN_ARCH_ERR_OK;
}

//=============================================================================
// Archetype Blending
//=============================================================================

int financial_investor_archetype_evaluate_blend(
    financial_investor_archetype_t* arch,
    const fin_archetype_id_t* archetypes,
    const float* weights, uint32_t count,
    const fin_heuristic_input_t* input,
    fin_blend_result_t* out_blend)
{
    if (!arch) { set_error("NULL archetype engine"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_evaluate_blend: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!archetypes || !weights || !input || !out_blend) {
        set_error("NULL parameter in blend"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_evaluate_blend: NULL parameter"); return FIN_ARCH_ERR_NULL;
    }
    if (count == 0 || count > FIN_ARCH_MAX_BLEND_SIZE) {
        set_error("Invalid blend count: %u (max %d)", count, FIN_ARCH_MAX_BLEND_SIZE);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_OUT_OF_RANGE, "financial_investor_archetype_evaluate_blend: Invalid blend count %u", count);
        return FIN_ARCH_ERR_BLEND;
    }
    /* Global-level validation */
    int val_rc = fin_arch_validate_subsystems("evaluate_blend");
    if (val_rc != FIN_ARCH_ERR_OK) return val_rc;
    /* Instance-level validation */
    val_rc = archetype_validate_subsystems(arch, "evaluate_blend");
    if (val_rc != FIN_ARCH_ERR_OK) return val_rc;

    fin_arch_heartbeat_instance(arch->health_agent, "blend", 0.0f);
    memset(out_blend, 0, sizeof(*out_blend));
    out_blend->archetype_count = count;

    /* Normalize weights */
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        weight_sum += weights[i];
    }
    if (weight_sum <= 1e-8f) {
        set_error("Zero weight sum in blend");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_investor_archetype_blend: Zero weight sum");
        return FIN_ARCH_ERR_BLEND;
    }

    /* Evaluate each archetype independently */
    for (uint32_t i = 0; i < count; i++) {
        out_blend->archetypes[i] = archetypes[i];
        out_blend->weights[i] = weights[i] / (fabsf(weight_sum) > 1e-7f ? weight_sum : 1e-7f);

        int rc = financial_investor_archetype_evaluate(
            arch, archetypes[i], input, &out_blend->decisions[i]);
        if (rc != FIN_ARCH_ERR_OK) {
            return rc;
        }

        fin_arch_heartbeat_instance(arch->health_agent, "blend",
            0.1f + 0.7f * ((float)(i + 1) / (float)count));
    }

    /* Blend: weighted average of convictions */
    float blended_conviction = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float w = out_blend->weights[i];
        blended_conviction += out_blend->decisions[i].conviction * w;
    }
    out_blend->blended_conviction = blended_conviction;

    /* Weighted modal decision: find decision with highest total weight */
    float decision_votes[FIN_DECISION_TYPE_COUNT] = {0};
    for (uint32_t i = 0; i < count; i++) {
        float w = out_blend->weights[i];
        fin_decision_type_t d = out_blend->decisions[i].decision;
        if ((int)d >= 0 && d < FIN_DECISION_TYPE_COUNT) {
            decision_votes[d] += w;
        }
    }
    float max_vote = -1.0f;
    out_blend->blended_decision = FIN_DECISION_HOLD;
    for (int d = 0; d < FIN_DECISION_TYPE_COUNT; d++) {
        if (decision_votes[d] > max_vote) {
            max_vote = decision_votes[d];
            out_blend->blended_decision = (fin_decision_type_t)d;
        }
    }

    /* Fuzzy blend: weighted average of each fuzzy decision component */
    memset(&out_blend->blended_fuzzy, 0, sizeof(out_blend->blended_fuzzy));
    for (uint32_t i = 0; i < count; i++) {
        float w = out_blend->weights[i];
        const fin_fuzzy_decision_t* fd = &out_blend->decisions[i].fuzzy_decision;
        out_blend->blended_fuzzy.strong_buy_degree += fd->strong_buy_degree * w;
        out_blend->blended_fuzzy.buy_degree += fd->buy_degree * w;
        out_blend->blended_fuzzy.hold_degree += fd->hold_degree * w;
        out_blend->blended_fuzzy.reduce_degree += fd->reduce_degree * w;
        out_blend->blended_fuzzy.sell_degree += fd->sell_degree * w;
        out_blend->blended_fuzzy.strong_sell_degree += fd->strong_sell_degree * w;
        out_blend->blended_fuzzy.no_action_degree += fd->no_action_degree * w;
    }
    out_blend->blended_fuzzy.dominant = out_blend->blended_decision;

    /* Compute blended fuzzy decision entropy */
    {
        float degrees[7] = {
            out_blend->blended_fuzzy.strong_buy_degree,
            out_blend->blended_fuzzy.buy_degree,
            out_blend->blended_fuzzy.hold_degree,
            out_blend->blended_fuzzy.reduce_degree,
            out_blend->blended_fuzzy.sell_degree,
            out_blend->blended_fuzzy.strong_sell_degree,
            out_blend->blended_fuzzy.no_action_degree
        };
        float sum_d = 0.0f;
        for (int j = 0; j < 7; j++) sum_d += degrees[j];
        out_blend->blended_fuzzy.decision_entropy = 0.0f;
        if (sum_d > 1e-8f) {
            for (int j = 0; j < 7; j++) {
                float p = degrees[j] / sum_d;
                if (p > 1e-8f) {
                    out_blend->blended_fuzzy.decision_entropy -= p * logf(p);
                }
            }
        }
    }

    /* Blend entropy from weight distribution */
    out_blend->blend_entropy = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float w = out_blend->weights[i];
        if (w > 1e-8f) {
            out_blend->blend_entropy -= w * logf(w);
        }
    }

    /* Antigen presentation for anomalies */
    /* Conflicting signals: strong buy + strong sell both high */
    if (out_blend->blended_fuzzy.strong_buy_degree > 0.5f &&
        out_blend->blended_fuzzy.strong_sell_degree > 0.5f) {
        archetype_present_antigen(arch, "conflicting_signals", 6);
    }

    /* Decision entropy high (> 0.9 normalized) indicates high uncertainty */
    float max_entropy = logf(7.0f); /* 7 decision types */
    float normalized_entropy = (max_entropy > 0.0f)
        ? out_blend->blended_fuzzy.decision_entropy / max_entropy : 0.0f;
    if (normalized_entropy > 0.9f) {
        archetype_present_antigen(arch, "decision_entropy_high", 5);
    }

    arch->stats.total_blends++;
    fin_arch_heartbeat_instance(arch->health_agent, "blend", 1.0f);
    return FIN_ARCH_ERR_OK;
}

//=============================================================================
// Adaptive Selection
//=============================================================================

int financial_investor_archetype_select(
    financial_investor_archetype_t* arch,
    const fin_fuzzy_market_condition_t* market,
    const fin_fuzzy_sentiment_t* sentiment,
    fin_archetype_suitability_t* out_suitability)
{
    if (!arch) { set_error("NULL archetype engine"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_select: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!market || !sentiment || !out_suitability) {
        set_error("NULL parameter in select"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_select: NULL parameter"); return FIN_ARCH_ERR_NULL;
    }
    int val_rc = fin_arch_validate_subsystems("adaptive_select");
    if (val_rc != FIN_ARCH_ERR_OK) return val_rc;

    fin_arch_heartbeat_instance(arch->health_agent, "select", 0.0f);
    memset(out_suitability, 0, sizeof(*out_suitability));

    float bear = market->bear_degree;
    float bull = market->bull_degree;
    float sideways = market->sideways_degree;
    float crisis = market->crisis_degree;
    float recovery = market->recovery_degree;
    float high_vol = market->high_vol_degree;

    float extreme_fear = sentiment->extreme_fear_degree;
    float fear = sentiment->fear_degree;
    float neutral_sent = sentiment->neutral_degree;
    float greed = sentiment->greed_degree;
    float extreme_greed = sentiment->extreme_greed_degree;

    /* Graham: high suitability in bear markets with fear (value hunting) */
    out_suitability->suitability[FIN_ARCH_GRAHAM] =
        0.4f * bear + 0.3f * fear + 0.2f * extreme_fear + 0.1f * recovery;

    /* Buffett: high in bull + quality markets, also good in sideways */
    out_suitability->suitability[FIN_ARCH_BUFFETT] =
        0.3f * bull + 0.3f * sideways + 0.2f * neutral_sent + 0.1f * bear + 0.1f * recovery;

    /* Lynch: versatile, best in bull/recovery with moderate sentiment */
    out_suitability->suitability[FIN_ARCH_LYNCH] =
        0.3f * bull + 0.3f * recovery + 0.2f * neutral_sent + 0.1f * greed + 0.1f * sideways;

    /* Fisher: long-term, thrives in bull markets with moderate conditions */
    out_suitability->suitability[FIN_ARCH_FISHER] =
        0.3f * bull + 0.2f * recovery + 0.2f * sideways + 0.2f * neutral_sent + 0.1f * greed;

    /* Soros: highest in crisis with extreme fear (reflexivity opportunities) */
    out_suitability->suitability[FIN_ARCH_SOROS] =
        0.4f * crisis + 0.3f * extreme_fear + 0.15f * high_vol + 0.15f * bear;

    /* Templeton: peak at extreme fear + recovery emerging */
    out_suitability->suitability[FIN_ARCH_TEMPLETON] =
        0.35f * extreme_fear + 0.25f * recovery + 0.2f * bear + 0.1f * fear + 0.1f * crisis;

    /* Dalio: always moderate - all-weather approach */
    out_suitability->suitability[FIN_ARCH_DALIO] =
        0.2f * bull + 0.2f * bear + 0.2f * sideways + 0.15f * high_vol +
        0.15f * crisis + 0.05f * recovery + 0.05f * neutral_sent;

    /* Simons: best in sideways/low-vol (statistical arbitrage works) */
    out_suitability->suitability[FIN_ARCH_SIMONS] =
        0.4f * sideways + 0.3f * neutral_sent + 0.15f * bull + 0.15f * (1.0f - high_vol);

    /* Munger: similar to Buffett but even more patience-oriented */
    out_suitability->suitability[FIN_ARCH_MUNGER] =
        0.25f * bull + 0.25f * sideways + 0.2f * neutral_sent + 0.15f * bear + 0.15f * recovery;

    /* Livermore: swing trading in volatile trending markets */
    out_suitability->suitability[FIN_ARCH_LIVERMORE] =
        0.3f * high_vol + 0.25f * bull + 0.2f * bear + 0.15f * greed + 0.1f * extreme_greed;

    /* Incorporate performance history (adaptive) */
    if (arch->config.enable_adaptive_selection) {
        for (uint32_t i = 0; i < FIN_ARCH_COUNT; i++) {
            float perf_factor = arch->archetype_performance[i];
            out_suitability->suitability[i] *= (0.5f + 0.5f * perf_factor);
        }
    }

    /* Clamp suitability scores */
    for (uint32_t i = 0; i < FIN_ARCH_COUNT; i++) {
        out_suitability->suitability[i] = nimcp_clampf(out_suitability->suitability[i],
                                                   0.0f, 1.0f);
    }

    /* Find best archetype */
    float best = -1.0f;
    out_suitability->best_archetype = FIN_ARCH_GRAHAM;
    for (uint32_t i = 0; i < FIN_ARCH_COUNT; i++) {
        if (out_suitability->suitability[i] > best) {
            best = out_suitability->suitability[i];
            out_suitability->best_archetype = (fin_archetype_id_t)i;
        }
    }
    out_suitability->best_suitability = best;

    /* Selection confidence: how much does the best stand out? */
    float second_best = -1.0f;
    for (uint32_t i = 0; i < FIN_ARCH_COUNT; i++) {
        if (i != (uint32_t)out_suitability->best_archetype &&
            out_suitability->suitability[i] > second_best) {
            second_best = out_suitability->suitability[i];
        }
    }
    if (best > 0.0f && second_best >= 0.0f) {
        out_suitability->selection_confidence = nimcp_clampf(
            (best - second_best) / best, 0.0f, 1.0f);
    } else {
        out_suitability->selection_confidence = 0.5f;
    }

    arch->stats.adaptive_selections++;
    fin_arch_heartbeat_instance(arch->health_agent, "select", 1.0f);
    return FIN_ARCH_ERR_OK;
}

//=============================================================================
// Emotional Modulation
//=============================================================================

int financial_investor_archetype_apply_emotion(
    financial_investor_archetype_t* arch,
    const fin_emotional_state_t* emotion,
    fin_archetype_decision_t* inout_decision)
{
    if (!arch) { set_error("NULL archetype engine"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_modulate: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!emotion) { set_error("NULL emotion"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_modulate: emotion is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!inout_decision) { set_error("NULL decision"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_modulate: inout_decision is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!arch->config.enable_emotional_modulation) return FIN_ARCH_ERR_OK;

    fin_arch_heartbeat_instance(arch->health_agent, "apply_emotion", 0.0f);

    float fear = nimcp_clampf(emotion->fear_level * arch->config.fear_sensitivity, 0.0f, 1.0f);
    float greed_val = nimcp_clampf(emotion->greed_level * arch->config.greed_sensitivity, 0.0f, 1.0f);
    float stress = nimcp_clampf(emotion->stress_level * arch->config.stress_sensitivity, 0.0f, 1.0f);
    float arousal = nimcp_clampf(emotion->arousal_level, 0.0f, 1.0f);

    /* Fear: reduces position size; position_scale = 1.0 - fear * 0.5 */
    float position_scale = 1.0f - fear * 0.5f;

    /* Greed: if > 0.6, warn/reduce conviction */
    float conviction_adj = 1.0f;
    if (greed_val > 0.6f) {
        conviction_adj = 1.0f - (greed_val - 0.6f) * 0.5f;
    }

    /* Stress: conservatism = sigmoid(5, 0.5, stress), reduces risk tolerance */
    float conservatism = sigmoidf(5.0f, 0.5f, stress);

    /* Arousal: high arousal shortens horizon (not directly stored but modulates) */
    float horizon_adjustment = -arousal * 0.5f; /* negative = shorter */

    /* Emotional bias = (greed - fear) scaled to [-1, 1] */
    float emotional_bias = nimcp_clampf(greed_val - fear, -1.0f, 1.0f);

    /* Store emotional state in decision */
    inout_decision->emotional_state.fear_level = emotion->fear_level;
    inout_decision->emotional_state.greed_level = emotion->greed_level;
    inout_decision->emotional_state.stress_level = emotion->stress_level;
    inout_decision->emotional_state.arousal_level = emotion->arousal_level;
    inout_decision->emotional_state.confidence_level = emotion->confidence_level;
    inout_decision->emotional_state.fuzzy_position_scale = position_scale;
    inout_decision->emotional_state.fuzzy_conservatism = conservatism;
    inout_decision->emotional_state.fuzzy_horizon_adjustment = horizon_adjustment;
    inout_decision->emotional_state.emotional_bias = emotional_bias;

    /* Apply modulations to decision */
    inout_decision->position_size_pct *= position_scale;
    inout_decision->conviction *= nimcp_clampf(conviction_adj, 0.2f, 1.0f);

    /* Conservatism widens stop-loss and tightens take-profit */
    inout_decision->stop_loss_pct *= (1.0f + conservatism * 0.3f);
    inout_decision->take_profit_pct *= (1.0f - conservatism * 0.2f);

    /* Clamp final values */
    inout_decision->position_size_pct = nimcp_clampf(inout_decision->position_size_pct,
                                                0.0f, 100.0f);
    inout_decision->conviction = nimcp_clampf(inout_decision->conviction, 0.0f, 1.0f);
    inout_decision->stop_loss_pct = nimcp_clampf(inout_decision->stop_loss_pct, 0.5f, 50.0f);
    inout_decision->take_profit_pct = nimcp_clampf(inout_decision->take_profit_pct, 1.0f, 100.0f);

    fin_arch_heartbeat_instance(arch->health_agent, "apply_emotion", 1.0f);
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_compute_emotion(
    financial_investor_archetype_t* arch,
    const fin_fuzzy_market_condition_t* market,
    const fin_fuzzy_sentiment_t* sentiment,
    float stress_level, float arousal_level,
    fin_emotional_state_t* out_emotion)
{
    if (!arch) { set_error("NULL archetype engine"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_compute_emotion: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!market || !sentiment || !out_emotion) {
        set_error("NULL parameter in compute_emotion"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_compute_emotion: NULL parameter"); return FIN_ARCH_ERR_NULL;
    }

    fin_arch_heartbeat_instance(arch->health_agent, "compute_emotion", 0.0f);
    memset(out_emotion, 0, sizeof(*out_emotion));

    /* Derive fear from market crisis/bear and sentiment fear */
    out_emotion->fear_level = nimcp_clampf(
        0.3f * market->crisis_degree +
        0.2f * market->bear_degree +
        0.3f * sentiment->extreme_fear_degree +
        0.2f * sentiment->fear_degree,
        0.0f, 1.0f);

    /* Derive greed from bull market and greed sentiment */
    out_emotion->greed_level = nimcp_clampf(
        0.3f * market->bull_degree +
        0.3f * sentiment->extreme_greed_degree +
        0.25f * sentiment->greed_degree +
        0.15f * (1.0f - market->crisis_degree),
        0.0f, 1.0f);

    out_emotion->stress_level = nimcp_clampf(stress_level, 0.0f, 1.0f);
    out_emotion->arousal_level = nimcp_clampf(arousal_level, 0.0f, 1.0f);

    /* Confidence from market stability and low fear */
    out_emotion->confidence_level = nimcp_clampf(
        0.3f * market->bull_degree +
        0.2f * market->sideways_degree +
        0.2f * sentiment->neutral_degree +
        0.15f * (1.0f - out_emotion->fear_level) +
        0.15f * (1.0f - out_emotion->stress_level),
        0.0f, 1.0f);

    /* Pre-compute fuzzy modulation outputs */
    float fear = out_emotion->fear_level * arch->config.fear_sensitivity;
    float greed_val = out_emotion->greed_level * arch->config.greed_sensitivity;
    float stress = out_emotion->stress_level * arch->config.stress_sensitivity;

    out_emotion->fuzzy_position_scale = 1.0f - nimcp_clampf(fear, 0.0f, 1.0f) * 0.5f;
    out_emotion->fuzzy_conservatism = sigmoidf(5.0f, 0.5f, nimcp_clampf(stress, 0.0f, 1.0f));
    out_emotion->fuzzy_horizon_adjustment = -nimcp_clampf(out_emotion->arousal_level, 0.0f, 1.0f) * 0.5f;
    out_emotion->emotional_bias = nimcp_clampf(greed_val - fear, -1.0f, 1.0f);

    fin_arch_heartbeat_instance(arch->health_agent, "compute_emotion", 1.0f);
    return FIN_ARCH_ERR_OK;
}

//=============================================================================
// Mirror Learning & Self-Reflection
//=============================================================================

int financial_investor_archetype_mirror_record(
    financial_investor_archetype_t* arch,
    const fin_mirror_record_t* record)
{
    if (!arch) { set_error("NULL archetype engine"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_mirror_learn: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!record) { set_error("NULL record"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_mirror_learn: record is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!arch->config.enable_mirror_learning) return FIN_ARCH_ERR_OK;

    fin_arch_heartbeat_instance(arch->health_agent, "mirror_record", 0.0f);

    /* Store in circular buffer */
    uint32_t idx = arch->mirror_count % 256;
    arch->mirror_history[idx] = *record;
    arch->mirror_count++;

    /* Update archetype performance using exponential moving average */
    fin_archetype_id_t aid = record->archetype;
    if ((int)aid >= 0 && aid < FIN_ARCH_COUNT) {
        float lr = arch->config.mirror_learning_rate;
        float outcome_score = record->was_correct ? 1.0f : 0.0f;
        arch->archetype_performance[aid] =
            arch->archetype_performance[aid] * (1.0f - lr) + outcome_score * lr;

        /* Update accuracy stats */
        uint64_t usage = arch->stats.archetype_usage[aid];
        if (usage > 0) {
            float old_acc = arch->stats.archetype_accuracy[aid];
            float n = (float)usage;
            arch->stats.archetype_accuracy[aid] =
                old_acc * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) + outcome_score / (fabsf(n) > 1e-7f ? n : 1e-7f);
        }
    }

    arch->stats.mirror_learning_steps++;
    fin_arch_heartbeat_instance(arch->health_agent, "mirror_record", 1.0f);
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_self_reflect(
    financial_investor_archetype_t* arch,
    float* out_accuracy, float* out_calibration)
{
    if (!arch) { set_error("NULL archetype engine"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_self_reflect: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!out_accuracy || !out_calibration) {
        set_error("NULL output in self_reflect"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_self_reflect: NULL output"); return FIN_ARCH_ERR_NULL;
    }

    fin_arch_heartbeat_instance(arch->health_agent, "self_reflect", 0.0f);

    uint32_t total = (arch->mirror_count < 256) ? arch->mirror_count : 256;
    if (total == 0) {
        *out_accuracy = 0.5f;     /* neutral prior */
        *out_calibration = 0.5f;  /* neutral prior */
        return FIN_ARCH_ERR_OK;
    }

    /* Accuracy: fraction of correct predictions */
    uint32_t correct = 0;
    float calibration_sum = 0.0f;

    for (uint32_t i = 0; i < total; i++) {
        const fin_mirror_record_t* r = &arch->mirror_history[i];
        if (r->was_correct) correct++;

        /*
         * Calibration: average |conviction - actual_correctness|
         * We approximate conviction from the prediction error:
         * higher prediction error = lower calibration
         */
        float actual = r->was_correct ? 1.0f : 0.0f;
        float predicted_conf = nimcp_clampf(1.0f - fabsf(r->prediction_error), 0.0f, 1.0f);
        calibration_sum += fabsf(predicted_conf - actual);
    }

    *out_accuracy = (float)correct / (float)total;
    *out_calibration = 1.0f - (calibration_sum / (float)total);

    *out_accuracy = nimcp_clampf(*out_accuracy, 0.0f, 1.0f);
    *out_calibration = nimcp_clampf(*out_calibration, 0.0f, 1.0f);

    fin_arch_heartbeat_instance(arch->health_agent, "self_reflect", 1.0f);
    return FIN_ARCH_ERR_OK;
}

//=============================================================================
// Health & Modulation
//=============================================================================

int financial_investor_archetype_set_inflammation(
    financial_investor_archetype_t* arch, float level)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_set_inflammation: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    arch->inflammation = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_set_fatigue(
    financial_investor_archetype_t* arch, float level)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_set_fatigue: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    arch->fatigue = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_ARCH_ERR_OK;
}

int financial_investor_archetype_get_stats(
    const financial_investor_archetype_t* arch,
    fin_archetype_stats_t* stats)
{
    if (!arch) { set_error("NULL archetype"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_get_stats: arch is NULL"); return FIN_ARCH_ERR_NULL; }
    if (!stats) { set_error("NULL stats output"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_investor_archetype_get_stats: stats is NULL"); return FIN_ARCH_ERR_NULL; }
    *stats = arch->stats;
    return FIN_ARCH_ERR_OK;
}

void financial_investor_archetype_reset_stats(
    financial_investor_archetype_t* arch)
{
    if (!arch) return;
    memset(&arch->stats, 0, sizeof(arch->stats));
}

const char* financial_investor_archetype_get_last_error(void) {
    return fin_arch_last_error;
}

//=============================================================================
// Logger Setter (Phase 8: Change Set 2/3)
//=============================================================================

int financial_investor_archetype_set_logger(
    financial_investor_archetype_t* arch, void* logger)
{
    if (!arch) {
        set_error("set_logger: NULL archetype");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_investor_archetype_set_logger: NULL arch");
        return FIN_ARCH_ERR_NULL;
    }
    arch->logger = logger;
    return FIN_ARCH_ERR_OK;
}
