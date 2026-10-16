/**
 * @file nimcp_motor_substrate_bridge.h
 * @brief Bridge between Motor system and neural substrate
 *
 * WHAT: Links motor control to metabolic state
 * WHY: Motor execution requires significant ATP for muscle and motor cortex
 * HOW: Monitors ATP/fatigue; modulates motor precision, speed, endurance
 *
 * BIOLOGICAL BASIS:
 * - Motor control involves motor cortex, basal ganglia, cerebellum
 * - ATP depletion directly affects muscle contraction
 * - Fatigue reduces motor precision and speed
 * - Metabolic stress affects motor learning and coordination
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_MOTOR_SUBSTRATE_BRIDGE_H
#define NIMCP_MOTOR_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_MOTOR 0x1230

typedef struct {
    float motor_precision;        /* Precision of movements [0-1] */
    float motor_speed;            /* Speed of motor execution [0-1] */
    float motor_endurance;        /* Motor endurance capacity [0-1] */
    float coordination;           /* Movement coordination [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} motor_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} motor_substrate_config_t;

typedef struct motor_substrate_bridge motor_substrate_bridge_t;

motor_substrate_config_t motor_substrate_default_config(void);
motor_substrate_bridge_t* motor_substrate_bridge_create(void* motor, neural_substrate_t* substrate, const motor_substrate_config_t* config);
void motor_substrate_bridge_destroy(motor_substrate_bridge_t* bridge);
int motor_substrate_bridge_update(motor_substrate_bridge_t* bridge);
int motor_substrate_bridge_get_effects(const motor_substrate_bridge_t* bridge, motor_substrate_effects_t* effects);
int motor_substrate_bridge_apply_effects(motor_substrate_bridge_t* bridge);
int motor_substrate_bridge_register_bio_async(motor_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MOTOR_SUBSTRATE_BRIDGE_H */
