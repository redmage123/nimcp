/**
 * @file nimcp_snn_global_workspace_bridge.h
 * @brief SNN-Global Workspace integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and global workspace
 * WHY:  Enable spike-based conscious broadcast and competition
 * HOW:  Encode workspace broadcasts as spike rates, compete via population rates
 *
 * BIOLOGICAL BASIS:
 * - Global workspace = prefrontal-parietal network broadcast
 * - Broadcast strength encoded in spike rate (conscious access)
 * - Competition resolved by population firing rates
 * - Workspace ignition = threshold crossing in spike activity
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_GLOBAL_WORKSPACE_BRIDGE_H
#define NIMCP_SNN_GLOBAL_WORKSPACE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

typedef void* global_workspace_t;
typedef uint32_t cognitive_module_t;

typedef struct snn_global_workspace_config_s {
    float competition_rate_threshold;
    float broadcast_encoding_gain;
    float ignition_rate_threshold;
    uint32_t workspace_population_id;
    uint32_t competition_population_id;
    float update_interval_ms;
    bool enable_bio_async;
    cognitive_module_t module_id;
} snn_global_workspace_config_t;

typedef struct snn_global_workspace_state_s {
    float current_broadcast_rate;
    float competition_strength;
    bool is_broadcasting;
    bool is_competing;
    cognitive_module_t broadcast_source;
    uint32_t broadcast_count;
    uint32_t competition_wins;
} snn_global_workspace_state_t;

typedef struct snn_global_workspace_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    global_workspace_t* workspace;
    snn_global_workspace_config_t config;
    snn_global_workspace_state_t state;
    snn_population_t* workspace_pop;
    snn_population_t* competition_pop;
    float* broadcast_buffer;
    uint32_t buffer_capacity;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_global_workspace_bridge_t;

void snn_global_workspace_config_default(snn_global_workspace_config_t* config);
snn_global_workspace_bridge_t* snn_global_workspace_bridge_create(const snn_global_workspace_config_t* config, snn_network_t* snn, global_workspace_t* workspace);
void snn_global_workspace_bridge_destroy(snn_global_workspace_bridge_t* bridge);
int snn_global_workspace_bridge_connect_bio_async(snn_global_workspace_bridge_t* bridge);
int snn_global_workspace_bridge_disconnect_bio_async(snn_global_workspace_bridge_t* bridge);
bool snn_global_workspace_bridge_is_bio_async_connected(const snn_global_workspace_bridge_t* bridge);
int snn_global_workspace_bridge_process_broadcast(snn_global_workspace_bridge_t* bridge);
int snn_global_workspace_bridge_submit_competition(snn_global_workspace_bridge_t* bridge);
int snn_global_workspace_bridge_update(snn_global_workspace_bridge_t* bridge, float dt);
float snn_global_workspace_compute_competition_strength(const snn_global_workspace_bridge_t* bridge, float spike_rate);
int snn_global_workspace_bridge_get_state(const snn_global_workspace_bridge_t* bridge, snn_global_workspace_state_t* state);
float snn_global_workspace_get_broadcast_rate(const snn_global_workspace_bridge_t* bridge);
float snn_global_workspace_get_competition_strength(const snn_global_workspace_bridge_t* bridge);
bool snn_global_workspace_is_broadcasting(const snn_global_workspace_bridge_t* bridge);
int snn_global_workspace_get_stats(const snn_global_workspace_bridge_t* bridge, uint32_t* broadcast_count, uint32_t* wins, float* avg_strength);
void snn_global_workspace_reset_stats(snn_global_workspace_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_GLOBAL_WORKSPACE_BRIDGE_H */
