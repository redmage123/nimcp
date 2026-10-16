//=============================================================================
// nimcp_brain_init_network_kg_wiring.h - Wave W5 Network-Level KG Wiring
//=============================================================================
/**
 * @file nimcp_brain_init_network_kg_wiring.h
 * @brief Structural registration of the 6 neural-network roots into the KG.
 *
 * Part of the KG-integration retrofit Wave W5 (2026-04-24). Wires each of
 * the 6 network types (LNN, SNN, CNN, FNO, HNN, main ANN) as a structural
 * root node in `brain->internal_kg`, plus sample sub-nodes for SNN
 * populations. Network state-transition event emitters live in each
 * network's main .c file and are triggered on anomaly (mode collapse,
 * gradient explosion, energy drift, spectral shift, etc.).
 *
 * Runs as Wave 32, after Wave 31 (region_kg_bridges) and Wave 22
 * (internal_kg creation). Null-tolerant if internal_kg is disabled.
 *
 * See docs/claude/kg-integration-audit-2026-04-24.md §2.12 and
 * docs/claude/kg-node-naming-registry.md §3 for naming conventions.
 */

#ifndef NIMCP_BRAIN_INIT_NETWORK_KG_WIRING_H
#define NIMCP_BRAIN_INIT_NETWORK_KG_WIRING_H

#include <stdbool.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the 6 network root nodes + sample SNN population nodes
 *        into brain->internal_kg. Idempotent.
 * @return true on success (or no-op if KG disabled); false on fatal fault.
 */
NIMCP_EXPORT bool nimcp_brain_factory_init_network_kg_wiring_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_NETWORK_KG_WIRING_H */
