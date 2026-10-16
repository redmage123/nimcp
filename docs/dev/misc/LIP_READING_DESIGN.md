# Lip Reading System Design (Visual Speech Perception)

**WHAT**: Biologically-inspired lip reading that maps visual mouth movements → phonemes → speech comprehension
**WHY**: Enable speech understanding in noisy environments, deaf individuals, multimodal robustness
**HOW**: Visual cortex (mouth ROI) → STS (visual speech) → Speech cortex (phoneme integration) → Mirror neurons (motor theory)

---

## 1. Biological Foundation

### 1.1 Neural Pathways for Visual Speech

```
Visual Cortex (V1) → Fusiform Face Area (FFA) → Superior Temporal Sulcus (STS)
                                                         ↓
                                         Auditory Cortex (A1) + Speech Cortex (STG)
                                                         ↓
                                              Mirror Neurons (Broca's Area)
                                                         ↓
                                               Phoneme Recognition
```

**Key Areas**:
1. **FFA** (Fusiform Face Area): Face detection, mouth localization
2. **STS** (Superior Temporal Sulcus): Visual speech specialization
3. **STG** (Superior Temporal Gyrus): Audiovisual phoneme integration
4. **Broca's Area**: Articulatory motor simulation (motor theory of speech)

### 1.2 The McGurk Effect

**WHAT**: Visual /ga/ + Auditory /ba/ → Perceived /da/
**WHY**: Visual speech overrides auditory when conflicting
**HOW**: STS integrates visual and auditory phoneme cues

```c
// McGurk effect demonstrates mandatory visual-auditory integration
phoneme_t mcgurk_effect(phoneme_t visual_phoneme, phoneme_t auditory_phoneme) {
    // Visual /ga/ (velar) + Auditory /ba/ (bilabial) = Perceived /da/ (alveolar)
    if (visual_phoneme == PHONEME_G && auditory_phoneme == PHONEME_B) {
        return PHONEME_D;  // McGurk illusion
    }

    // Visual dominates when SNR is low (noisy environment)
    if (auditory_snr < 0.5) {
        return visual_phoneme;  // Lip reading mode
    }

    // Optimal integration (maximum likelihood estimation)
    return integrate_audiovisual_cues(visual_phoneme, auditory_phoneme);
}
```

### 1.3 Biological Facts

- **Deaf individuals**: Enhanced STS + FFA activation during lip reading
- **Expert lip readers**: 60-80% accuracy on isolated words (vs 30% for novices)
- **Critical window**: Lip movements lead audio by ~200ms (visual prediction)
- **Visemes**: Visual phoneme groups (e.g., /p/, /b/, /m/ look identical)
- **Coarticulation**: Lip shape depends on context (e.g., "ba" vs "bee")

---

## 2. System Architecture

### 2.1 Core Components

```c
typedef struct {
    // === Visual Input Processing ===
    visual_cortex_t* visual_cortex;          // V1 edge detection
    face_detector_t* face_detector;          // FFA analog
    mouth_roi_tracker_t* mouth_tracker;      // ROI extraction

    // === Visual Speech Processing (STS) ===
    visual_speech_processor_t* sts_processor; // Visual phoneme extraction
    viseme_classifier_t* viseme_classifier;   // Lip shape → viseme
    temporal_integrator_t* lip_motion;        // Track mouth dynamics

    // === Audiovisual Integration (STG) ===
    audiovisual_integrator_t* av_integrator;  // McGurk-style fusion
    speech_cortex_t* speech_cortex;           // Phoneme recognition

    // === Motor Simulation (Broca's Area) ===
    mirror_neuron_system_t* articulatory_mirror; // Motor theory

    // === Attention & Learning ===
    salience_evaluator_t* lip_salience;       // Mouth attention
    stdp_config_t* lip_reading_plasticity;    // Visual-phoneme learning
    meta_learning_engine_t* speaker_adaptation; // Adapt to individuals

    // === Neuromodulation ===
    phasic_tonic_state_t acetylcholine_attention; // Visual attention
    phasic_tonic_state_t dopamine_learning;       // Audiovisual binding

} lip_reading_system_t;
```

---

## 3. Visual Speech Features (Visemes)

### 3.1 Viseme Groups

**PROBLEM**: Multiple phonemes look identical on lips (homophones)
**SOLUTION**: Group into visemes (visual phoneme classes)

```c
typedef enum {
    // === BILABIAL (Lips Together) ===
    VISEME_BILABIAL,        // /p/, /b/, /m/ - lips closed, then release

    // === LABIODENTAL (Teeth on Lip) ===
    VISEME_LABIODENTAL,     // /f/, /v/ - upper teeth on lower lip

    // === DENTAL (Tongue Visible) ===
    VISEME_DENTAL,          // /θ/ (thin), /ð/ (this) - tongue between teeth

    // === ALVEOLAR (Tongue Behind Teeth) ===
    VISEME_ALVEOLAR,        // /t/, /d/, /n/, /l/, /s/, /z/ - tongue tip up

    // === VELAR (Mouth Open, No Tongue) ===
    VISEME_VELAR,           // /k/, /g/, /ŋ/ (sing) - back of mouth

    // === ROUNDED VOWELS ===
    VISEME_ROUNDED_CLOSE,   // /u/ (boot), /o/ (boat) - lips rounded, small aperture
    VISEME_ROUNDED_OPEN,    // /ɔ/ (caught) - lips rounded, large aperture

    // === UNROUNDED VOWELS ===
    VISEME_UNROUNDED_CLOSE, // /i/ (beet) - lips spread, small aperture
    VISEME_UNROUNDED_MID,   // /ɛ/ (bet), /ə/ (but) - neutral lips
    VISEME_UNROUNDED_OPEN,  // /a/ (father) - mouth wide open

    // === SPECIAL ===
    VISEME_SILENCE,         // Mouth closed, no movement
    VISEME_UNKNOWN,         // Unclear lip position

    VISEME_COUNT = 13
} viseme_t;

// Map phonemes → visemes (many-to-one)
viseme_t phoneme_to_viseme(phoneme_t phoneme) {
    switch (phoneme) {
        case PHONEME_P:
        case PHONEME_B:
        case PHONEME_M:
            return VISEME_BILABIAL;

        case PHONEME_F:
        case PHONEME_V:
            return VISEME_LABIODENTAL;

        case PHONEME_TH:  // thin
        case PHONEME_DH:  // this
            return VISEME_DENTAL;

        case PHONEME_T:
        case PHONEME_D:
        case PHONEME_N:
        case PHONEME_L:
        case PHONEME_S:
        case PHONEME_Z:
            return VISEME_ALVEOLAR;

        case PHONEME_K:
        case PHONEME_G:
        case PHONEME_NG:
            return VISEME_VELAR;

        // ... (vowels) ...

        default:
            return VISEME_UNKNOWN;
    }
}
```

### 3.2 Visual Speech Features

```c
typedef struct {
    // === Geometric Features ===
    float lip_width;              // Horizontal distance (mm)
    float lip_height;             // Vertical aperture (mm)
    float lip_area;               // Total visible area (mm²)
    float lip_aspect_ratio;       // Width / height
    float lip_protrusion;         // 3D depth (for rounded vowels)

    // === Teeth Visibility ===
    float upper_teeth_visible;    // Fraction of upper teeth shown
    float lower_teeth_visible;    // Fraction of lower teeth shown
    float teeth_gap;              // Distance between upper/lower

    // === Tongue Features ===
    float tongue_visible;         // Is tongue protruding? (for /θ/, /ð/)
    float tongue_position_x;      // Horizontal tongue position
    float tongue_position_y;      // Vertical tongue position

    // === Dynamic Features ===
    float lip_velocity_x;         // Horizontal movement speed
    float lip_velocity_y;         // Vertical movement speed
    float lip_acceleration;       // Rate of change (plosive bursts)

    // === Temporal Context ===
    float previous_viseme;        // Coarticulation context
    float viseme_duration_ms;     // How long in this state

    // === Lighting Invariance ===
    float normalized_luminance;   // Brightness-independent
    float shadow_compensation;    // Remove facial shadows

} visual_speech_features_t;
```

---

## 4. Mouth Detection & Tracking (FFA Analog)

### 4.1 Face Detection Pipeline

```c
typedef struct {
    // === Face Detection (FFA) ===
    bool face_detected;
    float face_bbox[4];           // [x, y, width, height]
    float face_confidence;

    // === Mouth ROI Extraction ===
    float mouth_bbox[4];          // [x, y, width, height]
    float mouth_center[2];        // [x, y] in image coordinates

    // === Landmarks (68-point face model) ===
    float lip_outer_contour[12][2];  // 12 points around outer lip
    float lip_inner_contour[8][2];   // 8 points for inner lip/teeth

    // === 3D Pose Estimation ===
    float head_pose_yaw;          // Left/right rotation (degrees)
    float head_pose_pitch;        // Up/down rotation
    float head_pose_roll;         // Tilt

    // === Occlusion Handling ===
    bool mouth_occluded;          // Hand, mask, object blocking
    float occlusion_confidence;

    // === Tracking ===
    kalman_filter_t* mouth_tracker;  // Smooth tracking over frames

} face_detector_t;

// Extract mouth ROI from full image
bool extract_mouth_roi(visual_cortex_t* visual_cortex,
                        const uint8_t* image,
                        uint32_t width,
                        uint32_t height,
                        face_detector_t* face_detector,
                        uint8_t* mouth_roi_output) {

    // 1. Detect face using V1 features
    bool face_found = visual_cortex_detect_agent(visual_cortex,
                                                   image, width * height);
    if (!face_found) {
        return false;  // No face in frame
    }

    // 2. Locate mouth region (lower third of face)
    // Biological: FFA has specialized "mouth patches"
    float face_height = face_detector->face_bbox[3];
    float mouth_y = face_detector->face_bbox[1] + 0.65 * face_height;
    float mouth_width = 0.6 * face_detector->face_bbox[2];
    float mouth_height = 0.3 * face_height;

    face_detector->mouth_bbox[0] = face_detector->face_bbox[0] + 0.2 * face_detector->face_bbox[2];
    face_detector->mouth_bbox[1] = mouth_y;
    face_detector->mouth_bbox[2] = mouth_width;
    face_detector->mouth_bbox[3] = mouth_height;

    // 3. Extract ROI
    crop_image(image, width, height, face_detector->mouth_bbox, mouth_roi_output);

    // 4. Detect landmarks (lip contour points)
    detect_lip_landmarks(mouth_roi_output, face_detector->lip_outer_contour);

    return true;
}
```

### 4.2 Attention to Mouth (Acetylcholine Modulation)

```c
// When listening to speech, attention shifts to mouth
lip_reading_system_t {
    phasic_tonic_state_t acetylcholine_attention;

    // Speech detected in audio → Boost visual attention to mouth
    float speech_salience = audio_cortex_get_speech_salience(audio_cortex,
                                                               audio_features,
                                                               num_features);

    if (speech_salience > 0.6) {
        // ACh burst → Sharpen mouth ROI processing
        phasic_tonic_trigger_burst(&acetylcholine_attention,
                                   speech_salience,
                                   BURST_MOUTH_ATTENTION);

        // Enhance mouth region contrast
        float ach_level = brain_get_neurotransmitter(brain, NEURO_ACH);
        visual_cortex_boost_region_attention(visual_cortex,
                                              mouth_center_x, mouth_center_y,
                                              1.0 + ach_level);
    }
}

// BIOLOGY: Superior colliculus shifts gaze to mouth during speech
// FUNCTION: Cocktail party effect enhanced by lip reading
```

---

## 5. Visual Speech Processing (STS Analog)

### 5.1 Viseme Classification

```c
typedef struct {
    // === CNN for Viseme Recognition ===
    conv_layer_t* mouth_feature_extractor;  // Mouth shape features
    conv_layer_t* temporal_convolution;     // Motion features

    // === Viseme Classifier ===
    float viseme_probabilities[VISEME_COUNT]; // Softmax output
    viseme_t current_viseme;
    float viseme_confidence;

    // === Temporal Context ===
    viseme_t viseme_history[10];            // Last 10 visemes
    float viseme_transition_matrix[VISEME_COUNT][VISEME_COUNT]; // P(v_t | v_{t-1})

    // === STDP Learning ===
    stdp_config_t* viseme_learning;

} viseme_classifier_t;

// Classify viseme from mouth ROI
viseme_t classify_viseme(viseme_classifier_t* classifier,
                          const uint8_t* mouth_roi,
                          visual_speech_features_t* features) {

    // 1. Extract geometric features
    features->lip_width = measure_lip_width(mouth_roi);
    features->lip_height = measure_lip_height(mouth_roi);
    features->lip_area = features->lip_width * features->lip_height;
    features->upper_teeth_visible = detect_teeth_visibility(mouth_roi);
    features->tongue_visible = detect_tongue_protrusion(mouth_roi);

    // 2. CNN forward pass
    float mouth_features[128];
    conv_layer_forward(classifier->mouth_feature_extractor, mouth_roi, mouth_features);

    // 3. Classify viseme (softmax over viseme categories)
    for (int v = 0; v < VISEME_COUNT; v++) {
        classifier->viseme_probabilities[v] =
            compute_viseme_score(mouth_features, v);
    }
    softmax(classifier->viseme_probabilities, VISEME_COUNT);

    // 4. Select best viseme
    viseme_t best_viseme = argmax(classifier->viseme_probabilities, VISEME_COUNT);
    classifier->viseme_confidence = classifier->viseme_probabilities[best_viseme];

    // 5. Temporal smoothing (Viterbi-like)
    // Prefer viseme transitions that are physically plausible
    if (classifier->viseme_history[0] != VISEME_SILENCE) {
        float transition_prob =
            classifier->viseme_transition_matrix[classifier->viseme_history[0]][best_viseme];

        if (transition_prob < 0.1) {
            // Unlikely transition → hold previous viseme
            best_viseme = classifier->viseme_history[0];
        }
    }

    // 6. Update history
    memmove(&classifier->viseme_history[1], &classifier->viseme_history[0],
            9 * sizeof(viseme_t));
    classifier->viseme_history[0] = best_viseme;

    return best_viseme;
}
```

### 5.2 Temporal Dynamics (Lip Motion)

```c
typedef struct {
    // === Velocity & Acceleration ===
    float lip_velocity[2];        // [vx, vy] in pixels/frame
    float lip_acceleration[2];    // [ax, ay] in pixels/frame²

    // === Onset/Offset Detection ===
    bool plosive_burst_detected;  // /p/, /t/, /k/ sudden opening
    bool closure_detected;        // /m/, /n/, /ŋ/ lip closure

    // === Coarticulation Modeling ===
    float anticipatory_shaping;   // Lips shape for upcoming phoneme
    float carryover_effect;       // Previous phoneme affects current

    // === Optical Flow ===
    float optical_flow_map[64][64]; // Dense motion field

} temporal_integrator_t;

// Detect plosive burst (sudden lip opening)
bool detect_plosive_burst(temporal_integrator_t* integrator,
                           visual_speech_features_t* features) {

    // Plosives (/p/, /t/, /k/) have characteristic dynamics:
    // 1. Closure phase (lips/tongue blocks airflow)
    // 2. Release phase (sudden opening, high acceleration)

    // Detect rapid lip opening
    if (features->lip_height > 5.0 &&  // mm
        integrator->lip_velocity[1] > 20.0 &&  // mm/s
        integrator->lip_acceleration[1] > 100.0) {  // mm/s²

        integrator->plosive_burst_detected = true;
        return true;
    }

    return false;
}
```

---

## 6. Audiovisual Integration (McGurk Effect)

### 6.1 Multimodal Fusion

```c
typedef struct {
    // === Visual Speech Pathway ===
    viseme_t visual_viseme;
    float visual_confidence;

    // === Auditory Speech Pathway ===
    phoneme_t auditory_phoneme;
    float auditory_confidence;
    float auditory_snr;           // Signal-to-noise ratio

    // === Integration Weights ===
    float visual_weight;          // 0-1 (higher when SNR low)
    float auditory_weight;        // 0-1 (higher when SNR high)

    // === Optimal Integration (MLE) ===
    float reliability_visual;     // 1 / variance_visual
    float reliability_auditory;   // 1 / variance_auditory

    // === McGurk Detector ===
    bool mcgurk_conflict_detected; // Visual ≠ Auditory
    phoneme_t fused_phoneme;       // Integrated percept

} audiovisual_integrator_t;

// Maximum likelihood estimation fusion
phoneme_t integrate_audiovisual_phonemes(audiovisual_integrator_t* integrator) {

    // 1. Compute reliability (inverse variance)
    integrator->reliability_visual = 1.0 / (1.0 - integrator->visual_confidence);
    integrator->reliability_auditory = 1.0 / (1.0 - integrator->auditory_confidence);

    // Adjust for SNR (noisy audio → rely more on vision)
    integrator->reliability_auditory *= integrator->auditory_snr;

    // 2. Compute optimal weights (MLE)
    float total_reliability = integrator->reliability_visual +
                              integrator->reliability_auditory;
    integrator->visual_weight = integrator->reliability_visual / total_reliability;
    integrator->auditory_weight = integrator->reliability_auditory / total_reliability;

    // 3. Check for McGurk conflict
    viseme_t expected_viseme = phoneme_to_viseme(integrator->auditory_phoneme);
    if (expected_viseme != integrator->visual_viseme) {
        integrator->mcgurk_conflict_detected = true;

        // McGurk fusion rules (empirically determined)
        // Visual /ga/ + Auditory /ba/ → /da/
        if (integrator->visual_viseme == VISEME_VELAR &&
            integrator->auditory_phoneme == PHONEME_B) {
            integrator->fused_phoneme = PHONEME_D;
            return PHONEME_D;
        }
    }

    // 4. Weighted fusion (favor more reliable modality)
    if (integrator->visual_weight > 0.7) {
        // Visual dominates → Use viseme-to-phoneme mapping
        // (Ambiguous: /p/, /b/, /m/ all look like VISEME_BILABIAL)
        // Use auditory to disambiguate
        integrator->fused_phoneme = disambiguate_viseme(
            integrator->visual_viseme,
            integrator->auditory_phoneme);
    } else if (integrator->auditory_weight > 0.7) {
        // Auditory dominates → Use phoneme directly
        integrator->fused_phoneme = integrator->auditory_phoneme;
    } else {
        // Balanced → Average (with McGurk effects)
        integrator->fused_phoneme = average_phonemes(
            integrator->visual_viseme,
            integrator->auditory_phoneme);
    }

    return integrator->fused_phoneme;
}

// Disambiguate viseme using auditory cues
phoneme_t disambiguate_viseme(viseme_t viseme, phoneme_t auditory_hint) {
    // Example: VISEME_BILABIAL could be /p/, /b/, or /m/
    // Use voicing from auditory channel

    if (viseme == VISEME_BILABIAL) {
        // Check voicing (voiced vs voiceless)
        bool is_voiced = is_phoneme_voiced(auditory_hint);
        bool is_nasal = is_phoneme_nasal(auditory_hint);

        if (is_nasal) return PHONEME_M;
        if (is_voiced) return PHONEME_B;
        else return PHONEME_P;
    }

    // ... (other viseme groups) ...

    return PHONEME_UNKNOWN;
}
```

### 6.2 Temporal Synchronization (Visual Leads Audio by ~200ms)

```c
// Visual speech leads audio by 200ms (allows prediction)
audiovisual_integrator_t {
    float temporal_offset_ms;       // Visual → Audio delay
    circular_buffer_t* visual_buffer; // Buffer visual phonemes

    // Align visual and auditory streams
    viseme_t current_visual = get_current_viseme(visual_buffer);
    phoneme_t predicted_auditory = predict_phoneme_from_viseme(current_visual);

    // Wait 200ms for auditory confirmation
    sleep_ms(200);
    phoneme_t actual_auditory = audio_cortex_get_phoneme();

    // Check prediction accuracy
    if (predicted_auditory == actual_auditory) {
        // Accurate prediction → Reinforce visual-phoneme mapping
        stdp_apply(&lip_reading_plasticity, current_time, current_time + 200,
                   1.0 /* LTP */);
    }
}

// BIOLOGY: Visual speech anticipates auditory (aids comprehension)
// FUNCTION: Predict upcoming phoneme before hearing it
```

---

## 7. Motor Theory of Speech Perception (Mirror Neurons)

### 7.1 Articulatory Simulation

```c
// Broca's area simulates how to PRODUCE observed phonemes
lip_reading_system_t {
    mirror_neuron_system_t* articulatory_mirror;

    // Observe viseme → Activate motor program for producing it
    viseme_t observed_viseme = classify_viseme(classifier, mouth_roi, features);

    // Mirror neurons map observation → action
    articulatory_action_t motor_program =
        viseme_to_motor_command(observed_viseme);

    // Simulate motor execution (internally, no actual speech)
    mirror_neuron_observe(articulatory_mirror,
                          motor_program,
                          ACTION_SPEECH_OBSERVATION);

    // Motor simulation aids recognition
    // "I see /p/ on lips → I imagine closing MY lips → /p/ recognized"
}

// Map viseme → articulatory motor commands
articulatory_action_t viseme_to_motor_command(viseme_t viseme) {
    articulatory_action_t action;

    switch (viseme) {
        case VISEME_BILABIAL:  // /p/, /b/, /m/
            action.lips_closed = true;
            action.tongue_position = TONGUE_NEUTRAL;
            action.velum_open = false;  // (except /m/ for nasal)
            break;

        case VISEME_LABIODENTAL:  // /f/, /v/
            action.upper_teeth_on_lower_lip = true;
            action.airflow_friction = true;
            break;

        case VISEME_DENTAL:  // /θ/, /ð/
            action.tongue_between_teeth = true;
            action.airflow_friction = true;
            break;

        // ... (other visemes) ...
    }

    return action;
}

// BIOLOGY: Broca's area (BA 44/45) active during BOTH speech production & perception
// THEORY: "Motor theory of speech perception" (Liberman 1967)
// EVIDENCE: TMS to Broca's area disrupts lip reading
```

---

## 8. Learning & Adaptation

### 8.1 STDP for Visual-Phoneme Mapping

```c
// Learn association: Viseme → Phoneme
lip_reading_system_t {
    stdp_config_t* lip_reading_plasticity;

    // Pre: Viseme observed
    // Post: Phoneme recognized (from audio)
    // → Strengthen viseme-phoneme connection

    uint64_t viseme_time = get_viseme_timestamp();
    uint64_t phoneme_time = get_phoneme_timestamp();

    // Hebbian learning: co-occurring viseme + phoneme
    if (abs(phoneme_time - viseme_time) < 500) {  // 500ms window
        stdp_apply(&lip_reading_plasticity,
                   viseme_time, phoneme_time,
                   1.0 /* LTP strength */);
    }
}

// BIOLOGY: Critical period for visual-phoneme learning (infancy)
// DEVELOPMENTAL: Deaf children learn enhanced lip reading
```

### 8.2 Meta-Learning for Speaker Adaptation

```c
// Rapidly adapt to individual speaker's lip patterns
lip_reading_system_t {
    meta_learning_engine_t* speaker_adaptation;

    // Each speaker has unique lip morphology
    // Meta-learn normalization strategy

    // Observe speaker for 10 seconds
    for (int i = 0; i < 300; i++) {  // 30 FPS * 10s
        viseme_t observed = classify_viseme(classifier, mouth_roi, features);
        phoneme_t actual = audio_cortex_get_phoneme();

        // Update speaker-specific model
        meta_learning_update(speaker_adaptation,
                             observed, actual,
                             META_LEARN_SPEAKER_LIPS);
    }

    // After 10 seconds → Optimized for this speaker
}

// BIOLOGY: Rapid perceptual learning for familiar speakers
// FUNCTION: 30% accuracy → 80% accuracy after brief exposure
```

### 8.3 Curiosity-Driven Attention to Mouth

```c
// Novel lip patterns trigger curiosity
lip_reading_system_t {
    curiosity_engine_t* visual_speech_curiosity;

    // Detect novel viseme sequence
    float viseme_novelty = compute_sequence_novelty(viseme_history, 10);

    if (viseme_novelty > 0.7) {
        // Trigger curiosity
        knowledge_gap_t gap = curiosity_detect_gap(visual_speech_curiosity,
                                                    "novel_lip_pattern");

        // Increase attention to mouth
        phasic_tonic_trigger_burst(&acetylcholine_attention,
                                   viseme_novelty,
                                   BURST_NOVEL_SPEECH);

        // Generate question: "What phoneme is this?"
    }
}

// BIOLOGY: Infants attend more to novel mouth movements
// FUNCTION: Drives phoneme category learning
```

---

## 9. Practical Applications

### 9.1 Noisy Environment Speech Enhancement

```c
// Cocktail party: Audio SNR = -5dB → Rely on lip reading
bool enhance_speech_in_noise(lip_reading_system_t* lip_reading,
                               audio_cortex_t* audio_cortex,
                               const float* noisy_audio,
                               char* recognized_speech) {

    // Measure SNR
    float snr = compute_snr(noisy_audio);

    if (snr < 0.0) {  // Negative SNR (noise > signal)
        // Visual speech dominates
        viseme_t visual_phonemes[100];
        int num_phonemes = extract_visual_phonemes(lip_reading, visual_phonemes);

        // Convert visemes → text (using language model)
        visemes_to_text(visual_phonemes, num_phonemes, recognized_speech);

        return true;
    } else {
        // Audio good enough → Normal speech recognition
        return false;  // Use audio-only
    }
}

// APPLICATION: Restaurant conversation, crowded airports, factory floors
```

### 9.2 Silent Speech Recognition (No Audio)

```c
// Recognize speech from lip movements alone (deaf users)
bool recognize_silent_speech(lip_reading_system_t* lip_reading,
                               const uint8_t* video_frames,
                               int num_frames,
                               char* transcription) {

    // Extract viseme sequence
    viseme_t viseme_sequence[1000];
    int num_visemes = 0;

    for (int f = 0; f < num_frames; f++) {
        // Extract mouth ROI
        uint8_t mouth_roi[64 * 64];
        extract_mouth_roi(visual_cortex, video_frames[f], 640, 480,
                          face_detector, mouth_roi);

        // Classify viseme
        visual_speech_features_t features;
        viseme_t v = classify_viseme(viseme_classifier, mouth_roi, &features);

        viseme_sequence[num_visemes++] = v;
    }

    // Decode viseme sequence → text (using HMM/language model)
    viseme_sequence_to_text(viseme_sequence, num_visemes, transcription);

    return true;
}

// APPLICATION: Silent communication (sign language + lip reading)
//              Video captioning (accessibility)
```

### 9.3 Foreign Accent Detection

```c
// Detect speaker accent from lip articulation patterns
accent_t detect_accent(lip_reading_system_t* lip_reading,
                        const uint8_t* video,
                        int num_frames) {

    // Different accents have different viseme dynamics
    // French: Lip rounding for /r/
    // German: Strong lip protrusion for /ü/
    // English: Alveolar /r/ (tongue retroflexion)

    // Analyze viseme statistics
    float lip_rounding_frequency = measure_rounding(video, num_frames);
    float alveolar_frequency = measure_alveolar(video, num_frames);

    if (lip_rounding_frequency > 0.3) {
        return ACCENT_FRENCH;
    } else if (alveolar_frequency > 0.4) {
        return ACCENT_ENGLISH;
    }

    return ACCENT_UNKNOWN;
}
```

---

## 10. Integration with Existing Modules

### 10.1 Visual Cortex Integration

```c
// Visual cortex provides edge detection + face detection
lip_reading_system_t {
    visual_cortex_t* visual_cortex;

    // V1 Gabor filters detect lip edges
    // FFA analog detects face/mouth location
    bool agent_detected = visual_cortex_detect_agent(visual_cortex,
                                                       image, width * height);

    if (agent_detected) {
        // Extract mouth ROI
        extract_mouth_roi(visual_cortex, image, width, height,
                          face_detector, mouth_roi);
    }
}
```

### 10.2 Speech Cortex Integration

```c
// Speech cortex receives audiovisual phonemes
lip_reading_system_t {
    speech_cortex_t* speech_cortex;
    audiovisual_integrator_t* av_integrator;

    // Fuse visual viseme + auditory phoneme
    phoneme_t fused = integrate_audiovisual_phonemes(av_integrator);

    // Feed to speech cortex for word recognition
    phoneme_event_t phoneme_events[100];
    phoneme_events[0].phoneme = fused;
    phoneme_events[0].confidence = av_integrator->visual_weight;

    // Recognize word from fused phonemes
    char word[64];
    float confidence;
    speech_cortex_recognize_word(speech_cortex, &fused, 1,
                                  word, sizeof(word), &confidence);
}
```

### 10.3 Global Workspace Integration

```c
// Global workspace binds visual + auditory speech
brain_t {
    global_workspace_t* multimodal_speech;

    // Broadcast visual speech to workspace
    global_workspace_broadcast(multimodal_speech,
                                MODALITY_VISUAL_SPEECH,
                                viseme_features);

    // Broadcast auditory speech
    global_workspace_broadcast(multimodal_speech,
                                MODALITY_AUDITORY_SPEECH,
                                phoneme_features);

    // Global workspace integrates → Unified speech percept
}
```

---

## 11. Performance Optimizations

### 11.1 Real-Time Processing (30 FPS Video)

```c
// Target: 33ms per frame (30 FPS)
// Budget breakdown:
// - Face detection: 10ms
// - Mouth ROI extraction: 5ms
// - Viseme classification: 15ms
// - Temporal smoothing: 3ms

// Use GPU for CNN inference
viseme_classifier_t {
    bool use_gpu_acceleration;
    cudaStream_t viseme_stream;

    if (use_gpu_acceleration) {
        cuda_conv_forward(mouth_feature_extractor, mouth_roi,
                          viseme_features, viseme_stream);
    }
}

// PERFORMANCE: 100x speedup on GPU (10ms → 0.1ms)
```

### 11.2 Adaptive Frame Rate

```c
// Reduce frame rate when mouth is static
lip_reading_system_t {
    float current_fps;
    float base_fps = 30.0;

    // Measure mouth motion
    float mouth_motion = compute_optical_flow(mouth_roi_current, mouth_roi_prev);

    if (mouth_motion < 0.1) {
        // Mouth static → Downsample to 10 FPS
        current_fps = 10.0;  // 3x reduction
    } else {
        current_fps = base_fps;
    }
}

// PERFORMANCE: 3x power reduction during pauses
```

---

## 12. Testing & Validation

### 12.1 Unit Tests
- Viseme classification accuracy (target: >90% on LRW dataset)
- McGurk effect reproduction (visual /ga/ + audio /ba/ → /da/)
- Temporal synchronization (visual leads audio by 200ms)

### 12.2 Integration Tests
- Audiovisual fusion in noise (SNR = -10dB → 80% accuracy)
- Silent speech recognition (visual-only, 60% word accuracy)
- Speaker adaptation (10s exposure → 30% improvement)

### 12.3 Clinical Validation
- Deaf lip readers: Enhanced STS activation
- Hearing impaired: Improved speech in noise
- Normal hearing: McGurk effect magnitude

---

## 13. Implementation Priority

### Phase 1: Core Lip Reading (Week 1)
1. Mouth detection + ROI extraction
2. Viseme classification (13 viseme categories)
3. Basic visual-phoneme mapping

### Phase 2: Audiovisual Integration (Week 2)
4. McGurk effect implementation
5. Optimal audiovisual fusion (MLE)
6. Temporal synchronization (200ms lead)

### Phase 3: Learning & Adaptation (Week 3)
7. STDP for visual-phoneme learning
8. Meta-learning for speaker adaptation
9. Curiosity-driven lip attention

### Phase 4: Applications (Week 4)
10. Speech enhancement in noise
11. Silent speech recognition
12. Real-time video captioning

---

## Summary

The lip reading system integrates:

1. **Visual Cortex** - Mouth detection, edge detection
2. **Speech Cortex** - Phoneme recognition, lexical access
3. **Mirror Neurons** - Motor simulation (Broca's area)
4. **STDP** - Visual-phoneme association learning
5. **Meta-Learning** - Rapid speaker adaptation
6. **Phasic/Tonic** - Attention to mouth during speech
7. **Global Workspace** - Audiovisual binding
8. **Curiosity** - Novel viseme exploration

**Key Capabilities**:
- McGurk effect (visual /ga/ + audio /ba/ → /da/)
- Speech in noise (SNR = -10dB)
- Silent speech recognition (no audio needed)
- Speaker adaptation (10s exposure)
- Real-time (30 FPS video)

**Next Step**: Would you like me to implement Phase 1 (core lip reading) or integrate this with the existing visual and speech cortex modules?
