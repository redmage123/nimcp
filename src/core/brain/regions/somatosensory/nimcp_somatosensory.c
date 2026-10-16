/**
 * @file nimcp_somatosensory.c
 * @brief Somatosensory Cortex Implementation
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 */

#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(somatosensory, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define SOMA_TOUCH_BUFFER_SIZE      64
#define SOMA_PAIN_BUFFER_SIZE       32
#define SOMA_ACTIVATION_DECAY       NIMCP_ELIGIBILITY_DECAY_DEFAULT
#define SOMA_MIN_ACTIVATION         0.001f

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static uint64_t soma_get_time_ms(void) {
    /* Platform-specific timing - simplified */
    static _Atomic uint64_t counter = 0;
    return counter++;
}

static float soma_get_default_magnification(body_segment_t segment) {
    switch (segment) {
        case BODY_SEG_THUMB_L:
        case BODY_SEG_THUMB_R:
            return 20.0f;
        case BODY_SEG_INDEX_L:
        case BODY_SEG_INDEX_R:
        case BODY_SEG_MIDDLE_L:
        case BODY_SEG_MIDDLE_R:
        case BODY_SEG_RING_L:
        case BODY_SEG_RING_R:
        case BODY_SEG_PINKY_L:
        case BODY_SEG_PINKY_R:
            return 15.0f;
        case BODY_SEG_LIPS:
            return 25.0f;
        case BODY_SEG_TONGUE:
            return 30.0f;
        case BODY_SEG_FACE:
            return 10.0f;
        case BODY_SEG_HAND_L:
        case BODY_SEG_HAND_R:
            return 8.0f;
        case BODY_SEG_GENITALS:
            return 12.0f;
        case BODY_SEG_FOOT_L:
        case BODY_SEG_FOOT_R:
            return 3.0f;
        case BODY_SEG_TOES_L:
        case BODY_SEG_TOES_R:
            return 5.0f;
        case BODY_SEG_FOREARM_L:
        case BODY_SEG_FOREARM_R:
        case BODY_SEG_UPPER_ARM_L:
        case BODY_SEG_UPPER_ARM_R:
            return 2.0f;
        default:
            return 1.0f;  /* Trunk, back, etc. */
    }
}

static float soma_get_default_two_point(body_segment_t segment) {
    switch (segment) {
        case BODY_SEG_TONGUE:
            return 1.5f;
        case BODY_SEG_THUMB_L:
        case BODY_SEG_THUMB_R:
        case BODY_SEG_INDEX_L:
        case BODY_SEG_INDEX_R:
        case BODY_SEG_MIDDLE_L:
        case BODY_SEG_MIDDLE_R:
        case BODY_SEG_RING_L:
        case BODY_SEG_RING_R:
        case BODY_SEG_PINKY_L:
        case BODY_SEG_PINKY_R:
            return 2.0f;
        case BODY_SEG_LIPS:
            return 4.0f;
        case BODY_SEG_HAND_L:
        case BODY_SEG_HAND_R:
            return 10.0f;
        case BODY_SEG_FOOT_L:
        case BODY_SEG_FOOT_R:
            return 15.0f;
        case BODY_SEG_FOREARM_L:
        case BODY_SEG_FOREARM_R:
            return 40.0f;
        case BODY_SEG_BACK:
        case BODY_SEG_CHEST:
        case BODY_SEG_ABDOMEN:
            return 60.0f;
        default:
            return 30.0f;
    }
}

static float soma_get_default_receptor_density(body_segment_t segment) {
    float magnification = soma_get_default_magnification(segment);
    return magnification * 5.0f;  /* Simplified: density proportional to magnification */
}

static int soma_init_body_map(nimcp_somatosensory_t* soma) {
    for (int i = 0; i < BODY_SEG_COUNT; i++) {
        soma_body_map_entry_t* entry = &soma->body_map[i];
        entry->segment = (body_segment_t)i;
        entry->cortical_area_size = (uint32_t)(soma_get_default_magnification((body_segment_t)i) * 100.0f);
        entry->receptor_density = soma_get_default_receptor_density((body_segment_t)i);
        entry->two_point_threshold = soma_get_default_two_point((body_segment_t)i);
        entry->activation_level = 0.0f;
        entry->sensitivity_modifier = 1.0f;
        entry->num_neurons = 0;
        entry->neuron_ids = NULL;
        entry->neighbors = NULL;
        entry->num_neighbors = 0;
        memset(entry->position_estimate, 0, sizeof(entry->position_estimate));
    }
    return 0;
}

static int soma_init_proprio_state(nimcp_somatosensory_t* soma) {
    for (int i = 0; i < BODY_SEG_COUNT; i++) {
        soma_proprio_state_t* state = &soma->proprio_state[i];
        state->segment = (body_segment_t)i;
        memset(state->position, 0, sizeof(state->position));
        memset(state->velocity, 0, sizeof(state->velocity));
        memset(state->acceleration, 0, sizeof(state->acceleration));
        state->muscle_tension = 0.0f;
        state->muscle_length = 1.0f;
        state->joint_angle = 0.0f;
        state->confidence = 0.0f;
        state->last_update = 0;
    }
    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

soma_config_t soma_default_config(void) {
    soma_config_t config = {
        .num_area_3a_neurons = SOMA_DEFAULT_AREA_3A_NEURONS,
        .num_area_3b_neurons = SOMA_DEFAULT_AREA_3B_NEURONS,
        .num_area_1_neurons = SOMA_DEFAULT_AREA_1_NEURONS,
        .num_area_2_neurons = SOMA_DEFAULT_AREA_2_NEURONS,
        .num_s2_neurons = SOMA_DEFAULT_S2_NEURONS,
        .max_receptors = SOMA_MAX_RECEPTORS,
        .pain_threshold = SOMA_PAIN_THRESHOLD,
        .adaptation_rate = 0.1f,
        .lateral_inhibition_strength = 0.3f,
        .receptive_field_plasticity = 0.01f,
        .enable_detailed_body_map = true,
        .enable_tool_use_extension = true,
        .enable_motor_efference_copy = true,
        .enable_prime_resonance = true,
        .enable_all_bridges = true,
        .gate_control_weight = 0.5f,
        .enable_descending_modulation = true
    };
    return config;
}

nimcp_somatosensory_t* soma_create(const soma_config_t* config) {
    soma_config_t default_config;
    if (!config) {
        default_config = soma_default_config();
        config = &default_config;
    }

    nimcp_somatosensory_t* soma = (nimcp_somatosensory_t*)nimcp_calloc(1, sizeof(nimcp_somatosensory_t));
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "soma_create: allocation failed");
        return NULL;
    }

    memcpy(&soma->config, config, sizeof(soma_config_t));
    soma->status = SOMA_STATUS_IDLE;
    soma->last_error = SOMA_ERROR_NONE;

    /* Allocate receptors */
    soma->receptors = (soma_receptor_t*)nimcp_calloc(config->max_receptors, sizeof(soma_receptor_t));
    if (!soma->receptors) {
        nimcp_free(soma);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "soma_create: soma->receptors is NULL");
        return NULL;
    }
    soma->num_receptors = 0;

    /* Allocate touch events buffer */
    soma->max_touch_events = SOMA_TOUCH_BUFFER_SIZE;
    soma->active_touch_events = (soma_touch_event_t*)nimcp_calloc(soma->max_touch_events, sizeof(soma_touch_event_t));
    if (!soma->active_touch_events) {
        nimcp_free(soma->receptors);
        nimcp_free(soma);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "soma_create: soma->active_touch_events is NULL");
        return NULL;
    }
    soma->num_active_touch = 0;

    /* Allocate pain events buffer */
    soma->max_pain_events = SOMA_PAIN_BUFFER_SIZE;
    soma->active_pain_events = (soma_pain_event_t*)nimcp_calloc(soma->max_pain_events, sizeof(soma_pain_event_t));
    if (!soma->active_pain_events) {
        nimcp_free(soma->active_touch_events);
        nimcp_free(soma->receptors);
        nimcp_free(soma);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "soma_create: soma->active_pain_events is NULL");
        return NULL;
    }
    soma->num_active_pain = 0;

    /* Allocate activation buffers */
    uint32_t total_neurons = config->num_area_3a_neurons + config->num_area_3b_neurons +
                             config->num_area_1_neurons + config->num_area_2_neurons +
                             config->num_s2_neurons;

    soma->area_3a_activation = (float*)nimcp_calloc(config->num_area_3a_neurons, sizeof(float));
    soma->area_3b_activation = (float*)nimcp_calloc(config->num_area_3b_neurons, sizeof(float));
    soma->area_1_activation = (float*)nimcp_calloc(config->num_area_1_neurons, sizeof(float));
    soma->area_2_activation = (float*)nimcp_calloc(config->num_area_2_neurons, sizeof(float));
    soma->s2_activation = (float*)nimcp_calloc(config->num_s2_neurons, sizeof(float));

    if (!soma->area_3a_activation || !soma->area_3b_activation ||
        !soma->area_1_activation || !soma->area_2_activation || !soma->s2_activation) {
        soma_destroy(soma);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "soma_create: activation buffer allocation failed");
        return NULL;
    }

    /* Initialize body map */
    soma_init_body_map(soma);

    /* Initialize proprioceptive state */
    soma_init_proprio_state(soma);

    /* Initialize bridges to not-initialized state */
    memset(&soma->prime_resonance_bridge, 0, sizeof(soma->prime_resonance_bridge));
    memset(&soma->immune_bridge, 0, sizeof(soma->immune_bridge));
    memset(&soma->bio_async_bridge, 0, sizeof(soma->bio_async_bridge));
    memset(&soma->brain_init_bridge, 0, sizeof(soma->brain_init_bridge));
    memset(&soma->security_bridge, 0, sizeof(soma->security_bridge));
    memset(&soma->logging_bridge, 0, sizeof(soma->logging_bridge));
    memset(&soma->cognitive_bridge, 0, sizeof(soma->cognitive_bridge));
    memset(&soma->training_bridge, 0, sizeof(soma->training_bridge));
    memset(&soma->omni_bridge, 0, sizeof(soma->omni_bridge));
    memset(&soma->hypothalamus_bridge, 0, sizeof(soma->hypothalamus_bridge));
    memset(&soma->substrate_bridge, 0, sizeof(soma->substrate_bridge));
    memset(&soma->thalamus_bridge, 0, sizeof(soma->thalamus_bridge));
    memset(&soma->motor_bridge, 0, sizeof(soma->motor_bridge));
    memset(&soma->parietal_bridge, 0, sizeof(soma->parietal_bridge));
    memset(&soma->snn_bridge, 0, sizeof(soma->snn_bridge));
    memset(&soma->plasticity_bridge, 0, sizeof(soma->plasticity_bridge));
    memset(&soma->portia_bridge, 0, sizeof(soma->portia_bridge));
    memset(&soma->dragonfly_bridge, 0, sizeof(soma->dragonfly_bridge));
    memset(&soma->perception_bridge, 0, sizeof(soma->perception_bridge));

    soma->creation_time = soma_get_time_ms();
    soma->last_update_time = soma->creation_time;
    soma->updates_processed = 0;
    soma->touch_events_total = 0;
    soma->pain_events_total = 0;

    soma->status = SOMA_STATUS_READY;
    return soma;
}

void soma_destroy(nimcp_somatosensory_t* soma) {
    if (!soma) return;

    /* Free activation buffers */
    if (soma->area_3a_activation) nimcp_free(soma->area_3a_activation);
    if (soma->area_3b_activation) nimcp_free(soma->area_3b_activation);
    if (soma->area_1_activation) nimcp_free(soma->area_1_activation);
    if (soma->area_2_activation) nimcp_free(soma->area_2_activation);
    if (soma->s2_activation) nimcp_free(soma->s2_activation);

    /* Free event buffers */
    if (soma->active_touch_events) nimcp_free(soma->active_touch_events);
    if (soma->active_pain_events) nimcp_free(soma->active_pain_events);

    /* Free receptors */
    if (soma->receptors) nimcp_free(soma->receptors);

    /* Free body map neuron IDs and neighbors */
    for (int i = 0; i < BODY_SEG_COUNT; i++) {
        if (soma->body_map[i].neuron_ids) nimcp_free(soma->body_map[i].neuron_ids);
        if (soma->body_map[i].neighbors) nimcp_free(soma->body_map[i].neighbors);
    }

    /* Free cortical columns if allocated */
    if (soma->area_3a_columns) nimcp_free(soma->area_3a_columns);
    if (soma->area_3b_columns) nimcp_free(soma->area_3b_columns);
    if (soma->area_1_columns) nimcp_free(soma->area_1_columns);
    if (soma->area_2_columns) nimcp_free(soma->area_2_columns);
    if (soma->s2_columns) nimcp_free(soma->s2_columns);

    nimcp_free(soma);
}

int soma_reset(nimcp_somatosensory_t* soma) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_reset: soma is NULL");
        return -1;
    }

    /* Reset activation buffers */
    memset(soma->area_3a_activation, 0, soma->config.num_area_3a_neurons * sizeof(float));
    memset(soma->area_3b_activation, 0, soma->config.num_area_3b_neurons * sizeof(float));
    memset(soma->area_1_activation, 0, soma->config.num_area_1_neurons * sizeof(float));
    memset(soma->area_2_activation, 0, soma->config.num_area_2_neurons * sizeof(float));
    memset(soma->s2_activation, 0, soma->config.num_s2_neurons * sizeof(float));

    /* Reset events */
    soma->num_active_touch = 0;
    soma->num_active_pain = 0;

    /* Reset body map activations */
    for (int i = 0; i < BODY_SEG_COUNT; i++) {
        soma->body_map[i].activation_level = 0.0f;
    }

    /* Reset proprioception */
    soma_init_proprio_state(soma);

    /* Reset counters */
    soma->updates_processed = 0;
    soma->touch_events_total = 0;
    soma->pain_events_total = 0;

    soma->status = SOMA_STATUS_READY;
    soma->last_error = SOMA_ERROR_NONE;

    return 0;
}

int soma_update(nimcp_somatosensory_t* soma, float dt) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_update: soma is NULL");
        return -1;
    }
    if (soma->status == SOMA_STATUS_ERROR) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "soma_update: validation failed");
        return -1;
    }

    /* Decay all activations */
    float decay = expf(-dt / 0.1f);  /* 100ms time constant */

    for (uint32_t i = 0; i < soma->config.num_area_3a_neurons; i++) {
        soma->area_3a_activation[i] *= decay;
        if (soma->area_3a_activation[i] < SOMA_MIN_ACTIVATION) {
            soma->area_3a_activation[i] = 0.0f;
        }
    }

    for (uint32_t i = 0; i < soma->config.num_area_3b_neurons; i++) {
        soma->area_3b_activation[i] *= decay;
        if (soma->area_3b_activation[i] < SOMA_MIN_ACTIVATION) {
            soma->area_3b_activation[i] = 0.0f;
        }
    }

    for (uint32_t i = 0; i < soma->config.num_area_1_neurons; i++) {
        soma->area_1_activation[i] *= decay;
        if (soma->area_1_activation[i] < SOMA_MIN_ACTIVATION) {
            soma->area_1_activation[i] = 0.0f;
        }
    }

    for (uint32_t i = 0; i < soma->config.num_area_2_neurons; i++) {
        soma->area_2_activation[i] *= decay;
        if (soma->area_2_activation[i] < SOMA_MIN_ACTIVATION) {
            soma->area_2_activation[i] = 0.0f;
        }
    }

    for (uint32_t i = 0; i < soma->config.num_s2_neurons; i++) {
        soma->s2_activation[i] *= decay;
        if (soma->s2_activation[i] < SOMA_MIN_ACTIVATION) {
            soma->s2_activation[i] = 0.0f;
        }
    }

    /* Update body map activations */
    for (int i = 0; i < BODY_SEG_COUNT; i++) {
        soma->body_map[i].activation_level *= decay;
    }

    /* Update proprioceptive states */
    for (int i = 0; i < BODY_SEG_COUNT; i++) {
        /* Simple position integration from velocity */
        for (int j = 0; j < 3; j++) {
            soma->proprio_state[i].position[j] += soma->proprio_state[i].velocity[j] * dt;
        }
    }

    /* Process touch event timeouts */
    uint64_t current_time = soma_get_time_ms();
    for (uint32_t i = 0; i < soma->num_active_touch; i++) {
        if (soma->active_touch_events[i].is_active) {
            float event_duration = (float)(current_time - soma->active_touch_events[i].timestamp);
            if (event_duration > soma->active_touch_events[i].duration_ms) {
                soma->active_touch_events[i].is_active = false;
            }
        }
    }

    soma->updates_processed++;
    soma->last_update_time = current_time;

    return 0;
}

/*=============================================================================
 * TOUCH PROCESSING FUNCTIONS
 *===========================================================================*/

int soma_process_touch(nimcp_somatosensory_t* soma,
                       body_segment_t segment,
                       const float* position,
                       float intensity,
                       touch_modality_t modality,
                       uint32_t* event_id_out) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_update: soma is NULL");
        return -1;
    }
    if (segment >= BODY_SEG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "soma_update: capacity exceeded");
        return -1;
    }
    if (intensity < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "soma_update: validation failed");
        return -1;
    }

    soma->status = SOMA_STATUS_PROCESSING_TOUCH;

    /* Find free slot */
    uint32_t slot = soma->num_active_touch;
    if (slot >= soma->max_touch_events) {
        /* Overwrite oldest inactive */
        for (uint32_t i = 0; i < soma->max_touch_events; i++) {
            if (!soma->active_touch_events[i].is_active) {
                slot = i;
                break;
            }
        }
        if (slot >= soma->max_touch_events) {
            slot = 0;  /* Overwrite first */
        }
    } else {
        soma->num_active_touch++;
    }

    /* Create event */
    soma_touch_event_t* event = &soma->active_touch_events[slot];
    event->event_id = soma->touch_events_total++;
    event->segment = segment;
    event->modality = modality;
    event->intensity = intensity;
    event->timestamp = soma_get_time_ms();
    event->duration_ms = 100.0f;  /* Default duration */
    event->is_active = true;

    if (position) {
        memcpy(event->position, position, 3 * sizeof(float));
    } else {
        memset(event->position, 0, sizeof(event->position));
    }

    /* Update body map activation */
    float magnification = soma->body_map[segment].cortical_area_size / 100.0f;
    soma->body_map[segment].activation_level += intensity * magnification *
                                                 soma->body_map[segment].sensitivity_modifier;
    if (soma->body_map[segment].activation_level > 1.0f) {
        soma->body_map[segment].activation_level = 1.0f;
    }

    /* Update cortical activations based on modality and segment */
    uint32_t neuron_idx = segment * (soma->config.num_area_3b_neurons / BODY_SEG_COUNT);
    if (neuron_idx < soma->config.num_area_3b_neurons) {
        soma->area_3b_activation[neuron_idx] += intensity;
        if (soma->area_3b_activation[neuron_idx] > 1.0f) {
            soma->area_3b_activation[neuron_idx] = 1.0f;
        }
    }

    if (event_id_out) {
        *event_id_out = event->event_id;
    }

    soma->status = SOMA_STATUS_READY;
    return 0;
}

int soma_process_touch_full(nimcp_somatosensory_t* soma,
                            const soma_touch_event_t* event,
                            uint32_t* event_id_out) {
    if (!soma || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_update: required parameter is NULL (soma, event)");
        return -1;
    }

    return soma_process_touch(soma, event->segment, event->position,
                              event->intensity, event->modality, event_id_out);
}

int soma_get_active_touches(nimcp_somatosensory_t* soma,
                            soma_touch_event_t* events,
                            uint32_t max_events,
                            uint32_t* num_events) {
    if (!soma || !events || !num_events) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_update: required parameter is NULL (soma, events, num_events)");
        return -1;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < soma->num_active_touch && count < max_events; i++) {
        if (soma->active_touch_events[i].is_active) {
            memcpy(&events[count], &soma->active_touch_events[i], sizeof(soma_touch_event_t));
            count++;
        }
    }

    *num_events = count;
    return 0;
}

int soma_two_point_discrimination(nimcp_somatosensory_t* soma,
                                  body_segment_t segment,
                                  const float* point1,
                                  const float* point2,
                                  bool* can_discriminate,
                                  float* discrimination_confidence) {
    if (!soma || !point1 || !point2 || !can_discriminate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_update: required parameter is NULL (soma, point1, point2, can_discriminate)");
        return -1;
    }
    if (segment >= BODY_SEG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "soma_update: capacity exceeded");
        return -1;
    }

    /* Calculate distance */
    float dx = point2[0] - point1[0];
    float dy = point2[1] - point1[1];
    float dz = point2[2] - point1[2];
    float distance = sqrtf(dx*dx + dy*dy + dz*dz);

    float threshold = soma->body_map[segment].two_point_threshold;
    *can_discriminate = (distance >= threshold);

    if (discrimination_confidence) {
        if (distance < threshold * 0.5f) {
            *discrimination_confidence = 0.0f;
        } else if (distance > threshold * 2.0f) {
            *discrimination_confidence = 1.0f;
        } else {
            *discrimination_confidence = (distance - threshold * 0.5f) / (threshold * 1.5f);
        }
    }

    return 0;
}

/*=============================================================================
 * PAIN PROCESSING FUNCTIONS
 *===========================================================================*/

int soma_process_pain(nimcp_somatosensory_t* soma,
                      body_segment_t segment,
                      pain_type_t type,
                      float intensity,
                      uint32_t* event_id_out) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: soma is NULL");
        return -1;
    }
    if (segment >= BODY_SEG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "unknown: capacity exceeded");
        return -1;
    }
    if (intensity < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    soma->status = SOMA_STATUS_PROCESSING_PAIN;

    /* Find free slot */
    uint32_t slot = soma->num_active_pain;
    if (slot >= soma->max_pain_events) {
        for (uint32_t i = 0; i < soma->max_pain_events; i++) {
            if (!soma->active_pain_events[i].is_active) {
                slot = i;
                break;
            }
        }
        if (slot >= soma->max_pain_events) {
            slot = 0;
        }
    } else {
        soma->num_active_pain++;
    }

    /* Create event */
    soma_pain_event_t* event = &soma->active_pain_events[slot];
    event->event_id = soma->pain_events_total++;
    event->segment = segment;
    event->type = type;
    event->intensity = intensity;
    event->onset_time = soma_get_time_ms();
    event->is_active = true;
    event->is_chronic = false;
    event->has_referred = false;
    event->attention_modulation = 1.0f;
    event->emotional_modulation = 1.0f;
    event->expected_pain = intensity;

    /* High intensity pain affects attention */
    if (intensity > soma->config.pain_threshold) {
        soma->cognitive_bridge.body_attention[segment] = 1.0f;
    }

    if (event_id_out) {
        *event_id_out = event->event_id;
    }

    soma->status = SOMA_STATUS_READY;
    return 0;
}

float soma_get_pain_level(nimcp_somatosensory_t* soma, body_segment_t segment) {
    if (!soma || segment >= BODY_SEG_COUNT) return 0.0f;

    float total = 0.0f;
    for (uint32_t i = 0; i < soma->num_active_pain; i++) {
        if (soma->active_pain_events[i].is_active &&
            soma->active_pain_events[i].segment == segment) {
            total += soma->active_pain_events[i].intensity *
                     soma->active_pain_events[i].attention_modulation *
                     soma->active_pain_events[i].emotional_modulation;
        }
    }
    return total > 10.0f ? 10.0f : total;
}

int soma_get_pain_state(nimcp_somatosensory_t* soma,
                        float* total_pain,
                        body_segment_t* max_pain_segment,
                        pain_type_t* dominant_type) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "soma_get_pain_state: soma is NULL");
        return -1;
    }

    /* Aggregate across all body segments:
     *   total_pain      = sum of per-segment pain levels
     *   max_pain_segment = segment with highest pain
     *   dominant_type    = most-activated pain_type across all active events,
     *                      weighted by each event's contribution magnitude. */
    float sum = 0.0f;
    float max_level = 0.0f;
    body_segment_t worst = (body_segment_t)0;
    float type_weights[PAIN_REFERRED + 1] = {0};

    for (body_segment_t s = 0; s < BODY_SEG_COUNT; s++) {
        float lvl = soma_get_pain_level(soma, s);
        sum += lvl;
        if (lvl > max_level) {
            max_level = lvl;
            worst = s;
        }
    }
    for (uint32_t i = 0; i < soma->num_active_pain; i++) {
        const soma_pain_event_t* ev = &soma->active_pain_events[i];
        if (!ev->is_active) continue;
        if ((int)ev->type < 0 || (int)ev->type > PAIN_REFERRED) continue;
        float w = ev->intensity * ev->attention_modulation * ev->emotional_modulation;
        type_weights[(int)ev->type] += w;
    }
    pain_type_t dom = PAIN_NONE;
    float dom_w = 0.0f;
    for (int t = PAIN_NONE; t <= PAIN_REFERRED; t++) {
        if (type_weights[t] > dom_w) {
            dom_w = type_weights[t];
            dom = (pain_type_t)t;
        }
    }

    if (total_pain)        *total_pain        = sum;
    if (max_pain_segment)  *max_pain_segment  = worst;
    if (dominant_type)     *dominant_type     = dom;
    return 0;
}

/*=============================================================================
 * PROPRIOCEPTION FUNCTIONS
 *===========================================================================*/

int soma_update_proprioception(nimcp_somatosensory_t* soma,
                               body_segment_t segment,
                               const float* position,
                               const float* velocity,
                               float muscle_tension,
                               float muscle_length) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_get_pain_level: soma is NULL");
        return -1;
    }
    if (segment >= BODY_SEG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "soma_get_pain_level: capacity exceeded");
        return -1;
    }

    soma->status = SOMA_STATUS_PROCESSING_PROPRIO;

    soma_proprio_state_t* state = &soma->proprio_state[segment];

    if (position) {
        memcpy(state->position, position, 3 * sizeof(float));
    }
    if (velocity) {
        memcpy(state->velocity, velocity, 3 * sizeof(float));
    }

    state->muscle_tension = muscle_tension;
    state->muscle_length = muscle_length;
    state->last_update = soma_get_time_ms();
    state->confidence = 0.9f;  /* High confidence from direct measurement */

    /* Update Area 3a (proprioceptive) */
    uint32_t neuron_idx = segment * (soma->config.num_area_3a_neurons / BODY_SEG_COUNT);
    if (neuron_idx < soma->config.num_area_3a_neurons) {
        float activation = (muscle_tension + muscle_length) * 0.5f;
        soma->area_3a_activation[neuron_idx] = activation > 1.0f ? 1.0f : activation;
    }

    soma->status = SOMA_STATUS_READY;
    return 0;
}

int soma_get_proprioception(nimcp_somatosensory_t* soma,
                            body_segment_t segment,
                            soma_proprio_state_t* state) {
    if (!soma || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_get_pain_level: required parameter is NULL (soma, state)");
        return -1;
    }
    if (segment >= BODY_SEG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "soma_get_pain_level: capacity exceeded");
        return -1;
    }

    memcpy(state, &soma->proprio_state[segment], sizeof(soma_proprio_state_t));
    return 0;
}

int soma_get_body_position(nimcp_somatosensory_t* soma,
                           float* positions,
                           uint32_t max_segments,
                           uint32_t* num_segments) {
    if (!soma || !positions || !num_segments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_get_pain_level: required parameter is NULL (soma, positions, num_segments)");
        return -1;
    }

    uint32_t count = max_segments < BODY_SEG_COUNT ? max_segments : BODY_SEG_COUNT;

    for (uint32_t i = 0; i < count; i++) {
        memcpy(&positions[i * 3], soma->proprio_state[i].position, 3 * sizeof(float));
    }

    *num_segments = count;
    return 0;
}

float soma_get_joint_angle(nimcp_somatosensory_t* soma, body_segment_t segment) {
    if (!soma || segment >= BODY_SEG_COUNT) return 0.0f;
    return soma->proprio_state[segment].joint_angle;
}

/*=============================================================================
 * TEMPERATURE PROCESSING
 *===========================================================================*/

int soma_process_temperature(nimcp_somatosensory_t* soma,
                             body_segment_t segment,
                             float temperature,
                             temp_sensation_t* sensation_out) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_get_joint_angle: soma is NULL");
        return -1;
    }
    if (segment >= BODY_SEG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "soma_get_joint_angle: capacity exceeded");
        return -1;
    }

    soma->status = SOMA_STATUS_PROCESSING_TEMP;

    temp_sensation_t sensation;
    if (temperature < 10.0f) {
        sensation = TEMP_COLD_EXTREME;
    } else if (temperature < 15.0f) {
        sensation = TEMP_COLD;
    } else if (temperature < 25.0f) {
        sensation = TEMP_COOL;
    } else if (temperature < 35.0f) {
        sensation = TEMP_NEUTRAL;
    } else if (temperature < 40.0f) {
        sensation = TEMP_WARM;
    } else if (temperature < 45.0f) {
        sensation = TEMP_HOT;
    } else {
        sensation = TEMP_HOT_EXTREME;
    }

    if (sensation_out) {
        *sensation_out = sensation;
    }

    /* Extreme temperatures may trigger pain */
    if (sensation == TEMP_COLD_EXTREME || sensation == TEMP_HOT_EXTREME) {
        float pain_intensity = (sensation == TEMP_COLD_EXTREME) ?
                               (10.0f - temperature) / 10.0f :
                               (temperature - 45.0f) / 10.0f;
        soma_process_pain(soma, segment, PAIN_BURNING, pain_intensity, NULL);
    }

    soma->status = SOMA_STATUS_READY;
    return 0;
}

temp_sensation_t soma_get_temperature_sensation(nimcp_somatosensory_t* soma,
                                                 body_segment_t segment) {
    (void)soma;
    (void)segment;
    return TEMP_NEUTRAL;  /* Would need to track per-segment temperature */
}

/*=============================================================================
 * BODY MAP FUNCTIONS
 *===========================================================================*/

const soma_body_map_entry_t* soma_get_body_map_entry(nimcp_somatosensory_t* soma,
                                                      body_segment_t segment) {
    if (!soma || segment >= BODY_SEG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "soma_get_joint_angle: soma is NULL");
        return NULL;
    }
    return &soma->body_map[segment];
}

float soma_get_cortical_magnification(nimcp_somatosensory_t* soma,
                                      body_segment_t segment) {
    if (!soma || segment >= BODY_SEG_COUNT) return 0.0f;
    return soma->body_map[segment].cortical_area_size / 100.0f;
}

int soma_update_body_map(nimcp_somatosensory_t* soma,
                         body_segment_t segment,
                         float sensitivity_change) {
    if (!soma || segment >= BODY_SEG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "soma_get_joint_angle: soma is NULL");
        return -1;
    }

    soma->body_map[segment].sensitivity_modifier *= (1.0f + sensitivity_change);
    if (soma->body_map[segment].sensitivity_modifier < 0.1f) {
        soma->body_map[segment].sensitivity_modifier = 0.1f;
    }
    if (soma->body_map[segment].sensitivity_modifier > 10.0f) {
        soma->body_map[segment].sensitivity_modifier = 10.0f;
    }

    return 0;
}

float soma_get_two_point_threshold(nimcp_somatosensory_t* soma,
                                   body_segment_t segment) {
    if (!soma || segment >= BODY_SEG_COUNT) return SOMA_TWO_POINT_MIN_MM;
    return soma->body_map[segment].two_point_threshold;
}

int soma_calibrate_body_map(nimcp_somatosensory_t* soma) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_calibrate_body_map: soma is NULL");
        return -1;
    }

    soma->status = SOMA_STATUS_CALIBRATING;

    /* Reset all sensitivities to default */
    for (int i = 0; i < BODY_SEG_COUNT; i++) {
        soma->body_map[i].sensitivity_modifier = 1.0f;
        soma->body_map[i].cortical_area_size =
            (uint32_t)(soma_get_default_magnification((body_segment_t)i) * 100.0f);
        soma->body_map[i].two_point_threshold =
            soma_get_default_two_point((body_segment_t)i);
    }

    soma->brain_init_bridge.body_map_calibrated = true;
    soma->brain_init_bridge.last_calibration = soma_get_time_ms();

    soma->status = SOMA_STATUS_READY;
    return 0;
}

/*=============================================================================
 * CORTICAL AREA ACCESS
 *===========================================================================*/

int soma_get_area_activation(nimcp_somatosensory_t* soma,
                             soma_area_t area,
                             float* activation,
                             uint32_t max_size,
                             uint32_t* actual_size) {
    if (!soma || !activation || !actual_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_calibrate_body_map: required parameter is NULL (soma, activation, actual_size)");
        return -1;
    }

    float* source;
    uint32_t size;

    switch (area) {
        case SOMA_AREA_3A:
            source = soma->area_3a_activation;
            size = soma->config.num_area_3a_neurons;
            break;
        case SOMA_AREA_3B:
            source = soma->area_3b_activation;
            size = soma->config.num_area_3b_neurons;
            break;
        case SOMA_AREA_1:
            source = soma->area_1_activation;
            size = soma->config.num_area_1_neurons;
            break;
        case SOMA_AREA_2:
            source = soma->area_2_activation;
            size = soma->config.num_area_2_neurons;
            break;
        case SOMA_AREA_S2:
            source = soma->s2_activation;
            size = soma->config.num_s2_neurons;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "soma_calibrate_body_map: operation failed");
            return -1;
    }

    uint32_t copy_size = size < max_size ? size : max_size;
    memcpy(activation, source, copy_size * sizeof(float));
    *actual_size = copy_size;

    return 0;
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW
 *===========================================================================*/

int soma_process_incoming(nimcp_somatosensory_t* soma) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_process_incoming: soma is NULL");
        return -1;
    }
    /* Process messages from connected modules */
    return 0;
}

int soma_send_outgoing(nimcp_somatosensory_t* soma) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_send_outgoing: soma is NULL");
        return -1;
    }
    /* Send updates to connected modules */
    return 0;
}

int soma_bidirectional_update(nimcp_somatosensory_t* soma, float dt) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_bidirectional_update: soma is NULL");
        return -1;
    }

    soma_process_incoming(soma);
    soma_update(soma, dt);
    soma_send_outgoing(soma);

    return 0;
}

int soma_sync_thalamus(nimcp_somatosensory_t* soma) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_sync_thalamus: soma is NULL");
        return -1;
    }
    if (!soma->thalamus_bridge.initialized) return 0;  /* No-op if not initialized */
    /* Sync with VPL/VPM thalamic nuclei */
    return 0;
}

int soma_sync_motor_cortex(nimcp_somatosensory_t* soma) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_sync_motor_cortex: soma is NULL");
        return -1;
    }
    if (!soma->motor_bridge.initialized) return 0;  /* No-op if not initialized */
    /* Process efference copy and sensorimotor integration */
    return 0;
}

int soma_sync_parietal(nimcp_somatosensory_t* soma) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_sync_parietal: soma is NULL");
        return -1;
    }
    if (!soma->parietal_bridge.initialized) return 0;  /* No-op if not initialized */
    /* Update body schema in parietal cortex */
    return 0;
}

int soma_sync_hypothalamus(nimcp_somatosensory_t* soma) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_sync_hypothalamus: soma is NULL");
        return -1;
    }
    if (!soma->hypothalamus_bridge.initialized) return 0;  /* No-op if not initialized */
    /* Temperature regulation and homeostasis */
    return 0;
}

/*=============================================================================
 * BRIDGE INITIALIZATION FUNCTIONS
 *===========================================================================*/

int soma_init_prime_resonance_bridge(nimcp_somatosensory_t* soma, void* pr_memory) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_prime_resonance_bridge: soma is NULL");
        return -1;
    }
    soma->prime_resonance_bridge.pr_memory_ctx = pr_memory;
    soma->prime_resonance_bridge.initialized = true;
    return 0;
}

int soma_init_immune_bridge(nimcp_somatosensory_t* soma, void* immune) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_immune_bridge: soma is NULL");
        return -1;
    }
    soma->immune_bridge.immune_system = immune;
    soma->immune_bridge.initialized = true;
    soma->immune_bridge.health_score = 1.0f;
    return 0;
}

int soma_init_bio_async_bridge(nimcp_somatosensory_t* soma, void* runtime) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_bio_async_bridge: soma is NULL");
        return -1;
    }
    soma->bio_async_bridge.runtime = runtime;
    soma->bio_async_bridge.initialized = true;
    return 0;
}

int soma_init_brain_init_bridge(nimcp_somatosensory_t* soma, void* brain_init) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_brain_init_bridge: soma is NULL");
        return -1;
    }
    soma->brain_init_bridge.brain_init_ctx = brain_init;
    soma->brain_init_bridge.initialized = true;
    return 0;
}

int soma_init_security_bridge(nimcp_somatosensory_t* soma, void* security_ctx, void* security_ops) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_security_bridge: soma is NULL");
        return -1;
    }
    soma->security_bridge.security_ctx = security_ctx;
    soma->security_bridge.security_ops = security_ops;
    soma->security_bridge.initialized = true;
    return 0;
}

int soma_init_logging_bridge(nimcp_somatosensory_t* soma, void* logger) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_logging_bridge: soma is NULL");
        return -1;
    }
    soma->logging_bridge.logger = logger;
    soma->logging_bridge.initialized = true;
    strncpy(soma->logging_bridge.log_prefix, "SOMA", sizeof(soma->logging_bridge.log_prefix) - 1);
    return 0;
}

int soma_init_cognitive_bridge(nimcp_somatosensory_t* soma, void* cognitive) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_cognitive_bridge: soma is NULL");
        return -1;
    }
    soma->cognitive_bridge.cognitive_ctx = cognitive;
    soma->cognitive_bridge.initialized = true;
    return 0;
}

int soma_init_training_bridge(nimcp_somatosensory_t* soma, void* training) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_training_bridge: soma is NULL");
        return -1;
    }
    soma->training_bridge.training_ctx = training;
    soma->training_bridge.initialized = true;
    return 0;
}

int soma_init_omni_bridge(nimcp_somatosensory_t* soma, void* omni) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_omni_bridge: soma is NULL");
        return -1;
    }
    soma->omni_bridge.omni_ctx = omni;
    soma->omni_bridge.initialized = true;
    return 0;
}

int soma_init_hypothalamus_bridge(nimcp_somatosensory_t* soma, void* hypothalamus) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_hypothalamus_bridge: soma is NULL");
        return -1;
    }
    soma->hypothalamus_bridge.hypothalamus = hypothalamus;
    soma->hypothalamus_bridge.initialized = true;
    return 0;
}

int soma_init_substrate_bridge(nimcp_somatosensory_t* soma, void* substrate) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_substrate_bridge: soma is NULL");
        return -1;
    }
    soma->substrate_bridge.substrate = substrate;
    soma->substrate_bridge.initialized = true;
    return 0;
}

int soma_init_thalamus_bridge(nimcp_somatosensory_t* soma, void* thalamus) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_thalamus_bridge: soma is NULL");
        return -1;
    }
    soma->thalamus_bridge.thalamus = thalamus;
    soma->thalamus_bridge.initialized = true;
    return 0;
}

int soma_init_motor_bridge(nimcp_somatosensory_t* soma, void* motor) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_motor_bridge: soma is NULL");
        return -1;
    }
    soma->motor_bridge.motor_ctx = motor;
    soma->motor_bridge.initialized = true;
    return 0;
}

int soma_init_parietal_bridge(nimcp_somatosensory_t* soma, void* parietal) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_parietal_bridge: soma is NULL");
        return -1;
    }
    soma->parietal_bridge.parietal_ctx = parietal;
    soma->parietal_bridge.initialized = true;
    return 0;
}

int soma_init_snn_bridge(nimcp_somatosensory_t* soma, void* snn) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_snn_bridge: soma is NULL");
        return -1;
    }
    soma->snn_bridge.snn_network = snn;
    soma->snn_bridge.initialized = true;
    return 0;
}

int soma_init_plasticity_bridge(nimcp_somatosensory_t* soma, void* plasticity, void* stdp) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_plasticity_bridge: soma is NULL");
        return -1;
    }
    soma->plasticity_bridge.plasticity_ctx = plasticity;
    soma->plasticity_bridge.stdp_ctx = stdp;
    soma->plasticity_bridge.initialized = true;
    return 0;
}

int soma_init_portia_bridge(nimcp_somatosensory_t* soma, void* portia) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_portia_bridge: soma is NULL");
        return -1;
    }
    soma->portia_bridge.portia_ctx = portia;
    soma->portia_bridge.initialized = true;
    return 0;
}

int soma_init_dragonfly_bridge(nimcp_somatosensory_t* soma, void* dragonfly) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_dragonfly_bridge: soma is NULL");
        return -1;
    }
    soma->dragonfly_bridge.dragonfly_ctx = dragonfly;
    soma->dragonfly_bridge.initialized = true;
    return 0;
}

int soma_init_perception_bridge(nimcp_somatosensory_t* soma, void* perception) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_perception_bridge: soma is NULL");
        return -1;
    }
    soma->perception_bridge.perception_ctx = perception;
    soma->perception_bridge.initialized = true;
    return 0;
}

int soma_init_all_bridges(nimcp_somatosensory_t* soma, void** bridge_contexts) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_init_all_bridges: soma is NULL");
        return -1;
    }

    if (bridge_contexts) {
        /* Initialize all bridges from context array.
         * Note: avoid multiple i++ in a single function call (undefined behavior). */
        int i = 0;
        soma_init_prime_resonance_bridge(soma, bridge_contexts[i]); i++;
        soma_init_immune_bridge(soma, bridge_contexts[i]); i++;
        soma_init_bio_async_bridge(soma, bridge_contexts[i]); i++;
        soma_init_brain_init_bridge(soma, bridge_contexts[i]); i++;
        /* security_bridge takes 2 context args */
        void* sec_ctx = bridge_contexts[i]; i++;
        void* sec_ops = bridge_contexts[i]; i++;
        soma_init_security_bridge(soma, sec_ctx, sec_ops);
        soma_init_logging_bridge(soma, bridge_contexts[i]); i++;
        soma_init_cognitive_bridge(soma, bridge_contexts[i]); i++;
        soma_init_training_bridge(soma, bridge_contexts[i]); i++;
        soma_init_omni_bridge(soma, bridge_contexts[i]); i++;
        soma_init_hypothalamus_bridge(soma, bridge_contexts[i]); i++;
        soma_init_substrate_bridge(soma, bridge_contexts[i]); i++;
        soma_init_thalamus_bridge(soma, bridge_contexts[i]); i++;
        soma_init_motor_bridge(soma, bridge_contexts[i]); i++;
        soma_init_parietal_bridge(soma, bridge_contexts[i]); i++;
        soma_init_snn_bridge(soma, bridge_contexts[i]); i++;
        /* plasticity_bridge takes 2 context args */
        void* plast_ctx = bridge_contexts[i]; i++;
        void* stdp_ctx = bridge_contexts[i]; i++;
        soma_init_plasticity_bridge(soma, plast_ctx, stdp_ctx);
        soma_init_portia_bridge(soma, bridge_contexts[i]); i++;
        soma_init_dragonfly_bridge(soma, bridge_contexts[i]); i++;
        soma_init_perception_bridge(soma, bridge_contexts[i]); i++;
    } else {
        /* Initialize all bridges with NULL contexts */
        soma_init_prime_resonance_bridge(soma, NULL);
        soma_init_immune_bridge(soma, NULL);
        soma_init_bio_async_bridge(soma, NULL);
        soma_init_brain_init_bridge(soma, NULL);
        soma_init_security_bridge(soma, NULL, NULL);
        soma_init_logging_bridge(soma, NULL);
        soma_init_cognitive_bridge(soma, NULL);
        soma_init_training_bridge(soma, NULL);
        soma_init_omni_bridge(soma, NULL);
        soma_init_hypothalamus_bridge(soma, NULL);
        soma_init_substrate_bridge(soma, NULL);
        soma_init_thalamus_bridge(soma, NULL);
        soma_init_motor_bridge(soma, NULL);
        soma_init_parietal_bridge(soma, NULL);
        soma_init_snn_bridge(soma, NULL);
        soma_init_plasticity_bridge(soma, NULL, NULL);
        soma_init_portia_bridge(soma, NULL);
        soma_init_dragonfly_bridge(soma, NULL);
        soma_init_perception_bridge(soma, NULL);
    }

    return 0;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

soma_status_t soma_get_status(nimcp_somatosensory_t* soma) {
    if (!soma) return SOMA_STATUS_ERROR;
    return soma->status;
}

soma_error_t soma_get_last_error(nimcp_somatosensory_t* soma) {
    if (!soma) return SOMA_ERROR_INTERNAL;
    return soma->last_error;
}

const char* soma_error_string(soma_error_t error) {
    switch (error) {
        case SOMA_ERROR_NONE: return "No error";
        case SOMA_ERROR_INVALID_INPUT: return "Invalid input";
        case SOMA_ERROR_RECEPTOR_OVERLOAD: return "Receptor overload";
        case SOMA_ERROR_BODY_MAP_ERROR: return "Body map error";
        case SOMA_ERROR_PROCESSING_FAILED: return "Processing failed";
        case SOMA_ERROR_CALIBRATION_FAILED: return "Calibration failed";
        case SOMA_ERROR_BRIDGE_ERROR: return "Bridge error";
        case SOMA_ERROR_THRESHOLD_EXCEEDED: return "Threshold exceeded";
        case SOMA_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* soma_status_string(soma_status_t status) {
    switch (status) {
        case SOMA_STATUS_IDLE: return "Idle";
        case SOMA_STATUS_READY: return "Ready";
        case SOMA_STATUS_PROCESSING_TOUCH: return "Processing touch";
        case SOMA_STATUS_PROCESSING_PAIN: return "Processing pain";
        case SOMA_STATUS_PROCESSING_PROPRIO: return "Processing proprioception";
        case SOMA_STATUS_PROCESSING_TEMP: return "Processing temperature";
        case SOMA_STATUS_INTEGRATING: return "Integrating";
        case SOMA_STATUS_CALIBRATING: return "Calibrating";
        case SOMA_STATUS_ERROR: return "Error";
        default: return "Unknown status";
    }
}

int soma_get_stats(nimcp_somatosensory_t* soma, soma_stats_t* stats) {
    if (!soma || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_get_stats: required parameter is NULL (soma, stats)");
        return -1;
    }

    stats->touch_events_processed = soma->touch_events_total;
    stats->pain_events_processed = soma->pain_events_total;
    stats->proprio_updates = soma->updates_processed;
    stats->last_update_time = soma->last_update_time;
    stats->updates_processed = soma->updates_processed;

    /* Calculate average activations */
    float total_3a = 0.0f, total_3b = 0.0f, total_1 = 0.0f, total_2 = 0.0f, total_s2 = 0.0f;

    for (uint32_t i = 0; i < soma->config.num_area_3a_neurons; i++) {
        total_3a += soma->area_3a_activation[i];
    }
    for (uint32_t i = 0; i < soma->config.num_area_3b_neurons; i++) {
        total_3b += soma->area_3b_activation[i];
    }
    for (uint32_t i = 0; i < soma->config.num_area_1_neurons; i++) {
        total_1 += soma->area_1_activation[i];
    }
    for (uint32_t i = 0; i < soma->config.num_area_2_neurons; i++) {
        total_2 += soma->area_2_activation[i];
    }
    for (uint32_t i = 0; i < soma->config.num_s2_neurons; i++) {
        total_s2 += soma->s2_activation[i];
    }

    stats->area_3a_activity = (soma->config.num_area_3a_neurons > 0) ?
        total_3a / soma->config.num_area_3a_neurons : 0.0f;
    stats->area_3b_activity = (soma->config.num_area_3b_neurons > 0) ?
        total_3b / soma->config.num_area_3b_neurons : 0.0f;
    stats->area_1_activity = (soma->config.num_area_1_neurons > 0) ?
        total_1 / soma->config.num_area_1_neurons : 0.0f;
    stats->area_2_activity = (soma->config.num_area_2_neurons > 0) ?
        total_2 / soma->config.num_area_2_neurons : 0.0f;
    stats->s2_activity = (soma->config.num_s2_neurons > 0) ?
        total_s2 / soma->config.num_s2_neurons : 0.0f;

    return 0;
}

int soma_get_config(nimcp_somatosensory_t* soma, soma_config_t* config) {
    if (!soma || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_get_config: required parameter is NULL (soma, config)");
        return -1;
    }
    memcpy(config, &soma->config, sizeof(soma_config_t));
    return 0;
}

float soma_get_health_status(nimcp_somatosensory_t* soma) {
    if (!soma) return 0.0f;
    if (soma->status == SOMA_STATUS_ERROR) return 0.0f;

    float health = 1.0f;

    /* Check immune bridge health */
    if (soma->immune_bridge.initialized) {
        health *= soma->immune_bridge.health_score;
    }

    /* Check substrate health */
    if (soma->substrate_bridge.initialized) {
        health *= soma->substrate_bridge.atp_level;
    }

    return health;
}

int soma_log_diagnostics(nimcp_somatosensory_t* soma) {
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_log_diagnostics: soma is NULL");
        return -1;
    }
    /* Would log to logging bridge if initialized */
    return 0;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* soma_segment_name(body_segment_t segment) {
    static const char* names[] = {
        "Head", "Face", "Lips", "Tongue", "Neck",
        "Left Shoulder", "Right Shoulder",
        "Left Upper Arm", "Right Upper Arm",
        "Left Elbow", "Right Elbow",
        "Left Forearm", "Right Forearm",
        "Left Wrist", "Right Wrist",
        "Left Hand", "Right Hand",
        "Left Thumb", "Right Thumb",
        "Left Index", "Right Index",
        "Left Middle", "Right Middle",
        "Left Ring", "Right Ring",
        "Left Pinky", "Right Pinky",
        "Chest", "Back", "Abdomen",
        "Left Hip", "Right Hip",
        "Left Thigh", "Right Thigh",
        "Left Knee", "Right Knee",
        "Left Lower Leg", "Right Lower Leg",
        "Left Ankle", "Right Ankle",
        "Left Foot", "Right Foot",
        "Left Toes", "Right Toes",
        "Genitals"
    };

    if (segment >= BODY_SEG_COUNT) return "Unknown";
    return names[segment];
}

const char* soma_receptor_type_name(soma_receptor_type_t type) {
    switch (type) {
        case SOMA_RECEPTOR_MEISSNER: return "Meissner (light touch)";
        case SOMA_RECEPTOR_MERKEL: return "Merkel (pressure)";
        case SOMA_RECEPTOR_PACINIAN: return "Pacinian (vibration)";
        case SOMA_RECEPTOR_RUFFINI: return "Ruffini (stretch)";
        case SOMA_RECEPTOR_FREE_NERVE: return "Free nerve (pain/temp)";
        case SOMA_RECEPTOR_MUSCLE_SPINDLE: return "Muscle spindle";
        case SOMA_RECEPTOR_GOLGI_TENDON: return "Golgi tendon organ";
        case SOMA_RECEPTOR_JOINT: return "Joint receptor";
        default: return "Unknown";
    }
}

const char* soma_area_name(soma_area_t area) {
    switch (area) {
        case SOMA_AREA_3A: return "Area 3a (proprioception)";
        case SOMA_AREA_3B: return "Area 3b (fine touch)";
        case SOMA_AREA_1: return "Area 1 (texture)";
        case SOMA_AREA_2: return "Area 2 (size/shape)";
        case SOMA_AREA_S2: return "S2 (bilateral)";
        default: return "Unknown";
    }
}

const char* soma_pain_type_name(pain_type_t type) {
    switch (type) {
        case PAIN_NONE: return "None";
        case PAIN_SHARP: return "Sharp";
        case PAIN_DULL: return "Dull";
        case PAIN_BURNING: return "Burning";
        case PAIN_ACHING: return "Aching";
        case PAIN_REFERRED: return "Referred";
        default: return "Unknown";
    }
}

/*=============================================================================
 * SERIALIZATION (STUB)
 *===========================================================================*/

size_t soma_get_serialization_size(nimcp_somatosensory_t* soma) {
    if (!soma) return 0;
    /* Calculate required buffer size */
    return sizeof(soma_config_t) +
           sizeof(soma->body_map) +
           sizeof(soma->proprio_state) +
           soma->config.num_area_3a_neurons * sizeof(float) +
           soma->config.num_area_3b_neurons * sizeof(float) +
           soma->config.num_area_1_neurons * sizeof(float) +
           soma->config.num_area_2_neurons * sizeof(float) +
           soma->config.num_s2_neurons * sizeof(float);
}

int soma_serialize(nimcp_somatosensory_t* soma, uint8_t* buffer, size_t size, size_t* written) {
    if (!soma || !buffer || !written) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_serialize: required parameter is NULL (soma, buffer, written)");
        return -1;
    }

    size_t needed = soma_get_serialization_size(soma);
    if (size < needed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "soma_serialize: validation failed");
        return -1;
    }

    /* Simplified serialization */
    size_t offset = 0;
    memcpy(buffer + offset, &soma->config, sizeof(soma_config_t));
    offset += sizeof(soma_config_t);

    memcpy(buffer + offset, soma->body_map, sizeof(soma->body_map));
    offset += sizeof(soma->body_map);

    *written = offset;
    return 0;
}

nimcp_somatosensory_t* soma_deserialize(const uint8_t* buffer, size_t size, size_t* bytes_read) {
    if (!buffer || !bytes_read) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "soma_deserialize: required parameter is NULL (buffer, bytes_read)");
        return NULL;
    }

    if (size < sizeof(soma_config_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "soma_deserialize: validation failed");
        return NULL;
    }

    soma_config_t config;
    memcpy(&config, buffer, sizeof(soma_config_t));

    nimcp_somatosensory_t* soma = soma_create(&config);
    if (!soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "soma_deserialize: allocation failed");
        return NULL;
    }

    *bytes_read = sizeof(soma_config_t);
    return soma;
}
