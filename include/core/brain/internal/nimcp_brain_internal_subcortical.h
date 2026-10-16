//=============================================================================
// nimcp_brain_internal_subcortical.h - Subcortical Systems Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_subcortical.h
 * @brief Internal brain_struct fields for subcortical systems
 *
 * WHAT: Defines brain_struct fields for subcortical structures
 * WHY:  Modularize brain_internal.h - separate subcortical fields
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * SUBCORTICAL SYSTEMS:
 * - Thalamus: Central relay gateway to cortex (all modalities except olfaction)
 * - Basal Ganglia: Action selection, motor control, reinforcement learning
 * - Medulla Oblongata: Brainstem autonomic regulation (arousal, protection)
 * - Dragonfly: Bio-inspired target tracking and interception
 *
 * BIOLOGICAL BASIS:
 * - Thalamus: LGN, MGN, VPL/VPM, VA/VL, Pulvinar, MD, Anterior, TRN nuclei
 * - Basal ganglia: Striatum, GPe/GPi, STN, SNc/SNr with DA modulation
 * - Medulla: Arousal state, protective cutoff, circadian rhythm
 * - Dragonfly: TSDN population coding, CSTMD1 attention, IMM prediction
 *
 * REFACTORING HISTORY:
 * - Extracted from monolithic nimcp_brain_internal.h (Phase B3.1)
 *
 * @version Phase B3.1: Subcortical Modularization
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_SUBCORTICAL_H
#define NIMCP_BRAIN_INTERNAL_SUBCORTICAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Subcortical Types
//=============================================================================

// Basal Ganglia (enhanced with all subcomponents)
struct bg_enhanced;
typedef struct bg_enhanced bg_enhanced_t;

// Medulla Oblongata (brainstem regulator)
struct medulla_struct;
typedef struct medulla_struct* medulla_t;

// Dragonfly System (target tracking)
struct dragonfly_system_s;
typedef struct dragonfly_system_s dragonfly_system_t;

// Dragonfly-Medulla Bridge
struct dragonfly_medulla_bridge_s;
typedef struct dragonfly_medulla_bridge_s* dragonfly_medulla_bridge_t;

// Thalamus (cortical relay gateway)
// Note: thalamus_t is defined in nimcp_thalamus.h as anonymous struct typedef
// Forward declaration not needed here - brain_internal.h includes the full header

//=============================================================================
// Subcortical Fields for brain_struct
//=============================================================================

/**
 * @brief Macro defining subcortical fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 *
 * SUBSYSTEMS:
 * 1. MEDULLA OBLONGATA (Brainstem Autonomic Regulation)
 *    - Arousal State: Global activation level
 *    - Protective Cutoff: Emergency shutdown
 *    - Circadian Rhythm: 24-hour biological clock
 *
 * 2. DRAGONFLY (Bio-inspired Target Tracking)
 *    - TSDN: Population vector encoding (16 neurons, 360°)
 *    - CSTMD1: Winner-take-all selective attention
 *    - Prediction: IMM filter trajectory prediction
 *    - Interception: Proportional navigation guidance
 *
 * 3. BASAL GANGLIA (Action Selection & Motor Control)
 *    - Core BG: Striatum (D1/D2 MSNs), GPe/GPi, STN, SNc/SNr
 *    - Beta Oscillations: 13-30 Hz movement suppression
 *    - Multi-Neuromodulators: DA, 5HT, ACh, NE, adenosine
 *    - Hierarchical RL: Options framework with primitives
 *    - Model-Based Planning: World model + arbitration
 *
 * 4. THALAMUS (Cortical Relay Gateway)
 *    - First-Order: LGN (visual), MGN (auditory), VPL/VPM (somatosensory)
 *    - Motor Relay: VA (BG), VL (cerebellar)
 *    - Higher-Order: Pulvinar (attention), MD (executive), Anterior (limbic)
 *    - TRN: Thalamic Reticular Nucleus for gating
 *    - Firing Modes: Tonic (awake), Burst (drowsy), Inhibited (TRN gated)
 */
#define BRAIN_INTERNAL_FIELDS_SUBCORTICAL                                      \
    /* === MEDULLA OBLONGATA (Brainstem Autonomic Regulation) === */           \
    medulla_t medulla;                          /* Medulla oblongata regulator */ \
    bool medulla_enabled;                       /* Medulla enabled */          \
    uint64_t last_medulla_update_us;            /* Last medulla update time */ \
                                                                               \
    /* === DRAGONFLY (Bio-inspired Target Tracking) === */                     \
    dragonfly_system_t* dragonfly;              /* Dragonfly tracking system */ \
    bool dragonfly_enabled;                     /* Dragonfly enabled */        \
    uint64_t last_dragonfly_update_us;          /* Last dragonfly update */    \
    dragonfly_medulla_bridge_t dragonfly_medulla_bridge; /* Dragonfly-medulla bridge */ \
                                                                               \
    /* === BASAL GANGLIA (Action Selection & Motor Control) === */             \
    bg_enhanced_t* basal_ganglia;               /* Enhanced basal ganglia */   \
    bool basal_ganglia_enabled;                 /* Basal ganglia enabled */    \
    uint64_t last_basal_ganglia_update_us;      /* Last BG update timestamp */ \
                                                                               \
    /* === THALAMUS (Cortical Relay Gateway) === */                            \
    thalamus_t* thalamus;                       /* Thalamic nuclei system */   \
    bool thalamus_enabled;                      /* Thalamus enabled */         \
    uint64_t last_thalamus_update_us;           /* Last thalamus update */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_SUBCORTICAL_H */
