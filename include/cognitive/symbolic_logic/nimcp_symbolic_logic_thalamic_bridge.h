/**
 * @file nimcp_symbolic_logic_thalamic_bridge.h
 * @brief Bridge between Symbolic Logic system and thalamic router
 *
 * WHAT: Routes symbolic logic signals through thalamic attention pathways
 * WHY: Logical operations require conscious attention for rule application
 * HOW: Packages logic signals, routes via MD nucleus pathway
 *
 * BIOLOGICAL BASIS:
 * - Symbolic reasoning involves DLPFC-thalamic circuits
 * - MD nucleus supports rule maintenance and application
 * - Pulvinar coordinates attention during logical operations
 * - Anterior thalamus links premises to working memory
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_THALAMIC_BRIDGE_H
#define NIMCP_SYMBOLIC_LOGIC_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYMBOLIC_SIGNAL_RULE_APPLY    0x3501  /**< Rule application */
#define SYMBOLIC_SIGNAL_UNIFICATION   0x3502  /**< Unification step */
#define SYMBOLIC_SIGNAL_INFERENCE     0x3503  /**< Logical inference */
#define SYMBOLIC_SIGNAL_PROOF         0x3504  /**< Proof step */

typedef struct {
    uint32_t signal_type;
    float logic_urgency;
    float complexity;
    float confidence;
    float proof_depth;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} symbolic_logic_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_complexity_routing;
    float min_urgency_threshold;
    float complexity_boost;
} symbolic_logic_thalamic_config_t;

typedef struct {
    uint64_t rules_applied;
    uint64_t unifications;
    uint64_t inferences;
    uint64_t proof_steps;
    uint64_t signals_gated;
    float avg_complexity;
    float avg_proof_depth;
} symbolic_logic_thalamic_stats_t;

typedef struct symbolic_logic_thalamic_bridge symbolic_logic_thalamic_bridge_t;

symbolic_logic_thalamic_config_t symbolic_logic_thalamic_default_config(void);
symbolic_logic_thalamic_bridge_t* symbolic_logic_thalamic_bridge_create(
    void* symbolic_logic, thalamic_router_t* router,
    const symbolic_logic_thalamic_config_t* config);
void symbolic_logic_thalamic_bridge_destroy(symbolic_logic_thalamic_bridge_t* bridge);
int symbolic_logic_thalamic_bridge_reset(symbolic_logic_thalamic_bridge_t* bridge);

int symbolic_logic_thalamic_route_signal(
    symbolic_logic_thalamic_bridge_t* bridge,
    const symbolic_logic_thalamic_signal_t* signal);
int symbolic_logic_thalamic_apply_rule(
    symbolic_logic_thalamic_bridge_t* bridge,
    float complexity, float urgency);

int symbolic_logic_thalamic_set_attention(symbolic_logic_thalamic_bridge_t* bridge, float attention);
int symbolic_logic_thalamic_get_attention(const symbolic_logic_thalamic_bridge_t* bridge, float* attention);
int symbolic_logic_thalamic_bridge_get_stats(
    const symbolic_logic_thalamic_bridge_t* bridge,
    symbolic_logic_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SYMBOLIC_LOGIC_THALAMIC_BRIDGE_H */
