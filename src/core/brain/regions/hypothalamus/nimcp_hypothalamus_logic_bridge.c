/**
 * @file nimcp_hypothalamus_logic_bridge.c
 * @brief Bidirectional Hypothalamus-Logic Integration Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_logic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <ctype.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_logic_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_LOGIC_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct hypo_logic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Connected systems */
    hypo_drive_system_handle_t* drives;
    symbolic_logic_t* logic;

    /* Configuration */
    hypo_logic_config_t config;

    /* Modulation state (Hypo -> Logic) */
    hypo_logic_modulation_t modulation;
    bool modulation_valid;

    /* Goals */
    hypo_motivated_goal_t goals[HYPO_LOGIC_MAX_GOALS];
    uint32_t num_goals;

    /* Conclusions cache (Logic -> Hypo) */
    hypo_logic_conclusion_t conclusions[HYPO_LOGIC_MAX_CONCLUSIONS];
    uint32_t num_conclusions;
    uint32_t conclusion_head;

    /* Anticipation state */
    hypo_logic_anticipation_t anticipation;

    /* FEP state */
    hypo_logic_fep_state_t fep_state;
    hypo_logic_prediction_t predictions[HYPO_DRIVE_COUNT];

    /* Bio-async */
    bool bio_registered;
    bio_module_context_t bio_ctx;

    /* Statistics */
    hypo_logic_stats_t stats;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
};

/**
 * @brief Check if predicate name matches a pattern
 *
 * Simple pattern matching - checks if name contains pattern as substring
 * or starts with pattern.
 */
static bool predicate_matches(const char* name, const char* pattern) {
    if (!name || !pattern) {
        return false;
    }
    return strstr(name, pattern) != NULL;
}

/**
 * @brief Convert string to lowercase for comparison
 */
static void to_lowercase(const char* src, char* dst, size_t max_len) {
    size_t i = 0;
    while (src[i] && i < max_len - 1) {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

/*=============================================================================
 * PREDICATE CATEGORY DETECTION
 *===========================================================================*/

hypo_predicate_category_t hypo_logic_get_predicate_category(const char* predicate_name) {
    if (!predicate_name) return HYPO_PRED_CAT_NEUTRAL;

    char lower[LOGIC_MAX_NAME_LENGTH];
    to_lowercase(predicate_name, lower, sizeof(lower));

    /* NOTE: Order matters! More specific patterns must come before generic ones.
     * E.g., "threat" contains "eat", "heat" contains "eat", so threat/temperature
     * must be checked before food patterns. */

    /* Threat-related (must be before food - "threat" contains "eat") */
    if (predicate_matches(lower, "threat") ||
        predicate_matches(lower, "danger") ||
        predicate_matches(lower, "harm") ||
        predicate_matches(lower, "attack") ||
        predicate_matches(lower, "predator") ||
        predicate_matches(lower, "enemy") ||
        predicate_matches(lower, "hostile")) {
        return HYPO_PRED_CAT_THREAT;
    }

    /* Temperature-related (must be before food - "heat" contains "eat") */
    if (predicate_matches(lower, "temp") ||
        predicate_matches(lower, "heat") ||
        predicate_matches(lower, "hot") ||
        predicate_matches(lower, "warm") ||
        predicate_matches(lower, "cold") ||
        predicate_matches(lower, "cool") ||
        predicate_matches(lower, "thermal") ||
        predicate_matches(lower, "freeze")) {
        return HYPO_PRED_CAT_TEMPERATURE;
    }

    /* Food-related */
    if (predicate_matches(lower, "food") ||
        predicate_matches(lower, "eat") ||
        predicate_matches(lower, "hungry") ||
        predicate_matches(lower, "nourish") ||
        predicate_matches(lower, "meal") ||
        predicate_matches(lower, "fruit") ||
        predicate_matches(lower, "edible")) {
        return HYPO_PRED_CAT_FOOD;
    }

    /* Water-related */
    if (predicate_matches(lower, "water") ||
        predicate_matches(lower, "drink") ||
        predicate_matches(lower, "thirst") ||
        predicate_matches(lower, "hydrat") ||
        predicate_matches(lower, "liquid")) {
        return HYPO_PRED_CAT_WATER;
    }

    /* Safety-related */
    if (predicate_matches(lower, "safe") ||
        predicate_matches(lower, "shelter") ||
        predicate_matches(lower, "protect") ||
        predicate_matches(lower, "secure") ||
        predicate_matches(lower, "refuge")) {
        return HYPO_PRED_CAT_SAFETY;
    }

    /* Social-related */
    if (predicate_matches(lower, "social") ||
        predicate_matches(lower, "friend") ||
        predicate_matches(lower, "ally") ||
        predicate_matches(lower, "companion") ||
        predicate_matches(lower, "cooperat") ||
        predicate_matches(lower, "help") ||
        predicate_matches(lower, "together")) {
        return HYPO_PRED_CAT_SOCIAL;
    }

    /* Knowledge-related */
    if (predicate_matches(lower, "know") ||
        predicate_matches(lower, "learn") ||
        predicate_matches(lower, "discover") ||
        predicate_matches(lower, "understand") ||
        predicate_matches(lower, "curious") ||
        predicate_matches(lower, "explore") ||
        predicate_matches(lower, "information")) {
        return HYPO_PRED_CAT_KNOWLEDGE;
    }

    /* Rest-related */
    if (predicate_matches(lower, "rest") ||
        predicate_matches(lower, "sleep") ||
        predicate_matches(lower, "tired") ||
        predicate_matches(lower, "fatigue") ||
        predicate_matches(lower, "energy")) {
        return HYPO_PRED_CAT_REST;
    }

    /* Achievement-related */
    if (predicate_matches(lower, "achieve") ||
        predicate_matches(lower, "accomplish") ||
        predicate_matches(lower, "success") ||
        predicate_matches(lower, "master") ||
        predicate_matches(lower, "skill") ||
        predicate_matches(lower, "competen")) {
        return HYPO_PRED_CAT_ACHIEVEMENT;
    }

    /* Autonomy-related */
    if (predicate_matches(lower, "autonomy") ||
        predicate_matches(lower, "choice") ||
        predicate_matches(lower, "control") ||
        predicate_matches(lower, "freedom") ||
        predicate_matches(lower, "independ")) {
        return HYPO_PRED_CAT_AUTONOMY;
    }

    return HYPO_PRED_CAT_NEUTRAL;
}

hypo_drive_type_t hypo_logic_category_to_drive(hypo_predicate_category_t category) {
    switch (category) {
        case HYPO_PRED_CAT_FOOD:        return HYPO_DRIVE_HUNGER;
        case HYPO_PRED_CAT_WATER:       return HYPO_DRIVE_THIRST;
        case HYPO_PRED_CAT_THREAT:      return HYPO_DRIVE_SAFETY;
        case HYPO_PRED_CAT_SAFETY:      return HYPO_DRIVE_SAFETY;
        case HYPO_PRED_CAT_SOCIAL:      return HYPO_DRIVE_SOCIAL;
        case HYPO_PRED_CAT_KNOWLEDGE:   return HYPO_DRIVE_CURIOSITY;
        case HYPO_PRED_CAT_TEMPERATURE: return HYPO_DRIVE_TEMPERATURE;
        case HYPO_PRED_CAT_REST:        return HYPO_DRIVE_FATIGUE;
        case HYPO_PRED_CAT_ACHIEVEMENT: return HYPO_DRIVE_COMPETENCE;
        case HYPO_PRED_CAT_AUTONOMY:    return HYPO_DRIVE_AUTONOMY;
        default:                        return HYPO_DRIVE_COUNT;
    }
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypo_logic_config_t hypo_logic_default_config(void) {
    hypo_logic_config_t config = {0};

    /* Motivated reasoning settings */
    config.base_inference_depth = 10.0f;
    config.min_inference_depth = 3.0f;
    config.urgency_depth_sensitivity = 0.5f;

    config.base_proof_threshold = 0.7f;
    config.wishful_thinking_max = 0.3f;
    config.threshold_drive_sensitivity = 0.4f;

    config.salience_drive_weight = 0.6f;
    config.salience_decay_rate = 0.05f;

    /* Logic-driven updates */
    config.anticipation_gain = 0.3f;
    config.frustration_gain = 0.4f;
    config.conclusion_decay_rate = 0.02f;

    /* FEP settings */
    config.enable_fep_integration = true;
    config.prediction_learning_rate = NIMCP_LEARNING_RATE_COARSE;
    config.precision_base = 0.5f;

    /* Fatigue/capacity */
    config.fatigue_capacity_weight = 0.6f;
    config.arousal_capacity_weight = 0.3f;

    /* Bio-async */
    config.enable_bio_async = true;
    config.update_interval_us = 10000;  /* 10ms */

    config.num_predicate_maps = 0;

    return config;
}

hypo_logic_bridge_t* hypo_logic_bridge_create(
    hypo_drive_system_handle_t* drives,
    symbolic_logic_t* logic,
    const hypo_logic_config_t* config)
{
    if (!drives || !logic) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_logic_bridge: NULL drives or logic");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_bridge_create: required parameter is NULL (drives, logic)");
        return NULL;
    }

    hypo_logic_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_logic_bridge_t));
    if (!bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_logic_bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "hypothalamus_logic") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_logic_bridge_create: bridge_base_init failed");
        return NULL;
    }

    bridge->drives = drives;
    bridge->logic = logic;

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hypo_logic_default_config();
    }

    /* Initialize modulation */
    bridge->modulation.max_inference_depth = (uint32_t)bridge->config.base_inference_depth;
    bridge->modulation.base_inference_depth = (uint32_t)bridge->config.base_inference_depth;
    bridge->modulation.depth_urgency_factor = bridge->config.urgency_depth_sensitivity;
    bridge->modulation.proof_threshold = bridge->config.base_proof_threshold;
    bridge->modulation.base_threshold = bridge->config.base_proof_threshold;
    bridge->modulation.wishful_thinking_bias = 0.0f;
    bridge->modulation.reasoning_capacity = 1.0f;
    bridge->modulation.effort_willingness = 1.0f;
    bridge->modulation.priority_drive = HYPO_DRIVE_COUNT;
    bridge->modulation.priority_weight = 0.0f;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->modulation.salience_boost[i] = 1.0f;
    }

    /* Initialize anticipation */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->anticipation.anticipation[i] = 0.0f;
        bridge->anticipation.frustration[i] = 0.0f;
        bridge->anticipation.confidence[i] = 0.5f;
    }

    /* Initialize FEP */
    bridge->fep_state.logical_free_energy = 0.0f;
    bridge->fep_state.prediction_error = 0.0f;
    bridge->fep_state.precision = bridge->config.precision_base;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->predictions[i].drive = (hypo_drive_type_t)i;
        bridge->predictions[i].predicted_probability = 0.5f;
        bridge->predictions[i].precision = bridge->config.precision_base;
        bridge->predictions[i].resolved = false;
    }

    /* Timing */
    bridge->creation_time_us = nimcp_time_get_us();
    bridge->last_update_us = bridge->creation_time_us;

    /* Auto-register common mappings */
    hypo_logic_auto_register_mappings(bridge);

    nimcp_log(LOG_LEVEL_INFO, "hypo_logic_bridge: created successfully");

    return bridge;
}

void hypo_logic_bridge_destroy(hypo_logic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_logic");

    nimcp_log(LOG_LEVEL_INFO, "hypo_logic_bridge: destroying");

    /* Clean up goals */
    for (uint32_t i = 0; i < bridge->num_goals; i++) {
        /* Note: goal_clause ownership may vary - be careful here */
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int hypo_logic_bridge_reset(hypo_logic_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Reset modulation */
    bridge->modulation.max_inference_depth = (uint32_t)bridge->config.base_inference_depth;
    bridge->modulation.proof_threshold = bridge->config.base_proof_threshold;
    bridge->modulation.wishful_thinking_bias = 0.0f;
    bridge->modulation.reasoning_capacity = 1.0f;
    bridge->modulation.priority_drive = HYPO_DRIVE_COUNT;
    bridge->modulation_valid = false;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->modulation.salience_boost[i] = 1.0f;
    }

    /* Reset goals */
    bridge->num_goals = 0;

    /* Reset conclusions */
    bridge->num_conclusions = 0;
    bridge->conclusion_head = 0;

    /* Reset anticipation */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->anticipation.anticipation[i] = 0.0f;
        bridge->anticipation.frustration[i] = 0.0f;
        bridge->anticipation.confidence[i] = 0.5f;
    }

    /* Reset FEP */
    bridge->fep_state.logical_free_energy = 0.0f;
    bridge->fep_state.prediction_error = 0.0f;
    bridge->fep_state.precision = bridge->config.precision_base;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->predictions[i].predicted_probability = 0.5f;
        bridge->predictions[i].precision = bridge->config.precision_base;
        bridge->predictions[i].resolved = false;
    }

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

int hypo_logic_bridge_update(hypo_logic_bridge_t* bridge, uint64_t delta_us) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    (void)delta_us;

    uint64_t start_us = nimcp_time_get_us();

    /* 1. Compute drive-based modulation */
    hypo_logic_compute_modulation(bridge);

    /* 2. Apply modulation to logic engine */
    hypo_logic_apply_modulation(bridge);

    /* 3. Decay anticipation and frustration */
    float decay = bridge->config.conclusion_decay_rate;
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->anticipation.anticipation[i] *= (1.0f - decay);
        bridge->anticipation.frustration[i] *= (1.0f - decay);
    }

    /* 4. Update FEP state */
    if (bridge->config.enable_fep_integration) {
        hypo_logic_compute_free_energy(bridge, &bridge->fep_state);
    }

    /* 5. Broadcast if bio-async enabled */
    if (bridge->config.enable_bio_async && bridge->bio_registered) {
        hypo_logic_broadcast_modulation(bridge);
    }

    bridge->last_update_us = nimcp_time_get_us();

    float latency = (float)(bridge->last_update_us - start_us);
    float new_avg = bridge->stats.avg_modulation_latency_us * 0.95f + latency * 0.05f;
    if (isfinite(new_avg)) {
        bridge->stats.avg_modulation_latency_us = new_avg;
    }

    return 0;
}

/*=============================================================================
 * HYPOTHALAMUS -> LOGIC: MOTIVATED REASONING
 *===========================================================================*/

int hypo_logic_compute_modulation(hypo_logic_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_drive_system_t drive_state;
    if (!hypo_drive_get_system_state(bridge->drives, &drive_state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logic_compute_modulation: hypo_drive_get_system_state is NULL");
        return -1;
    }

    hypo_logic_modulation_t* mod = &bridge->modulation;
    const hypo_logic_config_t* cfg = &bridge->config;

    /* Find highest urgency drive */
    float max_urgency = 0.0f;
    hypo_drive_type_t priority_drive = HYPO_DRIVE_COUNT;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        float urgency = drive_state.drives[i].urgency;
        if (urgency > max_urgency) {
            max_urgency = urgency;
            priority_drive = (hypo_drive_type_t)i;
        }
    }

    mod->priority_drive = priority_drive;
    mod->priority_weight = max_urgency;

    /*
     * INFERENCE DEPTH:
     * High urgency -> shallow inference (fast, heuristic)
     * Low urgency -> deep inference (thorough)
     */
    float depth_reduction = max_urgency * cfg->urgency_depth_sensitivity;
    float effective_depth = cfg->base_inference_depth * (1.0f - depth_reduction);
    mod->max_inference_depth = (uint32_t)fmaxf(cfg->min_inference_depth, effective_depth);

    /*
     * PROOF THRESHOLD:
     * Drives bias acceptance of desired conclusions (wishful thinking)
     */
    mod->wishful_thinking_bias = max_urgency * cfg->wishful_thinking_max;
    /* Note: actual threshold is goal-specific, computed in get_goal_threshold */
    mod->proof_threshold = cfg->base_proof_threshold;

    /*
     * SALIENCE BOOSTS:
     * Each drive boosts salience of related predicates
     */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        float urgency = drive_state.drives[i].urgency;
        float boost = 1.0f + urgency * cfg->salience_drive_weight;
        mod->salience_boost[i] = boost;
    }

    /*
     * REASONING CAPACITY:
     * Fatigue reduces capacity, arousal can help or hurt
     */
    float fatigue = drive_state.drives[HYPO_DRIVE_FATIGUE].urgency;
    float arousal = drive_state.arousal_level;

    /* Yerkes-Dodson: moderate arousal is best */
    float arousal_effect = 1.0f - fabsf(arousal - 0.5f) * 2.0f * cfg->arousal_capacity_weight;
    float fatigue_effect = 1.0f - fatigue * cfg->fatigue_capacity_weight;

    mod->reasoning_capacity = nimcp_clamp01(arousal_effect * fatigue_effect);
    mod->effort_willingness = nimcp_clamp01(1.0f - fatigue);

    mod->last_modulation_us = nimcp_time_get_us();
    bridge->modulation_valid = true;
    bridge->stats.salience_modulations++;
    bridge->stats.depth_modulations++;

    return 0;
}

int hypo_logic_get_modulation(
    const hypo_logic_bridge_t* bridge,
    hypo_logic_modulation_t* modulation)
{
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    *modulation = bridge->modulation;
    return 0;
}

int hypo_logic_apply_modulation(hypo_logic_bridge_t* bridge) {
    if (!bridge || !bridge->logic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_apply_modulation: bridge or logic is NULL");
        return -1;
    }

    /*
     * Apply modulation to the logic engine.
     * This would set inference parameters and predicate salience.
     *
     * Note: The actual application depends on symbolic_logic API.
     * Here we just track that we did it.
     */

    /* The logic engine would need functions like:
     * - symbolic_logic_set_max_depth(logic, depth)
     * - symbolic_logic_set_predicate_salience(logic, name, salience)
     *
     * For now, we maintain the modulation state and let callers
     * query it when needed.
     */

    return 0;
}

float hypo_logic_get_predicate_salience(
    const hypo_logic_bridge_t* bridge,
    const char* predicate_name)
{
    if (!bridge || !predicate_name) return 1.0f;

    float salience = 1.0f;

    /* Check registered mappings first */
    for (uint32_t i = 0; i < bridge->config.num_predicate_maps; i++) {
        const hypo_logic_predicate_map_t* map = &bridge->config.predicate_maps[i];
        if (predicate_matches(predicate_name, map->predicate_name)) {
            hypo_drive_type_t drive = map->drive;
            if (drive < HYPO_DRIVE_COUNT) {
                salience *= bridge->modulation.salience_boost[drive] * map->relevance;
            }
        }
    }

    /* Fall back to automatic categorization */
    hypo_predicate_category_t cat = hypo_logic_get_predicate_category(predicate_name);
    hypo_drive_type_t drive = hypo_logic_category_to_drive(cat);

    if (drive < HYPO_DRIVE_COUNT) {
        salience *= bridge->modulation.salience_boost[drive];
    }

    /* Apply global bias */
    salience *= (1.0f + bridge->modulation.global_salience_bias);

    return nimcp_clampf(salience, 0.1f, 10.0f);
}

float hypo_logic_get_goal_threshold(
    const hypo_logic_bridge_t* bridge,
    const logic_clause_t* goal)
{
    if (!bridge) return 0.7f;

    float threshold = bridge->modulation.base_threshold;

    if (!goal) return threshold;

    /* Check if goal relates to a high-urgency drive */
    /* This implements "wishful thinking" - we accept weaker evidence
     * for conclusions we want to be true */

    /* Analyze goal predicates to determine drive relevance */
    if (goal->literals && goal->num_literals > 0) {
        atomic_formula_t* atom = goal->literals[0];
        if (atom && atom->name[0]) {
            hypo_predicate_category_t cat = hypo_logic_get_predicate_category(atom->name);
            hypo_drive_type_t drive = hypo_logic_category_to_drive(cat);

            if (drive < HYPO_DRIVE_COUNT) {
                /* Reduce threshold proportionally to drive urgency */
                float urgency = 0.0f;
                hypo_drive_state_t ds;
                if (hypo_drive_get_state(bridge->drives, drive, &ds)) {
                    urgency = ds.urgency;
                }

                float reduction = urgency * bridge->modulation.wishful_thinking_bias;
                threshold -= reduction;
            }
        }
    }

    return nimcp_clampf(threshold, 0.1f, 1.0f);
}

int hypo_logic_create_motivated_goal(
    hypo_logic_bridge_t* bridge,
    hypo_drive_type_t drive,
    logic_clause_t* goal_clause,
    float anticipated_satisfaction)
{
    if (!bridge || !goal_clause || drive >= HYPO_DRIVE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logic_create_motivated_goal: invalid parameters");
        return -1;
    }
    if (bridge->num_goals >= HYPO_LOGIC_MAX_GOALS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logic_create_motivated_goal: max goals reached");
        return -1;
    }

    hypo_motivated_goal_t* goal = &bridge->goals[bridge->num_goals];

    goal->motivating_drive = drive;
    goal->goal_clause = goal_clause;  /* Note: ownership semantics matter */
    goal->anticipated_satisfaction = nimcp_clamp01(anticipated_satisfaction);
    goal->active = true;
    goal->created_us = nimcp_time_get_us();
    goal->deadline_us = 0;

    /* Priority based on drive urgency */
    hypo_drive_state_t ds;
    if (hypo_drive_get_state(bridge->drives, drive, &ds)) {
        goal->priority = ds.urgency * anticipated_satisfaction;
    } else {
        goal->priority = anticipated_satisfaction * 0.5f;
    }

    bridge->num_goals++;
    bridge->stats.goals_created++;

    return 0;
}

int hypo_logic_get_prioritized_goals(
    const hypo_logic_bridge_t* bridge,
    hypo_motivated_goal_t* goals,
    uint32_t max_goals,
    uint32_t* num_goals)
{
    if (!bridge || !goals || !num_goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_get_prioritized_goals: NULL parameters");
        return -1;
    }

    /* Simple approach: copy active goals sorted by priority */
    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->num_goals && count < max_goals; i++) {
        if (bridge->goals[i].active) {
            goals[count++] = bridge->goals[i];
        }
    }

    /* Simple bubble sort by priority (descending) */
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (goals[j].priority > goals[i].priority) {
                hypo_motivated_goal_t tmp = goals[i];
                goals[i] = goals[j];
                goals[j] = tmp;
            }
        }
    }

    *num_goals = count;
    return 0;
}

uint32_t hypo_logic_get_recommended_depth(const hypo_logic_bridge_t* bridge) {
    if (!bridge) return 10;
    return bridge->modulation.max_inference_depth;
}

/*=============================================================================
 * LOGIC -> HYPOTHALAMUS: CONCLUSIONS AFFECT DRIVES
 *===========================================================================*/

hypo_conclusion_type_t hypo_logic_classify_conclusion(
    const hypo_logic_bridge_t* bridge,
    const logic_clause_t* conclusion)
{
    if (!bridge || !conclusion) return HYPO_CONCL_NEUTRAL;
    if (!conclusion->literals || conclusion->num_literals == 0) return HYPO_CONCL_NEUTRAL;

    atomic_formula_t* atom = conclusion->literals[0];
    if (!atom || !atom->name[0]) return HYPO_CONCL_NEUTRAL;

    char lower[LOGIC_MAX_NAME_LENGTH];
    to_lowercase(atom->name, lower, sizeof(lower));
    bool negated = atom->negated;

    /* Check for threat-related */
    if (predicate_matches(lower, "threat") ||
        predicate_matches(lower, "danger") ||
        predicate_matches(lower, "attack") ||
        predicate_matches(lower, "hostile")) {
        return negated ? HYPO_CONCL_THREAT_ABSENT : HYPO_CONCL_THREAT_PRESENT;
    }

    /* Check for safety-related */
    if (predicate_matches(lower, "safe") ||
        predicate_matches(lower, "secure") ||
        predicate_matches(lower, "protect")) {
        return negated ? HYPO_CONCL_THREAT_PRESENT : HYPO_CONCL_THREAT_ABSENT;
    }

    /* Check for impossibility (must be before opportunity - "impossible" contains "possible") */
    if (predicate_matches(lower, "impossible") ||
        predicate_matches(lower, "cannot") ||
        predicate_matches(lower, "unreachable") ||
        predicate_matches(lower, "blocked") ||
        predicate_matches(lower, "fail")) {
        return negated ? HYPO_CONCL_OPPORTUNITY : HYPO_CONCL_GOAL_IMPOSSIBLE;
    }

    /* Check for opportunity (must be before resource - to avoid "found" matching "opportunity_found") */
    if (predicate_matches(lower, "opportunity") ||
        predicate_matches(lower, "chance") ||
        predicate_matches(lower, "possible")) {
        return HYPO_CONCL_OPPORTUNITY;
    }

    /* Check for resource availability */
    if (predicate_matches(lower, "available") ||
        predicate_matches(lower, "nearby") ||
        predicate_matches(lower, "present") ||
        predicate_matches(lower, "accessible") ||
        predicate_matches(lower, "reachable") ||
        predicate_matches(lower, "source") ||
        predicate_matches(lower, "found")) {
        return negated ? HYPO_CONCL_RESOURCE_UNAVAILABLE : HYPO_CONCL_RESOURCE_AVAILABLE;
    }

    /* Check for achievement */
    if (predicate_matches(lower, "achieved") ||
        predicate_matches(lower, "accomplished") ||
        predicate_matches(lower, "success") ||
        predicate_matches(lower, "done") ||
        predicate_matches(lower, "completed")) {
        return negated ? HYPO_CONCL_GOAL_IMPOSSIBLE : HYPO_CONCL_GOAL_ACHIEVED;
    }

    return HYPO_CONCL_NEUTRAL;
}

hypo_drive_type_t hypo_logic_get_affected_drive(
    const hypo_logic_bridge_t* bridge,
    const logic_clause_t* conclusion)
{
    if (!bridge || !conclusion) return HYPO_DRIVE_COUNT;
    if (!conclusion->literals || conclusion->num_literals == 0) return HYPO_DRIVE_COUNT;

    atomic_formula_t* atom = conclusion->literals[0];
    if (!atom) return HYPO_DRIVE_COUNT;

    hypo_predicate_category_t cat = hypo_logic_get_predicate_category(atom->name);
    return hypo_logic_category_to_drive(cat);
}

int hypo_logic_process_conclusion(
    hypo_logic_bridge_t* bridge,
    logic_clause_t* conclusion,
    float confidence)
{
    if (!bridge || !conclusion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_process_conclusion: bridge or conclusion is NULL");
        return -1;
    }

    uint64_t start_us = nimcp_time_get_us();

    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, conclusion);
    hypo_drive_type_t affected = hypo_logic_get_affected_drive(bridge, conclusion);

    /* Cache conclusion */
    uint32_t idx = bridge->conclusion_head;
    bridge->conclusions[idx].conclusion = conclusion;
    bridge->conclusions[idx].type = type;
    bridge->conclusions[idx].affected_drive = affected;
    bridge->conclusions[idx].confidence = nimcp_clamp01(confidence);
    bridge->conclusions[idx].processed = false;
    bridge->conclusions[idx].timestamp_us = start_us;

    bridge->conclusion_head = (bridge->conclusion_head + 1) % HYPO_LOGIC_MAX_CONCLUSIONS;
    if (bridge->num_conclusions < HYPO_LOGIC_MAX_CONCLUSIONS) {
        bridge->num_conclusions++;
    }

    /* Process based on type */
    float impact = 0.0f;

    switch (type) {
        case HYPO_CONCL_RESOURCE_AVAILABLE:
            if (affected < HYPO_DRIVE_COUNT) {
                bridge->anticipation.anticipation[affected] += confidence * bridge->config.anticipation_gain;
                bridge->anticipation.anticipation[affected] = nimcp_clamp01(bridge->anticipation.anticipation[affected]);
                bridge->stats.drive_boosts++;
                impact = confidence * bridge->config.anticipation_gain;
            }
            break;

        case HYPO_CONCL_RESOURCE_UNAVAILABLE:
            if (affected < HYPO_DRIVE_COUNT) {
                bridge->anticipation.frustration[affected] += confidence * bridge->config.frustration_gain;
                bridge->anticipation.frustration[affected] = nimcp_clamp01(bridge->anticipation.frustration[affected]);
                bridge->stats.frustration_events++;
                impact = -confidence * bridge->config.frustration_gain;
            }
            break;

        case HYPO_CONCL_THREAT_PRESENT:
            /* Boost safety drive urgency */
            bridge->anticipation.anticipation[HYPO_DRIVE_SAFETY] += confidence * 0.5f;
            bridge->anticipation.anticipation[HYPO_DRIVE_SAFETY] =
                nimcp_clamp01(bridge->anticipation.anticipation[HYPO_DRIVE_SAFETY]);
            bridge->stats.drive_boosts++;
            impact = confidence * 0.5f;
            break;

        case HYPO_CONCL_THREAT_ABSENT:
            /* Reduce safety anticipation */
            bridge->anticipation.anticipation[HYPO_DRIVE_SAFETY] *= (1.0f - confidence * 0.3f);
            bridge->stats.drive_reductions++;
            impact = -confidence * 0.3f;
            break;

        case HYPO_CONCL_GOAL_ACHIEVED:
            if (affected < HYPO_DRIVE_COUNT) {
                bridge->stats.goals_achieved++;
                impact = confidence * bridge->config.anticipation_gain;
            }
            break;

        case HYPO_CONCL_GOAL_IMPOSSIBLE:
            if (affected < HYPO_DRIVE_COUNT) {
                bridge->anticipation.frustration[affected] += confidence * bridge->config.frustration_gain;
                bridge->anticipation.frustration[affected] = nimcp_clamp01(bridge->anticipation.frustration[affected]);
                bridge->stats.goals_abandoned++;
                impact = -confidence * bridge->config.frustration_gain;
            }
            break;

        case HYPO_CONCL_OPPORTUNITY:
            if (affected < HYPO_DRIVE_COUNT) {
                bridge->anticipation.anticipation[affected] += confidence * bridge->config.anticipation_gain * 0.5f;
                bridge->anticipation.anticipation[affected] = nimcp_clamp01(bridge->anticipation.anticipation[affected]);
                impact = confidence * bridge->config.anticipation_gain * 0.5f;
            }
            break;

        default:
            break;
    }

    bridge->conclusions[idx].drive_impact = impact;
    bridge->conclusions[idx].processed = true;
    bridge->stats.conclusions_processed++;
    bridge->stats.anticipation_updates++;

    /* Update FEP predictions */
    if (bridge->config.enable_fep_integration) {
        hypo_logic_update_predictions(bridge, conclusion);
    }

    float latency = (float)(nimcp_time_get_us() - start_us);
    float new_concl_avg = bridge->stats.avg_conclusion_latency_us * 0.95f + latency * 0.05f;
    if (isfinite(new_concl_avg)) {
        bridge->stats.avg_conclusion_latency_us = new_concl_avg;
    }

    return 0;
}

float hypo_logic_goal_achieved(
    hypo_logic_bridge_t* bridge,
    hypo_drive_type_t drive,
    const logic_clause_t* goal)
{
    if (!bridge || drive >= HYPO_DRIVE_COUNT) return 0.0f;

    (void)goal;

    /* Boost anticipation (will be consumed by actual satisfaction) */
    float reward = bridge->config.anticipation_gain;

    bridge->anticipation.anticipation[drive] += reward;
    bridge->anticipation.anticipation[drive] = nimcp_clamp01(bridge->anticipation.anticipation[drive]);

    /* Find and mark goal as achieved */
    for (uint32_t i = 0; i < bridge->num_goals; i++) {
        if (bridge->goals[i].active && bridge->goals[i].motivating_drive == drive) {
            bridge->goals[i].active = false;
            reward = bridge->goals[i].anticipated_satisfaction;
            break;
        }
    }

    bridge->stats.goals_achieved++;

    return reward;
}

float hypo_logic_goal_impossible(
    hypo_logic_bridge_t* bridge,
    hypo_drive_type_t drive,
    const logic_clause_t* goal)
{
    if (!bridge || drive >= HYPO_DRIVE_COUNT) return 0.0f;

    (void)goal;

    float frustration = bridge->config.frustration_gain;

    bridge->anticipation.frustration[drive] += frustration;
    bridge->anticipation.frustration[drive] = nimcp_clamp01(bridge->anticipation.frustration[drive]);

    /* Find and mark goal as abandoned */
    for (uint32_t i = 0; i < bridge->num_goals; i++) {
        if (bridge->goals[i].active && bridge->goals[i].motivating_drive == drive) {
            bridge->goals[i].active = false;
            break;
        }
    }

    bridge->stats.goals_abandoned++;
    bridge->stats.frustration_events++;

    return frustration;
}

int hypo_logic_get_anticipation(
    const hypo_logic_bridge_t* bridge,
    hypo_logic_anticipation_t* anticipation)
{
    if (!bridge || !anticipation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_get_anticipation: bridge or anticipation is NULL");
        return -1;
    }

    *anticipation = bridge->anticipation;
    return 0;
}

/*=============================================================================
 * FEP INTEGRATION
 *===========================================================================*/

int hypo_logic_generate_predictions(hypo_logic_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    hypo_drive_system_t drive_state;
    if (!hypo_drive_get_system_state(bridge->drives, &drive_state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logic_generate_predictions: hypo_drive_get_system_state is NULL");
        return -1;
    }

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        hypo_logic_prediction_t* pred = &bridge->predictions[i];

        /* Prediction: high urgency drives predict resource availability will be proven */
        float urgency = drive_state.drives[i].urgency;

        /* Higher urgency -> expect more focus on proving resource availability */
        pred->predicted_probability = 0.3f + urgency * 0.4f;
        pred->precision = bridge->config.precision_base + urgency * 0.2f;
        pred->resolved = false;

        bridge->stats.predictions_made++;
    }

    return 0;
}

float hypo_logic_update_predictions(
    hypo_logic_bridge_t* bridge,
    const logic_clause_t* conclusion)
{
    if (!bridge || !conclusion) return 0.0f;

    hypo_drive_type_t affected = hypo_logic_get_affected_drive(bridge, conclusion);
    if (affected >= HYPO_DRIVE_COUNT) return 0.0f;

    hypo_logic_prediction_t* pred = &bridge->predictions[affected];

    /* Determine actual outcome */
    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, conclusion);
    float actual = 0.0f;

    switch (type) {
        case HYPO_CONCL_RESOURCE_AVAILABLE:
        case HYPO_CONCL_OPPORTUNITY:
        case HYPO_CONCL_GOAL_ACHIEVED:
        case HYPO_CONCL_THREAT_ABSENT:
            actual = 1.0f;
            break;

        case HYPO_CONCL_RESOURCE_UNAVAILABLE:
        case HYPO_CONCL_GOAL_IMPOSSIBLE:
        case HYPO_CONCL_THREAT_PRESENT:
            actual = 0.0f;
            break;

        default:
            actual = 0.5f;
            break;
    }

    /* Compute prediction error */
    float error = actual - pred->predicted_probability;
    float weighted_error = error * pred->precision;

    pred->actual_result = actual;
    pred->resolved = true;

    /* Update FEP state */
    bridge->fep_state.prediction_error = weighted_error;

    /* Track stats */
    if (fabsf(error) < 0.3f) {
        bridge->stats.predictions_confirmed++;
    } else {
        bridge->stats.predictions_violated++;
    }

    /* Update running average */
    float new_pred_avg = bridge->stats.avg_prediction_error * 0.9f + fabsf(error) * 0.1f;
    if (isfinite(new_pred_avg)) {
        bridge->stats.avg_prediction_error = new_pred_avg;
    }

    /* Update prediction for next time (learning) */
    pred->predicted_probability += error * bridge->config.prediction_learning_rate;
    pred->predicted_probability = nimcp_clamp01(pred->predicted_probability);

    return weighted_error;
}

int hypo_logic_compute_free_energy(
    hypo_logic_bridge_t* bridge,
    hypo_logic_fep_state_t* fe_state)
{
    if (!bridge || !fe_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_compute_free_energy: bridge or fe_state is NULL");
        return -1;
    }

    hypo_drive_system_t drive_state;
    if (!hypo_drive_get_system_state(bridge->drives, &drive_state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logic_compute_free_energy: hypo_drive_get_system_state is NULL");
        return -1;
    }

    float total_fe = 0.0f;
    float total_uncertainty = 0.0f;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        float urgency = drive_state.drives[i].urgency;
        float anticipation = bridge->anticipation.anticipation[i];
        float frustration = bridge->anticipation.frustration[i];
        float confidence = bridge->anticipation.confidence[i];

        /* Free energy from unmet drives */
        float drive_fe = urgency * (1.0f - anticipation);

        /* Add frustration component */
        drive_fe += frustration * 0.5f;

        /* Weight by uncertainty (low confidence = high FE) */
        drive_fe *= (1.0f + (1.0f - confidence) * 0.5f);

        total_fe += drive_fe;
        total_uncertainty += (1.0f - confidence);
    }

    /* Normalize */
    total_fe /= HYPO_DRIVE_COUNT;
    total_uncertainty /= HYPO_DRIVE_COUNT;

    /* Add complexity cost (reasoning effort) */
    float complexity = (float)bridge->modulation.max_inference_depth /
                       bridge->config.base_inference_depth;
    float complexity_cost = complexity * 0.1f;

    /* Expected information gain from reasoning */
    float expected_gain = bridge->modulation.reasoning_capacity *
                          total_uncertainty * 0.2f;

    fe_state->logical_free_energy = total_fe + complexity_cost;
    fe_state->precision = 1.0f - total_uncertainty;
    fe_state->complexity_cost = complexity_cost;
    fe_state->expected_info_gain = expected_gain;
    fe_state->timestamp_us = nimcp_time_get_us();

    /* Also update internal FEP state */
    bridge->fep_state = *fe_state;

    /* Update stats */
    float new_fe_avg = bridge->stats.avg_logical_free_energy * 0.95f + total_fe * 0.05f;
    if (isfinite(new_fe_avg)) {
        bridge->stats.avg_logical_free_energy = new_fe_avg;
    }

    return 0;
}

int hypo_logic_get_fep_state(
    const hypo_logic_bridge_t* bridge,
    hypo_logic_fep_state_t* fe_state)
{
    if (!bridge || !fe_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_get_fep_state: bridge or fe_state is NULL");
        return -1;
    }

    *fe_state = bridge->fep_state;
    return 0;
}

/*=============================================================================
 * PREDICATE-DRIVE MAPPING
 *===========================================================================*/

int hypo_logic_register_predicate_map(
    hypo_logic_bridge_t* bridge,
    const hypo_logic_predicate_map_t* map)
{
    if (!bridge || !map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_register_predicate_map: bridge or map is NULL");
        return -1;
    }
    if (bridge->config.num_predicate_maps >= HYPO_LOGIC_MAX_PREDICATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logic_register_predicate_map: max predicates reached");
        return -1;
    }

    bridge->config.predicate_maps[bridge->config.num_predicate_maps] = *map;
    bridge->config.num_predicate_maps++;

    return 0;
}

int hypo_logic_auto_register_mappings(hypo_logic_bridge_t* bridge) {
    if (!bridge) return 0;

    int count = 0;

    /* Food predicates -> HUNGER */
    hypo_logic_predicate_map_t food_map = {
        .predicate_name = "food",
        .drive = HYPO_DRIVE_HUNGER,
        .relevance = 1.0f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };
    if (hypo_logic_register_predicate_map(bridge, &food_map) == 0) count++;

    hypo_logic_predicate_map_t eat_map = {
        .predicate_name = "eat",
        .drive = HYPO_DRIVE_HUNGER,
        .relevance = 0.9f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };
    if (hypo_logic_register_predicate_map(bridge, &eat_map) == 0) count++;

    /* Water predicates -> THIRST */
    hypo_logic_predicate_map_t water_map = {
        .predicate_name = "water",
        .drive = HYPO_DRIVE_THIRST,
        .relevance = 1.0f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };
    if (hypo_logic_register_predicate_map(bridge, &water_map) == 0) count++;

    hypo_logic_predicate_map_t drink_map = {
        .predicate_name = "drink",
        .drive = HYPO_DRIVE_THIRST,
        .relevance = 0.9f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };
    if (hypo_logic_register_predicate_map(bridge, &drink_map) == 0) count++;

    /* Safety predicates -> SAFETY */
    hypo_logic_predicate_map_t safe_map = {
        .predicate_name = "safe",
        .drive = HYPO_DRIVE_SAFETY,
        .relevance = 1.0f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };
    if (hypo_logic_register_predicate_map(bridge, &safe_map) == 0) count++;

    hypo_logic_predicate_map_t threat_map = {
        .predicate_name = "threat",
        .drive = HYPO_DRIVE_SAFETY,
        .relevance = 1.0f,
        .valence = -1.0f,
        .is_goal_predicate = false
    };
    if (hypo_logic_register_predicate_map(bridge, &threat_map) == 0) count++;

    hypo_logic_predicate_map_t danger_map = {
        .predicate_name = "danger",
        .drive = HYPO_DRIVE_SAFETY,
        .relevance = 0.9f,
        .valence = -1.0f,
        .is_goal_predicate = false
    };
    if (hypo_logic_register_predicate_map(bridge, &danger_map) == 0) count++;

    /* Social predicates -> SOCIAL */
    hypo_logic_predicate_map_t friend_map = {
        .predicate_name = "friend",
        .drive = HYPO_DRIVE_SOCIAL,
        .relevance = 0.9f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };
    if (hypo_logic_register_predicate_map(bridge, &friend_map) == 0) count++;

    hypo_logic_predicate_map_t ally_map = {
        .predicate_name = "ally",
        .drive = HYPO_DRIVE_SOCIAL,
        .relevance = 0.8f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };
    if (hypo_logic_register_predicate_map(bridge, &ally_map) == 0) count++;

    /* Knowledge predicates -> CURIOSITY */
    hypo_logic_predicate_map_t know_map = {
        .predicate_name = "know",
        .drive = HYPO_DRIVE_CURIOSITY,
        .relevance = 0.8f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };
    if (hypo_logic_register_predicate_map(bridge, &know_map) == 0) count++;

    hypo_logic_predicate_map_t discover_map = {
        .predicate_name = "discover",
        .drive = HYPO_DRIVE_CURIOSITY,
        .relevance = 1.0f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };
    if (hypo_logic_register_predicate_map(bridge, &discover_map) == 0) count++;

    return count;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

static nimcp_error_t logic_handle_conclusion(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t promise, void* user_data)
{
    (void)promise;
    hypo_logic_bridge_t* bridge = (hypo_logic_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_INVALID_PARAM;

    /* Message should contain a conclusion structure */
    if (msg_size < sizeof(logic_clause_t*)) return NIMCP_ERROR_INVALID_PARAM;

    logic_clause_t* conclusion = *(logic_clause_t**)msg;
    hypo_logic_process_conclusion(bridge, conclusion, 0.8f);

    bridge->stats.bio_messages_received++;
    return NIMCP_SUCCESS;
}

bool hypo_logic_bridge_register_bio(hypo_logic_bridge_t* bridge, bool use_kg_wiring) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_bridge_register_bio: bridge is NULL");
        return false;
    }
    if (bridge->bio_registered) return true;

    (void)use_kg_wiring;

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        nimcp_log(LOG_LEVEL_WARN, "hypo_logic_bridge: bio-async router not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "hypo_logic_bridge_register_bio: bio_router_is_initialized is NULL");
        return false;
    }

    bio_module_info_t info = {
        .module_id = HYPO_LOGIC_MODULE_ID,
        .module_name = "hypothalamus_logic_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        nimcp_log(LOG_LEVEL_WARN, "hypo_logic_bridge: bio-async not available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_bridge_register_bio: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handler for logic circuit complete messages (conclusion events) */
    nimcp_error_t err = bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_LOGIC_CIRCUIT_COMPLETE, logic_handle_conclusion);
    if (err != NIMCP_SUCCESS) {
        nimcp_log(LOG_LEVEL_WARN, "hypo_logic_bridge: failed to register handler");
    }

    bridge->bio_registered = true;
    nimcp_log(LOG_LEVEL_INFO, "hypo_logic_bridge: registered with bio-async");

    return true;
}

int hypo_logic_broadcast_modulation(hypo_logic_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logic_broadcast_modulation: bridge is NULL or not bio-registered");
        return -1;
    }

    /* Broadcast modulation state to all modules */
    nimcp_error_t err = bio_router_broadcast(bridge->bio_ctx,
                                              &bridge->modulation,
                                              sizeof(bridge->modulation));
    if (err != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logic_broadcast_modulation: validation failed");
        return -1;
    }

    bridge->stats.bio_messages_sent++;
    return 0;
}

int hypo_logic_broadcast_conclusion_impact(
    hypo_logic_bridge_t* bridge,
    const logic_clause_t* conclusion,
    float impact)
{
    if (!bridge || !bridge->bio_registered || !conclusion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logic_broadcast_conclusion_impact: invalid parameters");
        return -1;
    }

    struct {
        const logic_clause_t* conclusion;
        float impact;
    } msg = { conclusion, impact };

    nimcp_error_t err = bio_router_broadcast(bridge->bio_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logic_broadcast_conclusion_impact: validation failed");
        return -1;
    }

    bridge->stats.bio_messages_sent++;
    return 0;
}

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

int hypo_logic_get_stats(
    const hypo_logic_bridge_t* bridge,
    hypo_logic_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logic_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int hypo_logic_reset_stats(hypo_logic_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

void hypo_logic_print_summary(const hypo_logic_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_log(LOG_LEVEL_INFO, "=== Hypothalamus-Logic Bridge Summary ===");
    nimcp_log(LOG_LEVEL_INFO, "Priority drive: %s (weight=%.2f)",
              hypo_drive_type_string(bridge->modulation.priority_drive),
              bridge->modulation.priority_weight);
    nimcp_log(LOG_LEVEL_INFO, "Inference depth: %u, Threshold: %.2f",
              bridge->modulation.max_inference_depth,
              bridge->modulation.proof_threshold);
    nimcp_log(LOG_LEVEL_INFO, "Reasoning capacity: %.2f, Effort willingness: %.2f",
              bridge->modulation.reasoning_capacity,
              bridge->modulation.effort_willingness);
    nimcp_log(LOG_LEVEL_INFO, "Active goals: %u", bridge->num_goals);
    nimcp_log(LOG_LEVEL_INFO, "Logical FE: %.3f, Prediction error: %.3f",
              bridge->fep_state.logical_free_energy,
              bridge->fep_state.prediction_error);
    nimcp_log(LOG_LEVEL_INFO, "Conclusions processed: %lu, Goals achieved: %lu",
              bridge->stats.conclusions_processed, bridge->stats.goals_achieved);
}

/*=============================================================================
 * STRING UTILITIES
 *===========================================================================*/

const char* hypo_conclusion_type_name(hypo_conclusion_type_t type) {
    switch (type) {
        case HYPO_CONCL_RESOURCE_AVAILABLE:   return "resource_available";
        case HYPO_CONCL_RESOURCE_UNAVAILABLE: return "resource_unavailable";
        case HYPO_CONCL_THREAT_PRESENT:       return "threat_present";
        case HYPO_CONCL_THREAT_ABSENT:        return "threat_absent";
        case HYPO_CONCL_GOAL_ACHIEVED:        return "goal_achieved";
        case HYPO_CONCL_GOAL_IMPOSSIBLE:      return "goal_impossible";
        case HYPO_CONCL_OPPORTUNITY:          return "opportunity";
        case HYPO_CONCL_PREDICTION_CONFIRMED: return "prediction_confirmed";
        case HYPO_CONCL_PREDICTION_VIOLATED:  return "prediction_violated";
        case HYPO_CONCL_NEUTRAL:              return "neutral";
        default:                              return "unknown";
    }
}

const char* hypo_predicate_category_name(hypo_predicate_category_t category) {
    switch (category) {
        case HYPO_PRED_CAT_FOOD:        return "food";
        case HYPO_PRED_CAT_WATER:       return "water";
        case HYPO_PRED_CAT_THREAT:      return "threat";
        case HYPO_PRED_CAT_SAFETY:      return "safety";
        case HYPO_PRED_CAT_SOCIAL:      return "social";
        case HYPO_PRED_CAT_KNOWLEDGE:   return "knowledge";
        case HYPO_PRED_CAT_TEMPERATURE: return "temperature";
        case HYPO_PRED_CAT_REST:        return "rest";
        case HYPO_PRED_CAT_ACHIEVEMENT: return "achievement";
        case HYPO_PRED_CAT_AUTONOMY:    return "autonomy";
        case HYPO_PRED_CAT_NEUTRAL:     return "neutral";
        default:                        return "unknown";
    }
}
