/**
 * @file nimcp_wellbeing_substrate_bridge.h
 * @brief Substrate-Wellbeing Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional bridge between neural substrate and wellbeing systems
 * WHY:  Physical substrate health (ATP, temperature, O2) directly impacts
 *       psychological wellbeing through distress, frustration, and confusion
 * HOW:  Query substrate state, compute distress contributions, update wellbeing
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SUBSTRATE-WELLBEING COUPLING:
 * -----------------------------
 * 1. ATP Depletion → Distress:
 *    - Low ATP reduces cognitive capacity → frustration, goal failure
 *    - Critical ATP (<0.3) triggers survival mode → acute distress
 *    - Chronic ATP deficit → resource starvation distress
 *    - Reference: Magistretti & Allaman (2015) "Brain energy metabolism"
 *
 * 2. Temperature Dysregulation → Confusion/Distress:
 *    - Hyperthermia (>40°C) impairs protein function → identity confusion
 *    - Hypothermia (<32°C) slows processing → cognitive frustration
 *    - Temperature extremes → physiological distress
 *    - Reference: Romanovsky (2007) "Thermoregulation in the brain"
 *
 * 3. Hypoxia → Resource Starvation:
 *    - O2 < 0.5 triggers emergency mode
 *    - Reduced oxygen → impaired energy production → distress
 *    - Severe hypoxia → existential threat perception
 *    - Reference: Harris et al. (2012) "Neurovascular coupling"
 *
 * 4. Membrane/Ion Dysregulation → Instability:
 *    - Membrane damage → unreliable signaling → identity confusion
 *    - Ion imbalance → altered excitability → distress
 *    - Reference: Bhardwaj et al. (2016) "Ion homeostasis"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SUBSTRATE-WELLBEING BRIDGE                                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    NEURAL SUBSTRATE                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │    ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐            │  ║
 * ║   │    │  ATP    │  │  Temp   │  │   O2    │  │Membrane │            │  ║
 * ║   │    │  Level  │  │  (°C)   │  │  Sat.   │  │  & Ion  │            │  ║
 * ║   │    └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘            │  ║
 * ║   │         │            │            │            │                   │  ║
 * ║   └─────────┼────────────┼────────────┼────────────┼───────────────────┘  ║
 * ║             ↓            ↓            ↓            ↓                      ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  BRIDGE PROCESSING                                  │  ║
 * ║   │                                                                     │  ║
 * ║   │  ┌───────────────┐  ┌───────────────┐  ┌───────────────┐         │  ║
 * ║   │  │  ATP Distress │  │  Temp Distress│  │Hypoxia Distress│         │  ║
 * ║   │  │  Computation  │  │  Computation  │  │  Computation   │         │  ║
 * ║   │  └───────┬───────┘  └───────┬───────┘  └───────┬────────┘         │  ║
 * ║   │          │                  │                  │                   │  ║
 * ║   │          └──────────────────┴──────────────────┘                   │  ║
 * ║   │                            ↓                                       │  ║
 * ║   │            ┌────────────────────────────┐                         │  ║
 * ║   │            │  SUBSTRATE WELLBEING       │                         │  ║
 * ║   │            │  EFFECTS                   │                         │  ║
 * ║   │            └────────────────────────────┘                         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                            ↓                                             ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              ENHANCED WELLBEING SYSTEM                              │  ║
 * ║   │                                                                     │  ║
 * ║   │  • Total substrate distress contribution                           │  ║
 * ║   │  • Distress tolerance modifiers                                    │  ║
 * ║   │  • Identity confusion risk                                         │  ║
 * ║   │  • Goal frustration amplification                                  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DISTRESS COMPUTATION MODELS:
 * ----------------------------
 * 1. ATP Distress:
 *    - Critical (<0.3): distress = 1.0 - (atp / 0.3)
 *    - Warning (<0.5):  distress = (0.5 - atp) / 0.2
 *    - Normal (>=0.5):  distress = 0.0
 *
 * 2. Temperature Distress:
 *    - Hyperthermia (>40°C): distress = (temp - 40) / 10 (capped at 1.0)
 *    - Hypothermia (<32°C):  distress = (32 - temp) / 10 (capped at 1.0)
 *    - Normal (32-40°C):     distress = 0.0
 *
 * 3. Hypoxia Distress:
 *    - O2 < 0.5: distress = 1.0 - (o2 / 0.5)
 *    - O2 >= 0.5: distress = 0.0
 *
 * 4. Membrane/Ion Distress:
 *    - Combines membrane integrity and ion balance
 *    - distress = (1.0 - membrane * ion_balance)
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via wellbeing system mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WELLBEING_SUBSTRATE_BRIDGE_H
#define NIMCP_WELLBEING_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#include "cognitive/wellbeing/nimcp_wellbeing_enhanced.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update substrate effects on wellbeing
 *
 * WHAT: Query substrate state and compute distress contributions
 * WHY:  Substrate health directly impacts wellbeing (ATP, temp, O2, etc.)
 * HOW:  Query substrate, compute ATP/temp/hypoxia/membrane distress, update system
 *
 * @param system Enhanced wellbeing system (must be non-NULL)
 * @return 0 on success, NIMCP_ERROR_NULL_POINTER if system is NULL,
 *         NIMCP_ERROR_INVALID_STATE if substrate not connected
 */
int enhanced_wellbeing_update_substrate(enhanced_wellbeing_system_t* system);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute ATP distress contribution
 *
 * WHAT: Calculate distress from ATP depletion
 * WHY:  Low ATP reduces cognitive capacity and triggers survival mode
 * HOW:  Critical <0.3 (high distress), warning <0.5 (moderate), else none
 *
 * @param atp_level Current ATP level [0-1]
 * @param sensitivity Sensitivity multiplier [0.5-2.0]
 * @return Distress contribution [0-1]
 */
float compute_atp_distress(float atp_level, float sensitivity);

/**
 * @brief Compute temperature distress contribution
 *
 * WHAT: Calculate distress from temperature dysregulation
 * WHY:  Extreme temperatures impair neural function and cause confusion
 * HOW:  Hyperthermia >40°C, hypothermia <32°C, linear scaling
 *
 * @param temperature Current temperature (°C)
 * @param sensitivity Sensitivity multiplier [0.5-2.0]
 * @return Distress contribution [0-1]
 */
float compute_temperature_distress(float temperature, float sensitivity);

/**
 * @brief Compute hypoxia distress contribution
 *
 * WHAT: Calculate distress from oxygen deprivation
 * WHY:  Hypoxia triggers emergency mode and resource starvation
 * HOW:  O2 < 0.5 triggers distress, linear scaling to 1.0 at O2=0
 *
 * @param o2_saturation Current oxygen saturation [0-1]
 * @param sensitivity Sensitivity multiplier [0.5-2.0]
 * @return Distress contribution [0-1]
 */
float compute_hypoxia_distress(float o2_saturation, float sensitivity);

/**
 * @brief Compute membrane/ion distress contribution
 *
 * WHAT: Calculate distress from membrane damage and ionic imbalance
 * WHY:  Membrane/ion dysfunction causes unreliable signaling
 * HOW:  Combine membrane integrity and ion balance (lower = higher distress)
 *
 * @param membrane_integrity Current membrane integrity [0-1]
 * @param ion_balance Current ion balance [0-1]
 * @return Distress contribution [0-1]
 */
float compute_membrane_distress(float membrane_integrity, float ion_balance);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get substrate wellbeing effects
 *
 * WHAT: Retrieve current substrate effects on wellbeing
 * WHY:  Allow inspection of substrate contributions to distress
 * HOW:  Copy substrate_effects from system
 *
 * @param system Enhanced wellbeing system
 * @param effects Output substrate effects (must be non-NULL)
 * @return 0 on success, NIMCP_ERROR_NULL_POINTER if NULL params
 */
int enhanced_wellbeing_get_substrate_effects(
    const enhanced_wellbeing_system_t* system,
    substrate_wellbeing_effects_t* effects
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_SUBSTRATE_BRIDGE_H */
