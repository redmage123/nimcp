/**
 * @file nimcp_mental_health_substrate_bridge.h
 * @brief Bridge between Mental Health monitoring and neural substrate
 *
 * WHAT: Links mental health state to metabolic state
 * WHY: Mental health is fundamentally tied to metabolic wellness
 * HOW: Monitors ATP/fatigue; modulates resilience, coping, wellbeing
 *
 * BIOLOGICAL BASIS:
 * - Mental health involves whole-brain metabolic balance
 * - ATP depletion correlates with depression/anxiety
 * - Chronic fatigue impairs emotional regulation
 * - Metabolic dysfunction underlies many mental health issues
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_MENTAL_HEALTH_SUBSTRATE_BRIDGE_H
#define NIMCP_MENTAL_HEALTH_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_MENTAL_HEALTH 0x1224

typedef struct {
    float resilience_level;       /* Psychological resilience [0-1] */
    float coping_capacity;        /* Ability to cope with stress [0-1] */
    float emotional_stability;    /* Emotional stability [0-1] */
    float wellbeing_level;        /* Overall wellbeing [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} mental_health_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} mental_health_substrate_config_t;

typedef struct mental_health_substrate_bridge mental_health_substrate_bridge_t;

mental_health_substrate_config_t mental_health_substrate_default_config(void);
mental_health_substrate_bridge_t* mental_health_substrate_bridge_create(void* mental_health, neural_substrate_t* substrate, const mental_health_substrate_config_t* config);
void mental_health_substrate_bridge_destroy(mental_health_substrate_bridge_t* bridge);
int mental_health_substrate_bridge_update(mental_health_substrate_bridge_t* bridge);
int mental_health_substrate_bridge_get_effects(const mental_health_substrate_bridge_t* bridge, mental_health_substrate_effects_t* effects);
int mental_health_substrate_bridge_apply_effects(mental_health_substrate_bridge_t* bridge);
int mental_health_substrate_bridge_register_bio_async(mental_health_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MENTAL_HEALTH_SUBSTRATE_BRIDGE_H */
