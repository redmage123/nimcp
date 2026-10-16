/**
 * @file nimcp_hypothalamus_internal_bus.c
 * @brief Implementation of hypothalamus internal message bus
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_internal_bus.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_internal_bus, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Subscription entry
 */
typedef struct {
    bool active;
    hypo_internal_module_t subscriber;
    hypo_internal_event_type_t event_type;
    hypo_internal_module_t source_filter;  /* HYPO_IMOD_COUNT means no filter */
    hypo_ibus_callback_t callback;
    void* user_data;
} ibus_subscription_t;

/**
 * @brief Active modulation entry
 */
typedef struct {
    bool active;
    hypo_internal_event_type_t trigger_event;
    hypo_ibus_modulation_t modulation;
    float current_factor;
    uint64_t start_time_us;
    uint64_t remaining_us;
} ibus_active_modulation_t;

/**
 * @brief Modulation rule entry
 */
typedef struct {
    bool active;
    hypo_internal_event_type_t source_event;
    hypo_ibus_modulation_t modulation;
} ibus_modulation_rule_t;

#define MAX_MODULATION_RULES 32
#define MAX_ACTIVE_MODULATIONS 64

/**
 * @brief Internal bus structure
 */
struct hypo_internal_bus {
    hypo_ibus_config_t config;

    /* Subscriptions */
    ibus_subscription_t subscriptions[HYPO_IBUS_MAX_SUBSCRIBERS];
    uint32_t num_subscriptions;
    int next_subscription_id;

    /* Modulation rules */
    ibus_modulation_rule_t rules[MAX_MODULATION_RULES];
    uint32_t num_rules;

    /* Active modulations */
    ibus_active_modulation_t active_mods[MAX_ACTIVE_MODULATIONS];
    uint32_t num_active_mods;

    /* Per-module accumulated modulation factors */
    float module_modulation[HYPO_IMOD_COUNT];

    /* Statistics */
    hypo_ibus_stats_t stats;

    /* Sequence counter */
    uint32_t next_sequence_id;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * STATIC NAME TABLES
 * ============================================================================ */

static const char* MODULE_NAMES[HYPO_IMOD_COUNT] = {
    "Circadian",
    "HPA_Axis",
    "Drives",
    "Homeostasis",
    "Autonomic",
    "Appetite",
    "Hydration",
    "Thermoregulation",
    "Alignment"
};

static const char* EVENT_NAMES[HYPO_IEVT_COUNT] = {
    "CircadianPhaseChange",
    "MelatoninOnset",
    "MelatoninOffset",
    "CortisolAwakening",
    "StressOnset",
    "StressPeak",
    "StressRecovery",
    "CortisolElevated",
    "CortisolNormalized",
    "DriveThresholdCrossed",
    "DriveSatisfied",
    "DriveConflict",
    "HungerOnset",
    "FatigueOnset",
    "SafetyThreat",
    "SetpointDeviation",
    "SetpointRestored",
    "TemperatureAlert",
    "SympatheticActivation",
    "ParasympatheticActivation",
    "AutonomicBalanceShift",
    "AlignmentWarning",
    "AlignmentViolation"
};

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

int hypo_ibus_default_config(hypo_ibus_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_default_config: config is NULL");
        return -1;
    }

    config->max_subscribers = HYPO_IBUS_MAX_SUBSCRIBERS;
    config->max_queue_size = HYPO_IBUS_MAX_QUEUE;
    config->enable_async = false;
    config->enable_modulation = true;
    config->enable_logging = false;

    /* Biological parameters based on research */
    config->circadian_hunger_amplitude = 0.3f;    /* 30% hunger variation */
    config->circadian_fatigue_amplitude = 0.5f;   /* 50% fatigue variation */
    config->cortisol_appetite_suppression = 0.4f; /* 40% appetite reduction */
    config->fatigue_curiosity_reduction = 0.6f;   /* 60% curiosity reduction */
    config->social_safety_modulation = 0.3f;      /* 30% safety reduction */
    config->hunger_stress_threshold = 0.85f;      /* 85% hunger triggers stress */

    return 0;
}

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

hypo_ibus_t hypo_ibus_create(const hypo_ibus_config_t* config) {
    struct hypo_internal_bus* bus = nimcp_calloc(1, sizeof(struct hypo_internal_bus));
    if (!bus) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bus is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bus->config = *config;
    } else {
        hypo_ibus_default_config(&bus->config);
    }

    /* Initialize mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_RECURSIVE;
    bus->mutex = nimcp_mutex_create(&attr);
    if (!bus->mutex) {
        nimcp_free(bus);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_ibus_create: bus->mutex is NULL");
        return NULL;
    }

    /* Initialize module modulation factors to 1.0 (no modulation) */
    for (int i = 0; i < HYPO_IMOD_COUNT; i++) {
        bus->module_modulation[i] = 1.0f;
    }

    return bus;
}

void hypo_ibus_destroy(hypo_ibus_t bus) {
    if (!bus) return;

    if (bus->mutex) {
        nimcp_mutex_destroy(bus->mutex);
    }

    nimcp_free(bus);
}

int hypo_ibus_reset(hypo_ibus_t bus) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_reset: bus is NULL");
        return -1;
    }

    nimcp_mutex_lock(bus->mutex);

    /* Clear active modulations */
    memset(bus->active_mods, 0, sizeof(bus->active_mods));
    bus->num_active_mods = 0;

    /* Reset modulation factors */
    for (int i = 0; i < HYPO_IMOD_COUNT; i++) {
        bus->module_modulation[i] = 1.0f;
    }

    /* Reset statistics */
    memset(&bus->stats, 0, sizeof(bus->stats));

    /* Keep subscriptions and rules */

    nimcp_mutex_unlock(bus->mutex);
    return 0;
}

/* ============================================================================
 * SUBSCRIPTION
 * ============================================================================ */

int hypo_ibus_subscribe(
    hypo_ibus_t bus,
    hypo_internal_module_t subscriber,
    hypo_internal_event_type_t event_type,
    hypo_ibus_callback_t callback,
    void* user_data
) {
    if (!bus || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_subscribe: bus or callback is NULL");
        return -1;
    }
    if (subscriber >= HYPO_IMOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_subscribe: invalid subscriber");
        return -1;
    }
    if (event_type >= HYPO_IEVT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_subscribe: invalid event_type");
        return -1;
    }

    nimcp_mutex_lock(bus->mutex);

    /* Find free slot */
    int slot = -1;
    for (uint32_t i = 0; i < HYPO_IBUS_MAX_SUBSCRIBERS; i++) {
        if (!bus->subscriptions[i].active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_mutex_unlock(bus->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_subscribe: validation failed");
        return -1;  /* No free slots */
    }

    bus->subscriptions[slot].active = true;
    bus->subscriptions[slot].subscriber = subscriber;
    bus->subscriptions[slot].event_type = event_type;
    bus->subscriptions[slot].source_filter = HYPO_IMOD_COUNT;  /* No filter */
    bus->subscriptions[slot].callback = callback;
    bus->subscriptions[slot].user_data = user_data;

    bus->num_subscriptions++;
    bus->stats.active_subscribers++;

    int subscription_id = bus->next_subscription_id++;

    nimcp_mutex_unlock(bus->mutex);
    return subscription_id;
}

int hypo_ibus_subscribe_to_module(
    hypo_ibus_t bus,
    hypo_internal_module_t subscriber,
    hypo_internal_module_t source_module,
    hypo_ibus_callback_t callback,
    void* user_data
) {
    if (!bus || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_subscribe_to_module: bus or callback is NULL");
        return -1;
    }
    if (subscriber >= HYPO_IMOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_subscribe_to_module: invalid subscriber");
        return -1;
    }
    if (source_module >= HYPO_IMOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_subscribe_to_module: invalid source_module");
        return -1;
    }

    nimcp_mutex_lock(bus->mutex);

    int slot = -1;
    for (uint32_t i = 0; i < HYPO_IBUS_MAX_SUBSCRIBERS; i++) {
        if (!bus->subscriptions[i].active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_mutex_unlock(bus->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_subscribe_to_module: validation failed");
        return -1;
    }

    bus->subscriptions[slot].active = true;
    bus->subscriptions[slot].subscriber = subscriber;
    bus->subscriptions[slot].event_type = HYPO_IEVT_COUNT;  /* All events */
    bus->subscriptions[slot].source_filter = source_module;
    bus->subscriptions[slot].callback = callback;
    bus->subscriptions[slot].user_data = user_data;

    bus->num_subscriptions++;
    bus->stats.active_subscribers++;

    int subscription_id = bus->next_subscription_id++;

    nimcp_mutex_unlock(bus->mutex);
    return subscription_id;
}

int hypo_ibus_unsubscribe(hypo_ibus_t bus, int subscription_id) {
    if (!bus || subscription_id < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_unsubscribe: bus is NULL or invalid subscription_id");
        return -1;
    }

    nimcp_mutex_lock(bus->mutex);

    /* Find and deactivate subscription */
    for (uint32_t i = 0; i < HYPO_IBUS_MAX_SUBSCRIBERS; i++) {
        if (bus->subscriptions[i].active) {
            /* Simple approach: deactivate first active one */
            /* In production, would track subscription IDs properly */
            bus->subscriptions[i].active = false;
            bus->num_subscriptions--;
            if (bus->stats.active_subscribers > 0) {
                bus->stats.active_subscribers--;
            }
            nimcp_mutex_unlock(bus->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bus->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_unsubscribe: validation failed");
    return -1;
}

int hypo_ibus_unsubscribe_module(hypo_ibus_t bus, hypo_internal_module_t module) {
    if (!bus || module >= HYPO_IMOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_unsubscribe_module: bus is NULL or invalid module");
        return -1;
    }

    nimcp_mutex_lock(bus->mutex);

    int count = 0;
    for (uint32_t i = 0; i < HYPO_IBUS_MAX_SUBSCRIBERS; i++) {
        if (bus->subscriptions[i].active &&
            bus->subscriptions[i].subscriber == module) {
            bus->subscriptions[i].active = false;
            bus->num_subscriptions--;
            if (bus->stats.active_subscribers > 0) {
                bus->stats.active_subscribers--;
            }
            count++;
        }
    }

    nimcp_mutex_unlock(bus->mutex);
    return count;
}

/* ============================================================================
 * PUBLISHING
 * ============================================================================ */

int hypo_ibus_publish(hypo_ibus_t bus, const hypo_internal_event_t* event) {
    if (!bus || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_publish: bus or event is NULL");
        return -1;
    }
    if (event->type >= HYPO_IEVT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_publish: invalid event type");
        return -1;
    }
    if (event->source >= HYPO_IMOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_publish: invalid event source");
        return -1;
    }

    nimcp_mutex_lock(bus->mutex);

    /* Update statistics */
    bus->stats.events_published++;
    if (event->source < HYPO_IMOD_COUNT) {
        bus->stats.module_events[event->source]++;
    }

    /* Deliver to matching subscribers */
    int delivered = 0;
    for (uint32_t i = 0; i < HYPO_IBUS_MAX_SUBSCRIBERS; i++) {
        ibus_subscription_t* sub = &bus->subscriptions[i];
        if (!sub->active) continue;

        /* Check event type match */
        bool type_match = (sub->event_type == event->type) ||
                          (sub->event_type == HYPO_IEVT_COUNT);  /* All events */

        /* Check source filter */
        bool source_match = (sub->source_filter == HYPO_IMOD_COUNT) ||
                            (sub->source_filter == event->source);

        if (type_match && source_match) {
            /* Call subscriber callback */
            int result = sub->callback(event, sub->user_data);
            if (result >= 0) {
                delivered++;
                bus->stats.events_delivered++;
                if (sub->subscriber < HYPO_IMOD_COUNT) {
                    bus->stats.module_receives[sub->subscriber]++;
                }
            }
        }
    }

    /* Apply modulation rules if enabled */
    if (bus->config.enable_modulation) {
        for (uint32_t i = 0; i < MAX_MODULATION_RULES; i++) {
            ibus_modulation_rule_t* rule = &bus->rules[i];
            if (!rule->active) continue;

            if (rule->source_event == event->type) {
                /* Activate this modulation */
                for (uint32_t j = 0; j < MAX_ACTIVE_MODULATIONS; j++) {
                    if (!bus->active_mods[j].active) {
                        bus->active_mods[j].active = true;
                        bus->active_mods[j].trigger_event = event->type;
                        bus->active_mods[j].modulation = rule->modulation;
                        bus->active_mods[j].current_factor = rule->modulation.modulation_factor;
                        bus->active_mods[j].start_time_us = event->timestamp_us;
                        bus->active_mods[j].remaining_us = rule->modulation.duration_us;
                        bus->num_active_mods++;
                        bus->stats.modulations_applied++;

                        /* Update module modulation factor */
                        hypo_internal_module_t target = rule->modulation.target;
                        if (target < HYPO_IMOD_COUNT) {
                            if (rule->modulation.is_additive) {
                                bus->module_modulation[target] += (rule->modulation.modulation_factor - 1.0f);
                            } else {
                                bus->module_modulation[target] *= rule->modulation.modulation_factor;
                            }
                            /* Clamp to biologically reasonable range [0.0, 2.0] */
                            if (bus->module_modulation[target] < 0.0f) {
                                bus->module_modulation[target] = 0.0f;
                            } else if (bus->module_modulation[target] > 2.0f) {
                                bus->module_modulation[target] = 2.0f;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    nimcp_mutex_unlock(bus->mutex);
    return 0;  /* Success - delivery count tracked in stats */
}

int hypo_ibus_publish_circadian_phase(
    hypo_ibus_t bus,
    uint32_t old_phase,
    uint32_t new_phase,
    float melatonin,
    float cortisol,
    float alertness
) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_publish_circadian_phase: bus is NULL");
        return -1;
    }

    hypo_internal_event_t event = {0};
    event.type = HYPO_IEVT_CIRCADIAN_PHASE_CHANGE;
    event.source = HYPO_IMOD_CIRCADIAN;
    event.timestamp_us = nimcp_time_get_us();
    event.sequence_id = bus->next_sequence_id++;

    event.data.circadian.old_phase = old_phase;
    event.data.circadian.new_phase = new_phase;
    event.data.circadian.melatonin_level = melatonin;
    event.data.circadian.cortisol_level = cortisol;
    event.data.circadian.alertness = alertness;

    return hypo_ibus_publish(bus, &event);
}

int hypo_ibus_publish_stress(
    hypo_ibus_t bus,
    hypo_internal_event_type_t event_type,
    float stress_level,
    float cortisol,
    bool is_acute
) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_publish_stress: bus is NULL");
        return -1;
    }
    if (event_type != HYPO_IEVT_STRESS_ONSET &&
        event_type != HYPO_IEVT_STRESS_PEAK &&
        event_type != HYPO_IEVT_STRESS_RECOVERY &&
        event_type != HYPO_IEVT_CORTISOL_ELEVATED &&
        event_type != HYPO_IEVT_CORTISOL_NORMALIZED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_publish_stress: invalid event_type");
        return -1;
    }

    hypo_internal_event_t event = {0};
    event.type = event_type;
    event.source = HYPO_IMOD_HPA_AXIS;
    event.timestamp_us = nimcp_time_get_us();
    event.sequence_id = bus->next_sequence_id++;

    event.data.stress.stress_level = stress_level;
    event.data.stress.cortisol_level = cortisol;
    event.data.stress.is_acute = is_acute;

    return hypo_ibus_publish(bus, &event);
}

int hypo_ibus_publish_drive(
    hypo_ibus_t bus,
    hypo_internal_event_type_t event_type,
    uint32_t drive_type,
    float drive_level,
    float urgency
) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_publish_drive: bus is NULL");
        return -1;
    }

    hypo_internal_event_t event = {0};
    event.type = event_type;
    event.source = HYPO_IMOD_DRIVES;
    event.timestamp_us = nimcp_time_get_us();
    event.sequence_id = bus->next_sequence_id++;

    event.data.drive.drive_type = drive_type;
    event.data.drive.drive_level = drive_level;
    event.data.drive.urgency = urgency;

    return hypo_ibus_publish(bus, &event);
}

int hypo_ibus_publish_autonomic(
    hypo_ibus_t bus,
    hypo_internal_event_type_t event_type,
    float sympathetic,
    float parasympathetic
) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_publish_autonomic: bus is NULL");
        return -1;
    }

    hypo_internal_event_t event = {0};
    event.type = event_type;
    event.source = HYPO_IMOD_AUTONOMIC;
    event.timestamp_us = nimcp_time_get_us();
    event.sequence_id = bus->next_sequence_id++;

    event.data.autonomic.sympathetic_tone = sympathetic;
    event.data.autonomic.parasympathetic_tone = parasympathetic;
    event.data.autonomic.balance = sympathetic - parasympathetic;

    return hypo_ibus_publish(bus, &event);
}

/* ============================================================================
 * MODULATION
 * ============================================================================ */

int hypo_ibus_register_modulation(
    hypo_ibus_t bus,
    hypo_internal_event_type_t source_event,
    const hypo_ibus_modulation_t* modulation
) {
    if (!bus || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_register_modulation: bus or modulation is NULL");
        return -1;
    }
    if (source_event >= HYPO_IEVT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_register_modulation: invalid source_event");
        return -1;
    }
    if (modulation->target >= HYPO_IMOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_register_modulation: invalid modulation target");
        return -1;
    }

    nimcp_mutex_lock(bus->mutex);

    int slot = -1;
    for (uint32_t i = 0; i < MAX_MODULATION_RULES; i++) {
        if (!bus->rules[i].active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_mutex_unlock(bus->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_ibus_register_modulation: validation failed");
        return -1;
    }

    bus->rules[slot].active = true;
    bus->rules[slot].source_event = source_event;
    bus->rules[slot].modulation = *modulation;
    bus->num_rules++;

    nimcp_mutex_unlock(bus->mutex);
    return slot;
}

float hypo_ibus_get_modulation(
    hypo_ibus_t bus,
    hypo_internal_module_t module,
    uint32_t parameter_id
) {
    if (!bus || module >= HYPO_IMOD_COUNT) return 1.0f;

    nimcp_mutex_lock(bus->mutex);
    float factor = bus->module_modulation[module];
    nimcp_mutex_unlock(bus->mutex);

    return factor;
}

int hypo_ibus_update_modulations(hypo_ibus_t bus, uint64_t delta_us) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_update_modulations: bus is NULL");
        return -1;
    }

    nimcp_mutex_lock(bus->mutex);

    /* Decay active modulations */
    for (uint32_t i = 0; i < MAX_ACTIVE_MODULATIONS; i++) {
        ibus_active_modulation_t* mod = &bus->active_mods[i];
        if (!mod->active) continue;

        if (mod->remaining_us <= delta_us) {
            /* Modulation expired */
            hypo_internal_module_t target = mod->modulation.target;
            if (target < HYPO_IMOD_COUNT) {
                /* Remove modulation effect */
                if (mod->modulation.is_additive) {
                    bus->module_modulation[target] -= (mod->modulation.modulation_factor - 1.0f);
                } else {
                    if (mod->modulation.modulation_factor != 0.0f) {
                        bus->module_modulation[target] /= mod->modulation.modulation_factor;
                    }
                }
            }
            mod->active = false;
            bus->num_active_mods--;
        } else {
            mod->remaining_us -= delta_us;
        }
    }

    nimcp_mutex_unlock(bus->mutex);
    return 0;
}

int hypo_ibus_clear_modulations(hypo_ibus_t bus) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_clear_modulations: bus is NULL");
        return -1;
    }

    nimcp_mutex_lock(bus->mutex);

    memset(bus->active_mods, 0, sizeof(bus->active_mods));
    bus->num_active_mods = 0;

    for (int i = 0; i < HYPO_IMOD_COUNT; i++) {
        bus->module_modulation[i] = 1.0f;
    }

    nimcp_mutex_unlock(bus->mutex);
    return 0;
}

/* ============================================================================
 * QUERY
 * ============================================================================ */

int hypo_ibus_get_stats(hypo_ibus_t bus, hypo_ibus_stats_t* stats) {
    if (!bus || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_get_stats: bus or stats is NULL");
        return -1;
    }

    nimcp_mutex_lock(bus->mutex);
    *stats = bus->stats;

    /* Dynamically count active subscribers from subscription array */
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < HYPO_IBUS_MAX_SUBSCRIBERS; i++) {
        if (bus->subscriptions[i].active) {
            active_count++;
        }
    }
    stats->active_subscribers = active_count;

    nimcp_mutex_unlock(bus->mutex);

    return 0;
}

int hypo_ibus_reset_stats(hypo_ibus_t bus) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_reset_stats: bus is NULL");
        return -1;
    }

    nimcp_mutex_lock(bus->mutex);
    uint32_t active_subs = bus->stats.active_subscribers;
    memset(&bus->stats, 0, sizeof(bus->stats));
    bus->stats.active_subscribers = active_subs;
    nimcp_mutex_unlock(bus->mutex);

    return 0;
}

bool hypo_ibus_has_subscribers(hypo_ibus_t bus, hypo_internal_module_t module) {
    if (!bus || module >= HYPO_IMOD_COUNT) {
        return false;
    }

    nimcp_mutex_lock(bus->mutex);

    bool has_subs = false;
    for (uint32_t i = 0; i < HYPO_IBUS_MAX_SUBSCRIBERS; i++) {
        if (bus->subscriptions[i].active &&
            bus->subscriptions[i].source_filter == module) {
            has_subs = true;
            break;
        }
    }

    nimcp_mutex_unlock(bus->mutex);
    return has_subs;
}

uint32_t hypo_ibus_subscriber_count(
    hypo_ibus_t bus,
    hypo_internal_event_type_t event_type
) {
    if (!bus || event_type >= HYPO_IEVT_COUNT) return 0;

    nimcp_mutex_lock(bus->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < HYPO_IBUS_MAX_SUBSCRIBERS; i++) {
        if (bus->subscriptions[i].active &&
            (bus->subscriptions[i].event_type == event_type ||
             bus->subscriptions[i].event_type == HYPO_IEVT_COUNT)) {
            count++;
        }
    }

    nimcp_mutex_unlock(bus->mutex);
    return count;
}

/* ============================================================================
 * UTILITY
 * ============================================================================ */

const char* hypo_ibus_module_name(hypo_internal_module_t module) {
    if (module >= HYPO_IMOD_COUNT) return "Unknown";
    return MODULE_NAMES[module];
}

const char* hypo_ibus_event_name(hypo_internal_event_type_t event_type) {
    if (event_type >= HYPO_IEVT_COUNT) return "Unknown";
    return EVENT_NAMES[event_type];
}

void hypo_ibus_print_summary(hypo_ibus_t bus) {
    if (!bus) {
        printf("Hypothalamus Internal Bus: NULL\n");
        return;
    }

    nimcp_mutex_lock(bus->mutex);

    printf("=== Hypothalamus Internal Bus ===\n");
    printf("Subscriptions: %u active\n", bus->num_subscriptions);
    printf("Modulation Rules: %u\n", bus->num_rules);
    printf("Active Modulations: %u\n", bus->num_active_mods);
    printf("Events Published: %lu\n", (unsigned long)bus->stats.events_published);
    printf("Events Delivered: %lu\n", (unsigned long)bus->stats.events_delivered);

    printf("Module Modulations:\n");
    for (int i = 0; i < HYPO_IMOD_COUNT; i++) {
        if (bus->module_modulation[i] != 1.0f) {
            printf("  %s: %.3f\n", MODULE_NAMES[i], bus->module_modulation[i]);
        }
    }

    nimcp_mutex_unlock(bus->mutex);
}

void hypo_ibus_print_stats(const hypo_ibus_stats_t* stats) {
    if (!stats) {
        printf("Statistics: NULL\n");
        return;
    }

    printf("=== Internal Bus Statistics ===\n");
    printf("Events Published: %lu\n", (unsigned long)stats->events_published);
    printf("Events Delivered: %lu\n", (unsigned long)stats->events_delivered);
    printf("Events Dropped: %lu\n", (unsigned long)stats->events_dropped);
    printf("Modulations Applied: %lu\n", (unsigned long)stats->modulations_applied);
    printf("Active Subscribers: %u\n", stats->active_subscribers);

    printf("Events per Module:\n");
    for (int i = 0; i < HYPO_IMOD_COUNT; i++) {
        if (stats->module_events[i] > 0) {
            printf("  %s: %lu sent, %lu received\n",
                   MODULE_NAMES[i],
                   (unsigned long)stats->module_events[i],
                   (unsigned long)stats->module_receives[i]);
        }
    }
}

/* ============================================================================
 * DEFAULT BIOLOGICAL MODULATIONS
 * ============================================================================ */

int hypo_ibus_register_default_modulations(hypo_ibus_t bus) {
    if (!bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_ibus_register_default_modulations: bus is NULL");
        return -1;
    }

    int count = 0;
    hypo_ibus_modulation_t mod;

    /* 1. Circadian phase → Hunger modulation (meal timing) */
    memset(&mod, 0, sizeof(mod));
    mod.target = HYPO_IMOD_APPETITE;
    mod.modulation_factor = 1.0f + bus->config.circadian_hunger_amplitude;
    mod.duration_us = 3600ULL * 1000000ULL;  /* 1 hour */
    mod.is_additive = false;
    if (hypo_ibus_register_modulation(bus, HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, &mod) >= 0) {
        count++;
    }

    /* 2. Melatonin onset → Fatigue increase (nighttime drowsiness) */
    memset(&mod, 0, sizeof(mod));
    mod.target = HYPO_IMOD_DRIVES;  /* Affects fatigue drive */
    mod.modulation_factor = 1.0f + bus->config.circadian_fatigue_amplitude;
    mod.parameter_id = 3;  /* HYPO_DRIVE_FATIGUE */
    mod.duration_us = 8ULL * 3600ULL * 1000000ULL;  /* 8 hours */
    mod.is_additive = false;
    if (hypo_ibus_register_modulation(bus, HYPO_IEVT_MELATONIN_ONSET, &mod) >= 0) {
        count++;
    }

    /* 3. Cortisol elevated → Appetite suppression (stress response) */
    memset(&mod, 0, sizeof(mod));
    mod.target = HYPO_IMOD_APPETITE;
    mod.modulation_factor = 1.0f - bus->config.cortisol_appetite_suppression;
    mod.duration_us = 30ULL * 60ULL * 1000000ULL;  /* 30 minutes */
    mod.is_additive = false;
    if (hypo_ibus_register_modulation(bus, HYPO_IEVT_CORTISOL_ELEVATED, &mod) >= 0) {
        count++;
    }

    /* 4. Fatigue onset → Curiosity reduction (resource conservation) */
    memset(&mod, 0, sizeof(mod));
    mod.target = HYPO_IMOD_DRIVES;  /* Affects curiosity drive */
    mod.modulation_factor = 1.0f - bus->config.fatigue_curiosity_reduction;
    mod.parameter_id = 5;  /* HYPO_DRIVE_CURIOSITY */
    mod.duration_us = 2ULL * 3600ULL * 1000000ULL;  /* 2 hours */
    mod.is_additive = false;
    if (hypo_ibus_register_modulation(bus, HYPO_IEVT_FATIGUE_ONSET, &mod) >= 0) {
        count++;
    }

    /* 5. Stress onset → Sympathetic activation */
    memset(&mod, 0, sizeof(mod));
    mod.target = HYPO_IMOD_AUTONOMIC;
    mod.modulation_factor = 1.5f;  /* 50% increase in sympathetic */
    mod.duration_us = 15ULL * 60ULL * 1000000ULL;  /* 15 minutes */
    mod.is_additive = false;
    if (hypo_ibus_register_modulation(bus, HYPO_IEVT_STRESS_ONSET, &mod) >= 0) {
        count++;
    }

    /* 6. Temperature alert → Fatigue increase (hyperthermia) */
    memset(&mod, 0, sizeof(mod));
    mod.target = HYPO_IMOD_DRIVES;
    mod.modulation_factor = 1.3f;  /* 30% fatigue increase */
    mod.parameter_id = 3;  /* HYPO_DRIVE_FATIGUE */
    mod.duration_us = 1ULL * 3600ULL * 1000000ULL;  /* 1 hour */
    mod.is_additive = false;
    if (hypo_ibus_register_modulation(bus, HYPO_IEVT_TEMPERATURE_ALERT, &mod) >= 0) {
        count++;
    }

    /* 7. Parasympathetic activation → Reduced safety drive (relaxation) */
    memset(&mod, 0, sizeof(mod));
    mod.target = HYPO_IMOD_DRIVES;
    mod.modulation_factor = 1.0f - bus->config.social_safety_modulation;
    mod.parameter_id = 6;  /* HYPO_DRIVE_SAFETY */
    mod.duration_us = 30ULL * 60ULL * 1000000ULL;  /* 30 minutes */
    mod.is_additive = false;
    if (hypo_ibus_register_modulation(bus, HYPO_IEVT_PARASYMPATHETIC_ACTIVATION, &mod) >= 0) {
        count++;
    }

    return count;
}
