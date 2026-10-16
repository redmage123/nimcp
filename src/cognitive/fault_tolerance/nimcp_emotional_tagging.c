/**
 * @file nimcp_emotional_tagging.c
 * @brief Emotional Tagging for Critical Failures - Implementation
 *
 * WHAT: Implements emotional tagging of failure events for priority learning
 * WHY:  Critical failures need emotional salience for enhanced memory formation
 * HOW:  Compute emotions from episode context, boost memory, prioritize by emotion
 *
 * BIOLOGICAL INSPIRATION:
 * - Amygdala emotional tagging modulates hippocampal memory consolidation
 * - High arousal → stronger memory formation (stress hormone effects)
 * - Negative valence (threat) → prioritized learning for survival
 * - Positive valence (relief) → reinforcement of successful strategies
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 2.7.0 Phase 10.8
 */

#include "cognitive/fault_tolerance/nimcp_emotional_tagging.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"

#define LOG_MODULE "cognitive.fault.emotional_tag"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

// Use unique prefix to avoid conflict with cognitive/nimcp_emotional_tagging.c
BRIDGE_BOILERPLATE(ft_emotional_tagging, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define BIO_MODULE_COGNITIVE_FAULT_EMOTIONAL_TAG 0x0355


//=============================================================================
// Constants
//=============================================================================

/* Emotion computation thresholds */
#define CRITICAL_ERROR_THRESHOLD 0.8f       /**< Data loss risk for critical errors */
#define FAST_RECOVERY_THRESHOLD_US 1000     /**< Fast recovery time (1ms) */
#define SLOW_RECOVERY_THRESHOLD_US 5000000  /**< Slow recovery time (5s) */
#define HIGH_RETRY_THRESHOLD 5              /**< Many retries indicate frustration */
#define CRITICAL_RETRY_THRESHOLD 10         /**< Very many retries */

/* Memory boost parameters */
#define BASE_MEMORY_BOOST 1.0F              /**< Baseline memory strength */
#define MAX_MEMORY_BOOST 2.5F               /**< Maximum memory boost */
#define HIGH_AROUSAL_BOOST 2.0f             /**< Boost for high arousal events */
#define MODERATE_AROUSAL_BOOST 1.5f         /**< Boost for moderate arousal */

/* Priority computation parameters */
#define FEAR_PRIORITY_WEIGHT 0.2f           /**< Fear contribution to priority */

/* Value clamping macros */
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define CLAMP_01(x) CLAMP(x, 0.0F, 1.0F)
#define CLAMP_VALENCE(x) CLAMP(x, -1.0F, 1.0F)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Emotional tagger implementation
 */
struct nimcp_emotional_tagger {
    /* Statistics */
    nimcp_emotional_tagger_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t mutex;

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Check if error type is critical
 *
 * WHAT: Determines if error type represents a severe failure
 * WHY:  Critical errors deserve higher emotional arousal
 * HOW:  String comparison against known critical error types
 *
 * @param error_type Error type string
 * @return true if critical, false otherwise
 */
static bool is_critical_error(const char* error_type) {
    if (!error_type) {
        return false;
    }

    /* WHAT: Critical error types that indicate severe failures */
    const char* critical_errors[] = {
        "SIGSEGV",
        "SIGABRT",
        "DATA_CORRUPTION",
        "CATASTROPHIC",
        "MEMORY_CORRUPTION",
        "STACK_OVERFLOW",
        NULL
    };

    for (int i = 0; critical_errors[i] != NULL; i++) {
        if (strcmp(error_type, critical_errors[i]) == 0) {
            return true;
        }
    }

    /* Not a critical error - this is normal, not an error condition */
    return false;
}

/**
 * @brief Compute emotional valence from episode
 *
 * WHAT: Calculates emotional valence (good/bad) from recovery outcome
 * WHY:  Successful recovery is positive, failure is negative
 * HOW:  Base valence from success, modulated by severity and speed
 *
 * ALGORITHM:
 * - Success → positive valence (0.5 to 0.9)
 * - Failure → negative valence (-0.5 to -0.9)
 * - Modulate by data loss risk and recovery time
 *
 * @param episode Recovery episode
 * @return Valence in [-1.0, 1.0]
 */
static float compute_valence(const nimcp_recovery_episode_t* episode) {
    float valence = 0.0f;

    if (episode->success) {
        /* WHAT: Successful recovery → positive valence
         * WHY:  Positive outcome deserves positive emotion
         * HOW:  Base +0.7, reduced by slow recovery time */
        valence = 0.7F;

        /* Fast recovery → more positive */
        if (episode->recovery_time_us < FAST_RECOVERY_THRESHOLD_US) {
            valence += 0.2F;
        }
        /* Slow recovery → less positive */
        else if (episode->recovery_time_us > SLOW_RECOVERY_THRESHOLD_US) {
            valence -= 0.2F;
        }
    } else {
        /* WHAT: Failed recovery → negative valence
         * WHY:  Failure is bad, especially with data loss risk
         * HOW:  Base -0.6, worsened by critical errors and data loss */
        valence = -0.6F;

        /* Critical error → more negative */
        if (is_critical_error(episode->error_type)) {
            valence -= 0.2F;
        }

        /* Data loss risk → proportionally more negative
         * WHY:  Higher data loss risk means worse outcome
         * HOW:  Scale contribution with risk level for smooth escalation */
        valence -= episode->data_loss_risk * 0.15F;
    }

    return CLAMP_VALENCE(valence);
}

/**
 * @brief Compute emotional arousal from episode
 *
 * WHAT: Calculates arousal (importance/intensity) from episode severity
 * WHY:  Important events need high arousal for attention and memory
 * HOW:  Combine data loss risk, error criticality, retry count
 *
 * ALGORITHM:
 * - Base arousal from data loss risk
 * - Boost for critical errors
 * - Boost for many retries
 *
 * @param episode Recovery episode
 * @return Arousal in [0.0, 1.0]
 */
static float compute_arousal(const nimcp_recovery_episode_t* episode) {
    /* WHAT: Base arousal from data loss risk
     * WHY:  Data loss is always important
     * HOW:  Direct mapping from risk to arousal */
    float arousal = episode->data_loss_risk * 0.7F;

    /* WHAT: Critical error → higher arousal
     * WHY:  Severe failures need immediate attention */
    if (is_critical_error(episode->error_type)) {
        arousal += 0.25F;
    }

    /* WHAT: Many retries → moderate arousal boost
     * WHY:  Repeated failures indicate persistent problem */
    if (episode->retry_count >= CRITICAL_RETRY_THRESHOLD) {
        arousal += 0.15F;
    } else if (episode->retry_count >= HIGH_RETRY_THRESHOLD) {
        arousal += 0.1F;
    }

    /* WHAT: Successful recovery adds arousal if fast
     * WHY:  Quick resolution is noteworthy
     * HOW:  Fast recovery is a significant event worth remembering */
    if (episode->success && episode->recovery_time_us < FAST_RECOVERY_THRESHOLD_US) {
        arousal += 0.2F;
    }

    return CLAMP_01(arousal);
}

/**
 * @brief Compute fear emotion from episode
 *
 * WHAT: Calculates fear level based on threat (data loss, critical errors)
 * WHY:  Fear guides threat detection and avoidance learning
 * HOW:  Combine data loss risk with error severity
 *
 * @param episode Recovery episode
 * @return Fear level in [0.0, 1.0]
 */
static float compute_fear(const nimcp_recovery_episode_t* episode) {
    /* WHAT: Fear primarily from data loss risk
     * WHY:  Data loss is the primary threat
     * HOW:  Direct mapping with critical error boost */
    float fear = episode->data_loss_risk * 0.8F;

    /* WHAT: Critical errors amplify fear
     * WHY:  SIGSEGV, corruption are especially threatening */
    if (is_critical_error(episode->error_type)) {
        fear += 0.2F;
    }

    /* WHAT: Successful recovery reduces fear to zero
     * WHY:  Threat averted, no fear needed */
    if (episode->success) {
        fear = 0.0F;
    }

    return CLAMP_01(fear);
}

/**
 * @brief Compute relief emotion from episode
 *
 * WHAT: Calculates relief from successful recovery
 * WHY:  Relief reinforces successful coping strategies
 * HOW:  Combine success with recovery speed
 *
 * @param episode Recovery episode
 * @return Relief level in [0.0, 1.0]
 */
static float compute_relief(const nimcp_recovery_episode_t* episode) {
    /* WHAT: Relief only for successful recovery
     * WHY:  Can't feel relief if recovery failed */
    if (!episode->success) {
        return 0.0F;
    }

    /* WHAT: Base relief from success
     * WHY:  Any successful recovery provides some relief */
    float relief = 0.6F;

    /* WHAT: Fast recovery → higher relief
     * WHY:  Quick resolution is more satisfying
     * HOW:  Linear scale from recovery time */
    if (episode->recovery_time_us < FAST_RECOVERY_THRESHOLD_US) {
        relief += 0.3F;
    } else if (episode->recovery_time_us < SLOW_RECOVERY_THRESHOLD_US) {
        /* Moderate speed → moderate relief */
        relief += 0.1F;
    } else {
        /* Slow recovery → reduced relief */
        relief -= 0.2F;
    }

    /* WHAT: Higher data loss risk → more relief when recovered
     * WHY:  Averting catastrophe is very relieving */
    if (episode->data_loss_risk > CRITICAL_ERROR_THRESHOLD) {
        relief += 0.15F;
    }

    return CLAMP_01(relief);
}

/**
 * @brief Compute frustration emotion from episode
 *
 * WHAT: Calculates frustration from repeated failures
 * WHY:  Frustration indicates need for strategy change
 * HOW:  Scale with retry count and failure status
 *
 * @param episode Recovery episode
 * @return Frustration level in [0.0, 1.0]
 */
static float compute_frustration(const nimcp_recovery_episode_t* episode) {
    /* WHAT: Frustration from repeated failures
     * WHY:  Many retries indicate inability to resolve problem
     * HOW:  Exponential scale with retry count */

    /* Successful recovery → no frustration */
    if (episode->success && episode->retry_count < HIGH_RETRY_THRESHOLD) {
        return 0.0F;
    }

    /* WHAT: Scale frustration with retries
     * WHY:  More attempts → more frustration */
    float frustration = 0.0f;
    if (episode->retry_count >= CRITICAL_RETRY_THRESHOLD) {
        frustration = 0.8F + (episode->retry_count - CRITICAL_RETRY_THRESHOLD) * 0.02F;
    } else if (episode->retry_count >= HIGH_RETRY_THRESHOLD) {
        frustration = 0.5F + (episode->retry_count - HIGH_RETRY_THRESHOLD) * 0.06F;
    } else {
        frustration = episode->retry_count * 0.1F;
    }

    /* WHAT: Failure amplifies frustration only when retries have occurred
     * WHY:  Frustration comes from repeated inability to resolve, not single failures
     * HOW:  Only add failure penalty when there were actual retries */
    if (!episode->success && episode->retry_count > 0) {
        frustration += 0.15F;
    }

    return CLAMP_01(frustration);
}

/**
 * @brief Update statistics after computing tag
 *
 * WHAT: Updates running statistics with new emotional tag
 * WHY:  Track emotional patterns over time
 * HOW:  Running average for valence/arousal, counters for specific emotions
 *
 * @param tagger Tagger instance
 * @param emotion Computed emotional tag
 */
static void update_statistics(
    nimcp_emotional_tagger_t* tagger,
    const nimcp_emotional_tag_t* emotion
) {
    // Process pending bio-async messages
    if (tagger && tagger->bio_async_enabled && tagger->bio_ctx) {
        bio_router_process_inbox(tagger->bio_ctx, 5);
    }

    /* WHAT: Update running averages
     * WHY:  Track overall emotional tone
     * HOW:  Incremental average formula */
    uint64_t n = tagger->stats.total_tags;
    if (isfinite(emotion->valence) && n < UINT64_MAX) {
        tagger->stats.avg_valence = (tagger->stats.avg_valence * (float)n + emotion->valence) / (float)(n + 1);
    }
    if (isfinite(emotion->arousal) && n < UINT64_MAX) {
        tagger->stats.avg_arousal = (tagger->stats.avg_arousal * (float)n + emotion->arousal) / (float)(n + 1);
    }

    /* WHAT: Count high-intensity specific emotions
     * WHY:  Track frequency of extreme emotional events */
    if (emotion->fear > 0.7F) {
        tagger->stats.high_fear_count++;
    }
    if (emotion->relief > 0.7F) {
        tagger->stats.high_relief_count++;
    }
    if (emotion->frustration > 0.7F) {
        tagger->stats.high_frustration_count++;
    }

    tagger->stats.total_tags++;
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_emotional_tagger_t* nimcp_emotional_tagger_create(void) {
    /* Phase 8: Heartbeat at operation start */
    ft_emotional_tagging_heartbeat("emotional_ta_emotional_tagger_cre", 0.0f);


    LOG_DEBUG("Creating module");
    /* WHAT: Allocate tagger instance
     * WHY:  Need storage for statistics and mutex
     * HOW:  nimcp_calloc for zero-initialized memory */
    nimcp_emotional_tagger_t* tagger = nimcp_calloc(1, sizeof(nimcp_emotional_tagger_t));
    if (!tagger) {
        LOG_ERROR("Failed to allocate emotional tagger");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate tagger");

        return NULL;
    }

    /* WHAT: Initialize mutex for thread safety
     * WHY:  Statistics may be updated from multiple threads
     * HOW:  nimcp_mutex_init with default attributes */
    if (nimcp_mutex_init(&tagger->mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize tagger mutex");
        nimcp_free(tagger);
        tagger = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_emotional_tagger_create: validation failed");
        return NULL;
    }

    LOG_INFO("Emotional tagger created");
    
    // Bio-async registration
    tagger->bio_ctx = NULL;
    tagger->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_EMOTIONAL_TAGGING,
            .module_name = "emotional_tagging",
            .inbox_capacity = 32,
            .user_data = tagger
        };
        tagger->bio_ctx = bio_router_register_module(&bio_info);
        if (tagger->bio_ctx) {
            tagger->bio_async_enabled = true;
        }
    }

return tagger;
}

void nimcp_emotional_tagger_destroy(nimcp_emotional_tagger_t* tagger) {
    /* Phase 8: Heartbeat at operation start */
    ft_emotional_tagging_heartbeat("emotional_ta_emotional_tagger_des", 0.0f);


    LOG_DEBUG("Destroying module");
    /* Guard: NULL check */
    if (!tagger) {
        return;
    }

    /* WHAT: Destroy mutex
     * WHY:  Clean up resources
     * HOW:  nimcp_mutex_destroy */
    nimcp_mutex_destroy(&tagger->mutex);

    /* WHAT: Free tagger memory */
    // Unregister from bio-router
    if (tagger->bio_async_enabled && tagger->bio_ctx) {
        bio_router_unregister_module(tagger->bio_ctx);
        tagger->bio_ctx = NULL;
        tagger->bio_async_enabled = false;
    }

    nimcp_free(tagger);
    tagger = NULL;

    LOG_INFO("Emotional tagger destroyed");
}

bool nimcp_emotional_tagger_reset(nimcp_emotional_tagger_t* tagger) {
    /* Guard: NULL check */
    if (!tagger) {
        LOG_ERROR("Cannot reset NULL tagger");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_emotional_tagger_reset: tagger is NULL");
        return false;
    }

    /* WHAT: Clear all statistics
     * WHY:  Start fresh tracking
     * HOW:  Zero out stats structure (thread-safe) */
    /* Phase 8: Heartbeat at operation start */
    ft_emotional_tagging_heartbeat("emotional_ta_emotional_tagger_res", 0.0f);


    nimcp_mutex_lock(&tagger->mutex);
    memset(&tagger->stats, 0, sizeof(nimcp_emotional_tagger_stats_t));
    nimcp_mutex_unlock(&tagger->mutex);

    LOG_INFO("Emotional tagger statistics reset");
    return true;
}

bool nimcp_emotional_tagger_compute_tag(
    nimcp_emotional_tagger_t* tagger,
    const nimcp_recovery_episode_t* episode,
    nimcp_emotional_tag_t* output
) {
    /* =========================================================================
     * GUARD: Validate all parameters
     * ========================================================================= */

    if (!tagger) {
        LOG_ERROR("NULL tagger in compute_tag");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_emotional_tagger_compute_tag: tagger is NULL");
        return false;
    }

    if (!episode) {
        LOG_ERROR("NULL episode in compute_tag");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_emotional_tagger_compute_tag: episode is NULL");
        return false;
    }

    if (!output) {
        LOG_ERROR("NULL output in compute_tag");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_emotional_tagger_compute_tag: output is NULL");
        return false;
    }

    /* =========================================================================
     * COMPUTATION: Calculate emotional dimensions
     * ========================================================================= */

    /* WHAT: Compute core emotional dimensions
     * WHY:  Valence and arousal form the base of emotional space
     * HOW:  Call helper functions that analyze episode context */
    /* Phase 8: Heartbeat at operation start */
    ft_emotional_tagging_heartbeat("emotional_ta_emotional_tagger_com", 0.0f);


    output->valence = compute_valence(episode);
    output->arousal = compute_arousal(episode);

    /* WHAT: Compute specific emotions
     * WHY:  Discrete emotions provide richer information than dimensions alone
     * HOW:  Fear from threat, relief from success, frustration from retries */
    output->fear = compute_fear(episode);
    output->relief = compute_relief(episode);
    output->frustration = compute_frustration(episode);

    /* =========================================================================
     * STATISTICS: Update running statistics
     * ========================================================================= */

    nimcp_mutex_lock(&tagger->mutex);
    update_statistics(tagger, output);
    nimcp_mutex_unlock(&tagger->mutex);

    /* WHAT: Log high-intensity emotions for debugging
     * WHY:  Track emotionally salient events */
    if (output->arousal > 0.8F || fabsf(output->valence) > 0.8F) {
        LOG_INFO("High-intensity emotion: valence=%.2f, arousal=%.2f, "
                 "fear=%.2f, relief=%.2f, frustration=%.2f",
                 output->valence, output->arousal, output->fear,
                 output->relief, output->frustration);
    }

    return true;
}

float nimcp_emotional_memory_boost(const nimcp_emotional_tag_t* emotion) {
    /* Guard: NULL check */
    if (!emotion) {
        return BASE_MEMORY_BOOST;
    }

    /* WHAT: Compute memory boost from emotional intensity
     * WHY:  Emotionally arousing events form stronger memories (biological fact)
     * HOW:  Combine arousal and valence magnitude */

    /* WHAT: Base boost from arousal
     * WHY:  Arousal is primary driver of memory consolidation
     * HOW:  Linear mapping from arousal to boost range */
    /* Phase 8: Heartbeat at operation start */
    ft_emotional_tagging_heartbeat("emotional_ta_emotional_memory_boo", 0.0f);


    float boost = BASE_MEMORY_BOOST;

    if (emotion->arousal > 0.8F) {
        /* Very high arousal → maximum boost */
        boost = HIGH_AROUSAL_BOOST;

        /* WHAT: Extreme valence amplifies boost
         * WHY:  Very good or very bad events most memorable */
        if (fabsf(emotion->valence) > 0.8F) {
            boost += 0.3F;
        }
    } else if (emotion->arousal > 0.5F) {
        /* Moderate arousal → moderate boost */
        boost = MODERATE_AROUSAL_BOOST;

        /* Significant valence adds to boost */
        if (fabsf(emotion->valence) > 0.6F) {
            boost += 0.2F;
        }
    } else {
        /* Low arousal → minimal boost */
        boost = BASE_MEMORY_BOOST + emotion->arousal * 0.3F;
    }

    /* WHAT: Specific emotions can boost memory
     * WHY:  Fear and relief are evolutionary important
     * HOW:  Add small boost for significant fear or relief */
    if (emotion->fear > 0.7F) {
        boost += 0.2F; /* Fear enhances memory for threats */
    }
    if (emotion->relief > 0.75F) {
        boost += 0.15F; /* Relief reinforces successful strategies */
    }

    /* WHAT: Clamp to valid range
     * WHY:  Prevent unrealistic memory boosts */
    return CLAMP(boost, BASE_MEMORY_BOOST, MAX_MEMORY_BOOST);
}

float nimcp_emotional_priority(const nimcp_emotional_tag_t* emotion) {
    /* Guard: NULL check */
    if (!emotion) {
        return 0.0F;
    }

    /* WHAT: Compute priority from emotional salience
     * WHY:  High-emotion events need prioritized processing
     * HOW:  Base priority from arousal, boost from fear */

    /* WHAT: Base priority is arousal level
     * WHY:  Arousal gates attention and working memory access
     * HOW:  Direct mapping */
    /* Phase 8: Heartbeat at operation start */
    ft_emotional_tagging_heartbeat("emotional_ta_emotional_priority", 0.0f);


    float priority = emotion->arousal;

    /* WHAT: Fear increases priority
     * WHY:  Threat detection must be prioritized for survival
     * HOW:  Add weighted fear component */
    priority += emotion->fear * FEAR_PRIORITY_WEIGHT;

    /* WHAT: Clamp to valid range */
    return CLAMP_01(priority);
}

bool nimcp_emotional_tagger_get_stats(
    const nimcp_emotional_tagger_t* tagger,
    nimcp_emotional_tagger_stats_t* stats
) {
    /* Guard: NULL checks */
    if (!tagger) {
        LOG_ERROR("NULL tagger in get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_emotional_tagger_get_stats: tagger is NULL");
        return false;
    }

    if (!stats) {
        LOG_ERROR("NULL stats output in get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_emotional_tagger_get_stats: stats is NULL");
        return false;
    }

    /* WHAT: Thread-safe copy of statistics
     * WHY:  Statistics may be updated concurrently
     * HOW:  Mutex-protected memcpy */
    /* Phase 8: Heartbeat at operation start */
    ft_emotional_tagging_heartbeat("emotional_ta_emotional_tagger_get", 0.0f);


    nimcp_mutex_lock((nimcp_mutex_t*)&tagger->mutex);
    memcpy(stats, &tagger->stats, sizeof(nimcp_emotional_tagger_stats_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)&tagger->mutex);

    return true;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotional_tagging_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    ft_emotional_tagging_heartbeat("emotional_ta_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotional_Tagging");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                ft_emotional_tagging_heartbeat("emotional_ta_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("[KG-Self] %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotional_Tagging");
    if (connections) {
        for (uint32_t i = 0; i < connections->count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && connections->count > 256) {
                ft_emotional_tagging_heartbeat("emotional_ta_loop",
                                 (float)(i + 1) / (float)connections->count);
            }

            LOG_DEBUG("[KG-Rel] -> %s (%s)",
                      connections->relations[i]->to,
                      connections->relations[i]->relation_type);
        }
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotional_Tagging");
    if (incoming) {
        for (uint32_t i = 0; i < incoming->count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && incoming->count > 256) {
                ft_emotional_tagging_heartbeat("emotional_ta_loop",
                                 (float)(i + 1) / (float)incoming->count);
            }

            LOG_DEBUG("[KG-Rel] <- %s (%s)",
                      incoming->relations[i]->from,
                      incoming->relations[i]->relation_type);
        }
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void ft_emotional_tagging_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        (void)instance;  /* No struct-level health_agent; use global */
        g_ft_emotional_tagging_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Functions (FULL implementation)
 * ============================================================================ */
int emotional_tagging_training_begin(nimcp_emotional_tagger_t* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_training_begin: NULL argument");
        return -1;
    }
    struct nimcp_emotional_tagger* ctx = (struct nimcp_emotional_tagger*)instance;
    ft_emotional_tagging_heartbeat_instance(NULL, "emotional_ta_training_begin", 0.0f);
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    NIMCP_LOGGING_INFO("%s training begin: counters reset", "emotional_tagging");
    return 0;
}

int emotional_tagging_training_step(nimcp_emotional_tagger_t* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_training_step: NULL argument");
        return -1;
    }
    struct nimcp_emotional_tagger* ctx = (struct nimcp_emotional_tagger*)instance;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ft_emotional_tagging_heartbeat_instance(NULL, "emotional_ta_training_step", progress);
    /* Increment tagging stats during training step */
    ctx->stats.total_tags++;
    return 0;
}

int emotional_tagging_training_end(nimcp_emotional_tagger_t* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_training_end: NULL argument");
        return -1;
    }
    struct nimcp_emotional_tagger* ctx = (struct nimcp_emotional_tagger*)instance;
    ft_emotional_tagging_heartbeat_instance(NULL, "emotional_ta_training_end", 1.0f);
    NIMCP_LOGGING_INFO("%s training end: total_tags=%lu", "emotional_tagging",
                       (unsigned long)ctx->stats.total_tags);
    return 0;
}
