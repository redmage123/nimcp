/**
 * @file nimcp_theory_of_mind_substrate_bridge.h
 * @brief Backward Compatibility Header - Theory of Mind Substrate Bridge
 *
 * NOTE: This header is maintained for backward compatibility only.
 *       The implementation has been unified into cognitive/tom/nimcp_tom_substrate_bridge.h
 *       which provides both the new unified API and backward compatibility wrappers.
 *
 * Migration Guide:
 * - Old: #include "cognitive/theory_of_mind/nimcp_theory_of_mind_substrate_bridge.h"
 * - New: #include "cognitive/tom/nimcp_tom_substrate_bridge.h"
 *
 * The following backward compatibility functions are available:
 * - tom_substrate_bridge_update() -> wraps tom_substrate_update()
 * - tom_substrate_bridge_apply_effects() -> broadcasts via bio-async
 * - tom_substrate_bridge_get_effects() -> wraps tom_substrate_get_effects()
 * - tom_substrate_bridge_register_bio_async() -> wraps tom_substrate_connect_bio_async()
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_THEORY_OF_MIND_SUBSTRATE_BRIDGE_H
#define NIMCP_THEORY_OF_MIND_SUBSTRATE_BRIDGE_H

/* Include the unified ToM substrate bridge header */
#include "cognitive/tom/nimcp_tom_substrate_bridge.h"

#endif /* NIMCP_THEORY_OF_MIND_SUBSTRATE_BRIDGE_H */
