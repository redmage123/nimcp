/**
 * @file nimcp_security_fep_bridge.h
 * @brief Free Energy Principle bridge for unified Security system
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: FEP integration for comprehensive security - allostatic threat response
 * WHY:  Security system maintains allostatic balance under FEP
 * HOW:  Map security threats to free energy, use active inference for defense
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SECURITY AS ALLOSTATIC REGULATION:
 * - Core directives = fixed priors (protected beliefs)
 * - Input validation = precision filtering
 * - Skepticism = high precision on external information
 * - Encryption = maintaining prediction consistency
 * - Biological defenses = homeostatic stability
 *
 * FEP INTEGRATION:
 * ```
 * Security Observation (o) → Threat Detection
 *         ↓
 * Expected Security State μ (baseline)
 *         ↓
 * Prediction Error: ε = observed - expected security
 *         ↓
 * Free Energy F → Threat Level
 *         ↓
 * Active Inference: Security Actions
 *   - Low FE → ALLOW (normal state)
 *   - Medium FE → VALIDATE (skepticism)
 *   - High FE → BLOCK (threat mitigation)
 *   - Very high FE → EMERGENCY (system protection)
 * ```
 *
 * ALLOSTATIC BALANCE:
 * - Normal operations = low free energy
 * - Attacks = high surprise → defensive actions
 * - Adaptive security = precision modulation
 * - Multiple defenses = hierarchical predictive coding
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║             SECURITY - FEP BRIDGE (Allostatic Defense)                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║   ┌──────────────────┐         ┌──────────────────┐                      ║
 * ║   │  FEP System      │────────▶│  Security System │                      ║
 * ║   │  • Free Energy   │         │  • Directives    │                      ║
 * ║   │  • Active Inf    │         │  • Validation    │                      ║
 * ║   │  • Precision     │         │  • Encryption    │                      ║
 * ║   └──────────────────┘         └──────────────────┘                      ║
 * ║           ↓                              ↓                                ║
 * ║   FEP → Security:               Security → FEP:                           ║
 * ║   • FE → Threat level           • Threats → High surprise                ║
 * ║   • Surprise → Skepticism       • Validations → Belief updates           ║
 * ║   • Precision → Input strictness• Attacks → Precision increase           ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_FEP_BRIDGE_H
#define NIMCP_SECURITY_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>

#include "security/nimcp_security.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
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
    float threat_fe_threshold;
    float skepticism_threshold;
    float precision_learning_rate;
    bool enable_adaptive_security;
    bool enable_online_learning;
    float learning_rate;
} security_fep_config_t;

typedef struct {
    float threat_level_score;
    float skepticism_score;
    float input_strictness;
    nimcp_threat_level_t recommended_threat_level;
} security_fep_effects_t;

typedef struct {
    uint64_t threats_detected;
    uint64_t inputs_validated;
    uint64_t attacks_blocked;
    float avg_threat_level;
} fep_security_effects_t;

typedef struct {
    bool active;
    uint64_t update_count;
    uint64_t validation_count;
    float current_precision;
} security_fep_state_t;

typedef struct {
    uint64_t total_validations;
    uint64_t fep_based_detections;
    uint64_t threats_found;
    float avg_free_energy;
    float current_precision;
} security_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    security_fep_config_t config;
    fep_system_t* fep_system;
    security_fep_effects_t fep_effects;
    fep_security_effects_t security_effects;
    security_fep_state_t state;
    security_fep_stats_t stats;} security_fep_bridge_t;

/* ============================================================================
 * API
 * ============================================================================ */

int security_fep_default_config(security_fep_config_t* config);
security_fep_bridge_t* security_fep_create(const security_fep_config_t* config,
    fep_system_t* fep_system);
void security_fep_destroy(security_fep_bridge_t* bridge);
int security_fep_update(security_fep_bridge_t* bridge);
int security_fep_validate_input(security_fep_bridge_t* bridge, const char* input,
    nimcp_input_validation_t* result, nimcp_threat_level_t* threat);
int security_fep_apply_modulation(security_fep_bridge_t* bridge);
int security_fep_report_threat(security_fep_bridge_t* bridge,
    nimcp_threat_level_t level);
int security_fep_get_stats(const security_fep_bridge_t* bridge,
    security_fep_stats_t* stats);
int security_fep_get_effects(const security_fep_bridge_t* bridge,
    security_fep_effects_t* effects);
int security_fep_connect_bio_async(security_fep_bridge_t* bridge);
int security_fep_disconnect_bio_async(security_fep_bridge_t* bridge);
bool security_fep_is_bio_async_connected(const security_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_FEP_BRIDGE_H */
