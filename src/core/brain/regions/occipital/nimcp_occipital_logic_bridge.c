/**
 * @file nimcp_occipital_logic_bridge.c
 * @brief Implementation of Occipital-Logic bridge
 *
 * WHAT: Connects visual perception to neural logic for visual reasoning
 * WHY: Enable perception-to-logic conversion (visual predicates, scene logic)
 * HOW: Extracts visual features and converts them to logical propositions
 *
 * BIOLOGICAL BASIS:
 * - Inferior temporal cortex performs object categorization
 * - Prefrontal cortex integrates visual info for logical reasoning
 * - Parietal cortex encodes spatial relations
 *
 * @version Phase O1: Occipital Logic Integration
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/occipital/nimcp_occipital_logic_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(occipital_logic_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "OCCIPITAL_LOGIC_BRIDGE"


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define LOGIC_BRIDGE_LOG_MODULE "OCC_LOGIC"

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define MAX_PREDICATES 256
#define MAX_INFERENCES 64
#define MAX_OBJECTS 32
#define MAX_INFERENCE_DEPTH 8

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Detected object for predicate grounding
 */
typedef struct {
    uint32_t id;                     /**< Object ID */
    float x, y;                      /**< Center position (normalized) */
    float width, height;             /**< Bounding box size */
    float velocity_x, velocity_y;    /**< Motion vector */
    uint32_t category;               /**< Object category */
    uint32_t color;                  /**< Dominant color */
    uint32_t shape;                  /**< Shape category */
    float salience;                  /**< Visual salience */
    float confidence;                /**< Detection confidence */
    bool is_moving;                  /**< Motion detected */
    bool is_occluded;                /**< Partially occluded */
} detected_object_t;

/**
 * @brief Inference rule
 */
typedef struct {
    visual_predicate_type_t conclusion_type;
    visual_predicate_type_t premise1_type;
    visual_predicate_type_t premise2_type; /* PRED_COUNT if single premise */
    bool swap_objects;                      /* Swap a/b in conclusion */
} inference_rule_t;

/**
 * @brief Internal bridge structure
 */
struct occipital_logic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    occipital_logic_config_t config;

    /* Connected modules */
    occipital_adapter_t* occipital;
    brain_t brain;
    neural_logic_network_t* logic_network;
    bio_router_t router;

    /* Module ID for bio-async */
    uint32_t module_id;

    /* Detected objects */
    detected_object_t objects[MAX_OBJECTS];
    uint32_t object_count;

    /* Active predicates */
    visual_predicate_t predicates[MAX_PREDICATES];
    uint32_t predicate_count;

    /* Inference results */
    logic_inference_result_t inferences[MAX_INFERENCES];
    uint32_t inference_count;

    /* Current effects */
    occipital_logic_effects_t effects;

    /* Statistics */
    occipital_logic_stats_t stats;

    /* Timing */
    uint64_t last_update_us;
    uint64_t creation_time_us;
};

/*=============================================================================
 * INFERENCE RULES (Simplified first-order logic)
 *===========================================================================*/

static const inference_rule_t INFERENCE_RULES[] = {
    /* Transitivity: ABOVE(a,b) AND ABOVE(b,c) -> ABOVE(a,c) */
    {PRED_ABOVE, PRED_ABOVE, PRED_ABOVE, false},
    {PRED_BELOW, PRED_BELOW, PRED_BELOW, false},
    {PRED_LEFT_OF, PRED_LEFT_OF, PRED_LEFT_OF, false},
    {PRED_RIGHT_OF, PRED_RIGHT_OF, PRED_RIGHT_OF, false},

    /* Inverse: ABOVE(a,b) -> BELOW(b,a) */
    {PRED_BELOW, PRED_ABOVE, PRED_COUNT, true},
    {PRED_ABOVE, PRED_BELOW, PRED_COUNT, true},
    {PRED_RIGHT_OF, PRED_LEFT_OF, PRED_COUNT, true},
    {PRED_LEFT_OF, PRED_RIGHT_OF, PRED_COUNT, true},

    /* Occlusion implies NEAR */
    {PRED_NEAR, PRED_OCCLUDES, PRED_COUNT, false},

    /* Same category + same color = higher similarity */
    /* (handled specially, not as simple rule) */
};

static const uint32_t NUM_INFERENCE_RULES =
    sizeof(INFERENCE_RULES) / sizeof(INFERENCE_RULES[0]);

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Check if predicate already exists
 */
static bool predicate_exists(const occipital_logic_bridge_t* bridge,
                             visual_predicate_type_t type,
                             uint32_t obj_a, uint32_t obj_b) {
    for (uint32_t i = 0; i < bridge->predicate_count; i++) {
        const visual_predicate_t* p = &bridge->predicates[i];
        if (p->type == type &&
            p->object_a == obj_a &&
            p->object_b == obj_b) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Add predicate to active list
 */
static int add_predicate(occipital_logic_bridge_t* bridge,
                         const visual_predicate_t* pred) {
    if (bridge->predicate_count >= bridge->config.max_predicates) {
        /* Predicate capacity reached — normal operational limit, not immune-worthy */
        return -1;
    }

    /* Check for duplicate */
    if (predicate_exists(bridge, pred->type, pred->object_a, pred->object_b)) {
        /* Update existing */
        for (uint32_t i = 0; i < bridge->predicate_count; i++) {
            visual_predicate_t* p = &bridge->predicates[i];
            if (p->type == pred->type &&
                p->object_a == pred->object_a &&
                p->object_b == pred->object_b) {
                p->confidence = pred->confidence;
                p->truth_value = pred->truth_value;
                p->timestamp_us = pred->timestamp_us;
                return 0;
            }
        }
    }

    bridge->predicates[bridge->predicate_count++] = *pred;
    bridge->stats.predicates_grounded++;

    return 0;
}

/**
 * @brief Ground unary predicates for single object
 */
static void ground_unary_predicates(occipital_logic_bridge_t* bridge,
                                    const detected_object_t* obj) {
    uint64_t now = get_time_us();

    /* OBJECT_PRESENT */
    visual_predicate_t pred = {
        .type = PRED_OBJECT_PRESENT,
        .object_a = obj->id,
        .object_b = 0,
        .parameter = obj->category,
        .confidence = obj->confidence,
        .truth_value = obj->confidence,
        .timestamp_us = now
    };
    add_predicate(bridge, &pred);

    /* IS_MOVING */
    if (obj->is_moving) {
        pred.type = PRED_IS_MOVING;
        float velocity = sqrtf(obj->velocity_x * obj->velocity_x +
                               obj->velocity_y * obj->velocity_y);
        pred.truth_value = nimcp_clampf(velocity * 5.0f, 0.0f, 1.0f);
        add_predicate(bridge, &pred);
    }

    /* IS_OCCLUDED */
    if (obj->is_occluded) {
        pred.type = PRED_IS_OCCLUDED;
        pred.truth_value = 0.8f; /* Default occlusion confidence */
        add_predicate(bridge, &pred);
    }

    /* IN_FOVEA - check if near center */
    float dist_from_center = sqrtf(
        (obj->x - 0.5f) * (obj->x - 0.5f) +
        (obj->y - 0.5f) * (obj->y - 0.5f)
    );
    if (dist_from_center < 0.2f) {
        pred.type = PRED_IN_FOVEA;
        pred.truth_value = 1.0f - (dist_from_center / 0.2f);
        add_predicate(bridge, &pred);
    }

    /* HAS_COLOR */
    pred.type = PRED_HAS_COLOR;
    pred.parameter = obj->color;
    pred.truth_value = obj->confidence;
    add_predicate(bridge, &pred);

    /* HAS_SHAPE */
    pred.type = PRED_HAS_SHAPE;
    pred.parameter = obj->shape;
    pred.truth_value = obj->confidence * 0.9f;
    add_predicate(bridge, &pred);

    /* IS_SALIENT */
    if (obj->salience > 0.5f) {
        pred.type = PRED_IS_SALIENT;
        pred.truth_value = obj->salience;
        add_predicate(bridge, &pred);
    }
}

/**
 * @brief Ground binary predicates between two objects
 */
static void ground_binary_predicates(occipital_logic_bridge_t* bridge,
                                     const detected_object_t* a,
                                     const detected_object_t* b) {
    uint64_t now = get_time_us();
    float threshold = bridge->config.spatial_threshold;

    visual_predicate_t pred = {
        .object_a = a->id,
        .object_b = b->id,
        .parameter = 0,
        .timestamp_us = now
    };

    /* Spatial relations */
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dist = sqrtf(dx * dx + dy * dy);

    /* ABOVE/BELOW (Y increases downward typically) */
    if (fabsf(dy) > threshold && fabsf(dy) > fabsf(dx)) {
        if (dy > 0) {
            pred.type = PRED_ABOVE;
            pred.confidence = nimcp_clampf(fabsf(dy) * 2.0f, 0.5f, 1.0f);
            pred.truth_value = pred.confidence;
            add_predicate(bridge, &pred);
        } else {
            pred.type = PRED_BELOW;
            pred.confidence = nimcp_clampf(fabsf(dy) * 2.0f, 0.5f, 1.0f);
            pred.truth_value = pred.confidence;
            add_predicate(bridge, &pred);
        }
    }

    /* LEFT_OF/RIGHT_OF */
    if (fabsf(dx) > threshold && fabsf(dx) > fabsf(dy)) {
        if (dx > 0) {
            pred.type = PRED_LEFT_OF;
            pred.confidence = nimcp_clampf(fabsf(dx) * 2.0f, 0.5f, 1.0f);
            pred.truth_value = pred.confidence;
            add_predicate(bridge, &pred);
        } else {
            pred.type = PRED_RIGHT_OF;
            pred.confidence = nimcp_clampf(fabsf(dx) * 2.0f, 0.5f, 1.0f);
            pred.truth_value = pred.confidence;
            add_predicate(bridge, &pred);
        }
    }

    /* NEAR */
    float near_threshold = 0.2f;
    if (dist < near_threshold) {
        pred.type = PRED_NEAR;
        pred.confidence = 1.0f - (dist / near_threshold);
        pred.truth_value = pred.confidence;
        add_predicate(bridge, &pred);
    }

    /* SAME_CATEGORY */
    if (a->category == b->category && a->category > 0) {
        pred.type = PRED_SAME_CATEGORY;
        pred.confidence = a->confidence * b->confidence;
        pred.truth_value = 1.0f;
        add_predicate(bridge, &pred);
    }

    /* SAME_COLOR */
    if (a->color == b->color && a->color > 0) {
        pred.type = PRED_SAME_COLOR;
        pred.confidence = a->confidence * b->confidence;
        pred.truth_value = 1.0f;
        add_predicate(bridge, &pred);
    }

    /* MOVING_TOWARD */
    if (a->is_moving) {
        /* Check if A's velocity points toward B */
        float dot = a->velocity_x * dx + a->velocity_y * dy;
        if (dot > 0 && dist > 0.01f) {
            float a_speed = sqrtf(a->velocity_x * a->velocity_x +
                                  a->velocity_y * a->velocity_y);
            float alignment = dot / (a_speed * dist);
            if (alignment > 0.5f) {
                pred.type = PRED_MOVING_TOWARD;
                pred.confidence = alignment;
                pred.truth_value = alignment;
                add_predicate(bridge, &pred);
            }
        }
    }

    /* OCCLUDES - check bounding box overlap and depth */
    float overlap_x = fminf(a->x + a->width/2, b->x + b->width/2) -
                      fmaxf(a->x - a->width/2, b->x - b->width/2);
    float overlap_y = fminf(a->y + a->height/2, b->y + b->height/2) -
                      fmaxf(a->y - a->height/2, b->y - b->height/2);

    if (overlap_x > 0 && overlap_y > 0) {
        /* There's overlap, larger object likely occludes */
        float area_a = a->width * a->height;
        float area_b = b->width * b->height;
        if (area_a > area_b * 1.2f) {
            pred.type = PRED_OCCLUDES;
            float overlap_area = overlap_x * overlap_y;
            pred.confidence = nimcp_clampf(overlap_area / (area_b + 0.01f), 0.0f, 1.0f);
            pred.truth_value = pred.confidence;
            add_predicate(bridge, &pred);
        }
    }
}

/**
 * @brief Ground scene-level predicates
 */
static void ground_scene_predicates(occipital_logic_bridge_t* bridge) {
    uint64_t now = get_time_us();

    visual_predicate_t pred = {
        .object_a = 0,
        .object_b = 0,
        .parameter = 0,
        .timestamp_us = now
    };

    /* SCENE_CROWDED */
    if (bridge->object_count > 5) {
        pred.type = PRED_SCENE_CROWDED;
        pred.confidence = nimcp_clampf((float)bridge->object_count / 10.0f, 0.0f, 1.0f);
        pred.truth_value = pred.confidence;
        add_predicate(bridge, &pred);
    }

    /* SCENE_MOVING */
    uint32_t moving_count = 0;
    for (uint32_t i = 0; i < bridge->object_count; i++) {
        if (bridge->objects[i].is_moving) {
            moving_count++;
        }
    }
    if (moving_count > 0) {
        pred.type = PRED_SCENE_MOVING;
        pred.confidence = (float)moving_count / (float)bridge->object_count;
        pred.truth_value = pred.confidence;
        add_predicate(bridge, &pred);
    }

    /* SCENE_COHERENT - based on average detection confidence */
    float total_conf = 0.0f;
    for (uint32_t i = 0; i < bridge->object_count; i++) {
        total_conf += bridge->objects[i].confidence;
    }
    if (bridge->object_count > 0) {
        pred.type = PRED_SCENE_COHERENT;
        pred.confidence = total_conf / (float)bridge->object_count;
        pred.truth_value = pred.confidence;
        add_predicate(bridge, &pred);
    }
}

/**
 * @brief Apply single inference rule
 */
static int apply_inference_rule(occipital_logic_bridge_t* bridge,
                                const inference_rule_t* rule) {
    int new_inferences = 0;

    /* Single-premise rules */
    if (rule->premise2_type == PRED_COUNT) {
        for (uint32_t i = 0; i < bridge->predicate_count; i++) {
            const visual_predicate_t* p1 = &bridge->predicates[i];
            if (p1->type != rule->premise1_type) continue;

            uint32_t obj_a = rule->swap_objects ? p1->object_b : p1->object_a;
            uint32_t obj_b = rule->swap_objects ? p1->object_a : p1->object_b;

            if (!predicate_exists(bridge, rule->conclusion_type, obj_a, obj_b)) {
                visual_predicate_t conclusion = {
                    .type = rule->conclusion_type,
                    .object_a = obj_a,
                    .object_b = obj_b,
                    .parameter = p1->parameter,
                    .confidence = p1->confidence * 0.9f,
                    .truth_value = p1->truth_value * 0.9f,
                    .timestamp_us = get_time_us()
                };

                if (add_predicate(bridge, &conclusion) == 0) {
                    /* Record inference */
                    if (bridge->inference_count < MAX_INFERENCES) {
                        logic_inference_result_t* inf =
                            &bridge->inferences[bridge->inference_count++];
                        inf->conclusion = conclusion;
                        inf->premise_count = 1;
                        inf->premise_ids[0] = i;
                        inf->inference_confidence = conclusion.confidence;
                        inf->inference_depth = 1;
                    }
                    new_inferences++;
                    bridge->stats.inferences_performed++;
                }
            }
        }
        return new_inferences;
    }

    /* Two-premise rules (transitivity) */
    for (uint32_t i = 0; i < bridge->predicate_count; i++) {
        const visual_predicate_t* p1 = &bridge->predicates[i];
        if (p1->type != rule->premise1_type) continue;

        for (uint32_t j = 0; j < bridge->predicate_count; j++) {
            if (i == j) continue;
            const visual_predicate_t* p2 = &bridge->predicates[j];
            if (p2->type != rule->premise2_type) continue;

            /* Check transitivity: p1.b == p2.a */
            if (p1->object_b != p2->object_a) continue;

            /* Conclusion: relation between p1.a and p2.b */
            if (!predicate_exists(bridge, rule->conclusion_type,
                                  p1->object_a, p2->object_b)) {
                visual_predicate_t conclusion = {
                    .type = rule->conclusion_type,
                    .object_a = p1->object_a,
                    .object_b = p2->object_b,
                    .parameter = 0,
                    .confidence = p1->confidence * p2->confidence * 0.8f,
                    .truth_value = fminf(p1->truth_value, p2->truth_value) * 0.9f,
                    .timestamp_us = get_time_us()
                };

                if (conclusion.confidence >= bridge->config.truth_threshold) {
                    if (add_predicate(bridge, &conclusion) == 0) {
                        if (bridge->inference_count < MAX_INFERENCES) {
                            logic_inference_result_t* inf =
                                &bridge->inferences[bridge->inference_count++];
                            inf->conclusion = conclusion;
                            inf->premise_count = 2;
                            inf->premise_ids[0] = i;
                            inf->premise_ids[1] = j;
                            inf->inference_confidence = conclusion.confidence;
                            inf->inference_depth = 2;
                        }
                        new_inferences++;
                        bridge->stats.inferences_performed++;
                    }
                }
            }
        }
    }

    return new_inferences;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/* Note: Bio-async message handlers will be implemented when
 * the full bio-async infrastructure is integrated */

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

occipital_logic_config_t occipital_logic_default_config(void) {
    occipital_logic_config_t config = {
        .enable_unary_predicates = true,
        .enable_binary_predicates = true,
        .enable_scene_predicates = true,

        .detection_threshold = 0.3f,
        .truth_threshold = 0.5f,
        .spatial_threshold = 0.05f,

        .mode = OCC_LOGIC_MODE_FORWARD_CHAIN,
        .max_predicates = MAX_PREDICATES,
        .max_inference_depth = 4,
        .use_fuzzy_logic = true,

        .enable_bio_async = true
    };

    return config;
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

occipital_logic_bridge_t* occipital_logic_bridge_create(
    occipital_adapter_t* occipital,
    const occipital_logic_config_t* config) {

    if (!occipital) {
        LOG_ERROR(LOGIC_BRIDGE_LOG_MODULE, "NULL occipital adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital is NULL");
        return NULL;
    }

    occipital_logic_bridge_t* bridge =
        (occipital_logic_bridge_t*)nimcp_malloc(sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR(LOGIC_BRIDGE_LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_logic_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    if (bridge_base_init(&bridge->base, 0, "occipital_logic") != 0) { nimcp_free(bridge); return NULL; }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = occipital_logic_default_config();
    }

    bridge->occipital = occipital;

    bridge->creation_time_us = get_time_us();
    bridge->last_update_us = bridge->creation_time_us;

    bridge->module_id = BIO_MODULE_OCCIPITAL + 0x30;

    LOG_INFO(LOGIC_BRIDGE_LOG_MODULE, "Logic bridge created");

    return bridge;
}

void occipital_logic_bridge_destroy(occipital_logic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "occipital_logic");

    LOG_INFO(LOGIC_BRIDGE_LOG_MODULE, "Destroying logic bridge");

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int occipital_logic_bridge_reset(occipital_logic_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    memset(bridge->objects, 0, sizeof(bridge->objects));
    bridge->object_count = 0;

    memset(bridge->predicates, 0, sizeof(bridge->predicates));
    bridge->predicate_count = 0;

    memset(bridge->inferences, 0, sizeof(bridge->inferences));
    bridge->inference_count = 0;

    memset(&bridge->effects, 0, sizeof(bridge->effects));

    bridge->last_update_us = get_time_us();

    LOG_DEBUG(LOGIC_BRIDGE_LOG_MODULE, "Bridge reset");

    return 0;
}

/*=============================================================================
 * CONNECTION API
 *===========================================================================*/

int occipital_logic_connect_brain(
    occipital_logic_bridge_t* bridge,
    brain_t brain) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->brain = brain;

    LOG_INFO(LOGIC_BRIDGE_LOG_MODULE, "Connected to brain");

    return 0;
}

int occipital_logic_connect_network(
    occipital_logic_bridge_t* bridge,
    neural_logic_network_t* network) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->logic_network = network;

    LOG_INFO(LOGIC_BRIDGE_LOG_MODULE, "Connected to neural logic network");

    return 0;
}

int occipital_logic_bridge_register_bio_async(
    occipital_logic_bridge_t* bridge,
    struct bio_router_struct* router) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->router = router;

    if (router) {
        LOG_INFO(LOGIC_BRIDGE_LOG_MODULE, "Registered with bio-async router");
    }

    return 0;
}

/*=============================================================================
 * PREDICATE API
 *===========================================================================*/

int occipital_logic_ground_predicates(occipital_logic_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Clear old predicates */
    bridge->predicate_count = 0;
    bridge->inference_count = 0;

    /* In a real implementation, we would query the occipital adapter
       for detected objects. For now, simulate with test objects. */

    /* Simulate some detected objects for testing */
    if (bridge->object_count == 0) {
        /* Add test objects */
        bridge->objects[0] = (detected_object_t){
            .id = 1, .x = 0.3f, .y = 0.3f, .width = 0.1f, .height = 0.1f,
            .category = 1, .color = 2, .shape = 1,
            .salience = 0.8f, .confidence = 0.9f,
            .is_moving = false, .is_occluded = false
        };
        bridge->objects[1] = (detected_object_t){
            .id = 2, .x = 0.5f, .y = 0.5f, .width = 0.15f, .height = 0.15f,
            .category = 2, .color = 3, .shape = 2,
            .salience = 0.9f, .confidence = 0.85f,
            .is_moving = true, .velocity_x = 0.1f, .velocity_y = 0.0f,
            .is_occluded = false
        };
        bridge->objects[2] = (detected_object_t){
            .id = 3, .x = 0.7f, .y = 0.6f, .width = 0.08f, .height = 0.08f,
            .category = 1, .color = 2, .shape = 1,
            .salience = 0.6f, .confidence = 0.75f,
            .is_moving = false, .is_occluded = true
        };
        bridge->object_count = 3;
    }

    /* Ground unary predicates */
    if (bridge->config.enable_unary_predicates) {
        for (uint32_t i = 0; i < bridge->object_count; i++) {
            ground_unary_predicates(bridge, &bridge->objects[i]);
        }
    }

    /* Ground binary predicates */
    if (bridge->config.enable_binary_predicates) {
        for (uint32_t i = 0; i < bridge->object_count; i++) {
            for (uint32_t j = i + 1; j < bridge->object_count; j++) {
                ground_binary_predicates(bridge, &bridge->objects[i],
                                         &bridge->objects[j]);
            }
        }
    }

    /* Ground scene predicates */
    if (bridge->config.enable_scene_predicates) {
        ground_scene_predicates(bridge);
    }

    return (int)bridge->predicate_count;
}

int occipital_logic_assert_predicate(
    occipital_logic_bridge_t* bridge,
    const visual_predicate_t* predicate) {

    if (!bridge || !predicate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_logic_assert_predicate: required parameter is NULL");
        return -1;
    }

    int result = add_predicate(bridge, predicate);
    if (result == 0) {
        bridge->stats.predicates_asserted++;
    }

    return result;
}

int occipital_logic_query_predicate(
    occipital_logic_bridge_t* bridge,
    visual_predicate_type_t type,
    uint32_t object_a,
    uint32_t object_b,
    float* truth_value,
    float* confidence) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->stats.queries_processed++;

    for (uint32_t i = 0; i < bridge->predicate_count; i++) {
        const visual_predicate_t* p = &bridge->predicates[i];
        if (p->type == type &&
            p->object_a == object_a &&
            p->object_b == object_b) {
            if (truth_value) *truth_value = p->truth_value;
            if (confidence) *confidence = p->confidence;
            return 0;
        }
    }

    /* Not found - this is a normal query result, not an error */
    if (truth_value) *truth_value = 0.0f;
    if (confidence) *confidence = 0.0f;

    return -1;
}

int occipital_logic_get_predicates(
    const occipital_logic_bridge_t* bridge,
    visual_predicate_t* predicates,
    uint32_t max_predicates,
    uint32_t* count) {

    if (!bridge || !predicates || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_logic_get_predicates: required parameter is NULL");
        return -1;
    }

    uint32_t to_copy = bridge->predicate_count;
    if (to_copy > max_predicates) to_copy = max_predicates;

    memcpy(predicates, bridge->predicates, to_copy * sizeof(visual_predicate_t));
    *count = to_copy;

    return 0;
}

/*=============================================================================
 * INFERENCE API
 *===========================================================================*/

int occipital_logic_run_inference(occipital_logic_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    if (bridge->config.mode == OCC_LOGIC_MODE_GROUND_ONLY) {
        return 0;
    }

    int total_new = 0;

    /* Run forward chaining */
    for (uint32_t depth = 0; depth < bridge->config.max_inference_depth; depth++) {
        int new_this_round = 0;

        for (uint32_t r = 0; r < NUM_INFERENCE_RULES; r++) {
            new_this_round += apply_inference_rule(bridge, &INFERENCE_RULES[r]);
        }

        total_new += new_this_round;

        if (new_this_round == 0) {
            break; /* Fixed point reached */
        }
    }

    return total_new;
}

int occipital_logic_get_inferences(
    const occipital_logic_bridge_t* bridge,
    logic_inference_result_t* results,
    uint32_t max_results,
    uint32_t* count) {

    if (!bridge || !results || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_logic_get_inferences: required parameter is NULL");
        return -1;
    }

    uint32_t to_copy = bridge->inference_count;
    if (to_copy > max_results) to_copy = max_results;

    memcpy(results, bridge->inferences,
           to_copy * sizeof(logic_inference_result_t));
    *count = to_copy;

    return 0;
}

int occipital_logic_prove_goal(
    occipital_logic_bridge_t* bridge,
    const visual_predicate_t* goal,
    bool* provable,
    float* confidence) {

    if (!bridge || !goal || !provable) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_logic_prove_goal: required parameter is NULL");
        return -1;
    }

    /* Check if goal already exists */
    for (uint32_t i = 0; i < bridge->predicate_count; i++) {
        const visual_predicate_t* p = &bridge->predicates[i];
        if (p->type == goal->type &&
            p->object_a == goal->object_a &&
            p->object_b == goal->object_b) {
            *provable = (p->truth_value >= bridge->config.truth_threshold);
            if (confidence) *confidence = p->confidence;
            return 0;
        }
    }

    /* Run inference and check again */
    occipital_logic_run_inference(bridge);

    for (uint32_t i = 0; i < bridge->predicate_count; i++) {
        const visual_predicate_t* p = &bridge->predicates[i];
        if (p->type == goal->type &&
            p->object_a == goal->object_a &&
            p->object_b == goal->object_b) {
            *provable = (p->truth_value >= bridge->config.truth_threshold);
            if (confidence) *confidence = p->confidence;
            return 0;
        }
    }

    *provable = false;
    if (confidence) *confidence = 0.0f;

    return 0;
}

/*=============================================================================
 * PROCESSING API
 *===========================================================================*/

int occipital_logic_bridge_update(occipital_logic_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Update effects */
    bridge->effects.active_predicates = bridge->predicate_count;
    bridge->effects.inferences_made = bridge->inference_count;

    /* Calculate averages */
    float total_conf = 0.0f;
    float total_truth = 0.0f;
    for (uint32_t i = 0; i < bridge->predicate_count; i++) {
        total_conf += bridge->predicates[i].confidence;
        total_truth += bridge->predicates[i].truth_value;
    }

    if (bridge->predicate_count > 0) {
        bridge->effects.avg_predicate_confidence =
            total_conf / (float)bridge->predicate_count;
        bridge->effects.avg_truth_value =
            total_truth / (float)bridge->predicate_count;
    }

    /* Inference quality */
    if (bridge->inference_count > 0) {
        float total_inf_conf = 0.0f;
        for (uint32_t i = 0; i < bridge->inference_count; i++) {
            total_inf_conf += bridge->inferences[i].inference_confidence;
        }
        bridge->effects.inference_quality =
            total_inf_conf / (float)bridge->inference_count;
    }

    /* Logic network load */
    bridge->effects.logic_network_load =
        (float)bridge->predicate_count / (float)bridge->config.max_predicates;

    /* Reasoning coherence */
    bridge->effects.reasoning_coherence =
        bridge->effects.avg_truth_value *
        (1.0f - bridge->effects.logic_network_load * 0.2f);

    /* Update avg inference depth */
    if (bridge->inference_count > 0) {
        float total_depth = 0.0f;
        for (uint32_t i = 0; i < bridge->inference_count; i++) {
            total_depth += (float)bridge->inferences[i].inference_depth;
        }
        bridge->stats.avg_inference_depth =
            total_depth / (float)bridge->inference_count;
    }

    bridge->last_update_us = get_time_us();

    return 0;
}

int occipital_logic_bridge_get_effects(
    const occipital_logic_bridge_t* bridge,
    occipital_logic_effects_t* effects) {

    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_logic_bridge_get_effects: required parameter is NULL");
        return -1;
    }

    *effects = bridge->effects;

    return 0;
}

/*=============================================================================
 * QUERY API
 *===========================================================================*/

int occipital_logic_bridge_get_stats(
    const occipital_logic_bridge_t* bridge,
    occipital_logic_stats_t* stats) {

    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_logic_bridge_get_stats: required parameter is NULL");
        return -1;
    }

    *stats = bridge->stats;

    return 0;
}

void occipital_logic_bridge_reset_stats(occipital_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_logic_bridge_reset_stats: bridge is NULL");
        return;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

bool occipital_logic_is_brain_connected(const occipital_logic_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->brain != NULL;
}

int occipital_logic_bridge_get_config(
    const occipital_logic_bridge_t* bridge,
    occipital_logic_config_t* config) {

    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_logic_bridge_get_config: required parameter is NULL");
        return -1;
    }

    *config = bridge->config;

    return 0;
}
