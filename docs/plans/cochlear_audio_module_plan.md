# Cochlear Audio Module Implementation Plan (Enhanced)

**Version**: 3.0
**Created**: 2026-01-03
**Updated**: 2026-01-03
**Status**: Pending Approval

> **COMPREHENSIVE INTEGRATION**: This plan includes full integration with Internal Brain KG,
> Cortical Columns, Recursive Cognition, Collective Cognition, Bio-Async with verified
> bidirectional data flows, Occipital Lobe, and Broca's Region.

---

## 1. Overview

### 1.1 What
A biologically-accurate cochlear processing module with **dog and bat auditory enhancements**, fully integrated into NIMCP's brain architecture, immune system, cognitive layers, and all relevant subsystems.

### 1.2 Why
- **Biological realism**: Model inner ear mechanics with active amplification
- **Extended hearing**: Dog ultrasonic (up to 65 kHz) and bat echolocation (up to 200 kHz)
- **Full integration**: Seamlessly connect to all 36+ brain subsystems
- **Self-awareness**: KG-based introspection of cochlear capabilities

### 1.3 How
Implement as a comprehensive perception module following NIMCP patterns, with complete brain factory initialization, immune system monitoring, medulla coupling, and cognitive layer integration.

---

## 2. Non-Human Biological Enhancements

### 2.1 Dog Auditory System (Canis lupus familiaris)

| Feature | Human | Dog | Implementation |
|---------|-------|-----|----------------|
| Frequency range | 20 Hz - 20 kHz | 67 Hz - 45 kHz | Extended filterbank |
| Upper limit | 20 kHz | 45-65 kHz | Ultrasonic channels |
| Directional hearing | ~1° accuracy | ~0.5° accuracy | Enhanced ITD/ILD |
| Ear mobility | None | 18+ muscles per ear | Attention-driven gain |
| Sound localization | Moderate | Excellent | Superior colliculus integration |

**Dog-Specific Features**:
```c
typedef struct {
    // Extended frequency range
    float max_freq_hz;              // 45000-65000 Hz (vs human 20000)
    float ultrasonic_sensitivity;   // Enhanced high-freq gain

    // Pinnae mobility simulation
    float ear_orientation_left;     // -90 to +90 degrees
    float ear_orientation_right;
    float pinnae_gain_factor;       // Directional gain boost

    // Enhanced sound localization
    float itd_resolution_us;        // Interaural time difference (finer than human)
    float ild_sensitivity_db;       // Interaural level difference

    // Breed-specific variations
    dog_breed_hearing_t breed_profile;  // German Shepherd, Beagle, etc.
} dog_auditory_config_t;

typedef enum {
    DOG_BREED_GENERIC,
    DOG_BREED_GERMAN_SHEPHERD,   // Excellent hearing
    DOG_BREED_BEAGLE,            // Scent-focused, good hearing
    DOG_BREED_DALMATIAN,         // Prone to deafness
    DOG_BREED_BORDER_COLLIE      // High-frequency sensitivity
} dog_breed_hearing_t;
```

### 2.2 Bat Auditory System (Chiroptera)

| Feature | Human | Bat | Implementation |
|---------|-------|-----|----------------|
| Frequency range | 20 Hz - 20 kHz | 1 kHz - 200 kHz | Ultrasonic filterbank |
| Echolocation | None | 20-200 kHz calls | Call/echo processing |
| Temporal resolution | ~2 ms | ~10-50 μs | Microsecond precision |
| Doppler processing | None | Exquisite | Velocity detection |
| Range resolution | N/A | ~1 cm | Time-of-flight |

**Bat-Specific Features**:
```c
typedef struct {
    // Ultrasonic frequency range
    float max_freq_hz;              // Up to 200000 Hz
    float echolocation_band_min;    // Typically 20000 Hz
    float echolocation_band_max;    // Species-specific (200000 Hz max)

    // Echolocation processing
    bool enable_echolocation;
    echolocation_call_type_t call_type;  // FM sweep, CF-FM, etc.
    float call_duration_ms;         // 0.5-20 ms typical
    float pulse_interval_ms;        // Interpulse interval

    // Doppler shift processing (CF bats)
    bool enable_doppler_processing;
    float reference_freq_hz;        // Resting frequency
    float doppler_sensitivity;      // Hz per m/s

    // Temporal processing
    float temporal_resolution_us;   // 10-50 microseconds
    float range_resolution_cm;      // ~1 cm precision

    // Species profiles
    bat_species_t species;
} bat_auditory_config_t;

typedef enum {
    BAT_SPECIES_GENERIC,
    BAT_SPECIES_HORSESHOE,         // CF-FM, Doppler specialists
    BAT_SPECIES_VESPERTILIONID,    // FM sweeps
    BAT_SPECIES_PHYLLOSTOMID,      // Leaf-nosed, short calls
    BAT_SPECIES_PTEROPODID         // Fruit bats (limited echolocation)
} bat_species_t;

typedef enum {
    ECHOLOCATION_CALL_FM,          // Frequency modulated sweep
    ECHOLOCATION_CALL_CF,          // Constant frequency
    ECHOLOCATION_CALL_CF_FM,       // CF with FM component
    ECHOLOCATION_CALL_CLICK        // Broadband click
} echolocation_call_type_t;

// Echolocation output structure
typedef struct {
    float* target_ranges_m;         // Detected target distances
    float* target_velocities_mps;   // Doppler-derived velocities
    float* target_angles_deg;       // Azimuth angles
    float* target_strengths;        // Echo intensity
    uint32_t num_targets;
} echolocation_result_t;
```

### 2.3 Unified Extended Hearing Mode

```c
typedef enum {
    HEARING_MODE_HUMAN,            // Standard 20 Hz - 20 kHz
    HEARING_MODE_DOG,              // Extended to 45-65 kHz + directional
    HEARING_MODE_BAT,              // Ultrasonic + echolocation
    HEARING_MODE_HYBRID            // Combined capabilities
} hearing_mode_t;

typedef struct {
    hearing_mode_t mode;
    dog_auditory_config_t dog_config;
    bat_auditory_config_t bat_config;
    bool enable_mode_switching;    // Dynamic mode selection
    float mode_transition_time_ms; // Smooth transitions
} extended_hearing_config_t;
```

---

## 3. Complete Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           nimcp_cochlea_t                                   │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                    BASILAR MEMBRANE MODEL                              │ │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐        │ │
│  │  │ Human Channels  │  │  Dog Ultrasonic │  │ Bat Echolocation│        │ │
│  │  │   20Hz-20kHz    │  │   20kHz-65kHz   │  │  20kHz-200kHz   │        │ │
│  │  │ (128 channels)  │  │ (64 channels)   │  │ (128 channels)  │        │ │
│  │  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘        │ │
│  │           └────────────────────┼────────────────────┘                 │ │
│  └────────────────────────────────┼──────────────────────────────────────┘ │
│                                   ▼                                         │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                      HAIR CELL TRANSDUCTION                            │ │
│  │  ┌─────────────────────────┐  ┌─────────────────────────────────────┐ │ │
│  │  │   Outer Hair Cells      │  │      Inner Hair Cells               │ │ │
│  │  │ (Active Amplification)  │  │   (Sensory Transduction)            │ │ │
│  │  │  • Prestin motor        │  │   • Ribbon synapses                 │ │ │
│  │  │  • 40-60 dB gain        │  │   • Glutamate release               │ │ │
│  │  │  • Frequency sharpening │  │   • Rate/place coding               │ │ │
│  │  └───────────┬─────────────┘  └─────────────┬───────────────────────┘ │ │
│  └──────────────┼──────────────────────────────┼────────────────────────┘ │
│                 │                              │                           │
│  ┌──────────────▼──────────────────────────────▼────────────────────────┐ │
│  │                    AUDITORY NERVE FIBERS                              │ │
│  │  • High/Med/Low spontaneous rate populations                          │ │
│  │  • Phase locking (< 4 kHz)                                            │ │
│  │  • Bat: Microsecond precision (< 50 μs)                               │ │
│  │  • Dog: Enhanced temporal resolution                                   │ │
│  └──────────────────────────────┬───────────────────────────────────────┘ │
└─────────────────────────────────┼───────────────────────────────────────────┘
                                  │
    ┌─────────────────────────────┼─────────────────────────────────────────┐
    │                             ▼                                          │
    │  ┌──────────────────────────────────────────────────────────────────┐ │
    │  │                    MEDULLA / BRAINSTEM                            │ │
    │  │  • Cochlear nucleus → Superior olive → Inferior colliculus       │ │
    │  │  • Protective cutoff for loud sounds                              │ │
    │  │  • Arousal modulation of auditory sensitivity                     │ │
    │  │  • Circadian gating of auditory processing                        │ │
    │  └────────────────────────────┬─────────────────────────────────────┘ │
    │                               ▼                                        │
    │  ┌──────────────────────────────────────────────────────────────────┐ │
    │  │                    THALAMUS (MGN)                                 │ │
    │  │  • Medial geniculate nucleus - auditory relay                    │ │
    │  │  • Attention gating via TRN                                       │ │
    │  │  • Tonic/burst mode switching                                     │ │
    │  └────────────────────────────┬─────────────────────────────────────┘ │
    │                               ▼                                        │
    │  ┌──────────────────────────────────────────────────────────────────┐ │
    │  │                AUDIO CORTEX (A1)                                  │ │
    │  │  • Tonotopic organization                                         │ │
    │  │  • Cortical columns for frequency processing                      │ │
    │  │  • Speech/non-speech segregation                                  │ │
    │  └────────────────────────────┬─────────────────────────────────────┘ │
    │                               │                                        │
    │  ┌────────────────────────────┼─────────────────────────────────────┐ │
    │  │              CROSS-MODAL INTEGRATION                              │ │
    │  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐               │ │
    │  │  │ Audiovisual │  │  Speech     │  │ Echolocation│               │ │
    │  │  │   Bridge    │  │  Cortex     │  │   Mapping   │               │ │
    │  │  │ (Lip-read)  │  │  (Broca)    │  │ (Bat mode)  │               │ │
    │  │  └─────────────┘  └─────────────┘  └─────────────┘               │ │
    │  └──────────────────────────────────────────────────────────────────┘ │
    │                                                                        │
    │  ┌──────────────────────────────────────────────────────────────────┐ │
    │  │              COGNITIVE INTEGRATION                                │ │
    │  │  • Attention system (cocktail party effect)                       │ │
    │  │  • Memory consolidation (auditory memories)                       │ │
    │  │  • Emotion processing (amygdala for alarm sounds)                 │ │
    │  │  • Salience detection (novel/important sounds)                    │ │
    │  │  • Introspection (self-monitoring of auditory state)              │ │
    │  └──────────────────────────────────────────────────────────────────┘ │
    │                                                                        │
    │  ┌──────────────────────────────────────────────────────────────────┐ │
    │  │              IMMUNE & PROTECTION                                  │ │
    │  │  • Brain immune: Antigen presentation for audio anomalies        │ │
    │  │  • BBB protection: Audio input validation                        │ │
    │  │  • Medulla protective cutoff: Loud sound protection              │ │
    │  └──────────────────────────────────────────────────────────────────┘ │
    └────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Complete Integration Matrix

### 4.1 Brain Factory Integration

**File**: `src/core/brain/factory/init/nimcp_brain_init_cochlea.c`

```c
/**
 * @brief Initialize cochlear subsystem in brain factory
 *
 * WHAT: Create and configure cochlear module during brain initialization
 * WHY:  Cochlea is foundational to all auditory processing
 * HOW:  Called by brain factory after medulla, before audio cortex
 */
bool nimcp_brain_factory_init_cochlea_subsystem(brain_t brain);

/**
 * @brief Initialize cochlea-medulla bridge
 *
 * BIOLOGICAL: Cochlear nucleus → Superior olive → IC pathway
 */
bool nimcp_brain_factory_init_cochlea_medulla_bridge(brain_t brain);

/**
 * @brief Initialize cochlea-thalamus bridge
 *
 * BIOLOGICAL: IC → MGN relay with attention gating
 */
bool nimcp_brain_factory_init_cochlea_thalamic_bridge(brain_t brain);

/**
 * @brief Initialize cochlea substrate bridge
 *
 * BIOLOGICAL: ATP/metabolic modulation of cochlear sensitivity
 */
bool nimcp_brain_factory_init_cochlea_substrate_bridge(brain_t brain);

/**
 * @brief Initialize cochlea immune bridge
 *
 * BIOLOGICAL: Immune monitoring for ototoxicity, noise damage
 */
bool nimcp_brain_factory_init_cochlea_immune_bridge(brain_t brain);
```

### 4.2 Integration Points Summary

| System | Integration File | Purpose |
|--------|-----------------|---------|
| **Brain Factory** | `nimcp_brain_init_cochlea.c` | Create cochlea at brain startup |
| **Medulla** | `nimcp_cochlea_medulla_bridge.h` | Brainstem pathway, protective cutoff |
| **Thalamus** | `nimcp_cochlea_thalamic_bridge.h` | MGN relay, attention gating |
| **Audio Cortex** | `nimcp_cochlea_audio_cortex_bridge.h` | A1 tonotopic integration |
| **Cortical Columns** | `nimcp_cochlea_cortical_columns.h` | Frequency hypercolumns |
| **Occipital/AV** | `nimcp_cochlea_audiovisual_bridge.h` | Lip reading, multimodal |
| **Brain Immune** | `nimcp_cochlea_immune_bridge.h` | Antigen presentation |
| **Cognitive Modules** | `nimcp_cochlea_cognitive_bridge.h` | 36+ module integration |
| **Introspection** | `nimcp_cochlea_introspection.h` | Self-monitoring |
| **Logic Gates** | `nimcp_cochlea_logic_bridge.h` | Frequency comparisons |
| **SNN Bridge** | `nimcp_snn_cochlea_bridge.h` | Spike-based encoding |
| **Bio-Async** | Module context registration | Cross-module messaging |
| **Logging** | `NIMCP_LOG_COCHLEA` category | Debug/monitoring |
| **KG Reader** | Self-awareness entities | Capability introspection |

---

## 5. Detailed Integration Specifications

### 5.1 Medulla Integration

**File**: `include/core/medulla/nimcp_cochlea_medulla_bridge.h`

```c
/**
 * @brief Cochlea-Medulla integration bridge
 *
 * BIOLOGICAL BASIS:
 * - Cochlear nucleus: First brainstem relay
 * - Superior olivary complex: Binaural processing, ITD/ILD
 * - Inferior colliculus: Multimodal auditory integration
 * - Protective reflexes: Acoustic stapedius reflex
 *
 * PROTECTIVE FUNCTIONS:
 * - Loud sound detection → Medulla protective cutoff
 * - Arousal modulation of cochlear gain
 * - Circadian gating of auditory sensitivity
 */

typedef struct {
    cochlea_t* cochlea;
    medulla_t* medulla;

    // Brainstem nuclei simulation
    float* cochlear_nucleus_output;      // CN firing rates
    float* superior_olive_output;        // SOC binaural processing
    float* inferior_colliculus_output;   // IC integration

    // Protective mechanisms
    float acoustic_reflex_threshold_db;  // Stapedius reflex threshold
    float reflex_latency_ms;             // ~25-150 ms
    bool reflex_active;

    // Arousal coupling
    arousal_level_t current_arousal;
    float arousal_gain_modulation;       // Sensitivity scaling

    // Circadian coupling
    circadian_phase_t current_phase;
    float circadian_sensitivity_curve[24]; // Hour-by-hour

} cochlea_medulla_bridge_t;

// Core API
cochlea_medulla_bridge_t* cochlea_medulla_bridge_create(
    cochlea_t* cochlea,
    medulla_t* medulla,
    const cochlea_medulla_config_t* config
);

int cochlea_medulla_bridge_update(
    cochlea_medulla_bridge_t* bridge,
    float dt_ms
);

// Protective cutoff integration
int cochlea_medulla_trigger_protective_cutoff(
    cochlea_medulla_bridge_t* bridge,
    float sound_level_db,
    protection_level_t level
);

// Arousal modulation
int cochlea_medulla_set_arousal(
    cochlea_medulla_bridge_t* bridge,
    arousal_level_t level
);
```

### 5.2 Brain Immune System Integration

**File**: `include/cognitive/immune/nimcp_cochlea_immune_bridge.h`

```c
/**
 * @brief Cochlea-Immune system integration
 *
 * BIOLOGICAL BASIS:
 * - Ototoxicity detection (aminoglycoside, cisplatin damage)
 * - Noise-induced damage monitoring
 * - Age-related degeneration (presbycusis)
 * - Autoimmune inner ear disease
 *
 * THREAT CATEGORIES:
 * - Acute: Sudden loud sound exposure
 * - Chronic: Sustained noise damage
 * - Chemical: Ototoxic substances
 * - Autoimmune: Self-attack on hair cells
 */

typedef enum {
    COCHLEA_THREAT_NONE = 0,
    COCHLEA_THREAT_NOISE_ACUTE,        // Sudden loud exposure
    COCHLEA_THREAT_NOISE_CHRONIC,      // Sustained damage
    COCHLEA_THREAT_OTOTOXICITY,        // Chemical damage
    COCHLEA_THREAT_AUTOIMMUNE,         // Immune attack
    COCHLEA_THREAT_AGING,              // Presbycusis
    COCHLEA_THREAT_INFECTION           // Labyrinthitis
} cochlea_threat_type_t;

typedef struct {
    cochlea_t* cochlea;
    brain_immune_system_t* immune_system;

    // Threat monitoring
    cochlea_threat_type_t active_threats[8];
    uint32_t num_active_threats;

    // Hair cell health
    float ihc_survival_rate;           // 0-1, 1 = all healthy
    float ohc_survival_rate;           // OHC more vulnerable
    float* frequency_damage_map;       // Per-channel damage

    // Inflammatory markers
    float cytokine_il1b;               // Pro-inflammatory
    float cytokine_il6;
    float cytokine_tnfa;
    float cytokine_il10;               // Anti-inflammatory

    // Recovery tracking
    float recovery_potential;          // Regeneration capacity
    uint64_t damage_onset_time;

} cochlea_immune_bridge_t;

// Present cochlear antigen to immune system
int cochlea_immune_present_antigen(
    cochlea_immune_bridge_t* bridge,
    cochlea_threat_type_t threat,
    float severity,
    uint32_t affected_channel
);

// Check for immune-mediated damage
int cochlea_immune_check_autoimmune(
    cochlea_immune_bridge_t* bridge,
    float* autoimmune_score
);

// Apply protective response
int cochlea_immune_apply_protection(
    cochlea_immune_bridge_t* bridge,
    brain_inflammation_level_t level
);
```

### 5.3 Cortical Columns Integration

**File**: `include/core/cortical_columns/nimcp_cochlea_cortical_columns.h`

```c
/**
 * @brief Cochlea to A1 cortical column mapping
 *
 * BIOLOGICAL BASIS:
 * - Tonotopic organization: Low freq → rostral, high freq → caudal
 * - Minicolumns: ~100 neurons with shared frequency preference
 * - Hypercolumns: Multiple minicolumns spanning frequency range
 * - Lateral inhibition: Mexican hat connectivity for sharpening
 */

typedef struct {
    cochlea_t* cochlea;
    cortical_column_pool_t* column_pool;

    // Tonotopic mapping
    uint32_t num_frequency_columns;    // One hypercolumn per critical band
    hypercolumn_t** frequency_hypercolumns;
    float* center_frequencies_hz;

    // Extended frequency columns (dog/bat)
    uint32_t num_ultrasonic_columns;
    hypercolumn_t** ultrasonic_hypercolumns;
    float* ultrasonic_center_freqs;

    // Competition mode
    cc_competition_mode_t competition;
    float inhibition_radius_octaves;   // Lateral inhibition width

} cochlea_cortical_columns_t;

// Create tonotopic hypercolumn organization
cochlea_cortical_columns_t* cochlea_cortical_columns_create(
    cochlea_t* cochlea,
    cortical_column_pool_t* pool,
    const cochlea_cortical_config_t* config
);

// Process cochlear output through cortical columns
int cochlea_cortical_columns_process(
    cochlea_cortical_columns_t* cc,
    const cochlea_output_t* cochlea_output,
    float* column_activations
);
```

### 5.4 Occipital/Audiovisual Integration

**File**: Bridge to `nimcp_occipital_audiovisual_bridge.h`

```c
/**
 * @brief Cochlea contribution to audiovisual integration
 *
 * BIOLOGICAL BASIS:
 * - McGurk effect: Visual lip movements alter auditory perception
 * - Audio-visual speech: 150ms lip-audio offset
 * - Sound localization: Audio + visual gaze integration
 * - Bat echolocation: Audio maps to spatial representation
 */

typedef struct {
    // Lip-reading support
    float speech_envelope[SPEECH_ENVELOPE_SIZE];
    float phoneme_probabilities[NUM_PHONEMES];

    // Sound localization for visual binding
    float azimuth_deg;
    float elevation_deg;
    float source_distance_m;

    // Echolocation spatial mapping (bat mode)
    echolocation_result_t* echo_map;
    bool echolocation_active;

} cochlea_audiovisual_data_t;

// Provide cochlear data for audiovisual binding
int cochlea_provide_audiovisual_data(
    cochlea_t* cochlea,
    cochlea_audiovisual_data_t* av_data
);
```

### 5.5 Logic Gate Integration

**File**: `include/core/logic/nimcp_cochlea_logic_bridge.h`

```c
/**
 * @brief Neural logic operations for cochlear processing
 *
 * USE CASES:
 * - Frequency discrimination: freq_A > freq_B
 * - Amplitude thresholding: level > threshold
 * - Harmonic detection: AND(f1, f2, f3) for harmonic stack
 * - Echolocation gates: echo_detected AND target_in_range
 */

typedef struct {
    cochlea_t* cochlea;
    neural_logic_factory_t* logic_factory;

    // Pre-built logic circuits
    neural_logic_t* frequency_comparator;    // A > B frequency
    neural_logic_t* amplitude_threshold;     // Level > threshold
    neural_logic_t* harmonic_detector;       // Multiple freq AND
    neural_logic_t* onset_detector;          // dLevel/dt > threshold
    neural_logic_t* echo_gate;               // Echolocation target detection

} cochlea_logic_bridge_t;

// Create logic bridge with pre-built circuits
cochlea_logic_bridge_t* cochlea_logic_bridge_create(
    cochlea_t* cochlea,
    brain_t brain
);

// Evaluate frequency comparison
float cochlea_logic_compare_frequencies(
    cochlea_logic_bridge_t* bridge,
    uint32_t channel_a,
    uint32_t channel_b
);

// Evaluate harmonic detector
float cochlea_logic_detect_harmonic(
    cochlea_logic_bridge_t* bridge,
    float fundamental_hz,
    uint32_t num_harmonics
);
```

### 5.6 Cognitive Layer Integration

All 36+ cognitive modules connect via standard thalamic bridge pattern:

```c
// Example: Attention-Cochlea integration
typedef struct {
    cochlea_t* cochlea;
    attention_system_t* attention;
    thalamic_router_t* router;

    // Cocktail party effect
    float attended_frequency_hz;
    float attention_bandwidth_octaves;
    float attention_gain;

} cochlea_attention_bridge_t;

// Example: Emotion-Cochlea integration
typedef struct {
    cochlea_t* cochlea;
    emotion_system_t* emotion;
    amygdala_t* amygdala;

    // Alarm sound detection
    float alarm_salience;
    float startle_response;

} cochlea_emotion_bridge_t;

// Standard cognitive bridges required:
// - nimcp_cochlea_attention_bridge.h
// - nimcp_cochlea_memory_bridge.h
// - nimcp_cochlea_emotion_bridge.h
// - nimcp_cochlea_salience_bridge.h
// - nimcp_cochlea_curiosity_bridge.h
// - nimcp_cochlea_prediction_bridge.h
// (... all 36 cognitive modules)
```

### 5.7 Introspection Integration

**File**: `include/cognitive/introspection/nimcp_cochlea_introspection.h`

```c
/**
 * @brief Cochlear introspection for self-awareness
 *
 * WHAT: Query cochlear state and capabilities
 * WHY:  Enable "What am I hearing?" self-monitoring
 * HOW:  Population queries, uncertainty estimation, KG integration
 */

typedef struct {
    // Active neuron population in cochlea
    neuron_population_t active_channels;

    // Current hearing state
    hearing_mode_t active_mode;
    float overall_sensitivity;
    float frequency_resolution;

    // Uncertainty about auditory percept
    brain_uncertainty_t perception_uncertainty;

    // Self-description from KG
    char* capability_description;
    char* current_mode_description;

} cochlea_introspection_state_t;

// Query cochlear self-state
int cochlea_introspect(
    cochlea_t* cochlea,
    cochlea_introspection_state_t* state
);

// Register with KG for self-awareness
int cochlea_register_kg_entities(
    cochlea_t* cochlea,
    kg_reader_t* kg
);
```

### 5.8 KG Self-Awareness Entities

```jsonl
{"type":"entity","name":"Cochlea_Module","entityType":"Module","observations":["Biologically-accurate inner ear model","Supports human, dog, and bat hearing modes","Gammatone filterbank with 128-320 channels","Active OHC amplification with 40-60 dB gain","Integrated with medulla for protective reflexes"]}
{"type":"entity","name":"Cochlea_Dog_Mode","entityType":"Capability","observations":["Extended frequency range to 65 kHz","Enhanced directional hearing","Pinnae mobility simulation","Superior sound localization"]}
{"type":"entity","name":"Cochlea_Bat_Mode","entityType":"Capability","observations":["Ultrasonic frequency range to 200 kHz","Echolocation call/echo processing","Microsecond temporal precision","Doppler shift velocity detection"]}
{"type":"relation","from":"Cochlea_Module","to":"Audio_Cortex","relationType":"feeds_into"}
{"type":"relation","from":"Cochlea_Module","to":"Medulla","relationType":"receives_protection_from"}
{"type":"relation","from":"Cochlea_Module","to":"Brain_Immune_System","relationType":"monitored_by"}
{"type":"relation","from":"Cochlea_Module","to":"Recursive_Cognition","relationType":"provides_input_to"}
{"type":"relation","from":"Cochlea_Module","to":"Collective_Cognition","relationType":"synchronizes_with"}
{"type":"relation","from":"Cochlea_Module","to":"Broca_Region","relationType":"feeds_speech_to"}
{"type":"relation","from":"Cochlea_Module","to":"Occipital_Lobe","relationType":"binds_audiovisual_with"}
```

---

## 5A. Advanced Integration Specifications

### 5A.1 Internal Brain Knowledge Graph Integration

**File**: `include/perception/bridges/nimcp_cochlea_brain_kg_bridge.h`

```c
/**
 * @brief Cochlea-Brain KG bidirectional integration
 *
 * WHAT: Full CRUD integration with brain's internal knowledge graph
 * WHY:  Enable self-awareness of cochlear state, capabilities, and connections
 * HOW:  Register cochlea nodes, update properties, query semantic relationships
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea → KG: State updates, capability registration
 * - INBOUND:  KG → Cochlea: Configuration queries, semantic context
 */

#include "core/brain/nimcp_brain_kg.h"

typedef struct {
    cochlea_t* cochlea;
    brain_kg_t* brain_kg;

    // Registered nodes in brain KG
    brain_kg_node_id_t cochlea_node;
    brain_kg_node_id_t basilar_membrane_node;
    brain_kg_node_id_t ihc_node;
    brain_kg_node_id_t ohc_node;
    brain_kg_node_id_t anf_node;
    brain_kg_node_id_t dog_mode_node;
    brain_kg_node_id_t bat_mode_node;

    // Registered edges
    brain_kg_edge_id_t* connection_edges;
    uint32_t num_edges;

    // Real-time property updates
    bool enable_state_sync;
    uint32_t sync_interval_ms;

    // Bidirectional verification
    uint64_t last_outbound_update;
    uint64_t last_inbound_query;
    bool bidirectional_verified;

} cochlea_brain_kg_bridge_t;

// Core API
cochlea_brain_kg_bridge_t* cochlea_brain_kg_bridge_create(
    cochlea_t* cochlea,
    brain_kg_t* brain_kg,
    const cochlea_brain_kg_config_t* config
);

// Register cochlea module in brain KG (called at init)
int cochlea_brain_kg_register_module(cochlea_brain_kg_bridge_t* bridge);

// Update cochlea state in KG (called periodically)
int cochlea_brain_kg_sync_state(cochlea_brain_kg_bridge_t* bridge);

// Query semantic context from KG (bidirectional)
int cochlea_brain_kg_query_context(
    cochlea_brain_kg_bridge_t* bridge,
    const char* query,
    brain_kg_query_result_t* result
);

// Get connected modules from KG
int cochlea_brain_kg_get_connections(
    cochlea_brain_kg_bridge_t* bridge,
    brain_kg_edge_list_t** connections
);

// Verify bidirectional data flow
bool cochlea_brain_kg_verify_bidirectional(cochlea_brain_kg_bridge_t* bridge);
```

### 5A.2 Recursive Cognition Engine Integration

**File**: `include/cognitive/recursive/nimcp_rcog_cochlea_bridge.h`

```c
/**
 * @brief Cochlea-Recursive Cognition bidirectional bridge
 *
 * WHAT: Connect cochlear processing to recursive language-model-style cognition
 * WHY:  Enable goal-directed auditory processing, context-aware listening
 * HOW:  Register as tool router capability, provide context variables
 *
 * BIOLOGICAL BASIS:
 * - Top-down auditory attention from prefrontal cortex
 * - Goal-directed listening (cocktail party, selective attention)
 * - Predictive processing of expected sounds
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea → RCOG: Audio events, speech detection, sound features
 * - INBOUND:  RCOG → Cochlea: Attention commands, listening goals, predictions
 */

#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/recursive/nimcp_rcog_tool_router.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"

typedef struct {
    cochlea_t* cochlea;
    rcog_engine_t* rcog_engine;

    // Tool router integration
    rcog_tool_t* cochlea_tool;           // "listen", "detect_sound", "localize"

    // Context store integration
    rcog_variable_t* current_audio_context;
    rcog_variable_t* detected_sounds;
    rcog_variable_t* active_listening_goal;

    // Bidirectional message channels
    bio_module_context_t* bio_context;

    // Inbound: Goals from RCOG
    struct {
        char* target_sound_class;        // "speech", "alarm", "music"
        float target_frequency_hz;       // Frequency to attend
        float attention_bandwidth;       // Bandwidth of attention
        bool suppress_background;        // Cocktail party mode
    } listening_goal;

    // Outbound: Events to RCOG
    struct {
        bool speech_detected;
        bool alarm_detected;
        float dominant_frequency;
        float sound_azimuth_deg;
        float confidence;
    } audio_events;

    // Verification
    uint64_t last_goal_received;
    uint64_t last_event_sent;
    bool bidirectional_verified;

} cochlea_rcog_bridge_t;

// Core API
cochlea_rcog_bridge_t* cochlea_rcog_bridge_create(
    cochlea_t* cochlea,
    rcog_engine_t* engine,
    const cochlea_rcog_config_t* config
);

// Register cochlea as RCOG tool
int cochlea_rcog_register_tool(cochlea_rcog_bridge_t* bridge);

// Process listening goal from RCOG (inbound)
int cochlea_rcog_receive_goal(
    cochlea_rcog_bridge_t* bridge,
    const rcog_goal_t* goal
);

// Send audio event to RCOG (outbound)
int cochlea_rcog_send_event(
    cochlea_rcog_bridge_t* bridge,
    const cochlea_audio_event_t* event
);

// Update context variables
int cochlea_rcog_update_context(cochlea_rcog_bridge_t* bridge);

// Verify bidirectional flow
bool cochlea_rcog_verify_bidirectional(cochlea_rcog_bridge_t* bridge);
```

### 5A.3 Collective Cognition Integration

**File**: `include/cognitive/collective_cognition/nimcp_cochlea_collective_bridge.h`

```c
/**
 * @brief Cochlea-Collective Cognition bidirectional bridge
 *
 * WHAT: Synchronize cochlear processing across distributed brain instances
 * WHY:  Enable collective listening, shared auditory attention, audio swarm
 * HOW:  Hyperscanning for audio sync, shared intentionality for joint attention
 *
 * THEORETICAL BASIS:
 * - Joint attention: Multiple agents attending to same sound source
 * - Collective hearing: Distributed frequency coverage (bat colony)
 * - Auditory hyperscanning: Phase synchronization across instances
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Local cochlea → Collective: Audio features, detections, phi
 * - INBOUND:  Collective → Local cochlea: Shared attention, distributed goals
 */

#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "cognitive/collective_cognition/nimcp_hyperscanning.h"
#include "cognitive/collective_cognition/nimcp_shared_intentionality.h"

typedef struct {
    cochlea_t* cochlea;
    collective_cognition_t* collective;
    hyperscanning_t* hyperscanning;
    shared_intentionality_t* shared_intent;

    // Instance identity
    uint32_t instance_id;
    uint32_t collective_size;

    // Hyperscanning state (audio-specific)
    struct {
        float phase_coherence[SYNC_BAND_COUNT];
        float gamma_sync;                // High-freq binding
        float theta_sync;                // Memory/attention
    } audio_sync;

    // Shared listening goals
    struct {
        float shared_attention_freq_hz;
        float shared_attention_azimuth;
        bool joint_localization_active;
    } shared_goal;

    // Distributed coverage (bat colony mode)
    struct {
        float my_freq_range_min;
        float my_freq_range_max;
        bool distributed_echolocation;
    } distributed_coverage;

    // Collective phi contribution
    float local_phi_contribution;

    // Bidirectional verification
    uint64_t last_outbound_sync;
    uint64_t last_inbound_goal;
    bool bidirectional_verified;

} cochlea_collective_bridge_t;

// Core API
cochlea_collective_bridge_t* cochlea_collective_bridge_create(
    cochlea_t* cochlea,
    collective_cognition_t* collective,
    const cochlea_collective_config_t* config
);

// Join collective listening session
int cochlea_collective_join(
    cochlea_collective_bridge_t* bridge,
    uint32_t session_id
);

// Sync audio features to collective (outbound)
int cochlea_collective_sync_audio(
    cochlea_collective_bridge_t* bridge,
    const cochlea_output_t* output
);

// Receive shared attention goal (inbound)
int cochlea_collective_receive_goal(
    cochlea_collective_bridge_t* bridge,
    shared_goal_t* goal
);

// Compute phi contribution
float cochlea_collective_compute_phi(cochlea_collective_bridge_t* bridge);

// Verify bidirectional flow
bool cochlea_collective_verify_bidirectional(cochlea_collective_bridge_t* bridge);
```

### 5A.4 Cortical Columns Deep Integration

**File**: `include/core/cortical_columns/nimcp_cochlea_cortical_deep.h`

```c
/**
 * @brief Deep bidirectional Cochlea-Cortical Columns integration
 *
 * WHAT: Full tonotopic cortical column organization with bidirectional plasticity
 * WHY:  Enable learning, adaptation, and top-down modulation
 * HOW:  Hypercolumns per critical band, minicolumns per frequency, STDP
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea → Columns: Frequency activations, onset events
 * - INBOUND:  Columns → Cochlea: Attention modulation, learned expectations
 *
 * CORTICAL PLASTICITY:
 * - STDP: Tonotopic map refinement
 * - Lateral inhibition: Mexican hat sharpening
 * - Top-down: Attention-based gain modulation
 */

#include "core/cortical_columns/nimcp_cortical_column.h"
#include "core/cortical_columns/nimcp_columnar_connectivity.h"

typedef struct {
    cochlea_t* cochlea;
    cortical_column_pool_t* pool;

    // Tonotopic hypercolumns (one per critical band)
    uint32_t num_hypercolumns;
    hypercolumn_t** frequency_hypercolumns;
    float* center_frequencies;

    // Minicolumns per hypercolumn (~100 neurons each)
    uint32_t minicolumns_per_hypercolumn;

    // Lateral connectivity (Mexican hat)
    float excitatory_radius_octaves;
    float inhibitory_radius_octaves;

    // STDP plasticity
    stdp_config_t tonotopic_plasticity;
    bool enable_plasticity;

    // Top-down modulation (inbound)
    struct {
        float* attention_gain;           // Per-column gain
        float* expected_activation;      // Predictive coding
        uint32_t num_columns;
    } top_down;

    // Bottom-up output (outbound)
    struct {
        float* column_activations;
        float* prediction_errors;
        uint32_t num_columns;
    } bottom_up;

    // Bidirectional verification
    uint64_t last_bottom_up_time;
    uint64_t last_top_down_time;
    bool bidirectional_verified;

} cochlea_cortical_deep_t;

// Core API
cochlea_cortical_deep_t* cochlea_cortical_deep_create(
    cochlea_t* cochlea,
    cortical_column_pool_t* pool,
    const cochlea_cortical_deep_config_t* config
);

// Process cochlea → columns (bottom-up)
int cochlea_cortical_process_bottom_up(
    cochlea_cortical_deep_t* cc,
    const cochlea_output_t* cochlea_output
);

// Apply columns → cochlea modulation (top-down)
int cochlea_cortical_apply_top_down(
    cochlea_cortical_deep_t* cc,
    const float* attention_pattern,
    uint32_t pattern_size
);

// Compute prediction error
int cochlea_cortical_compute_prediction_error(
    cochlea_cortical_deep_t* cc,
    float* prediction_error
);

// Apply STDP learning
int cochlea_cortical_apply_stdp(cochlea_cortical_deep_t* cc, float dt_ms);

// Verify bidirectional flow
bool cochlea_cortical_verify_bidirectional(cochlea_cortical_deep_t* cc);
```

### 5A.5 Occipital Lobe / Audiovisual Integration

**File**: `include/core/brain/regions/occipital/nimcp_cochlea_occipital_bridge.h`

```c
/**
 * @brief Cochlea-Occipital (Visual) bidirectional bridge
 *
 * WHAT: Audiovisual binding between cochlea and visual cortex
 * WHY:  McGurk effect, lip reading, sound-source localization
 * HOW:  Temporal alignment, spatial binding, cross-modal attention
 *
 * BIOLOGICAL BASIS:
 * - Superior temporal sulcus (STS): Audiovisual integration
 * - McGurk effect: Visual lip movements alter auditory percept
 * - Sound-induced flash illusion: Audio affects visual perception
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea → Occipital: Speech envelope, sound azimuth, echo map
 * - INBOUND:  Occipital → Cochlea: Lip positions, visual gaze, object locations
 */

#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "core/brain/regions/occipital/nimcp_occipital_audiovisual_bridge.h"

typedef struct {
    cochlea_t* cochlea;
    occipital_adapter_t* occipital;
    occipital_audiovisual_bridge_t* av_bridge;

    // Audio → Visual (outbound)
    struct {
        float speech_envelope[256];
        float phoneme_features[64];
        float sound_azimuth_deg;
        float sound_elevation_deg;
        echolocation_result_t* echo_spatial_map;
    } audio_to_visual;

    // Visual → Audio (inbound)
    struct {
        float lip_aperture;
        float lip_protrusion;
        float jaw_position;
        float visual_gaze_x;
        float visual_gaze_y;
        float expected_sound_azimuth;
    } visual_to_audio;

    // Temporal alignment
    float av_offset_ms;                  // ~150ms lips lead audio
    float* alignment_buffer;
    uint32_t buffer_size;

    // McGurk processing
    bool mcgurk_mode_active;
    float visual_weight;                 // 0-1, higher = more visual influence

    // Echolocation → Visual mapping (bat mode)
    bool echo_to_visual_active;

    // Bidirectional verification
    uint64_t last_audio_to_visual;
    uint64_t last_visual_to_audio;
    bool bidirectional_verified;

} cochlea_occipital_bridge_t;

// Core API
cochlea_occipital_bridge_t* cochlea_occipital_bridge_create(
    cochlea_t* cochlea,
    occipital_adapter_t* occipital,
    const cochlea_occipital_config_t* config
);

// Send audio features to occipital (outbound)
int cochlea_occipital_send_audio(
    cochlea_occipital_bridge_t* bridge,
    const cochlea_output_t* output
);

// Receive visual features (inbound)
int cochlea_occipital_receive_visual(
    cochlea_occipital_bridge_t* bridge,
    const occipital_visual_features_t* features
);

// Perform audiovisual binding
int cochlea_occipital_bind(cochlea_occipital_bridge_t* bridge);

// Verify bidirectional flow
bool cochlea_occipital_verify_bidirectional(cochlea_occipital_bridge_t* bridge);
```

### 5A.6 Broca's Region Integration

**File**: `include/core/brain/regions/broca/nimcp_cochlea_broca_bridge.h`

```c
/**
 * @brief Cochlea-Broca's Region bidirectional bridge
 *
 * WHAT: Connect auditory input to speech production system
 * WHY:  Speech perception-production link, phonological processing
 * HOW:  Feed phonemes to Broca, receive articulatory expectations
 *
 * BIOLOGICAL BASIS:
 * - Wernicke-Geschwind model: Auditory → Wernicke → Broca
 * - Mirror neurons: Hearing speech activates speech production areas
 * - Phonological loop: Subvocal rehearsal for working memory
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea → Broca: Phoneme features, speech envelope, prosody
 * - INBOUND:  Broca → Cochlea: Predicted phonemes, articulatory templates
 */

#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/broca/nimcp_phonological.h"

typedef struct {
    cochlea_t* cochlea;
    broca_adapter_t* broca;
    phonological_processor_t* phonological;

    // Audio → Speech (outbound)
    struct {
        float* phoneme_activations;      // Detected phonemes
        uint32_t num_phonemes;
        float speech_envelope[256];
        float pitch_contour[64];         // Prosody
        float speech_rate;
        bool voice_detected;
    } audio_to_speech;

    // Speech → Audio (inbound) - Predictions
    struct {
        float* expected_phonemes;        // What Broca expects to hear
        uint32_t num_expected;
        float* articulatory_template;    // Motor-to-auditory mapping
        bool subvocal_active;            // Inner speech mode
    } speech_to_audio;

    // Phonological loop
    bool phonological_loop_active;
    float* loop_buffer;
    uint32_t loop_position;

    // Mirror activation
    float mirror_neuron_activation;

    // Bidirectional verification
    uint64_t last_audio_to_speech;
    uint64_t last_speech_to_audio;
    bool bidirectional_verified;

} cochlea_broca_bridge_t;

// Core API
cochlea_broca_bridge_t* cochlea_broca_bridge_create(
    cochlea_t* cochlea,
    broca_adapter_t* broca,
    const cochlea_broca_config_t* config
);

// Send phoneme features to Broca (outbound)
int cochlea_broca_send_phonemes(
    cochlea_broca_bridge_t* bridge,
    const float* phoneme_features,
    uint32_t num_features
);

// Receive articulatory predictions (inbound)
int cochlea_broca_receive_predictions(
    cochlea_broca_bridge_t* bridge,
    broca_prediction_t* predictions
);

// Activate phonological loop
int cochlea_broca_activate_loop(cochlea_broca_bridge_t* bridge);

// Verify bidirectional flow
bool cochlea_broca_verify_bidirectional(cochlea_broca_bridge_t* bridge);
```

### 5A.7 Bio-Async Full Integration with Verification

**File**: `include/async/nimcp_cochlea_bio_async.h`

```c
/**
 * @brief Cochlea Bio-Async complete integration with bidirectional verification
 *
 * WHAT: Full bio-async integration with all message types and verification
 * WHY:  Ensure reliable cross-module communication
 * HOW:  Register handlers, send/receive messages, verify round-trips
 *
 * MESSAGE CATEGORIES:
 * - COCHLEA_TO_*: Outbound messages to other modules
 * - *_TO_COCHLEA: Inbound messages from other modules
 *
 * VERIFICATION:
 * - Ping-pong tests for each connected module
 * - Latency measurement
 * - Message delivery confirmation
 */

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Cochlea-specific bio-async module ID
#define BIO_MODULE_COCHLEA              0x1100
#define BIO_MODULE_COCHLEA_DOG          0x1101
#define BIO_MODULE_COCHLEA_BAT          0x1102

// Outbound message types (Cochlea → Others)
typedef enum {
    COCHLEA_MSG_AUDIO_ONSET = 0x1100,
    COCHLEA_MSG_AUDIO_OFFSET,
    COCHLEA_MSG_FREQUENCY_PEAK,
    COCHLEA_MSG_SPEECH_DETECTED,
    COCHLEA_MSG_ALARM_DETECTED,
    COCHLEA_MSG_ECHOLOCATION_TARGET,
    COCHLEA_MSG_SOUND_LOCALIZED,
    COCHLEA_MSG_ULTRASONIC_DETECTED,
    COCHLEA_MSG_STATE_UPDATE,
    COCHLEA_MSG_DAMAGE_ALERT,
    COCHLEA_MSG_PING                    // For verification
} cochlea_outbound_msg_t;

// Inbound message types (Others → Cochlea)
typedef enum {
    COCHLEA_MSG_ATTENTION_COMMAND = 0x1180,
    COCHLEA_MSG_GAIN_MODULATION,
    COCHLEA_MSG_FREQUENCY_FOCUS,
    COCHLEA_MSG_PROTECTION_TRIGGER,
    COCHLEA_MSG_MODE_SWITCH,
    COCHLEA_MSG_PREDICTION_UPDATE,
    COCHLEA_MSG_VISUAL_CUE,
    COCHLEA_MSG_GOAL_UPDATE,
    COCHLEA_MSG_PONG                    // For verification
} cochlea_inbound_msg_t;

typedef struct {
    cochlea_t* cochlea;
    bio_router_t* router;
    bio_module_context_t* context;

    // Registered handlers
    bio_message_handler_t handlers[32];
    uint32_t num_handlers;

    // Connected modules for verification
    struct {
        bio_module_id_t module_id;
        const char* module_name;
        uint64_t last_outbound;
        uint64_t last_inbound;
        float latency_ms;
        bool verified;
    } connections[32];
    uint32_t num_connections;

    // Statistics
    struct {
        uint64_t messages_sent;
        uint64_t messages_received;
        uint64_t verification_passes;
        uint64_t verification_fails;
    } stats;

} cochlea_bio_async_t;

// Core API
cochlea_bio_async_t* cochlea_bio_async_create(
    cochlea_t* cochlea,
    bio_router_t* router,
    const cochlea_bio_async_config_t* config
);

// Register cochlea module with router
int cochlea_bio_async_register(cochlea_bio_async_t* ba);

// Register message handler
int cochlea_bio_async_add_handler(
    cochlea_bio_async_t* ba,
    cochlea_inbound_msg_t msg_type,
    bio_message_handler_t handler
);

// Send message (outbound)
int cochlea_bio_async_send(
    cochlea_bio_async_t* ba,
    bio_module_id_t dest,
    cochlea_outbound_msg_t msg_type,
    const void* payload,
    size_t payload_size
);

// Process inbox (called in cochlea update loop)
int cochlea_bio_async_process_inbox(cochlea_bio_async_t* ba);

// Verify bidirectional connection to specific module
bool cochlea_bio_async_verify_connection(
    cochlea_bio_async_t* ba,
    bio_module_id_t module_id,
    float timeout_ms
);

// Verify all connections
int cochlea_bio_async_verify_all_connections(
    cochlea_bio_async_t* ba,
    uint32_t* passed,
    uint32_t* failed
);

// Get connection statistics
void cochlea_bio_async_get_stats(
    cochlea_bio_async_t* ba,
    cochlea_bio_async_stats_t* stats
);
```

---

## 5B. Bidirectional Data Flow Verification System

### 5B.1 Verification Framework

```c
/**
 * @brief Unified bidirectional verification for all cochlea bridges
 *
 * WHAT: Verify all bidirectional data flows are functional
 * WHY:  Ensure reliable integration across all connected modules
 * HOW:  Ping-pong tests, latency measurement, data integrity checks
 */

typedef struct {
    const char* bridge_name;
    bool outbound_verified;
    bool inbound_verified;
    bool bidirectional_verified;
    float outbound_latency_ms;
    float inbound_latency_ms;
    uint64_t last_verification_time;
    char error_message[256];
} bridge_verification_result_t;

typedef struct {
    cochlea_t* cochlea;

    // All bridges to verify
    cochlea_brain_kg_bridge_t* kg_bridge;
    cochlea_rcog_bridge_t* rcog_bridge;
    cochlea_collective_bridge_t* collective_bridge;
    cochlea_cortical_deep_t* cortical_bridge;
    cochlea_occipital_bridge_t* occipital_bridge;
    cochlea_broca_bridge_t* broca_bridge;
    cochlea_bio_async_t* bio_async;
    cochlea_medulla_bridge_t* medulla_bridge;
    cochlea_immune_bridge_t* immune_bridge;
    cochlea_thalamic_bridge_t* thalamic_bridge;

    // Verification results
    bridge_verification_result_t results[16];
    uint32_t num_bridges;

    // Overall status
    bool all_verified;
    uint32_t verified_count;
    uint32_t failed_count;

} cochlea_verification_system_t;

// Create verification system
cochlea_verification_system_t* cochlea_verification_create(cochlea_t* cochlea);

// Register bridge for verification
int cochlea_verification_register_bridge(
    cochlea_verification_system_t* vs,
    const char* bridge_name,
    void* bridge,
    bool (*verify_func)(void*)
);

// Run verification for all bridges
int cochlea_verification_run_all(
    cochlea_verification_system_t* vs,
    bridge_verification_result_t* results,
    uint32_t* num_results
);

// Get overall verification status
bool cochlea_verification_all_passed(cochlea_verification_system_t* vs);

// Print verification report
void cochlea_verification_print_report(cochlea_verification_system_t* vs);
```

### 5B.2 Verification Test Protocol

```
BIDIRECTIONAL VERIFICATION PROTOCOL:

For each bridge:
1. OUTBOUND TEST
   a. Send test message from cochlea to target module
   b. Wait for acknowledgment (timeout: 100ms)
   c. Measure latency
   d. Record pass/fail

2. INBOUND TEST
   a. Request target module to send test message to cochlea
   b. Receive and validate message
   c. Measure latency
   d. Record pass/fail

3. ROUND-TRIP TEST
   a. Send data from cochlea
   b. Target module transforms data
   c. Transformed data returned to cochlea
   d. Verify transformation correctness
   e. Measure total latency

4. DATA INTEGRITY TEST
   a. Send structured data (frequencies, amplitudes, timestamps)
   b. Receive and verify data matches expected values
   c. Check for corruption or loss

SUCCESS CRITERIA:
- All outbound tests pass
- All inbound tests pass
- Round-trip latency < 10ms
- Data integrity 100%
```

---

## 6. Complete File Organization

```
include/perception/
├── nimcp_cochlea.h                      # Main integration header
├── nimcp_basilar_membrane.h             # Filterbank API
├── nimcp_hair_cells.h                   # IHC/OHC API
├── nimcp_auditory_nerve.h               # ANF API
├── nimcp_cochlea_extended.h             # Dog/Bat extensions
└── nimcp_echolocation.h                 # Bat echolocation

include/perception/bridges/
├── nimcp_cochlea_audio_cortex_bridge.h
├── nimcp_cochlea_medulla_bridge.h
├── nimcp_cochlea_thalamic_bridge.h
├── nimcp_cochlea_substrate_bridge.h
├── nimcp_cochlea_immune_bridge.h
├── nimcp_cochlea_cortical_columns.h
├── nimcp_cochlea_audiovisual_bridge.h
├── nimcp_cochlea_logic_bridge.h
├── nimcp_cochlea_cognitive_bridge.h
├── nimcp_cochlea_introspection.h
├── nimcp_snn_cochlea_bridge.h
├── nimcp_cochlea_brain_kg_bridge.h      # NEW: Internal KG integration
├── nimcp_cochlea_rcog_bridge.h          # NEW: Recursive cognition
├── nimcp_cochlea_collective_bridge.h    # NEW: Collective cognition
├── nimcp_cochlea_cortical_deep.h        # NEW: Deep cortical columns
├── nimcp_cochlea_occipital_bridge.h     # NEW: Occipital/AV binding
├── nimcp_cochlea_broca_bridge.h         # NEW: Broca's region
├── nimcp_cochlea_bio_async.h            # NEW: Full bio-async
└── nimcp_cochlea_verification.h         # NEW: Bidirectional verification

src/lib/perception/
├── nimcp_cochlea.c
├── nimcp_basilar_membrane.c
├── nimcp_hair_cells.c
├── nimcp_auditory_nerve.c
├── nimcp_cochlea_extended.c
├── nimcp_echolocation.c
└── bridges/
    ├── nimcp_cochlea_audio_cortex_bridge.c
    ├── nimcp_cochlea_medulla_bridge.c
    ├── nimcp_cochlea_thalamic_bridge.c
    ├── nimcp_cochlea_substrate_bridge.c
    ├── nimcp_cochlea_immune_bridge.c
    ├── nimcp_cochlea_cortical_columns.c
    ├── nimcp_cochlea_audiovisual_bridge.c
    ├── nimcp_cochlea_logic_bridge.c
    ├── nimcp_cochlea_cognitive_bridge.c
    ├── nimcp_cochlea_introspection.c
    ├── nimcp_snn_cochlea_bridge.c
    ├── nimcp_cochlea_brain_kg_bridge.c      # NEW
    ├── nimcp_cochlea_rcog_bridge.c          # NEW
    ├── nimcp_cochlea_collective_bridge.c    # NEW
    ├── nimcp_cochlea_cortical_deep.c        # NEW
    ├── nimcp_cochlea_occipital_bridge.c     # NEW
    ├── nimcp_cochlea_broca_bridge.c         # NEW
    ├── nimcp_cochlea_bio_async.c            # NEW
    └── nimcp_cochlea_verification.c         # NEW

src/core/brain/factory/init/
├── nimcp_brain_init_cochlea.c
└── nimcp_brain_init_cochlea.h

test/unit/perception/cochlea/
├── test_basilar_membrane.cpp
├── test_hair_cells.cpp
├── test_auditory_nerve.cpp
├── test_cochlea_integration.cpp
├── test_cochlea_dog_mode.cpp
├── test_cochlea_bat_mode.cpp
├── test_echolocation.cpp
├── test_cochlea_medulla_bridge.cpp
├── test_cochlea_immune_bridge.cpp
├── test_cochlea_logic_bridge.cpp
├── test_cochlea_introspection.cpp
├── test_cochlea_brain_kg_bridge.cpp         # NEW: KG bridge
├── test_cochlea_rcog_bridge.cpp             # NEW: Recursive cognition
├── test_cochlea_collective_bridge.cpp       # NEW: Collective cognition
├── test_cochlea_cortical_deep.cpp           # NEW: Deep cortical
├── test_cochlea_occipital_bridge.cpp        # NEW: Occipital/AV
├── test_cochlea_broca_bridge.cpp            # NEW: Broca's region
└── test_cochlea_bio_async.cpp               # NEW: Bio-async

test/integration/perception/cochlea/
├── test_cochlea_audio_cortex_integration.cpp
├── test_cochlea_medulla_integration.cpp
├── test_cochlea_thalamus_integration.cpp
├── test_cochlea_cortical_columns_integration.cpp
├── test_cochlea_immune_integration.cpp
├── test_cochlea_cognitive_integration.cpp
├── test_cochlea_multimodal_integration.cpp
├── test_cochlea_brain_factory_integration.cpp
├── test_cochlea_rcog_integration.cpp        # NEW: RCOG integration
├── test_cochlea_collective_integration.cpp  # NEW: Collective integration
├── test_cochlea_broca_integration.cpp       # NEW: Broca integration
├── test_cochlea_occipital_integration.cpp   # NEW: Occipital integration
└── test_cochlea_brain_kg_integration.cpp    # NEW: Brain KG integration

test/integration/perception/cochlea/bidirectional/
├── test_cochlea_bidirectional_kg.cpp        # NEW: KG bidirectional
├── test_cochlea_bidirectional_rcog.cpp      # NEW: RCOG bidirectional
├── test_cochlea_bidirectional_collective.cpp # NEW: Collective bidirectional
├── test_cochlea_bidirectional_cortical.cpp  # NEW: Cortical bidirectional
├── test_cochlea_bidirectional_occipital.cpp # NEW: Occipital bidirectional
├── test_cochlea_bidirectional_broca.cpp     # NEW: Broca bidirectional
├── test_cochlea_bidirectional_bio_async.cpp # NEW: Bio-async bidirectional
└── test_cochlea_verification_system.cpp     # NEW: Full verification

test/regression/perception/cochlea/
├── test_cochlea_frequency_response_regression.cpp
├── test_cochlea_ohc_gain_regression.cpp
├── test_cochlea_phase_locking_regression.cpp
├── test_cochlea_dog_mode_regression.cpp
├── test_cochlea_bat_mode_regression.cpp
├── test_cochlea_performance_regression.cpp
└── test_cochlea_bidirectional_latency_regression.cpp  # NEW

test/e2e/
├── e2e_test_cochlea_pipeline.cpp
├── e2e_test_cochlea_to_speech_pipeline.cpp
├── e2e_test_cochlea_echolocation_pipeline.cpp
├── e2e_test_cochlea_immune_protection.cpp
├── e2e_test_cochlea_brain_integration.cpp
├── e2e_test_cochlea_rcog_goal_pipeline.cpp  # NEW: Goal-directed listening
├── e2e_test_cochlea_collective_swarm.cpp    # NEW: Collective hearing
├── e2e_test_cochlea_broca_speech.cpp        # NEW: Speech perception
├── e2e_test_cochlea_occipital_av.cpp        # NEW: Audiovisual binding
└── e2e_test_cochlea_full_bidirectional.cpp  # NEW: All bridges verified

docs/claude/modules/
└── cochlear.md
```

---

## 7. Complete Test Suite Specifications

### 7.1 Unit Tests (95 tests)

#### Basilar Membrane Tests
```cpp
// test/unit/perception/cochlea/test_basilar_membrane.cpp
TEST(BasilarMembrane, CreateWithValidConfig)
TEST(BasilarMembrane, CreateWithInvalidConfigReturnsNull)
TEST(BasilarMembrane, GammatoneImpulseResponse)
TEST(BasilarMembrane, GammachirpPhaseCompensation)
TEST(BasilarMembrane, ERBSpacingCorrect)
TEST(BasilarMembrane, CenterFrequencyAccuracy)
TEST(BasilarMembrane, FilterBandwidthMatchesERB)
TEST(BasilarMembrane, ProcessAudioOutputsCorrectShape)
TEST(BasilarMembrane, FrequencyResponsePeaksAtCF)
TEST(BasilarMembrane, UltrasonicChannelsDogMode)
TEST(BasilarMembrane, UltrasonicChannelsBatMode)
TEST(BasilarMembrane, MemoryCleanupOnDestroy)
```

#### Hair Cell Tests
```cpp
// test/unit/perception/cochlea/test_hair_cells.cpp
TEST(HairCells, IHCCreateWithValidConfig)
TEST(HairCells, OHCCreateWithValidConfig)
TEST(HairCells, IHCBoltzmannTransduction)
TEST(HairCells, OHCElectromotility)
TEST(HairCells, OHCGain40to60dB)
TEST(HairCells, AdaptationFastTimeConstant)
TEST(HairCells, AdaptationSlowTimeConstant)
TEST(HairCells, FrequencyTuningSharpening)
TEST(HairCells, RibbonSynapseRelease)
TEST(HairCells, OtoacousticEmission)
TEST(HairCells, ACHModulationOfOHC)
TEST(HairCells, BankProcessingPerformance)
```

#### Auditory Nerve Tests
```cpp
// test/unit/perception/cochlea/test_auditory_nerve.cpp
TEST(AuditoryNerve, HighSRFiberProperties)
TEST(AuditoryNerve, MedSRFiberProperties)
TEST(AuditoryNerve, LowSRFiberProperties)
TEST(AuditoryNerve, SpontaneousRateDistribution)
TEST(AuditoryNerve, RateLevelFunction)
TEST(AuditoryNerve, PhaseLockingBelow4kHz)
TEST(AuditoryNerve, NoPhaseLockingAbove4kHz)
TEST(AuditoryNerve, RefractoryPeriod)
TEST(AuditoryNerve, MicrosecondPrecisionBatMode)
TEST(AuditoryNerve, PopulationFiringRate)
TEST(AuditoryNerve, NeurogramGeneration)
```

#### Dog Mode Tests
```cpp
// test/unit/perception/cochlea/test_cochlea_dog_mode.cpp
TEST(DogMode, FrequencyRangeTo45kHz)
TEST(DogMode, FrequencyRangeTo65kHz)
TEST(DogMode, UltrasonicSensitivity)
TEST(DogMode, PinnaeMobilityGain)
TEST(DogMode, ITDResolutionEnhanced)
TEST(DogMode, ILDSensitivityEnhanced)
TEST(DogMode, BreedSpecificProfiles)
TEST(DogMode, DirectionalHearingAccuracy)
```

#### Bat Mode Tests
```cpp
// test/unit/perception/cochlea/test_cochlea_bat_mode.cpp
TEST(BatMode, FrequencyRangeTo200kHz)
TEST(BatMode, EcholocationCallFM)
TEST(BatMode, EcholocationCallCF)
TEST(BatMode, EcholocationCallCFFM)
TEST(BatMode, DopplerShiftProcessing)
TEST(BatMode, RangeResolution1cm)
TEST(BatMode, TargetDetection)
TEST(BatMode, SpeciesProfiles)
TEST(BatMode, TemporalResolution50us)
```

#### Bridge Tests
```cpp
// test/unit/perception/cochlea/test_cochlea_medulla_bridge.cpp
TEST(CochleaMedulla, CreateBridge)
TEST(CochleaMedulla, CochlearNucleusRelay)
TEST(CochleaMedulla, SuperiorOliveProcessing)
TEST(CochleaMedulla, ProtectiveCutoffTriggering)
TEST(CochleaMedulla, ArousalModulation)
TEST(CochleaMedulla, CircadianGating)

// test/unit/perception/cochlea/test_cochlea_immune_bridge.cpp
TEST(CochleaImmune, CreateBridge)
TEST(CochleaImmune, PresentAntigenNoise)
TEST(CochleaImmune, PresentAntigenOtotoxicity)
TEST(CochleaImmune, HairCellDamageTracking)
TEST(CochleaImmune, CytokineResponse)
TEST(CochleaImmune, RecoveryPotential)

// test/unit/perception/cochlea/test_cochlea_logic_bridge.cpp
TEST(CochleaLogic, FrequencyComparison)
TEST(CochleaLogic, AmplitudeThreshold)
TEST(CochleaLogic, HarmonicDetection)
TEST(CochleaLogic, OnsetDetection)
TEST(CochleaLogic, EcholocationGate)
```

#### Advanced Bridge Tests (NEW)
```cpp
// test/unit/perception/cochlea/test_cochlea_brain_kg_bridge.cpp
TEST(CochleaBrainKG, CreateBridge)
TEST(CochleaBrainKG, RegisterModule)
TEST(CochleaBrainKG, SyncState)
TEST(CochleaBrainKG, QueryContext)
TEST(CochleaBrainKG, GetConnections)
TEST(CochleaBrainKG, VerifyBidirectional)

// test/unit/perception/cochlea/test_cochlea_rcog_bridge.cpp
TEST(CochleaRCOG, CreateBridge)
TEST(CochleaRCOG, RegisterTool)
TEST(CochleaRCOG, ReceiveGoal)
TEST(CochleaRCOG, SendEvent)
TEST(CochleaRCOG, UpdateContext)
TEST(CochleaRCOG, VerifyBidirectional)

// test/unit/perception/cochlea/test_cochlea_collective_bridge.cpp
TEST(CochleaCollective, CreateBridge)
TEST(CochleaCollective, JoinSession)
TEST(CochleaCollective, SyncAudio)
TEST(CochleaCollective, ReceiveGoal)
TEST(CochleaCollective, ComputePhi)
TEST(CochleaCollective, VerifyBidirectional)

// test/unit/perception/cochlea/test_cochlea_cortical_deep.cpp
TEST(CochleaCorticalDeep, CreateBridge)
TEST(CochleaCorticalDeep, ProcessBottomUp)
TEST(CochleaCorticalDeep, ApplyTopDown)
TEST(CochleaCorticalDeep, ComputePredictionError)
TEST(CochleaCorticalDeep, ApplySTDP)
TEST(CochleaCorticalDeep, VerifyBidirectional)

// test/unit/perception/cochlea/test_cochlea_occipital_bridge.cpp
TEST(CochleaOccipital, CreateBridge)
TEST(CochleaOccipital, SendAudio)
TEST(CochleaOccipital, ReceiveVisual)
TEST(CochleaOccipital, Bind)
TEST(CochleaOccipital, McGurk)
TEST(CochleaOccipital, VerifyBidirectional)

// test/unit/perception/cochlea/test_cochlea_broca_bridge.cpp
TEST(CochleaBroca, CreateBridge)
TEST(CochleaBroca, SendPhonemes)
TEST(CochleaBroca, ReceivePredictions)
TEST(CochleaBroca, ActivateLoop)
TEST(CochleaBroca, MirrorActivation)
TEST(CochleaBroca, VerifyBidirectional)

// test/unit/perception/cochlea/test_cochlea_bio_async.cpp
TEST(CochleaBioAsync, CreateBridge)
TEST(CochleaBioAsync, Register)
TEST(CochleaBioAsync, AddHandler)
TEST(CochleaBioAsync, Send)
TEST(CochleaBioAsync, ProcessInbox)
TEST(CochleaBioAsync, VerifyConnection)
TEST(CochleaBioAsync, VerifyAllConnections)
```

### 7.2 Integration Tests (45 tests)

```cpp
// test/integration/perception/cochlea/test_cochlea_audio_cortex_integration.cpp
TEST(CochleaAudioCortex, EndToEndProcessing)
TEST(CochleaAudioCortex, TonotopicMapping)
TEST(CochleaAudioCortex, FeatureExtraction)
TEST(CochleaAudioCortex, AttentionModulation)

// test/integration/perception/cochlea/test_cochlea_medulla_integration.cpp
TEST(CochleaMedullaIntegration, BrainstemPathway)
TEST(CochleaMedullaIntegration, ProtectiveReflexLoop)
TEST(CochleaMedullaIntegration, ArousalCoupling)
TEST(CochleaMedullaIntegration, CircadianSynchronization)

// test/integration/perception/cochlea/test_cochlea_thalamus_integration.cpp
TEST(CochleaThalamusIntegration, MGNRelay)
TEST(CochleaThalamusIntegration, AttentionGating)
TEST(CochleaThalamusIntegration, TonicBurstMode)

// test/integration/perception/cochlea/test_cochlea_cortical_columns_integration.cpp
TEST(CochleaCorticalColumns, HypercolumnCreation)
TEST(CochleaCorticalColumns, CompetitiveDynamics)
TEST(CochleaCorticalColumns, LateralInhibition)

// test/integration/perception/cochlea/test_cochlea_immune_integration.cpp
TEST(CochleaImmuneIntegration, AntigenPresentationFlow)
TEST(CochleaImmuneIntegration, ProtectiveResponse)
TEST(CochleaImmuneIntegration, DamageRecovery)

// test/integration/perception/cochlea/test_cochlea_cognitive_integration.cpp
TEST(CochleaCognitive, AttentionCocktailParty)
TEST(CochleaCognitive, MemoryConsolidation)
TEST(CochleaCognitive, EmotionAlarmResponse)
TEST(CochleaCognitive, SalienceNovelSound)

// test/integration/perception/cochlea/test_cochlea_multimodal_integration.cpp
TEST(CochleaMultimodal, AudiovisualBinding)
TEST(CochleaMultimodal, LipReadingEnhancement)
TEST(CochleaMultimodal, SpatialAudioVisual)

// test/integration/perception/cochlea/test_cochlea_brain_factory_integration.cpp
TEST(CochleaBrainFactory, InitializationSequence)

// test/integration/perception/cochlea/test_cochlea_rcog_integration.cpp (NEW)
TEST(CochleaRCOGIntegration, GoalDirectedListening)
TEST(CochleaRCOGIntegration, ContextVariableUpdates)
TEST(CochleaRCOGIntegration, ToolRouterRegistration)

// test/integration/perception/cochlea/test_cochlea_collective_integration.cpp (NEW)
TEST(CochleaCollectiveIntegration, JointAttention)
TEST(CochleaCollectiveIntegration, DistributedCoverage)
TEST(CochleaCollectiveIntegration, PhiContribution)

// test/integration/perception/cochlea/test_cochlea_broca_integration.cpp (NEW)
TEST(CochleaBrocaIntegration, PhonemeFlow)
TEST(CochleaBrocaIntegration, PhonologicalLoop)
TEST(CochleaBrocaIntegration, SpeechMotorLink)

// test/integration/perception/cochlea/test_cochlea_occipital_integration.cpp (NEW)
TEST(CochleaOccipitalIntegration, AudiovisualTiming)
TEST(CochleaOccipitalIntegration, SpatialBinding)
TEST(CochleaOccipitalIntegration, EchoVisualMapping)

// test/integration/perception/cochlea/test_cochlea_brain_kg_integration.cpp (NEW)
TEST(CochleaKGIntegration, NodeRegistration)
TEST(CochleaKGIntegration, EdgeCreation)
TEST(CochleaKGIntegration, PropertyUpdates)
```

### 7.2.1 Bidirectional Verification Tests (NEW - 24 tests)

```cpp
// test/integration/perception/cochlea/bidirectional/test_cochlea_bidirectional_kg.cpp
TEST(CochleaBidirectionalKG, OutboundStateUpdate)
TEST(CochleaBidirectionalKG, InboundContextQuery)
TEST(CochleaBidirectionalKG, RoundTripVerification)

// test/integration/perception/cochlea/bidirectional/test_cochlea_bidirectional_rcog.cpp
TEST(CochleaBidirectionalRCOG, OutboundAudioEvent)
TEST(CochleaBidirectionalRCOG, InboundListeningGoal)
TEST(CochleaBidirectionalRCOG, RoundTripGoalFeedback)

// test/integration/perception/cochlea/bidirectional/test_cochlea_bidirectional_collective.cpp
TEST(CochleaBidirectionalCollective, OutboundAudioSync)
TEST(CochleaBidirectionalCollective, InboundSharedGoal)
TEST(CochleaBidirectionalCollective, RoundTripPhiSync)

// test/integration/perception/cochlea/bidirectional/test_cochlea_bidirectional_cortical.cpp
TEST(CochleaBidirectionalCortical, OutboundBottomUp)
TEST(CochleaBidirectionalCortical, InboundTopDown)
TEST(CochleaBidirectionalCortical, RoundTripPrediction)

// test/integration/perception/cochlea/bidirectional/test_cochlea_bidirectional_occipital.cpp
TEST(CochleaBidirectionalOccipital, OutboundSpeechEnvelope)
TEST(CochleaBidirectionalOccipital, InboundLipPosition)
TEST(CochleaBidirectionalOccipital, RoundTripMcGurk)

// test/integration/perception/cochlea/bidirectional/test_cochlea_bidirectional_broca.cpp
TEST(CochleaBidirectionalBroca, OutboundPhonemes)
TEST(CochleaBidirectionalBroca, InboundPredictions)
TEST(CochleaBidirectionalBroca, RoundTripMirror)

// test/integration/perception/cochlea/bidirectional/test_cochlea_bidirectional_bio_async.cpp
TEST(CochleaBidirectionalBioAsync, PingPongAllModules)
TEST(CochleaBidirectionalBioAsync, LatencyMeasurement)
TEST(CochleaBidirectionalBioAsync, DataIntegrityCheck)

// test/integration/perception/cochlea/bidirectional/test_cochlea_verification_system.cpp
TEST(CochleaVerification, CreateSystem)
TEST(CochleaVerification, RegisterAllBridges)
TEST(CochleaVerification, RunAllVerifications)
TEST(CochleaVerification, GenerateReport)
```

### 7.3 Regression Tests (12 tests)

```cpp
// test/regression/perception/cochlea/test_cochlea_frequency_response_regression.cpp
TEST(CochleaRegression, FilterBankFrequencyResponse)
TEST(CochleaRegression, FilterBandwidthStability)

// test/regression/perception/cochlea/test_cochlea_ohc_gain_regression.cpp
TEST(CochleaRegression, OHCGainConsistency)
TEST(CochleaRegression, CompressionRatioStability)

// test/regression/perception/cochlea/test_cochlea_phase_locking_regression.cpp
TEST(CochleaRegression, PhaseLockingAccuracy)
TEST(CochleaRegression, VectorStrengthStability)

// test/regression/perception/cochlea/test_cochlea_dog_mode_regression.cpp
TEST(CochleaRegression, DogModeFrequencyConsistency)
TEST(CochleaRegression, DogModeDirectionalAccuracy)

// test/regression/perception/cochlea/test_cochlea_bat_mode_regression.cpp
TEST(CochleaRegression, BatModeEcholocationAccuracy)
TEST(CochleaRegression, BatModeDopplerPrecision)

// test/regression/perception/cochlea/test_cochlea_performance_regression.cpp
TEST(CochleaRegression, ProcessingLatency)
TEST(CochleaRegression, MemoryUsage)
```

### 7.4 End-to-End Tests (8 tests)

```cpp
// test/e2e/e2e_test_cochlea_pipeline.cpp
TEST(CochleaE2E, FullAudioPipeline) {
    // Audio input → Cochlea → Audio Cortex → Feature extraction
    // Verify complete signal flow
}

TEST(CochleaE2E, RealTimeProcessing) {
    // Process audio at real-time rates
    // Verify latency < 10ms
}

// test/e2e/e2e_test_cochlea_to_speech_pipeline.cpp
TEST(CochleaSpeechE2E, SpeechRecognitionPath) {
    // Speech audio → Cochlea → A1 → Speech Cortex → Broca
}

TEST(CochleaSpeechE2E, CocktailPartyScenario) {
    // Multiple speakers → Attention-modulated cochlea → Target isolation
}

// test/e2e/e2e_test_cochlea_echolocation_pipeline.cpp
TEST(CochleaEchoE2E, BatEcholocationScenario) {
    // Emit call → Receive echoes → Target detection → Spatial map
}

TEST(CochleaEchoE2E, MovingTargetTracking) {
    // Doppler-based velocity tracking of moving target
}

// test/e2e/e2e_test_cochlea_immune_protection.cpp
TEST(CochleaImmuneE2E, LoudSoundProtection) {
    // Loud input → Medulla cutoff → Immune response → Recovery
}

// test/e2e/e2e_test_cochlea_brain_integration.cpp
TEST(CochleaBrainE2E, FullBrainIntegration) {
    // Create brain with cochlea → Process audio → Verify all subsystem responses
}
```

---

## 8. Implementation Phases

### Phase 1: Core Cochlear Model
1. Gammatone filterbank (human range)
2. IHC/OHC transduction
3. Auditory nerve fiber model
4. Basic cochlea integration layer
5. Unit tests for core components

### Phase 2: Brain Factory Integration
6. Brain factory initialization
7. Audio cortex bridge
8. SNN bridge
9. Bio-async registration
10. Logging integration

### Phase 3: Extended Hearing (Dog/Bat)
11. Dog ultrasonic extension
12. Bat echolocation processing
13. Mode switching logic
14. Extended tests

### Phase 4: Brainstem Integration
15. Medulla bridge
16. Protective cutoff integration
17. Arousal coupling
18. Circadian gating

### Phase 5: Thalamic & Cortical Integration
19. MGN thalamic relay
20. Cortical columns deep integration (bidirectional)
21. Cognitive bridges (attention, memory, emotion)
22. Integration tests

### Phase 6: Immune & Introspection
23. Immune system bridge
24. Introspection integration
25. KG self-awareness entities
26. Logic gate integration

### Phase 7: Advanced Cognitive Integration (NEW)
27. Internal Brain KG bridge (bidirectional)
28. Recursive Cognition engine bridge (bidirectional)
29. Collective Cognition bridge (bidirectional)
30. Verification framework for KG/RCOG/Collective

### Phase 8: Cross-Modal Integration (NEW)
31. Occipital lobe audiovisual bridge (bidirectional)
32. Broca's region speech bridge (bidirectional)
33. McGurk effect processing
34. Phonological loop integration

### Phase 9: Bio-Async Complete Integration (NEW)
35. Full bio-async message handlers
36. All module connection verification
37. Ping-pong latency testing
38. Data integrity verification

### Phase 10: Full Testing & Verification (NEW)
39. Bidirectional verification system
40. All bridge verification tests
41. Complete regression suite
42. E2E test suite
43. Performance optimization

---

## 9. Success Criteria

### Core Functionality
1. **Frequency Response**: Filterbank matches ERB scaling ±5%
2. **OHC Gain**: 40-60 dB amplification at low levels
3. **Phase Locking**: Vector strength > 0.5 below 2 kHz
4. **Dog Mode**: Reliable response up to 45 kHz
5. **Bat Mode**: Echolocation range resolution < 2 cm
6. **Real-time**: Process 48kHz audio with < 10ms latency

### Integration Criteria
7. **Total Bridges**: All 22 bridges functional
8. **Brain KG**: Module nodes registered, state syncing at 100ms intervals
9. **Recursive Cognition**: Tool registered, goals received, events sent
10. **Collective Cognition**: Phi contribution computed, goals synchronized
11. **Cortical Columns**: Bottom-up and top-down flows verified
12. **Occipital Bridge**: Audiovisual binding with <150ms latency
13. **Broca's Region**: Phoneme flow and predictions working

### Bidirectional Verification
14. **All Outbound**: 100% outbound message delivery verified
15. **All Inbound**: 100% inbound message reception verified
16. **Round-trip Latency**: <10ms for all bridges
17. **Data Integrity**: 100% data integrity across all flows

### Testing
18. **Unit Tests**: 95 tests, all passing
19. **Integration Tests**: 45 tests, all passing
20. **Bidirectional Tests**: 24 tests, all passing
21. **Regression Tests**: 13 tests, all passing
22. **E2E Tests**: 13 tests, all passing
23. **Total Tests**: 190 tests, all passing
24. **Coverage**: >85% code coverage

---

## 10. References

1. Patterson, R. D. (1994). The sound of a sinusoid: Spectral models
2. Meddis, R., & O'Mard, L. (1997). A unitary model of pitch perception
3. Zilany, M. S., Bruce, I. C. (2014). Updated parameters for auditory periphery model
4. Heffner, H. E. (1983). Hearing in large and small dogs. Behavioral Neuroscience
5. Moss, C. F., & Surlykke, A. (2010). Probing the natural scene by echolocation in bats
6. Simmons, J. A. (1979). Perception of echo phase information in bat sonar. Science
7. Ruggero, M. A. (1997). Mechanics of the mammalian cochlea. Physiological Reviews
