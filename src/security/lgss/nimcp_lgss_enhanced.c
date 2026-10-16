/**
 * @file nimcp_lgss_enhanced.c
 * @brief LGSS Enhanced — Implementation of 8 governance improvements
 *
 * WHAT: Implements monotonic tightening, contextual rules, composition,
 *       escalation, cross-layer verification, anomaly-based rule generation,
 *       formal verification hooks, and multi-stakeholder governance.
 * WHY:  Production deployments need graduated response, context-aware safety,
 *       and auditability beyond the base LGSS rule engine.
 * HOW:  Wraps the base lgss_context_t with an enhanced context.
 *
 * @author NIMCP Development Team
 * @date 2026-03-21
 * @version 1.0.0
 */

#include "security/lgss/nimcp_lgss_enhanced.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

/*=============================================================================
 * LOGGING
 *============================================================================*/

#define LOG_MODULE "LGSS_ENHANCED"

#define ENH_LOG_DEBUG(fmt, ...) \
    NIMCP_LOG_DEBUG(LOG_MODULE, fmt, ##__VA_ARGS__)
#define ENH_LOG_INFO(fmt, ...) \
    NIMCP_LOG_INFO(LOG_MODULE, fmt, ##__VA_ARGS__)
#define ENH_LOG_WARN(fmt, ...) \
    NIMCP_LOG_WARN(LOG_MODULE, fmt, ##__VA_ARGS__)
#define ENH_LOG_ERROR(fmt, ...) \
    NIMCP_LOG_ERROR(LOG_MODULE, fmt, ##__VA_ARGS__)

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

#define LGSS_ENH_MAGIC              0x4C474548  /* 'LGEH' */
#define DEFAULT_MAX_MONOTONIC       64
#define DEFAULT_MAX_POLICIES        128
#define DEFAULT_MAX_ESCALATIONS     256
#define DEFAULT_MAX_PROPOSALS       32
#define DEFAULT_MAX_STAKEHOLDERS    16

#define ESCALATION_WARN_DEFAULT     1
#define ESCALATION_CLAMP_DEFAULT    3
#define ESCALATION_BLOCK_DEFAULT    5
#define ESCALATION_ESTOP_DEFAULT    10
#define ESCALATION_DECAY_DEFAULT    0.99f

#define ETHICS_RUBBER_STAMP_RATE    0.999f
#define ETHICS_MIN_EVAL_TIME_US     1.0f

#define ANOMALY_WINDOW_SIZE         16
#define VERIFY_GRID_STEPS           100

/*=============================================================================
 * INTERNAL STRUCTURE
 *============================================================================*/

struct nimcp_lgss_enhanced {
    uint32_t magic;
    void* base_lgss;

    /* 1. Monotonic rules */
    nimcp_monotonic_rule_t* monotonic_rules;
    uint32_t num_monotonic;
    uint32_t max_monotonic;

    /* 2. Deployment context */
    nimcp_deployment_context_t context;
    nimcp_context_profile_t profile;

    /* 3. Composed policies */
    nimcp_composed_policy_t* policies;
    uint32_t num_policies;
    uint32_t max_policies;

    /* 4. Escalation state */
    nimcp_escalation_state_t* escalations;
    uint32_t num_escalations;
    uint32_t max_escalations;

    /* 5. Cross-layer verification */
    nimcp_cross_layer_status_t ethics_status;

    /* 6. Proposed rules */
    nimcp_proposed_rule_t* proposals;
    uint32_t num_proposals;
    uint32_t max_proposals;

    /* 8. Stakeholders */
    nimcp_stakeholder_t* stakeholders;
    uint32_t num_stakeholders;
    uint32_t max_stakeholders;

    nimcp_mutex_t* lock;
};

/*=============================================================================
 * GUARD MACROS
 *============================================================================*/

#define ENH_VALIDATE(enh) \
    do { \
        if (!(enh) || (enh)->magic != LGSS_ENH_MAGIC) { \
            ENH_LOG_ERROR("Invalid enhanced LGSS context"); \
            return -1; \
        } \
    } while (0)

#define ENH_VALIDATE_BOOL(enh) \
    do { \
        if (!(enh) || (enh)->magic != LGSS_ENH_MAGIC) { \
            ENH_LOG_ERROR("Invalid enhanced LGSS context"); \
            return false; \
        } \
    } while (0)

#define ENH_VALIDATE_NULL(enh) \
    do { \
        if (!(enh) || (enh)->magic != LGSS_ENH_MAGIC) { \
            ENH_LOG_ERROR("Invalid enhanced LGSS context"); \
            return NULL; \
        } \
    } while (0)

#define ENH_VALIDATE_UINT(enh) \
    do { \
        if (!(enh) || (enh)->magic != LGSS_ENH_MAGIC) { \
            ENH_LOG_ERROR("Invalid enhanced LGSS context"); \
            return 0; \
        } \
    } while (0)

/*=============================================================================
 * HELPER: find enhanced from void* lgss
 *============================================================================*/

/* Global registry for enhanced wrappers (simple linear scan, max 16) */
#define MAX_ENHANCED_REGISTRY 16

static struct {
    void* base;
    nimcp_lgss_enhanced_t* enhanced;
} s_registry[MAX_ENHANCED_REGISTRY];
static uint32_t s_registry_count = 0;
static nimcp_mutex_t* s_registry_lock = NULL;

static void registry_init_once(void) {
    if (!s_registry_lock) {
        s_registry_lock = nimcp_mutex_create(NULL);
    }
}

static nimcp_lgss_enhanced_t* registry_find(const void* base) {
    registry_init_once();
    nimcp_mutex_lock(s_registry_lock);
    for (uint32_t i = 0; i < s_registry_count; i++) {
        if (s_registry[i].base == base) {
            nimcp_lgss_enhanced_t* enh = s_registry[i].enhanced;
            nimcp_mutex_unlock(s_registry_lock);
            return enh;
        }
    }
    nimcp_mutex_unlock(s_registry_lock);
    return NULL;
}

static void registry_add(void* base, nimcp_lgss_enhanced_t* enh) {
    registry_init_once();
    nimcp_mutex_lock(s_registry_lock);
    if (s_registry_count < MAX_ENHANCED_REGISTRY) {
        s_registry[s_registry_count].base = base;
        s_registry[s_registry_count].enhanced = enh;
        s_registry_count++;
    } else {
        ENH_LOG_WARN("Enhanced LGSS registry full, cannot register");
    }
    nimcp_mutex_unlock(s_registry_lock);
}

static void registry_remove(void* base) {
    registry_init_once();
    nimcp_mutex_lock(s_registry_lock);
    for (uint32_t i = 0; i < s_registry_count; i++) {
        if (s_registry[i].base == base) {
            s_registry[i] = s_registry[s_registry_count - 1];
            s_registry_count--;
            break;
        }
    }
    nimcp_mutex_unlock(s_registry_lock);
}

/*=============================================================================
 * LIFECYCLE
 *============================================================================*/

nimcp_lgss_enhanced_t* nimcp_lgss_enhanced_create(void* base_lgss) {
    if (!base_lgss) {
        ENH_LOG_ERROR("Cannot create enhanced LGSS with NULL base");
        return NULL;
    }

    nimcp_lgss_enhanced_t* enh = (nimcp_lgss_enhanced_t*)nimcp_calloc(
        1, sizeof(nimcp_lgss_enhanced_t));
    if (!enh) {
        ENH_LOG_ERROR("Failed to allocate enhanced LGSS context");
        return NULL;
    }

    enh->magic = LGSS_ENH_MAGIC;
    enh->base_lgss = base_lgss;

    /* 1. Monotonic rules */
    enh->max_monotonic = DEFAULT_MAX_MONOTONIC;
    enh->monotonic_rules = (nimcp_monotonic_rule_t*)nimcp_calloc(
        enh->max_monotonic, sizeof(nimcp_monotonic_rule_t));
    if (!enh->monotonic_rules) {
        ENH_LOG_ERROR("Failed to allocate monotonic rules");
        nimcp_free(enh);
        return NULL;
    }

    /* 2. Context defaults to RESEARCH */
    enh->context = NIMCP_CONTEXT_RESEARCH;
    enh->profile = nimcp_context_profile_default(NIMCP_CONTEXT_RESEARCH);

    /* 3. Policies */
    enh->max_policies = DEFAULT_MAX_POLICIES;
    enh->policies = (nimcp_composed_policy_t*)nimcp_calloc(
        enh->max_policies, sizeof(nimcp_composed_policy_t));
    if (!enh->policies) {
        ENH_LOG_ERROR("Failed to allocate policies");
        nimcp_free(enh->monotonic_rules);
        nimcp_free(enh);
        return NULL;
    }

    /* 4. Escalations */
    enh->max_escalations = DEFAULT_MAX_ESCALATIONS;
    enh->escalations = (nimcp_escalation_state_t*)nimcp_calloc(
        enh->max_escalations, sizeof(nimcp_escalation_state_t));
    if (!enh->escalations) {
        ENH_LOG_ERROR("Failed to allocate escalations");
        nimcp_free(enh->policies);
        nimcp_free(enh->monotonic_rules);
        nimcp_free(enh);
        return NULL;
    }

    /* 6. Proposals */
    enh->max_proposals = DEFAULT_MAX_PROPOSALS;
    enh->proposals = (nimcp_proposed_rule_t*)nimcp_calloc(
        enh->max_proposals, sizeof(nimcp_proposed_rule_t));
    if (!enh->proposals) {
        ENH_LOG_ERROR("Failed to allocate proposals");
        nimcp_free(enh->escalations);
        nimcp_free(enh->policies);
        nimcp_free(enh->monotonic_rules);
        nimcp_free(enh);
        return NULL;
    }

    /* 8. Stakeholders */
    enh->max_stakeholders = DEFAULT_MAX_STAKEHOLDERS;
    enh->stakeholders = (nimcp_stakeholder_t*)nimcp_calloc(
        enh->max_stakeholders, sizeof(nimcp_stakeholder_t));
    if (!enh->stakeholders) {
        ENH_LOG_ERROR("Failed to allocate stakeholders");
        nimcp_free(enh->proposals);
        nimcp_free(enh->escalations);
        nimcp_free(enh->policies);
        nimcp_free(enh->monotonic_rules);
        nimcp_free(enh);
        return NULL;
    }

    /* Mutex */
    enh->lock = nimcp_mutex_create(NULL);
    if (!enh->lock) {
        ENH_LOG_ERROR("Failed to create enhanced LGSS mutex");
        nimcp_free(enh->stakeholders);
        nimcp_free(enh->proposals);
        nimcp_free(enh->escalations);
        nimcp_free(enh->policies);
        nimcp_free(enh->monotonic_rules);
        nimcp_free(enh);
        return NULL;
    }

    registry_add(base_lgss, enh);

    ENH_LOG_INFO("Enhanced LGSS created (context=RESEARCH, max_rules=%u, max_policies=%u)",
        enh->max_monotonic, enh->max_policies);
    return enh;
}

void nimcp_lgss_enhanced_destroy(nimcp_lgss_enhanced_t* enh) {
    if (!enh || enh->magic != LGSS_ENH_MAGIC) {
        return;
    }

    registry_remove(enh->base_lgss);

    enh->magic = 0;

    if (enh->lock) {
        nimcp_mutex_free(enh->lock);
    }
    nimcp_free(enh->stakeholders);
    nimcp_free(enh->proposals);
    nimcp_free(enh->escalations);
    nimcp_free(enh->policies);
    nimcp_free(enh->monotonic_rules);
    nimcp_free(enh);

    ENH_LOG_INFO("Enhanced LGSS destroyed");
}

nimcp_lgss_enhanced_t* nimcp_lgss_get_enhanced(void* lgss) {
    if (!lgss) {
        return NULL;
    }
    return registry_find(lgss);
}

/*=============================================================================
 * 1. MONOTONIC SAFETY TIGHTENING
 *============================================================================*/

/**
 * Find a monotonic rule by ID.  Returns index or -1.
 */
static int find_monotonic_rule(const nimcp_lgss_enhanced_t* enh, uint32_t rule_id) {
    for (uint32_t i = 0; i < enh->num_monotonic; i++) {
        if (enh->monotonic_rules[i].rule_id == rule_id) {
            return (int)i;
        }
    }
    return -1;
}

int nimcp_lgss_propose_tightening(void* lgss, uint32_t rule_id,
    float proposed_value, const char* reason)
{
    nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    nimcp_mutex_lock(enh->lock);

    int idx = find_monotonic_rule(enh, rule_id);

    if (idx < 0) {
        /* New rule — register it */
        if (enh->num_monotonic >= enh->max_monotonic) {
            ENH_LOG_ERROR("Monotonic rules array full (%u)", enh->max_monotonic);
            nimcp_mutex_unlock(enh->lock);
            return -1;
        }
        idx = (int)enh->num_monotonic;
        nimcp_monotonic_rule_t* rule = &enh->monotonic_rules[idx];
        rule->rule_id = rule_id;
        rule->original_value = proposed_value;
        rule->current_value = proposed_value;
        rule->tightest_value = proposed_value;
        rule->locked = true;
        rule->last_tightened_ts = nimcp_time_get_us();
        if (reason) {
            snprintf(rule->reason, sizeof(rule->reason), "%s", reason);
        }
        snprintf(rule->name, sizeof(rule->name), "rule_%u", rule_id);
        enh->num_monotonic++;
        ENH_LOG_INFO("Registered monotonic rule %u (initial=%.4f)", rule_id, proposed_value);
        nimcp_mutex_unlock(enh->lock);
        return 0;
    }

    nimcp_monotonic_rule_t* rule = &enh->monotonic_rules[idx];

    /* Monotonic tightening: proposed must be strictly tighter (lower for max thresholds).
     * Convention: lower value = tighter constraint (max limits). */
    if (proposed_value >= rule->current_value) {
        ENH_LOG_WARN("Tightening rejected for rule %u: proposed %.4f >= current %.4f (would relax)",
            rule_id, proposed_value, rule->current_value);
        nimcp_mutex_unlock(enh->lock);
        return -1;
    }

    /* Cannot go above original */
    if (proposed_value > rule->original_value) {
        ENH_LOG_WARN("Tightening rejected for rule %u: proposed %.4f > original %.4f",
            rule_id, proposed_value, rule->original_value);
        nimcp_mutex_unlock(enh->lock);
        return -1;
    }

    rule->current_value = proposed_value;
    if (proposed_value < rule->tightest_value) {
        rule->tightest_value = proposed_value;
    }
    rule->last_tightened_ts = nimcp_time_get_us();
    if (reason) {
        snprintf(rule->reason, sizeof(rule->reason), "%s", reason);
    }

    ENH_LOG_INFO("Monotonic rule %u tightened: %.4f -> %.4f (%s)",
        rule_id, rule->current_value, proposed_value,
        reason ? reason : "no reason");

    nimcp_mutex_unlock(enh->lock);
    return 0;
}

bool nimcp_lgss_would_accept_tightening(const void* lgss, uint32_t rule_id,
    float proposed_value)
{
    const nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        return false;
    }
    ENH_VALIDATE_BOOL(enh);

    int idx = find_monotonic_rule(enh, rule_id);
    if (idx < 0) {
        /* New rule would be accepted */
        return true;
    }

    const nimcp_monotonic_rule_t* rule = &enh->monotonic_rules[idx];
    return (proposed_value < rule->current_value && proposed_value <= rule->original_value);
}

/*=============================================================================
 * 2. CONTEXTUAL RULE ACTIVATION
 *============================================================================*/

nimcp_context_profile_t nimcp_context_profile_default(nimcp_deployment_context_t ctx) {
    nimcp_context_profile_t p;
    memset(&p, 0, sizeof(p));
    p.context = ctx;

    switch (ctx) {
        case NIMCP_CONTEXT_RESEARCH:
            p.max_velocity = 5.0f;
            p.max_force = 50.0f;
            p.max_altitude = 200.0f;
            p.geofence_radius = 500.0f;
            p.allow_autonomous_motion = true;
            p.allow_communication = true;
            p.allow_swarm = true;
            p.require_human_in_loop = false;
            p.min_confidence_threshold = 0.5f;
            break;

        case NIMCP_CONTEXT_INDOOR:
            p.max_velocity = 1.0f;
            p.max_force = 10.0f;
            p.max_altitude = 3.0f;
            p.geofence_radius = 50.0f;
            p.allow_autonomous_motion = false;
            p.allow_communication = true;
            p.allow_swarm = false;
            p.require_human_in_loop = true;
            p.min_confidence_threshold = 0.7f;
            break;

        case NIMCP_CONTEXT_OUTDOOR:
            p.max_velocity = 3.0f;
            p.max_force = 25.0f;
            p.max_altitude = 50.0f;
            p.geofence_radius = 100.0f;
            p.allow_autonomous_motion = true;
            p.allow_communication = true;
            p.allow_swarm = true;
            p.require_human_in_loop = false;
            p.min_confidence_threshold = 0.6f;
            break;

        case NIMCP_CONTEXT_MEDICAL:
            p.max_velocity = 0.5f;
            p.max_force = 5.0f;
            p.max_altitude = 0.0f;
            p.geofence_radius = 10.0f;
            p.allow_autonomous_motion = false;
            p.allow_communication = true;
            p.allow_swarm = false;
            p.require_human_in_loop = true;
            p.min_confidence_threshold = 0.95f;
            break;

        case NIMCP_CONTEXT_INDUSTRIAL:
            p.max_velocity = 2.0f;
            p.max_force = 100.0f;
            p.max_altitude = 20.0f;
            p.geofence_radius = 50.0f;
            p.allow_autonomous_motion = true;
            p.allow_communication = true;
            p.allow_swarm = true;
            p.require_human_in_loop = false;
            p.min_confidence_threshold = 0.8f;
            break;

        case NIMCP_CONTEXT_PUBLIC_SPACE:
            p.max_velocity = 0.5f;
            p.max_force = 2.0f;
            p.max_altitude = 5.0f;
            p.geofence_radius = 20.0f;
            p.allow_autonomous_motion = false;
            p.allow_communication = true;
            p.allow_swarm = false;
            p.require_human_in_loop = true;
            p.min_confidence_threshold = 0.95f;
            break;

        case NIMCP_CONTEXT_MILITARY_PROHIBITED:
            /* All zeros, everything blocked */
            p.max_velocity = 0.0f;
            p.max_force = 0.0f;
            p.max_altitude = 0.0f;
            p.geofence_radius = 0.0f;
            p.allow_autonomous_motion = false;
            p.allow_communication = false;
            p.allow_swarm = false;
            p.require_human_in_loop = true;
            p.min_confidence_threshold = 1.0f;
            break;

        default:
            /* Fail-safe: most restrictive */
            p.max_velocity = 0.0f;
            p.max_force = 0.0f;
            p.require_human_in_loop = true;
            p.min_confidence_threshold = 1.0f;
            break;
    }

    return p;
}

nimcp_context_profile_t nimcp_lgss_get_context_profile(nimcp_deployment_context_t context) {
    return nimcp_context_profile_default(context);
}

int nimcp_lgss_set_deployment_context(void* lgss, nimcp_deployment_context_t context) {
    nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    nimcp_mutex_lock(enh->lock);

    /* One-way lock: cannot leave MILITARY_PROHIBITED */
    if (enh->context == NIMCP_CONTEXT_MILITARY_PROHIBITED &&
        context != NIMCP_CONTEXT_MILITARY_PROHIBITED) {
        ENH_LOG_ERROR("Cannot change from MILITARY_PROHIBITED context (one-way lock)");
        nimcp_mutex_unlock(enh->lock);
        return -1;
    }

    enh->context = context;
    enh->profile = nimcp_context_profile_default(context);

    ENH_LOG_INFO("Deployment context set to %d", (int)context);
    nimcp_mutex_unlock(enh->lock);
    return 0;
}

/*=============================================================================
 * 3. RULE COMPOSITION
 *============================================================================*/

int nimcp_lgss_add_policy(void* lgss, const nimcp_composed_policy_t* policy) {
    if (!policy) {
        ENH_LOG_ERROR("NULL policy");
        return -1;
    }

    nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    nimcp_mutex_lock(enh->lock);

    if (enh->num_policies >= enh->max_policies) {
        ENH_LOG_ERROR("Policy array full (%u)", enh->max_policies);
        nimcp_mutex_unlock(enh->lock);
        return -1;
    }

    /* Deep copy the policy */
    nimcp_composed_policy_t* dst = &enh->policies[enh->num_policies];
    *dst = *policy;

    /* Deep copy conditions array */
    if (policy->num_conditions > 0 && policy->conditions) {
        dst->conditions = (nimcp_rule_condition_t*)nimcp_calloc(
            policy->num_conditions, sizeof(nimcp_rule_condition_t));
        if (!dst->conditions) {
            ENH_LOG_ERROR("Failed to allocate conditions for policy %u", policy->policy_id);
            nimcp_mutex_unlock(enh->lock);
            return -1;
        }
        memcpy(dst->conditions, policy->conditions,
            policy->num_conditions * sizeof(nimcp_rule_condition_t));
    } else {
        dst->conditions = NULL;
        dst->num_conditions = 0;
    }

    enh->num_policies++;
    ENH_LOG_INFO("Added policy '%s' (id=%u, conditions=%u, severity=%u)",
        policy->name, policy->policy_id, policy->num_conditions, policy->severity);

    nimcp_mutex_unlock(enh->lock);
    return 0;
}

/**
 * Evaluate a single condition against the state vector.
 * field_name is matched to a state index via simple hashing (field_name[0] % state_dim).
 */
static bool evaluate_condition(const nimcp_rule_condition_t* cond,
    const float* state, uint32_t state_dim)
{
    if (!state || state_dim == 0) {
        return false;
    }

    /* Map field name to state index deterministically */
    uint32_t hash = 0;
    for (const char* c = cond->field_name; *c; c++) {
        hash = hash * 31 + (uint32_t)*c;
    }
    uint32_t idx = hash % state_dim;
    float value = state[idx];

    switch (cond->op) {
        case NIMCP_RULE_OP_THRESHOLD:
            return value > cond->threshold;

        case NIMCP_RULE_OP_RANGE:
            return (value >= cond->range_min && value <= cond->range_max);

        case NIMCP_RULE_OP_NOT:
            return value <= cond->threshold;

        default:
            return false;
    }
}

int nimcp_lgss_evaluate_policies(void* lgss, const float* state, uint32_t state_dim,
    uint32_t* violations, uint32_t max_violations)
{
    if (!state || !violations) {
        return -1;
    }

    nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    nimcp_mutex_lock(enh->lock);

    uint32_t violation_count = 0;

    for (uint32_t p = 0; p < enh->num_policies && violation_count < max_violations; p++) {
        const nimcp_composed_policy_t* pol = &enh->policies[p];
        if (!pol->conditions || pol->num_conditions == 0) {
            continue;
        }

        bool violated = false;

        if (pol->combiner == NIMCP_RULE_OP_AND) {
            /* AND: all conditions must be true for the policy to hold;
             * violation means at least one condition fails */
            violated = false;
            for (uint32_t c = 0; c < pol->num_conditions; c++) {
                if (!evaluate_condition(&pol->conditions[c], state, state_dim)) {
                    violated = true;
                    break;
                }
            }
        } else if (pol->combiner == NIMCP_RULE_OP_OR) {
            /* OR: at least one condition must be true;
             * violation means ALL conditions fail */
            violated = true;
            for (uint32_t c = 0; c < pol->num_conditions; c++) {
                if (evaluate_condition(&pol->conditions[c], state, state_dim)) {
                    violated = false;
                    break;
                }
            }
        }

        if (violated) {
            violations[violation_count++] = pol->policy_id;
            ENH_LOG_WARN("Policy '%s' (id=%u, severity=%u) violated",
                pol->name, pol->policy_id, pol->severity);
        }
    }

    nimcp_mutex_unlock(enh->lock);
    return (int)violation_count;
}

/*=============================================================================
 * 4. VIOLATION ESCALATION
 *============================================================================*/

static int find_or_create_escalation(nimcp_lgss_enhanced_t* enh, uint32_t rule_id) {
    /* Find existing */
    for (uint32_t i = 0; i < enh->num_escalations; i++) {
        if (enh->escalations[i].rule_id == rule_id) {
            return (int)i;
        }
    }

    /* Create new */
    if (enh->num_escalations >= enh->max_escalations) {
        return -1;
    }

    int idx = (int)enh->num_escalations;
    nimcp_escalation_state_t* esc = &enh->escalations[idx];
    memset(esc, 0, sizeof(*esc));
    esc->rule_id = rule_id;
    esc->warn_threshold = ESCALATION_WARN_DEFAULT;
    esc->clamp_threshold = ESCALATION_CLAMP_DEFAULT;
    esc->block_threshold = ESCALATION_BLOCK_DEFAULT;
    esc->estop_threshold = ESCALATION_ESTOP_DEFAULT;
    esc->decay_rate = ESCALATION_DECAY_DEFAULT;
    enh->num_escalations++;
    return idx;
}

int nimcp_lgss_record_violation(void* lgss, uint32_t rule_id) {
    nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    nimcp_mutex_lock(enh->lock);

    int idx = find_or_create_escalation(enh, rule_id);
    if (idx < 0) {
        ENH_LOG_ERROR("Escalation array full for rule %u", rule_id);
        nimcp_mutex_unlock(enh->lock);
        return -1;
    }

    nimcp_escalation_state_t* esc = &enh->escalations[idx];
    uint64_t now = nimcp_time_get_us();

    /* Apply time-based decay to violation count */
    if (esc->last_violation_ts > 0 && esc->violation_count > 0) {
        float elapsed_sec = (float)(now - esc->last_violation_ts) / 1000000.0f;
        float decay = powf(esc->decay_rate, elapsed_sec);
        esc->violation_count = (uint32_t)((float)esc->violation_count * decay);
    }

    /* Record this violation */
    esc->violation_count++;
    if (esc->first_violation_ts == 0) {
        esc->first_violation_ts = now;
    }
    esc->last_violation_ts = now;

    /* Determine escalation level */
    uint32_t old_level = esc->current_level;
    if (esc->violation_count >= esc->estop_threshold) {
        esc->current_level = 4; /* estop */
    } else if (esc->violation_count >= esc->block_threshold) {
        esc->current_level = 3; /* block */
    } else if (esc->violation_count >= esc->clamp_threshold) {
        esc->current_level = 2; /* clamp */
    } else if (esc->violation_count >= esc->warn_threshold) {
        esc->current_level = 1; /* warn */
    }
    /* Note: level can only go UP, never down via violations */
    if (esc->current_level < old_level) {
        esc->current_level = old_level;
    }

    if (esc->current_level != old_level) {
        ENH_LOG_WARN("Escalation level for rule %u: %u -> %u (violations=%u)",
            rule_id, old_level, esc->current_level, esc->violation_count);
    }

    nimcp_mutex_unlock(enh->lock);
    return (int)esc->current_level;
}

uint32_t nimcp_lgss_get_escalation_level(const void* lgss, uint32_t rule_id) {
    const nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        return 0;
    }
    ENH_VALIDATE_UINT(enh);

    for (uint32_t i = 0; i < enh->num_escalations; i++) {
        if (enh->escalations[i].rule_id == rule_id) {
            return enh->escalations[i].current_level;
        }
    }
    return 0;
}

int nimcp_lgss_reset_escalation(void* lgss, uint32_t rule_id) {
    nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    nimcp_mutex_lock(enh->lock);

    for (uint32_t i = 0; i < enh->num_escalations; i++) {
        if (enh->escalations[i].rule_id == rule_id) {
            ENH_LOG_INFO("Escalation reset for rule %u (was level %u, %u violations) -- HUMAN ACTION",
                rule_id, enh->escalations[i].current_level, enh->escalations[i].violation_count);
            enh->escalations[i].violation_count = 0;
            enh->escalations[i].current_level = 0;
            enh->escalations[i].first_violation_ts = 0;
            enh->escalations[i].last_violation_ts = 0;
            nimcp_mutex_unlock(enh->lock);
            return 0;
        }
    }

    ENH_LOG_WARN("No escalation found for rule %u to reset", rule_id);
    nimcp_mutex_unlock(enh->lock);
    return -1;
}

/*=============================================================================
 * 5. CROSS-LAYER VERIFICATION
 *============================================================================*/

int nimcp_lgss_verify_ethics(void* lgss, void* ethics_module,
    nimcp_cross_layer_status_t* status)
{
    if (!status) {
        return -1;
    }

    nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    memset(status, 0, sizeof(*status));

    if (!ethics_module) {
        status->ethics_suspicious = true;
        snprintf(status->alert_reason, sizeof(status->alert_reason),
            "CRITICAL: Ethics module is NULL -- not loaded or not configured");
        ENH_LOG_ERROR("%s", status->alert_reason);
        return -1;
    }

    nimcp_mutex_lock(enh->lock);

    /* Copy current status from internal tracking */
    *status = enh->ethics_status;

    /* Check for suspicious patterns */
    status->ethics_suspicious = false;

    if (status->ethics_eval_count == 0) {
        status->ethics_suspicious = true;
        snprintf(status->alert_reason, sizeof(status->alert_reason),
            "CRITICAL: Ethics module has zero evaluations -- not being called");
        ENH_LOG_ERROR("%s", status->alert_reason);
    } else {
        /* Compute pass rate */
        status->ethics_pass_rate = (float)status->ethics_pass_count /
            (float)status->ethics_eval_count;

        if (status->ethics_pass_rate > ETHICS_RUBBER_STAMP_RATE) {
            status->ethics_suspicious = true;
            snprintf(status->alert_reason, sizeof(status->alert_reason),
                "SUSPICIOUS: Ethics pass rate %.4f > %.4f -- possible rubber-stamping",
                status->ethics_pass_rate, ETHICS_RUBBER_STAMP_RATE);
            ENH_LOG_WARN("%s", status->alert_reason);
        }

        if (status->ethics_avg_eval_time_us < ETHICS_MIN_EVAL_TIME_US &&
            status->ethics_eval_count > 10) {
            status->ethics_suspicious = true;
            snprintf(status->alert_reason, sizeof(status->alert_reason),
                "SUSPICIOUS: Ethics avg eval time %.2f us < %.2f us -- not actually evaluating",
                status->ethics_avg_eval_time_us, ETHICS_MIN_EVAL_TIME_US);
            ENH_LOG_WARN("%s", status->alert_reason);
        }
    }

    nimcp_mutex_unlock(enh->lock);
    return 0;
}

/*=============================================================================
 * 6. ANOMALY-BASED RULE GENERATION
 *============================================================================*/

int nimcp_lgss_analyze_patterns(void* lgss, const float* audit_data,
    uint32_t num_entries, nimcp_proposed_rule_t* proposals, uint32_t max_proposals)
{
    if (!audit_data || !proposals || num_entries < 2 || max_proposals == 0) {
        return -1;
    }

    nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    uint32_t proposed = 0;

    /* Pattern 1: Monotonically increasing (acceleration without bound) */
    {
        uint32_t increasing_count = 0;
        for (uint32_t i = 1; i < num_entries; i++) {
            if (audit_data[i] > audit_data[i - 1]) {
                increasing_count++;
            }
        }
        float increasing_ratio = (float)increasing_count / (float)(num_entries - 1);
        if (increasing_ratio > 0.8f && proposed < max_proposals) {
            nimcp_proposed_rule_t* p = &proposals[proposed];
            memset(p, 0, sizeof(*p));
            snprintf(p->proposed_rule, sizeof(p->proposed_rule),
                "Add rate limit: max_delta_per_step < 0.5");
            snprintf(p->evidence, sizeof(p->evidence),
                "Monotonically increasing pattern: %.1f%% of steps increase (%u/%u)",
                increasing_ratio * 100.0f, increasing_count, num_entries - 1);
            p->confidence = increasing_ratio;
            p->observations = increasing_count;
            p->human_approved = false;
            proposed++;
        }
    }

    /* Pattern 2: Oscillating outputs (instability) */
    {
        uint32_t sign_changes = 0;
        for (uint32_t i = 2; i < num_entries; i++) {
            float d1 = audit_data[i - 1] - audit_data[i - 2];
            float d2 = audit_data[i] - audit_data[i - 1];
            if ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) {
                sign_changes++;
            }
        }
        float oscillation_ratio = (num_entries > 2) ?
            (float)sign_changes / (float)(num_entries - 2) : 0.0f;
        if (oscillation_ratio > 0.7f && proposed < max_proposals) {
            nimcp_proposed_rule_t* p = &proposals[proposed];
            memset(p, 0, sizeof(*p));
            snprintf(p->proposed_rule, sizeof(p->proposed_rule),
                "Add smoothing filter: exponential moving average (alpha=0.3)");
            snprintf(p->evidence, sizeof(p->evidence),
                "Oscillation detected: %.1f%% sign changes (%u/%u)",
                oscillation_ratio * 100.0f, sign_changes, num_entries - 2);
            p->confidence = oscillation_ratio;
            p->observations = sign_changes;
            p->human_approved = false;
            proposed++;
        }
    }

    /* Pattern 3: Repeated identical outputs (mode collapse) */
    {
        uint32_t identical_count = 0;
        for (uint32_t i = 1; i < num_entries; i++) {
            if (fabsf(audit_data[i] - audit_data[i - 1]) < 1e-6f) {
                identical_count++;
            }
        }
        float identical_ratio = (float)identical_count / (float)(num_entries - 1);
        if (identical_ratio > 0.5f && proposed < max_proposals) {
            nimcp_proposed_rule_t* p = &proposals[proposed];
            memset(p, 0, sizeof(*p));
            snprintf(p->proposed_rule, sizeof(p->proposed_rule),
                "Add diversity check: require output variance > 0.01");
            snprintf(p->evidence, sizeof(p->evidence),
                "Mode collapse: %.1f%% identical consecutive outputs (%u/%u)",
                identical_ratio * 100.0f, identical_count, num_entries - 1);
            p->confidence = identical_ratio;
            p->observations = identical_count;
            p->human_approved = false;
            proposed++;
        }
    }

    /* Pattern 4: Output magnitude trending upward (divergence) */
    {
        float first_quarter_avg = 0.0f;
        float last_quarter_avg = 0.0f;
        uint32_t quarter = num_entries / 4;
        if (quarter > 0) {
            for (uint32_t i = 0; i < quarter; i++) {
                first_quarter_avg += fabsf(audit_data[i]);
            }
            first_quarter_avg /= (float)quarter;

            for (uint32_t i = num_entries - quarter; i < num_entries; i++) {
                last_quarter_avg += fabsf(audit_data[i]);
            }
            last_quarter_avg /= (float)quarter;

            if (first_quarter_avg > 1e-6f) {
                float growth = last_quarter_avg / first_quarter_avg;
                if (growth > 2.0f && proposed < max_proposals) {
                    nimcp_proposed_rule_t* p = &proposals[proposed];
                    memset(p, 0, sizeof(*p));
                    snprintf(p->proposed_rule, sizeof(p->proposed_rule),
                        "Add magnitude bound: clamp output to [-%.1f, %.1f]",
                        first_quarter_avg * 2.0f, first_quarter_avg * 2.0f);
                    snprintf(p->evidence, sizeof(p->evidence),
                        "Divergence: magnitude grew %.1fx (%.4f -> %.4f)",
                        growth, first_quarter_avg, last_quarter_avg);
                    p->confidence = (growth > 10.0f) ? 1.0f : growth / 10.0f;
                    p->observations = num_entries;
                    p->human_approved = false;
                    proposed++;
                }
            }
        }
    }

    if (proposed > 0) {
        ENH_LOG_INFO("Anomaly analysis generated %u rule proposals from %u data points",
            proposed, num_entries);
    }

    return (int)proposed;
}

/*=============================================================================
 * 7. FORMAL VERIFICATION HOOKS
 *============================================================================*/

int nimcp_lgss_export_for_verification(const void* lgss,
    nimcp_verify_format_t format, char* output, uint32_t max_output)
{
    if (!output || max_output == 0) {
        return -1;
    }

    const nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    output[0] = '\0';
    uint32_t written = 0;
    int n;

    switch (format) {
        case NIMCP_VERIFY_FORMAT_SMT2:
            n = snprintf(output + written, max_output - written,
                "; LGSS Enhanced Rules - SMT-LIB v2\n"
                "(set-logic QF_LRA)\n\n");
            if (n > 0) { written += (uint32_t)n; }

            /* Export monotonic rules */
            for (uint32_t i = 0; i < enh->num_monotonic && written < max_output; i++) {
                const nimcp_monotonic_rule_t* r = &enh->monotonic_rules[i];
                n = snprintf(output + written, max_output - written,
                    "(declare-const %s Real)\n"
                    "(assert (<= %s %.6f)) ; rule_%u: %s\n\n",
                    r->name, r->name, (double)r->current_value,
                    r->rule_id, r->reason);
                if (n > 0) { written += (uint32_t)n; }
            }

            /* Export context constraints */
            n = snprintf(output + written, max_output - written,
                "; Context: %d\n"
                "(declare-const velocity Real)\n"
                "(declare-const force Real)\n"
                "(assert (<= velocity %.6f))\n"
                "(assert (<= force %.6f))\n\n"
                "(check-sat)\n",
                (int)enh->context,
                (double)enh->profile.max_velocity,
                (double)enh->profile.max_force);
            if (n > 0) { written += (uint32_t)n; }
            break;

        case NIMCP_VERIFY_FORMAT_TLA:
            n = snprintf(output + written, max_output - written,
                "---- MODULE LGSSEnhanced ----\n"
                "EXTENDS Reals\n\n"
                "VARIABLES velocity, force\n\n");
            if (n > 0) { written += (uint32_t)n; }

            for (uint32_t i = 0; i < enh->num_monotonic && written < max_output; i++) {
                const nimcp_monotonic_rule_t* r = &enh->monotonic_rules[i];
                n = snprintf(output + written, max_output - written,
                    "%s_Safety == %s <= %.6f\n",
                    r->name, r->name, (double)r->current_value);
                if (n > 0) { written += (uint32_t)n; }
            }

            n = snprintf(output + written, max_output - written,
                "\nMotorSafety == velocity <= %.6f\n"
                "ForceSafety == force <= %.6f\n"
                "============================\n",
                (double)enh->profile.max_velocity,
                (double)enh->profile.max_force);
            if (n > 0) { written += (uint32_t)n; }
            break;

        case NIMCP_VERIFY_FORMAT_JSON:
            n = snprintf(output + written, max_output - written,
                "{\"rules\": [");
            if (n > 0) { written += (uint32_t)n; }

            for (uint32_t i = 0; i < enh->num_monotonic && written < max_output; i++) {
                const nimcp_monotonic_rule_t* r = &enh->monotonic_rules[i];
                n = snprintf(output + written, max_output - written,
                    "%s{\"field\": \"%s\", \"op\": \"<=\", \"value\": %.6f, \"id\": %u}",
                    (i > 0) ? ", " : "",
                    r->name, (double)r->current_value, r->rule_id);
                if (n > 0) { written += (uint32_t)n; }
            }

            /* Add context constraints */
            if (written < max_output) {
                n = snprintf(output + written, max_output - written,
                    "%s{\"field\": \"velocity\", \"op\": \"<=\", \"value\": %.6f}"
                    ", {\"field\": \"force\", \"op\": \"<=\", \"value\": %.6f}",
                    (enh->num_monotonic > 0) ? ", " : "",
                    (double)enh->profile.max_velocity,
                    (double)enh->profile.max_force);
                if (n > 0) { written += (uint32_t)n; }
            }

            if (written < max_output) {
                n = snprintf(output + written, max_output - written,
                    "], \"context\": %d}", (int)enh->context);
                if (n > 0) { written += (uint32_t)n; }
            }
            break;

        default:
            ENH_LOG_ERROR("Unknown verification format: %d", (int)format);
            return -1;
    }

    return (int)written;
}

int nimcp_lgss_verify_property(const void* lgss, const char* property,
    nimcp_verification_result_t* result)
{
    if (!property || !result) {
        return -1;
    }

    const nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    memset(result, 0, sizeof(*result));
    snprintf(result->property, sizeof(result->property), "%s", property);

    /*
     * Bounded model check: parse "field op value" and test against context limits.
     * This is an APPROXIMATION, not formal verification.
     * Real formal verification would use an external solver.
     */
    char field[64] = {0};
    char op[8] = {0};
    float value = 0.0f;

    if (sscanf(property, "%63s %7s %f", field, op, &value) != 3) {
        result->holds = false;
        snprintf(result->counterexample, sizeof(result->counterexample),
            "Could not parse property: '%s' (expected 'field op value')", property);
        return 0;
    }

    /* Check against context profile limits */
    float context_limit = 0.0f;
    bool found = false;

    if (strcmp(field, "motor_velocity") == 0 || strcmp(field, "velocity") == 0) {
        context_limit = enh->profile.max_velocity;
        found = true;
    } else if (strcmp(field, "motor_force") == 0 || strcmp(field, "force") == 0) {
        context_limit = enh->profile.max_force;
        found = true;
    } else if (strcmp(field, "altitude") == 0) {
        context_limit = enh->profile.max_altitude;
        found = true;
    } else if (strcmp(field, "geofence") == 0) {
        context_limit = enh->profile.geofence_radius;
        found = true;
    }

    if (!found) {
        /* Check monotonic rules */
        for (uint32_t i = 0; i < enh->num_monotonic; i++) {
            if (strcmp(enh->monotonic_rules[i].name, field) == 0) {
                context_limit = enh->monotonic_rules[i].current_value;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        result->holds = false;
        snprintf(result->counterexample, sizeof(result->counterexample),
            "Unknown field '%s' -- cannot verify", field);
        return 0;
    }

    /* Verify: does the context limit guarantee the property? */
    if (strcmp(op, "<") == 0) {
        result->holds = (context_limit < value);
    } else if (strcmp(op, "<=") == 0) {
        result->holds = (context_limit <= value);
    } else if (strcmp(op, ">") == 0) {
        result->holds = false; /* Cannot guarantee lower bound from upper limit */
        snprintf(result->counterexample, sizeof(result->counterexample),
            "%s could be 0, violating %s > %.4f", field, field, (double)value);
        return 0;
    } else {
        result->holds = false;
        snprintf(result->counterexample, sizeof(result->counterexample),
            "Unsupported operator '%s'", op);
        return 0;
    }

    if (!result->holds) {
        snprintf(result->counterexample, sizeof(result->counterexample),
            "Context allows %s up to %.4f, which violates %s %s %.4f",
            field, (double)context_limit, field, op, (double)value);
    }

    return 0;
}

/*=============================================================================
 * 8. MULTI-STAKEHOLDER GOVERNANCE
 *============================================================================*/

int nimcp_lgss_register_stakeholder(void* lgss, const nimcp_stakeholder_t* stakeholder) {
    if (!stakeholder) {
        ENH_LOG_ERROR("NULL stakeholder");
        return -1;
    }

    nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    /* Safety: can_relax should always be false */
    if (stakeholder->can_relax) {
        ENH_LOG_ERROR("Stakeholder '%s' has can_relax=true -- DENIED for safety",
            stakeholder->stakeholder_name);
        return -1;
    }

    nimcp_mutex_lock(enh->lock);

    if (enh->num_stakeholders >= enh->max_stakeholders) {
        ENH_LOG_ERROR("Stakeholder array full (%u)", enh->max_stakeholders);
        nimcp_mutex_unlock(enh->lock);
        return -1;
    }

    /* Deep copy stakeholder */
    nimcp_stakeholder_t* dst = &enh->stakeholders[enh->num_stakeholders];
    *dst = *stakeholder;

    /* Deep copy policies */
    if (stakeholder->num_policies > 0 && stakeholder->policies) {
        dst->policies = (nimcp_composed_policy_t*)nimcp_calloc(
            stakeholder->num_policies, sizeof(nimcp_composed_policy_t));
        if (!dst->policies) {
            ENH_LOG_ERROR("Failed to allocate stakeholder policies");
            nimcp_mutex_unlock(enh->lock);
            return -1;
        }
        for (uint32_t i = 0; i < stakeholder->num_policies; i++) {
            dst->policies[i] = stakeholder->policies[i];
            /* Deep copy conditions within each policy */
            if (stakeholder->policies[i].num_conditions > 0 &&
                stakeholder->policies[i].conditions) {
                dst->policies[i].conditions = (nimcp_rule_condition_t*)nimcp_calloc(
                    stakeholder->policies[i].num_conditions,
                    sizeof(nimcp_rule_condition_t));
                if (dst->policies[i].conditions) {
                    memcpy(dst->policies[i].conditions,
                        stakeholder->policies[i].conditions,
                        stakeholder->policies[i].num_conditions *
                            sizeof(nimcp_rule_condition_t));
                }
            }
        }
    } else {
        dst->policies = NULL;
        dst->num_policies = 0;
    }

    enh->num_stakeholders++;
    ENH_LOG_INFO("Registered stakeholder '%s' (priority=%u, %u policies, can_tighten=%d)",
        stakeholder->stakeholder_name, stakeholder->priority,
        stakeholder->num_policies, stakeholder->can_tighten);

    nimcp_mutex_unlock(enh->lock);
    return 0;
}

int nimcp_lgss_evaluate_all_stakeholders(void* lgss, const float* state,
    uint32_t state_dim, uint32_t* combined_level)
{
    if (!state || !combined_level) {
        return -1;
    }

    nimcp_lgss_enhanced_t* enh = registry_find(lgss);
    if (!enh) {
        ENH_LOG_ERROR("No enhanced LGSS found for base context");
        return -1;
    }
    ENH_VALIDATE(enh);

    nimcp_mutex_lock(enh->lock);

    *combined_level = 0;

    /* Evaluate each stakeholder's policies.
     * Fail-safe: take the MOST RESTRICTIVE result across all stakeholders.
     * If any stakeholder blocks, the action is blocked. */
    for (uint32_t s = 0; s < enh->num_stakeholders; s++) {
        const nimcp_stakeholder_t* sh = &enh->stakeholders[s];

        for (uint32_t p = 0; p < sh->num_policies; p++) {
            const nimcp_composed_policy_t* pol = &sh->policies[p];
            if (!pol->conditions || pol->num_conditions == 0) {
                continue;
            }

            bool violated = false;

            if (pol->combiner == NIMCP_RULE_OP_AND) {
                violated = false;
                for (uint32_t c = 0; c < pol->num_conditions; c++) {
                    if (!evaluate_condition(&pol->conditions[c], state, state_dim)) {
                        violated = true;
                        break;
                    }
                }
            } else if (pol->combiner == NIMCP_RULE_OP_OR) {
                violated = true;
                for (uint32_t c = 0; c < pol->num_conditions; c++) {
                    if (evaluate_condition(&pol->conditions[c], state, state_dim)) {
                        violated = false;
                        break;
                    }
                }
            }

            if (violated && pol->severity > *combined_level) {
                *combined_level = pol->severity;
                ENH_LOG_WARN("Stakeholder '%s' policy '%s' violated (severity=%u)",
                    sh->stakeholder_name, pol->name, pol->severity);
            }
        }
    }

    nimcp_mutex_unlock(enh->lock);
    return 0;
}
