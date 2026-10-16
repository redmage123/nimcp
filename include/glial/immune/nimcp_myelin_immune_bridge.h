/**
 * @file nimcp_myelin_immune_bridge.h
 * @brief Myelin-Immune Bridge - Models autoimmune demyelination (MS pathophysiology)
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bridge connecting myelin sheath to brain immune system
 * WHY:  Model autoimmune myelin attack (Multiple Sclerosis)
 * HOW:  Routes cytokines to modulate myelin integrity and conduction
 *
 * BIOLOGICAL BASIS:
 * - Autoimmune attack on myelin basic protein (MBP)
 * - Cytokine storm leads to rapid demyelination
 * - Conduction block from myelin loss
 * - Remyelination depends on inflammation resolution
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MYELIN_IMMUNE_BRIDGE_H
#define NIMCP_MYELIN_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MYELIN_IMMUNE_MODULE_NAME "myelin_immune_bridge"

typedef struct myelin_immune_bridge myelin_immune_bridge_t;
typedef struct nimcp_myelin_sheath nimcp_myelin_sheath_t;

typedef struct {
    float il1_damage_rate;
    float tnf_damage_rate;
    float ifn_gamma_damage_rate;
    float il10_repair_rate;
    float integrity_repair_rate;
    bool enable_bio_async;
    uint32_t inbox_capacity;
} myelin_immune_config_t;

typedef struct {
    float il1_damage;
    float tnf_damage;
    float ifn_gamma_damage;
    float il10_repair;
    float net_damage;
} myelin_cytokine_effects_t;

typedef struct {
    uint64_t damage_events;
    uint64_t repair_events;
    float total_integrity_lost;
    float total_integrity_restored;
    float current_integrity;
} myelin_immune_stats_t;

struct myelin_immune_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    myelin_immune_config_t config;
    nimcp_myelin_sheath_t* myelin;
    brain_immune_system_t* immune_system;
    myelin_cytokine_effects_t cytokine_effects;
    float sheath_integrity;
    float conduction_efficiency;
    float repair_rate;
    myelin_immune_stats_t stats;
    bool initialized;
};

int myelin_immune_default_config(myelin_immune_config_t* config);
myelin_immune_bridge_t* myelin_immune_create(
    const myelin_immune_config_t* config,
    nimcp_myelin_sheath_t* myelin,
    brain_immune_system_t* immune_system);
void myelin_immune_destroy(myelin_immune_bridge_t* bridge);

int myelin_immune_connect_bio_async(myelin_immune_bridge_t* bridge);
int myelin_immune_disconnect_bio_async(myelin_immune_bridge_t* bridge);
bool myelin_immune_is_bio_async_connected(const myelin_immune_bridge_t* bridge);

int myelin_immune_update_cytokine_effects(myelin_immune_bridge_t* bridge);
int myelin_immune_apply_damage(myelin_immune_bridge_t* bridge, float dt_ms);
int myelin_immune_apply_repair(myelin_immune_bridge_t* bridge, float dt_ms);
int myelin_immune_update(myelin_immune_bridge_t* bridge, float dt_ms);

float myelin_immune_get_integrity(const myelin_immune_bridge_t* bridge);
float myelin_immune_get_conduction_efficiency(const myelin_immune_bridge_t* bridge);
int myelin_immune_get_stats(const myelin_immune_bridge_t* bridge, myelin_immune_stats_t* stats);
void myelin_immune_reset_stats(myelin_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MYELIN_IMMUNE_BRIDGE_H */
