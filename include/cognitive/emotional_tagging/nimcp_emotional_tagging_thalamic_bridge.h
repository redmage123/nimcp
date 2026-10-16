/**
 * @file nimcp_emotional_tagging_thalamic_bridge.h
 * @brief Bridge between Emotional Tagging system and thalamic router
 *
 * WHAT: Routes emotional tag signals through thalamic pathways
 * WHY: Emotional tags require attention-gated routing to memory/cognition
 * HOW: Packages tagging signals, routes via amygdala-thalamic pathways
 *
 * BIOLOGICAL BASIS:
 * - Amygdala projects to thalamus for emotional arousal
 * - Mediodorsal nucleus integrates emotional with cognitive
 * - Pulvinar routes emotional salience to attention
 * - Anterior thalamus links emotion to memory encoding
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EMOTIONAL_TAGGING_THALAMIC_BRIDGE_H
#define NIMCP_EMOTIONAL_TAGGING_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Signal types for emotional tagging thalamic routing */
#define ETAG_SIGNAL_TAG_APPLY       0x2B01  /**< Apply emotional tag */
#define ETAG_SIGNAL_TAG_UPDATE      0x2B02  /**< Update existing tag */
#define ETAG_SIGNAL_TAG_RETRIEVE    0x2B03  /**< Retrieve tagged memory */
#define ETAG_SIGNAL_SALIENCE_BOOST  0x2B04  /**< Boost salience of tagged item */

typedef struct {
    uint32_t signal_type;
    float emotional_intensity;      /**< Intensity of emotion [0-1] */
    float valence;                  /**< Valence [-1 to +1] */
    float memory_strength;          /**< Memory encoding strength [0-1] */
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} emotional_tagging_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_intensity_boost;
    float min_intensity_threshold;
    float salience_boost_factor;
} emotional_tagging_thalamic_config_t;

typedef struct {
    uint64_t tags_applied;
    uint64_t tags_updated;
    uint64_t retrievals;
    uint64_t salience_boosts;
    uint64_t signals_gated;
    float avg_emotional_intensity;
} emotional_tagging_thalamic_stats_t;

typedef struct emotional_tagging_thalamic_bridge emotional_tagging_thalamic_bridge_t;

emotional_tagging_thalamic_config_t emotional_tagging_thalamic_default_config(void);
emotional_tagging_thalamic_bridge_t* emotional_tagging_thalamic_bridge_create(
    void* emotional_tagging, thalamic_router_t* router,
    const emotional_tagging_thalamic_config_t* config);
void emotional_tagging_thalamic_bridge_destroy(emotional_tagging_thalamic_bridge_t* bridge);
int emotional_tagging_thalamic_bridge_reset(emotional_tagging_thalamic_bridge_t* bridge);

int emotional_tagging_thalamic_route_signal(
    emotional_tagging_thalamic_bridge_t* bridge,
    const emotional_tagging_thalamic_signal_t* signal);
int emotional_tagging_thalamic_apply_tag(
    emotional_tagging_thalamic_bridge_t* bridge,
    float intensity, float valence);

int emotional_tagging_thalamic_set_attention(emotional_tagging_thalamic_bridge_t* bridge, float attention);
int emotional_tagging_thalamic_get_attention(emotional_tagging_thalamic_bridge_t* bridge, float* attention);
int emotional_tagging_thalamic_bridge_get_stats(
    const emotional_tagging_thalamic_bridge_t* bridge,
    emotional_tagging_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTIONAL_TAGGING_THALAMIC_BRIDGE_H */
