/**
 * @file nimcp_rate_limiter_sleep_bridge.h
 * @brief Sleep-Rate Limiter Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between sleep/wake system and rate limiter
 * WHY:  Sleep states affect acceptable rate limits and burst capacity
 * HOW:  Sleep state modulates rate limits, burst capacity, and penalty thresholds
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Standard rate limits (full vigilance)
 * - DROWSY: Slightly relaxed limits
 * - NREM: More permissive (metabolic demands decrease)
 * - REM: Moderate limits
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RATE_LIMITER_SLEEP_BRIDGE_H
#define NIMCP_RATE_LIMITER_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Rate limit modulation (higher = more permissive) */
#define RATE_LIMITER_SLEEP_RATE_AWAKE         1.0f
#define RATE_LIMITER_SLEEP_RATE_DROWSY        1.1f
#define RATE_LIMITER_SLEEP_RATE_LIGHT_NREM    1.3f
#define RATE_LIMITER_SLEEP_RATE_DEEP_NREM     1.5f
#define RATE_LIMITER_SLEEP_RATE_REM           1.2f

/* Burst capacity modulation */
#define RATE_LIMITER_SLEEP_BURST_AWAKE        1.0f
#define RATE_LIMITER_SLEEP_BURST_DROWSY       1.1f
#define RATE_LIMITER_SLEEP_BURST_LIGHT_NREM   1.4f
#define RATE_LIMITER_SLEEP_BURST_DEEP_NREM    1.6f
#define RATE_LIMITER_SLEEP_BURST_REM          1.3f

typedef struct {
    bool enable_rate_modulation;
    bool enable_burst_modulation;
    float modulation_strength;
} rate_limiter_sleep_config_t;

typedef struct {
    float rate_limit_factor;
    float burst_capacity_factor;
    float penalty_threshold_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool limits_relaxed;
} rate_limiter_sleep_effects_t;

typedef struct rate_limiter_sleep_bridge_struct* rate_limiter_sleep_bridge_t;

int rate_limiter_sleep_default_config(rate_limiter_sleep_config_t* config);
rate_limiter_sleep_bridge_t rate_limiter_sleep_bridge_create(
    const rate_limiter_sleep_config_t* config,
    sleep_system_t sleep_system);
void rate_limiter_sleep_bridge_destroy(rate_limiter_sleep_bridge_t bridge);
int rate_limiter_sleep_update(rate_limiter_sleep_bridge_t bridge);
int rate_limiter_sleep_get_effects(const rate_limiter_sleep_bridge_t bridge,
                                    rate_limiter_sleep_effects_t* effects);
float rate_limiter_sleep_get_effective_rate(const rate_limiter_sleep_bridge_t bridge, float base);
float rate_limiter_sleep_get_burst_capacity(const rate_limiter_sleep_bridge_t bridge, float base);
bool rate_limiter_sleep_is_relaxed(const rate_limiter_sleep_bridge_t bridge);

float rate_limiter_sleep_get_rate_factor(sleep_state_t state);
float rate_limiter_sleep_get_burst_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RATE_LIMITER_SLEEP_BRIDGE_H */
