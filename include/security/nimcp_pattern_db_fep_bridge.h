/**
 * @file nimcp_pattern_db_fep_bridge.h
 * @brief Free Energy Principle bridge for Pattern Database
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: FEP integration for pattern matching - learned generative model
 * WHY:  Pattern database represents learned threat priors in FEP framework
 * HOW:  Map pattern matches to low free energy, mismatches to high surprise
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PATTERN DATABASE AS GENERATIVE MODEL:
 * - Stored patterns = learned priors p(s) over threat types
 * - Pattern matching = likelihood p(o|s) computation
 * - Pattern updates = belief updates under FEP
 * - Pattern rollback = maintaining model consistency
 *
 * FEP INTEGRATION:
 * ```
 * Input String (o) → Pattern Matching
 *         ↓
 * Pattern Library = Generative Model p(o,s)
 *         ↓
 * Match → Low Free Energy (expected threat)
 * No Match → High Surprise (novel pattern)
 *         ↓
 * Active Inference: Add new patterns (minimize future surprise)
 * ```
 *
 * THREAT LEARNING:
 * - Known patterns = low surprise (already in model)
 * - Novel threats = high surprise → add to database
 * - Pattern versioning = model snapshots for rollback
 * - Adaptive thresholds = precision modulation
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           PATTERN DB - FEP BRIDGE (Generative Model Learning)            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║   ┌──────────────────┐         ┌──────────────────┐                      ║
 * ║   │  FEP System      │────────▶│  Pattern DB      │                      ║
 * ║   │  • Free Energy   │         │  • Patterns      │                      ║
 * ║   │  • Surprise      │         │  • Matching      │                      ║
 * ║   │  • Model Updates │         │  • Versioning    │                      ║
 * ║   └──────────────────┘         └──────────────────┘                      ║
 * ║           ↓                              ↓                                ║
 * ║   FEP → PatternDB:                  PatternDB → FEP:                      ║
 * ║   • Free energy → Match score       • Matches → Expected obs             ║
 * ║   • Surprise → Add pattern          • Misses → High surprise             ║
 * ║   • Precision → Match threshold     • Updates → Belief updates           ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PATTERN_DB_FEP_BRIDGE_H
#define NIMCP_PATTERN_DB_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>

#include "security/nimcp_pattern_db.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Structures
 * ============================================================================ */

typedef struct {
    float match_fe_threshold;
    float surprise_add_threshold;
    float precision_learning_rate;
    bool enable_adaptive_matching;
    bool enable_online_learning;
    float learning_rate;
} pattern_fep_config_t;

typedef struct {
    float match_score;
    float surprise_score;
    float match_threshold;
} pattern_fep_effects_t;

typedef struct {
    uint64_t patterns_matched;
    uint64_t patterns_added;
    uint64_t mismatches;
    float avg_match_score;
} fep_pattern_effects_t;

typedef struct {
    bool active;
    uint64_t update_count;
    uint64_t match_count;
    float current_precision;
} pattern_fep_state_t;

typedef struct {
    uint64_t total_matches;
    uint64_t fep_based_matches;
    uint64_t patterns_added_via_fep;
    float avg_free_energy;
    float current_precision;
} pattern_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    pattern_fep_config_t config;
    fep_system_t* fep_system;
    nimcp_pattern_db_t pattern_db;
    pattern_fep_effects_t fep_effects;
    fep_pattern_effects_t pattern_effects;
    pattern_fep_state_t state;
    pattern_fep_stats_t stats;} pattern_fep_bridge_t;

/* ============================================================================
 * API
 * ============================================================================ */

int pattern_fep_default_config(pattern_fep_config_t* config);
pattern_fep_bridge_t* pattern_fep_create(const pattern_fep_config_t* config,
    nimcp_pattern_db_t pattern_db, fep_system_t* fep_system);
void pattern_fep_destroy(pattern_fep_bridge_t* bridge);
int pattern_fep_update(pattern_fep_bridge_t* bridge);
int pattern_fep_match(pattern_fep_bridge_t* bridge, const char* input,
    nimcp_pattern_match_result_t* result);
int pattern_fep_apply_modulation(pattern_fep_bridge_t* bridge);
int pattern_fep_get_effects(const pattern_fep_bridge_t* bridge, pattern_fep_effects_t* effects);
int pattern_fep_get_stats(const pattern_fep_bridge_t* bridge, pattern_fep_stats_t* stats);
int pattern_fep_connect_bio_async(pattern_fep_bridge_t* bridge);
int pattern_fep_disconnect_bio_async(pattern_fep_bridge_t* bridge);
bool pattern_fep_is_bio_async_connected(const pattern_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PATTERN_DB_FEP_BRIDGE_H */
