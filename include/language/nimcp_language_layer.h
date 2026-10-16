//=============================================================================
// nimcp_language_layer.h - Language Layer Unified Header
//=============================================================================
/**
 * @file nimcp_language_layer.h
 * @brief Unified aggregate header for the NIMCP Language Layer
 *
 * WHAT: Single include for all language layer functionality
 * WHY:  Simplify integration - one include provides full language API
 * HOW:  Aggregates types, config, orchestrator, and bridge headers
 *
 * USAGE:
 * ```c
 * #include "language/nimcp_language_layer.h"
 *
 * // Create and configure
 * language_orchestrator_config_t config;
 * language_orchestrator_default_config(&config);
 * language_orchestrator_t* lang = language_orchestrator_create(&config);
 *
 * // Connect to other systems
 * language_orchestrator_connect_wernicke(lang, brain->wernicke);
 * language_orchestrator_connect_broca(lang, brain->broca);
 * language_orchestrator_connect_perception_bridge(lang, perception_bridge);
 * language_orchestrator_connect_cognitive_bridge(lang, cognitive_bridge);
 * language_orchestrator_connect_training_bridge(lang, training_bridge);
 *
 * // Start processing
 * language_orchestrator_start(lang);
 *
 * // In update loop
 * language_orchestrator_update(lang, current_time_ms);
 *
 * // Process input
 * language_orchestrator_process_phonemes(lang, phonemes, count);
 *
 * // Get results
 * language_comprehension_result_t result;
 * language_orchestrator_get_comprehension(lang, &result);
 * ```
 *
 * BIOLOGICAL BASIS:
 * - Models the human language network
 * - Wernicke's area (BA22): Comprehension (posterior STG)
 * - Broca's area (BA44/45): Production (inferior frontal gyrus)
 * - Arcuate fasciculus: Wernicke-Broca connection
 * - Integration with auditory, visual, motor cortices
 *
 * MODULE STRUCTURE:
 * ```
 * Language Layer
 * ├── Core
 * │   ├── nimcp_language_types.h      - Shared types and enums
 * │   ├── nimcp_language_config.h     - Configuration structures
 * │   ├── nimcp_language_orchestrator.h - Central coordinator
 * │   └── nimcp_language_bio_async.h  - Bio-async integration
 * │
 * ├── Bridges
 * │   ├── nimcp_language_perception_bridge.h  - Perception integration
 * │   ├── nimcp_language_cognitive_bridge.h   - Cognitive integration
 * │   ├── nimcp_language_training_bridge.h    - Training integration
 * │   ├── nimcp_language_omni_bridge.h        - Omni inference integration
 * │   ├── nimcp_language_immune_bridge.h      - Immune integration
 * │   └── nimcp_language_gpu_bridge.h         - GPU acceleration
 * │
 * └── Subsystems (from existing modules)
 *     ├── core/brain/regions/wernicke/*  - Comprehension
 *     ├── core/brain/regions/broca/*     - Production
 *     ├── nlp/*                          - NLP processing
 *     └── perception/nimcp_speech_cortex.h - Speech perception
 * ```
 *
 * @version 1.0.0 - Phase L1: Language Layer Core Infrastructure
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_LAYER_H
#define NIMCP_LANGUAGE_LAYER_H

//=============================================================================
// Core Language Layer Headers
//=============================================================================

/* Shared types and enums */
#include "language/nimcp_language_types.h"

/* Configuration structures */
#include "language/nimcp_language_config.h"

/* Central orchestrator */
#include "language/nimcp_language_orchestrator.h"

/* Bio-async integration */
#include "language/nimcp_language_bio_async.h"

//=============================================================================
// Language Layer Bridges
//=============================================================================

/* Perception integration (speech/audio/visual cortices) */
#include "language/bridges/nimcp_language_perception_bridge.h"

/* Cognitive integration (WM, attention, semantic memory, reasoning) */
#include "language/bridges/nimcp_language_cognitive_bridge.h"

/* Training integration (language learning) */
#include "language/bridges/nimcp_language_training_bridge.h"

/* Omnidirectional inference integration (predictive processing) */
#include "language/bridges/nimcp_language_omni_bridge.h"

/* Immune integration (inflammation effects) */
#include "language/bridges/nimcp_language_immune_bridge.h"

/* GPU acceleration */
#include "language/bridges/nimcp_language_gpu_bridge.h"

//=============================================================================
// Version Information
//=============================================================================

/**
 * @brief Get language layer version string
 * @return Version string (e.g., "1.0.0")
 */
const char* nimcp_language_layer_version(void);

/**
 * @brief Get language layer build info
 * @return Build info string
 */
const char* nimcp_language_layer_build_info(void);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Check if language layer version is at least the specified version
 */
#define NIMCP_LANGUAGE_LAYER_VERSION_AT_LEAST(major, minor, patch) \
    ((LANGUAGE_LAYER_VERSION_MAJOR > (major)) || \
     (LANGUAGE_LAYER_VERSION_MAJOR == (major) && LANGUAGE_LAYER_VERSION_MINOR > (minor)) || \
     (LANGUAGE_LAYER_VERSION_MAJOR == (major) && LANGUAGE_LAYER_VERSION_MINOR == (minor) && \
      LANGUAGE_LAYER_VERSION_PATCH >= (patch)))

#endif /* NIMCP_LANGUAGE_LAYER_H */
