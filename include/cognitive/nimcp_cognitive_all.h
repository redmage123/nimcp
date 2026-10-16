//=============================================================================
// nimcp_cognitive_all.h - Complete Cognitive Module Aggregate Header
//=============================================================================
/**
 * @file nimcp_cognitive_all.h
 * @brief All cognitive modules in a single include
 *
 * WHAT: Master aggregate header for all cognitive subsystems
 * WHY:  Provides single-include access to complete cognitive functionality
 * HOW:  Includes all category-specific aggregate headers
 *
 * USAGE:
 *   #include "cognitive/nimcp_cognitive_all.h"
 *
 * WARNING: This header includes ALL cognitive modules.
 *          For reduced compilation time, use specific aggregates:
 *          - nimcp_cognitive_core.h     (essential modules)
 *          - nimcp_cognitive_advanced.h (optional advanced modules)
 *          - nimcp_cognitive_emotional.h (emotional subsystems)
 *          - nimcp_cognitive_memory.h   (memory subsystems)
 */

#ifndef NIMCP_COGNITIVE_ALL_H
#define NIMCP_COGNITIVE_ALL_H

// Core cognitive modules (required)
#include "cognitive/nimcp_cognitive_core.h"

// Advanced cognitive modules (optional)
#include "cognitive/nimcp_cognitive_advanced.h"

// Emotional and social modules
#include "cognitive/nimcp_cognitive_emotional.h"

// Memory systems
#include "cognitive/nimcp_cognitive_memory.h"

// Additional specialized modules not in aggregates
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/parietal/nimcp_intuition_integrations.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

#endif // NIMCP_COGNITIVE_ALL_H
