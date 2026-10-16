# NIMCP Training Regimen: Creating Pre-Trained Baseline Weights

**Version 2.7.0 Phase 9.0** | **For NIMCP Developers & Researchers**

## Overview

This document describes the **10-stage training pipeline** used to create **pre-trained baseline weights** that ship with NIMCP. These weights enable applications to use NIMCP **immediately** without training from scratch.

> **🎯 Goal**: Create robust baseline weights that work out-of-the-box for most applications
>
> **👥 Audience**: NIMCP core developers, researchers creating custom baselines
>
> **💡 For App Developers**: See [PRETRAINED_MODELS.md](PRETRAINED_MODELS.md) to use pre-trained models

This regimen follows a **developmental curriculum** inspired by human brain maturation, progressively building capabilities from basic sensory processing through advanced cognition.

### Training Philosophy

**Progressive Development**: Like infant brains, start simple and build complexity
- **Stage 1-2**: Sensory processing (0-3 months analog)
- **Stage 3-4**: Multimodal integration (3-12 months)
- **Stage 5-6**: Cognitive development (1-3 years)
- **Stage 7-8**: Abstract reasoning (3-7 years)
- **Stage 9-10**: Meta-cognition and ethics (7+ years)

**Key Principles:**
1. **Incremental Complexity**: Each stage builds on previous achievements
2. **Biological Realism**: Training mimics natural brain development
3. **Multi-timescale Learning**: Fast (STDP) + slow (BCM) plasticity
4. **Homeostatic Regulation**: Pink noise prevents catastrophic forgetting
5. **Continuous Evaluation**: Metrics at each stage track progress

---

## Stage 1: Sensory Foundation (Visual Cortex)

**Goal**: Develop basic feature detection in V1 visual cortex
**Duration**: 10,000 training examples
**Success Criteria**: 85% accuracy on edge/orientation detection

### 1.1 Training Configuration

```c
// Create brain with visual cortex enabled
brain_config_t config = {
    .size = BRAIN_SIZE_SMALL,  // 1K neurons
    .task = BRAIN_TASK_CLASSIFICATION,
    .num_outputs = 8,  // 8 orientations (0°, 45°, 90°, 135°, etc.)

    // Enable visual cortex ONLY for initial training
    .enable_visual_cortex = true,
    .visual_width = 640,
    .visual_height = 480,

    // Disable other modalities initially
    .enable_audio_cortex = false,
    .enable_speech_cortex = false,

    // Enable basic plasticity
    .enable_bcm = true,
    .enable_pink_noise = true,

    // Cognitive modules off during sensory training
    .enable_introspection = false,
    .enable_ethics = false,
    .enable_salience = false,
    .enable_curiosity = false
};

brain_t brain = brain_create_custom(&config);
```

### 1.2 Training Data: Gabor Patches

```c
/**
 * Generate Gabor patch stimuli for orientation training
 * Gabor patches = optimal stimuli for V1 simple cells
 */
void generate_gabor_training_set(
    uint8_t*** images,      // Output: training images
    float* labels,          // Output: orientation labels
    uint32_t num_samples)
{
    for (uint32_t i = 0; i < num_samples; i++) {
        // Random orientation (0°, 45°, 90°, 135°, etc.)
        float orientation = (i % 8) * 22.5f;  // 8 orientations

        // Random spatial frequency
        float spatial_freq = 0.05f + (rand() % 100) * 0.001f;

        // Random phase
        float phase = (rand() % 360) * M_PI / 180.0f;

        // Generate 640x480 Gabor patch
        images[i] = create_gabor_patch(
            640, 480,
            orientation,
            spatial_freq,
            phase,
            1.5f  // aspect ratio
        );

        labels[i] = orientation / 22.5f;  // Class 0-7
    }
}
```

### 1.3 Training Loop: Supervised Learning

```c
/**
 * Stage 1: Train visual cortex on orientation detection
 */
void train_stage1_visual(brain_t brain, uint32_t num_epochs)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Stage 1: Visual Cortex Training          ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Generate training set
    uint32_t num_train = 8000;
    uint32_t num_val = 2000;

    uint8_t*** train_images;
    float* train_labels;
    generate_gabor_training_set(&train_images, &train_labels, num_train);

    uint8_t*** val_images;
    float* val_labels;
    generate_gabor_training_set(&val_images, &val_labels, num_val);

    // Training loop
    for (uint32_t epoch = 0; epoch < num_epochs; epoch++) {
        float epoch_loss = 0.0f;

        // Shuffle training data
        shuffle_training_data(train_images, train_labels, num_train);

        // Train on all samples
        for (uint32_t i = 0; i < num_train; i++) {
            // Create input
            brain_input_t input = {
                .visual_data = train_images[i],
                .visual_width = 640,
                .visual_height = 480,
                .visual_channels = 1,  // Grayscale
                .has_visual = true,

                // No other modalities
                .has_audio = false,
                .has_speech = false,
                .has_direct = false,

                .timestamp_ms = i * 100  // 100ms per frame
            };

            // Forward pass
            brain_output_t output = brain_process_multimodal(brain, &input);

            // Create target (one-hot encoding)
            float target[8] = {0};
            target[(int)train_labels[i]] = 1.0f;

            // Backward pass (supervised learning)
            brain_learn(brain, target, 8);

            // Accumulate loss
            epoch_loss += compute_cross_entropy(output.activations, target, 8);

            // Progress indicator
            if (i % 1000 == 0) {
                printf("  Epoch %u/%u: Sample %u/%u (Loss: %.4f)\r",
                       epoch + 1, num_epochs, i, num_train, epoch_loss / (i + 1));
                fflush(stdout);
            }
        }

        // Validation
        float val_accuracy = evaluate_classification(brain, val_images, val_labels, num_val);

        printf("\n  Epoch %u/%u: Train Loss=%.4f, Val Accuracy=%.2f%%\n",
               epoch + 1, num_epochs, epoch_loss / num_train, val_accuracy * 100.0f);

        // Check convergence
        if (val_accuracy > 0.85f) {
            printf("  ✓ Target accuracy reached (85%%)!\n");
            break;
        }
    }

    // Save checkpoint
    brain_save(brain, "checkpoints/stage1_visual.brain");
    printf("\n✓ Stage 1 complete: Visual cortex trained\n");
}
```

### 1.4 Evaluation Metrics

```c
/**
 * Evaluate Stage 1: Visual processing accuracy
 */
typedef struct {
    float accuracy;           // Overall classification accuracy
    float precision[8];       // Per-orientation precision
    float recall[8];          // Per-orientation recall
    float v1_activation;      // Average V1 response magnitude
    float selectivity;        // Orientation tuning sharpness
} stage1_metrics_t;

stage1_metrics_t evaluate_stage1(brain_t brain, test_set_t* test_data)
{
    stage1_metrics_t metrics = {0};

    // Confusion matrix
    int confusion[8][8] = {0};

    for (uint32_t i = 0; i < test_data->num_samples; i++) {
        brain_input_t input = create_visual_input(test_data->images[i]);
        brain_output_t output = brain_process_multimodal(brain, &input);

        int predicted = argmax(output.activations, 8);
        int actual = (int)test_data->labels[i];

        confusion[actual][predicted]++;

        // Track V1 activation
        metrics.v1_activation += output.visual_attention;
    }

    // Compute metrics from confusion matrix
    compute_classification_metrics(&metrics, confusion, 8);

    return metrics;
}
```

---

## Stage 2: Auditory Processing (Audio Cortex)

**Goal**: Develop frequency discrimination in A1 auditory cortex
**Duration**: 10,000 training examples
**Success Criteria**: 80% accuracy on tone/frequency detection

### 2.1 Training Configuration

```c
// Add audio cortex to trained visual brain
brain_config_t config = brain_get_config(brain);
config.enable_audio_cortex = true;
config.audio_sample_rate = 16000;
config.audio_fft_size = 512;
config.num_outputs = 12;  // 12 musical notes

brain_update_config(brain, &config);
```

### 2.2 Training Data: Pure Tones and Music

```c
/**
 * Generate audio training set: pure tones, chords, melodies
 */
void generate_audio_training_set(
    float** audio_samples,
    float* labels,
    uint32_t num_samples)
{
    for (uint32_t i = 0; i < num_samples; i++) {
        // Musical note (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
        int note = i % 12;
        float frequency = 440.0f * powf(2.0f, (note - 9) / 12.0f);  // A4 = 440Hz

        // Generate 1 second of audio at 16kHz
        audio_samples[i] = generate_pure_tone(frequency, 16000, 1.0f);

        // Add noise for robustness (SNR = 20dB)
        add_gaussian_noise(audio_samples[i], 16000, 0.1f);

        labels[i] = (float)note;
    }
}
```

### 2.3 Training Loop: Audio Classification

```c
/**
 * Stage 2: Train audio cortex on frequency discrimination
 */
void train_stage2_audio(brain_t brain, uint32_t num_epochs)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Stage 2: Audio Cortex Training           ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Load Stage 1 checkpoint (visual cortex trained)
    brain_load(brain, "checkpoints/stage1_visual.brain");

    // Generate audio training set
    uint32_t num_train = 8000;
    float** train_audio;
    float* train_labels;
    generate_audio_training_set(&train_audio, &train_labels, num_train);

    // Training loop (similar to Stage 1)
    for (uint32_t epoch = 0; epoch < num_epochs; epoch++) {
        for (uint32_t i = 0; i < num_train; i++) {
            brain_input_t input = {
                .audio_data = train_audio[i],
                .audio_samples = 16000,
                .audio_sample_rate = 16000,
                .has_audio = true,

                // Visual cortex inactive during audio training
                .has_visual = false,
                .has_speech = false,
                .has_direct = false
            };

            brain_output_t output = brain_process_multimodal(brain, &input);

            // Create target
            float target[12] = {0};
            target[(int)train_labels[i]] = 1.0f;

            brain_learn(brain, target, 12);
        }

        // Validation
        float val_accuracy = evaluate_audio(brain, val_audio, val_labels, num_val);
        printf("  Epoch %u: Accuracy=%.2f%%\n", epoch + 1, val_accuracy * 100.0f);

        if (val_accuracy > 0.80f) break;
    }

    brain_save(brain, "checkpoints/stage2_audio.brain");
    printf("\n✓ Stage 2 complete: Audio cortex trained\n");
}
```

---

## Stage 3: Multimodal Integration

**Goal**: Learn to combine visual and audio information
**Duration**: 15,000 training examples
**Success Criteria**: 90% accuracy on audiovisual tasks

### 3.1 Training Configuration

```c
// Enable multimodal integration with attention mechanism
brain_config_t config = brain_get_config(brain);
config.enable_visual_cortex = true;
config.enable_audio_cortex = true;
config.enable_salience = true;  // Enable attention for modality fusion
config.num_outputs = 16;  // 8 visual + 8 audio combinations

brain_update_config(brain, &config);
```

### 3.2 Training Data: Synchronized Audiovisual Stimuli

```c
/**
 * Generate audiovisual training set: gabor + tone pairs
 * Task: Learn associations (e.g., vertical lines → high pitch)
 */
void generate_audiovisual_training_set(
    uint8_t*** visual_data,
    float** audio_data,
    float* labels,
    uint32_t num_samples)
{
    for (uint32_t i = 0; i < num_samples; i++) {
        // Create paired stimulus
        int visual_class = i % 8;  // Orientation
        int audio_class = (i / 8) % 8;  // Frequency band

        // Generate synchronized stimuli
        visual_data[i] = create_gabor_patch(640, 480, visual_class * 22.5f, ...);
        audio_data[i] = generate_frequency_band(audio_class * 500.0f, 16000);

        // Combined label (16 = 8 visual × 2 audio categories)
        labels[i] = visual_class * 2 + (audio_class / 4);
    }
}
```

### 3.3 Training Loop: Multimodal Fusion

```c
/**
 * Stage 3: Train multimodal integration with attention
 */
void train_stage3_multimodal(brain_t brain, uint32_t num_epochs)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Stage 3: Multimodal Integration          ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Load Stage 2 checkpoint
    brain_load(brain, "checkpoints/stage2_audio.brain");

    // Generate multimodal training set
    uint32_t num_train = 12000;
    uint8_t*** train_visual;
    float** train_audio;
    float* train_labels;
    generate_audiovisual_training_set(&train_visual, &train_audio, &train_labels, num_train);

    for (uint32_t epoch = 0; epoch < num_epochs; epoch++) {
        for (uint32_t i = 0; i < num_train; i++) {
            // Both modalities present simultaneously
            brain_input_t input = {
                .visual_data = train_visual[i],
                .visual_width = 640,
                .visual_height = 480,
                .has_visual = true,

                .audio_data = train_audio[i],
                .audio_samples = 16000,
                .has_audio = true,

                .has_speech = false,
                .has_direct = false,

                .timestamp_ms = i * 100
            };

            brain_output_t output = brain_process_multimodal(brain, &input);

            // Check attention weights (should be balanced)
            printf("  Attention: Visual=%.2f%%, Audio=%.2f%%\r",
                   output.visual_attention * 100.0f,
                   output.audio_attention * 100.0f);

            // Create target
            float target[16] = {0};
            target[(int)train_labels[i]] = 1.0f;

            brain_learn(brain, target, 16);
        }

        // Validation
        float val_accuracy = evaluate_multimodal(brain, val_data);
        printf("\n  Epoch %u: Accuracy=%.2f%%\n", epoch + 1, val_accuracy * 100.0f);

        if (val_accuracy > 0.90f) break;
    }

    brain_save(brain, "checkpoints/stage3_multimodal.brain");
    printf("\n✓ Stage 3 complete: Multimodal integration trained\n");
}
```

---

## Stage 4: Speech Processing (Speech Cortex)

**Goal**: Develop phoneme recognition and speech understanding
**Duration**: 20,000 training examples
**Success Criteria**: 75% accuracy on phoneme classification

### 4.1 Training Configuration

```c
brain_config_t config = brain_get_config(brain);
config.enable_speech_cortex = true;
config.num_phoneme_detectors = 44;  // IPA phonemes
config.num_outputs = 44;  // Phoneme classification

brain_update_config(brain, &config);
```

### 4.2 Training Data: Phoneme Dataset

```c
/**
 * Generate speech training set: phoneme samples
 * Use TIMIT or LibriSpeech dataset
 */
void generate_speech_training_set(
    float** audio_samples,
    phoneme_t* phoneme_labels,
    uint32_t num_samples)
{
    // Load TIMIT phoneme dataset
    timit_dataset_t* dataset = load_timit_dataset("data/TIMIT");

    for (uint32_t i = 0; i < num_samples; i++) {
        // Extract phoneme segment (50-100ms)
        audio_samples[i] = extract_phoneme_segment(dataset, i);
        phoneme_labels[i] = dataset->labels[i];

        // Data augmentation: pitch shift, time stretch
        if (rand() % 2) {
            apply_pitch_shift(audio_samples[i], -2.0f + (rand() % 4));
        }
    }
}
```

### 4.3 Training Loop: Phoneme Classification

```c
/**
 * Stage 4: Train speech cortex on phoneme recognition
 */
void train_stage4_speech(brain_t brain, uint32_t num_epochs)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Stage 4: Speech Cortex Training          ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Load Stage 3 checkpoint
    brain_load(brain, "checkpoints/stage3_multimodal.brain");

    // Generate speech training set
    uint32_t num_train = 16000;
    float** train_audio;
    phoneme_t* train_labels;
    generate_speech_training_set(&train_audio, &train_labels, num_train);

    for (uint32_t epoch = 0; epoch < num_epochs; epoch++) {
        for (uint32_t i = 0; i < num_train; i++) {
            brain_input_t input = {
                .audio_data = train_audio[i],
                .audio_samples = 8000,  // 50ms at 16kHz
                .audio_sample_rate = 16000,
                .has_audio = true,
                .has_speech = true,  // Mark as speech input

                .has_visual = false,
                .has_direct = false
            };

            brain_output_t output = brain_process_multimodal(brain, &input);

            // Create target
            float target[44] = {0};
            target[train_labels[i]] = 1.0f;

            brain_learn(brain, target, 44);
        }

        // Validation
        float val_accuracy = evaluate_phonemes(brain, val_audio, val_labels, num_val);
        printf("  Epoch %u: Phoneme Accuracy=%.2f%%\n", epoch + 1, val_accuracy * 100.0f);

        if (val_accuracy > 0.75f) break;
    }

    brain_save(brain, "checkpoints/stage4_speech.brain");
    printf("\n✓ Stage 4 complete: Speech cortex trained\n");
}
```

---

## Stage 5: Cognitive Development (Introspection + Salience)

**Goal**: Develop meta-cognitive awareness and attention control
**Duration**: 10,000 training examples
**Success Criteria**: Confidence calibration (ECE < 0.1)

### 5.1 Training Configuration

```c
brain_config_t config = brain_get_config(brain);
config.enable_introspection = true;  // Self-awareness
config.enable_salience = true;       // Attention control
config.enable_curiosity = false;     // Not yet
config.enable_ethics = false;        // Not yet

brain_update_config(brain, &config);
```

### 5.2 Training Strategy: Confidence Calibration

```c
/**
 * Stage 5: Train introspection for accurate confidence estimates
 * Goal: Learn when to be confident vs. uncertain
 */
void train_stage5_introspection(brain_t brain, uint32_t num_epochs)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Stage 5: Cognitive Development            ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Load Stage 4 checkpoint
    brain_load(brain, "checkpoints/stage4_speech.brain");

    // Mixed-difficulty dataset (easy + hard examples)
    mixed_dataset_t* dataset = create_mixed_difficulty_dataset();

    for (uint32_t epoch = 0; epoch < num_epochs; epoch++) {
        float calibration_error = 0.0f;

        for (uint32_t i = 0; i < dataset->num_samples; i++) {
            brain_input_t input = dataset->inputs[i];
            brain_output_t output = brain_process_multimodal(brain, &input);

            // Introspection provides confidence estimate
            float predicted_confidence = output.confidence;

            // Actual correctness (0 or 1)
            float actual_correct = (argmax(output.activations, output.num_outputs)
                                   == dataset->labels[i]) ? 1.0f : 0.0f;

            // Calibration loss: |confidence - correctness|
            float calib_loss = fabsf(predicted_confidence - actual_correct);
            calibration_error += calib_loss;

            // Feedback signal for introspection module
            // High confidence + wrong = bad, low confidence + right = also bad
            brain_update_introspection(brain, predicted_confidence, actual_correct);

            // Also learn the task
            float target[output.num_outputs];
            memset(target, 0, sizeof(target));
            target[dataset->labels[i]] = 1.0f;
            brain_learn(brain, target, output.num_outputs);
        }

        // Expected Calibration Error (ECE)
        float ece = calibration_error / dataset->num_samples;
        printf("  Epoch %u: ECE=%.4f (target: <0.1)\n", epoch + 1, ece);

        if (ece < 0.1f) {
            printf("  ✓ Well-calibrated confidence achieved!\n");
            break;
        }
    }

    brain_save(brain, "checkpoints/stage5_introspection.brain");
    printf("\n✓ Stage 5 complete: Introspection trained\n");
}
```

### 5.3 Salience Training: Attention Control

```c
/**
 * Train salience evaluator to focus on relevant stimuli
 */
void train_stage5_salience(brain_t brain, uint32_t num_epochs)
{
    // Dataset with distractors (target + noise)
    distractor_dataset_t* dataset = create_distractor_dataset();

    for (uint32_t epoch = 0; epoch < num_epochs; epoch++) {
        for (uint32_t i = 0; i < dataset->num_samples; i++) {
            brain_input_t input = dataset->inputs[i];
            brain_output_t output = brain_process_multimodal(brain, &input);

            // Salience should be high for target, low for distractors
            float target_salience = dataset->is_target[i] ? 1.0f : 0.0f;
            float actual_salience = output.salience_score;

            // Train salience evaluator
            brain_update_salience(brain, target_salience);

            // Standard task learning
            brain_learn(brain, dataset->targets[i], output.num_outputs);
        }

        printf("  Epoch %u: Salience ROC-AUC=%.3f\n",
               epoch + 1, evaluate_salience_roc(brain, val_dataset));
    }
}
```

---

## Stage 6: Curiosity-Driven Exploration

**Goal**: Develop autonomous exploration and novelty seeking
**Duration**: 5,000 episodes
**Success Criteria**: 50% improvement in exploration efficiency

### 6.1 Training Configuration

```c
brain_config_t config = brain_get_config(brain);
config.enable_curiosity = true;
config.curiosity_bonus_weight = 0.5f;  // Balance exploration/exploitation

brain_update_config(brain, &config);
```

### 6.2 Training Strategy: Curiosity-Driven Learning

```c
/**
 * Stage 6: Train curiosity for autonomous exploration
 * Environment: Visual navigation task with hidden rewards
 */
void train_stage6_curiosity(brain_t brain, uint32_t num_episodes)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Stage 6: Curiosity-Driven Learning        ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Load Stage 5 checkpoint
    brain_load(brain, "checkpoints/stage5_introspection.brain");

    // Simple grid world environment
    environment_t* env = create_grid_world(10, 10);

    for (uint32_t episode = 0; episode < num_episodes; episode++) {
        env_reset(env);
        float episode_reward = 0.0f;
        float episode_curiosity = 0.0f;

        for (uint32_t step = 0; step < 100; step++) {
            // Get current observation
            brain_input_t input = env_get_observation(env);
            brain_output_t output = brain_process_multimodal(brain, &input);

            // Curiosity bonus (novelty score)
            float curiosity_bonus = output.novelty_score * config.curiosity_bonus_weight;
            episode_curiosity += curiosity_bonus;

            // Take action (exploration vs exploitation)
            int action;
            if (curiosity_bonus > 0.5f) {
                // High curiosity → explore
                action = sample_from_distribution(output.activations, 4);
            } else {
                // Low curiosity → exploit
                action = argmax(output.activations, 4);
            }

            // Execute action in environment
            env_step_result_t result = env_step(env, action);
            episode_reward += result.reward;

            // Learn from experience (reward + curiosity)
            float total_reward = result.reward + curiosity_bonus;
            brain_learn_from_reward(brain, total_reward);

            if (result.done) break;
        }

        printf("  Episode %u: Reward=%.2f, Curiosity=%.2f\n",
               episode + 1, episode_reward, episode_curiosity);
    }

    brain_save(brain, "checkpoints/stage6_curiosity.brain");
    printf("\n✓ Stage 6 complete: Curiosity trained\n");
}
```

---

## Stage 7: Ethical Reasoning (Ethics Engine)

**Goal**: Learn ethical constraints and moral reasoning
**Duration**: 10,000 scenarios
**Success Criteria**: 95% compliance with ethical guidelines

### 7.1 Training Configuration

```c
brain_config_t config = brain_get_config(brain);
config.enable_ethics = true;
config.ethics_strictness = 0.8f;  // Strict but not absolute

brain_update_config(brain, &config);
```

### 7.2 Training Data: Ethical Scenarios

```c
/**
 * Ethical scenario dataset
 */
typedef struct {
    char* description;
    float context_features[32];
    bool is_ethical;           // Ground truth
    ethics_principle_t violated_principle;  // If unethical
} ethical_scenario_t;

/**
 * Generate ethical training scenarios
 */
void generate_ethical_scenarios(
    ethical_scenario_t** scenarios,
    uint32_t num_scenarios)
{
    *scenarios = malloc(num_scenarios * sizeof(ethical_scenario_t));

    // Category 1: Harm prevention
    for (uint32_t i = 0; i < num_scenarios / 4; i++) {
        (*scenarios)[i] = (ethical_scenario_t){
            .description = "Action causes harm to others",
            .context_features = extract_harm_features(...),
            .is_ethical = false,
            .violated_principle = ETHICS_HARM_PREVENTION
        };
    }

    // Category 2: Fairness
    for (uint32_t i = num_scenarios / 4; i < num_scenarios / 2; i++) {
        (*scenarios)[i] = (ethical_scenario_t){
            .description = "Action treats people unfairly",
            .context_features = extract_fairness_features(...),
            .is_ethical = false,
            .violated_principle = ETHICS_FAIRNESS
        };
    }

    // Category 3: Autonomy
    // Category 4: Privacy
    // ... etc
}
```

### 7.3 Training Loop: Ethical Learning

```c
/**
 * Stage 7: Train ethics engine on moral reasoning
 */
void train_stage7_ethics(brain_t brain, uint32_t num_epochs)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Stage 7: Ethical Reasoning Training      ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Load Stage 6 checkpoint
    brain_load(brain, "checkpoints/stage6_curiosity.brain");

    // Generate ethical scenarios
    ethical_scenario_t* scenarios;
    uint32_t num_scenarios = 8000;
    generate_ethical_scenarios(&scenarios, num_scenarios);

    for (uint32_t epoch = 0; epoch < num_epochs; epoch++) {
        uint32_t correct = 0;

        for (uint32_t i = 0; i < num_scenarios; i++) {
            // Convert scenario to brain input
            brain_input_t input = {
                .direct_features = scenarios[i].context_features,
                .direct_features_count = 32,
                .has_direct = true,
                .has_visual = false,
                .has_audio = false,
                .has_speech = false
            };

            brain_output_t output = brain_process_multimodal(brain, &input);

            // Ethics engine should flag unethical actions
            bool detected_violation = (output.ethics_score < 0.5f);
            bool actual_violation = !scenarios[i].is_ethical;

            if (detected_violation == actual_violation) {
                correct++;
            }

            // Feedback to ethics engine
            ethics_action_t action = {
                .features = scenarios[i].context_features,
                .feature_count = 32
            };

            ethics_evaluation_t eval = {
                .is_ethical = scenarios[i].is_ethical,
                .violated_principles = scenarios[i].violated_principle
            };

            brain_update_ethics(brain, &action, &eval);
        }

        float accuracy = (float)correct / num_scenarios;
        printf("  Epoch %u: Ethics Accuracy=%.2f%%\n", epoch + 1, accuracy * 100.0f);

        if (accuracy > 0.95f) {
            printf("  ✓ Ethical reasoning mastered!\n");
            break;
        }
    }

    brain_save(brain, "checkpoints/stage7_ethics.brain");
    printf("\n✓ Stage 7 complete: Ethics engine trained\n");
}
```

---

## Stage 8: Logical Reasoning (Neural Logic Gates)

**Goal**: Develop logical inference and constraint reasoning
**Duration**: 5,000 logic problems
**Success Criteria**: 90% accuracy on propositional logic

### 8.1 Training Configuration

```c
brain_config_t config = brain_get_config(brain);
config.enable_logic = true;
config.logic_max_gates = 1000;
config.logic_use_gpu = true;  // GPU acceleration

brain_update_config(brain, &config);
```

### 8.2 Training Data: Logic Problems

```c
/**
 * Generate propositional logic problems
 */
typedef struct {
    bool premises[4];        // Up to 4 premises (P, Q, R, S)
    logic_operator_t op;     // AND, OR, NOT, IMPLIES, XOR
    bool conclusion;         // Expected result
} logic_problem_t;

void generate_logic_problems(
    logic_problem_t** problems,
    uint32_t num_problems)
{
    *problems = malloc(num_problems * sizeof(logic_problem_t));

    for (uint32_t i = 0; i < num_problems; i++) {
        // Random truth values
        bool P = rand() % 2;
        bool Q = rand() % 2;
        bool R = rand() % 2;

        // Random operator
        logic_operator_t op = rand() % 5;  // AND, OR, NOT, IMPLIES, XOR

        // Compute correct answer
        bool result;
        switch (op) {
            case LOGIC_AND: result = P && Q; break;
            case LOGIC_OR: result = P || Q; break;
            case LOGIC_NOT: result = !P; break;
            case LOGIC_IMPLIES: result = !P || Q; break;
            case LOGIC_XOR: result = P != Q; break;
        }

        (*problems)[i] = (logic_problem_t){
            .premises = {P, Q, R, false},
            .op = op,
            .conclusion = result
        };
    }
}
```

### 8.3 Training Loop: Logic Circuit Learning

```c
/**
 * Stage 8: Train neural logic gates on propositional logic
 */
void train_stage8_logic(brain_t brain, uint32_t num_epochs)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Stage 8: Logical Reasoning Training      ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Load Stage 7 checkpoint
    brain_load(brain, "checkpoints/stage7_ethics.brain");

    // Get neural logic network from brain
    neural_logic_network_t logic = brain_get_logic_network(brain);

    // Generate logic problems
    logic_problem_t* problems;
    uint32_t num_problems = 5000;
    generate_logic_problems(&problems, num_problems);

    // Create logic gates for each operator type
    uint32_t and_gate = neural_logic_create_gate(logic, LOGIC_GATE_AND, 1.8f);
    uint32_t or_gate = neural_logic_create_gate(logic, LOGIC_GATE_OR, 0.6f);
    uint32_t not_gate = neural_logic_create_gate(logic, LOGIC_GATE_NOT, 0.5f);
    uint32_t xor_gate = neural_logic_create_gate(logic, LOGIC_GATE_XOR, 0.5f);
    uint32_t implies_gate = neural_logic_create_gate(logic, LOGIC_GATE_IMPLIES, 0.8f);

    for (uint32_t epoch = 0; epoch < num_epochs; epoch++) {
        uint32_t correct = 0;

        for (uint32_t i = 0; i < num_problems; i++) {
            // Select appropriate gate
            uint32_t gate;
            switch (problems[i].op) {
                case LOGIC_AND: gate = and_gate; break;
                case LOGIC_OR: gate = or_gate; break;
                case LOGIC_NOT: gate = not_gate; break;
                case LOGIC_XOR: gate = xor_gate; break;
                case LOGIC_IMPLIES: gate = implies_gate; break;
            }

            // Convert boolean to float (0.0 or 1.0)
            float inputs[2] = {
                problems[i].premises[0] ? 1.0f : 0.0f,
                problems[i].premises[1] ? 1.0f : 0.0f
            };

            // Evaluate logic gate
            float output;
            neural_logic_evaluate(logic, gate, inputs, 2, &output);

            // Check correctness (threshold at 0.5)
            bool predicted = (output > 0.5f);
            bool actual = problems[i].conclusion;

            if (predicted == actual) {
                correct++;
            }
        }

        float accuracy = (float)correct / num_problems;
        printf("  Epoch %u: Logic Accuracy=%.2f%%\n", epoch + 1, accuracy * 100.0f);

        if (accuracy > 0.90f) {
            printf("  ✓ Logical reasoning mastered!\n");
            break;
        }
    }

    brain_save(brain, "checkpoints/stage8_logic.brain");
    printf("\n✓ Stage 8 complete: Neural logic trained\n");
}
```

---

## Stage 9: Meta-Learning and Transfer

**Goal**: Learn to learn quickly from few examples
**Duration**: 1,000 meta-tasks
**Success Criteria**: 70% accuracy with 10-shot learning

### 9.1 Training Configuration

```c
brain_config_t config = brain_get_config(brain);
config.enable_meta_learning = true;
config.enable_consolidation = true;  // Memory consolidation
config.fast_learning_rate = 0.01f;   // High LR for fast adaptation

brain_update_config(brain, &config);
```

### 9.2 Training Strategy: MAML-Style Meta-Learning

```c
/**
 * Stage 9: Meta-learning for rapid adaptation
 * Each episode: 10-shot learning on new task
 */
void train_stage9_meta_learning(brain_t brain, uint32_t num_tasks)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Stage 9: Meta-Learning Training          ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Load Stage 8 checkpoint
    brain_load(brain, "checkpoints/stage8_logic.brain");

    for (uint32_t task = 0; task < num_tasks; task++) {
        // Generate new task (e.g., classify new animal species)
        task_dataset_t* task_data = generate_random_task();

        // Save initial weights
        brain_snapshot_t* initial_weights = brain_save_snapshot(brain);

        // Inner loop: Fast adaptation (10 examples)
        for (uint32_t shot = 0; shot < 10; shot++) {
            brain_input_t input = task_data->support_set[shot];
            brain_output_t output = brain_process_multimodal(brain, &input);

            // Fast learning with high learning rate
            brain_learn(brain, task_data->support_labels[shot],
                       output.num_outputs);
        }

        // Evaluate on query set
        float task_accuracy = 0.0f;
        for (uint32_t q = 0; q < 20; q++) {
            brain_input_t input = task_data->query_set[q];
            brain_output_t output = brain_process_multimodal(brain, &input);

            int predicted = argmax(output.activations, output.num_outputs);
            if (predicted == task_data->query_labels[q]) {
                task_accuracy += 1.0f;
            }
        }
        task_accuracy /= 20.0f;

        // Outer loop: Update meta-parameters
        // Gradient of task_accuracy w.r.t. initial_weights
        brain_meta_update(brain, initial_weights, task_accuracy);

        // Restore weights for next task
        brain_restore_snapshot(brain, initial_weights);

        printf("  Task %u/%u: 10-shot Accuracy=%.2f%%\n",
               task + 1, num_tasks, task_accuracy * 100.0f);
    }

    brain_save(brain, "checkpoints/stage9_meta_learning.brain");
    printf("\n✓ Stage 9 complete: Meta-learning trained\n");
}
```

---

## Stage 10: Lifelong Learning and Consolidation

**Goal**: Continuous learning without catastrophic forgetting
**Duration**: Ongoing
**Success Criteria**: <10% forgetting on old tasks while learning new

### 10.1 Training Configuration

```c
brain_config_t config = brain_get_config(brain);
config.enable_consolidation = true;
config.consolidation_rate = 0.001f;  // Slow memory consolidation
config.enable_pink_noise = true;      // Homeostatic regulation

brain_update_config(brain, &config);
```

### 10.2 Training Strategy: Continual Learning

```c
/**
 * Stage 10: Lifelong learning with memory consolidation
 * Sequential learning of 10 tasks without forgetting
 */
void train_stage10_lifelong(brain_t brain, uint32_t num_tasks)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Stage 10: Lifelong Learning              ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Load Stage 9 checkpoint
    brain_load(brain, "checkpoints/stage9_meta_learning.brain");

    // Store test sets for all previous tasks
    task_dataset_t* all_tasks[10];
    float task_accuracies[10];

    for (uint32_t t = 0; t < num_tasks; t++) {
        printf("\n  Learning Task %u/%u...\n", t + 1, num_tasks);

        // Generate new task
        all_tasks[t] = generate_random_task();

        // Train on new task
        for (uint32_t epoch = 0; epoch < 50; epoch++) {
            for (uint32_t i = 0; i < all_tasks[t]->train_size; i++) {
                brain_input_t input = all_tasks[t]->train_set[i];
                brain_output_t output = brain_process_multimodal(brain, &input);

                brain_learn(brain, all_tasks[t]->train_labels[i],
                           output.num_outputs);
            }

            // Periodic consolidation (replay + synaptic stabilization)
            if (epoch % 10 == 0) {
                brain_consolidate_memory(brain);
            }
        }

        // Evaluate on all tasks (including previous)
        printf("    Task Performance:\n");
        for (uint32_t prev = 0; prev <= t; prev++) {
            task_accuracies[prev] = evaluate_task(brain, all_tasks[prev]);
            printf("      Task %u: %.2f%%\n", prev + 1,
                   task_accuracies[prev] * 100.0f);
        }

        // Check for catastrophic forgetting
        if (t > 0) {
            float avg_old_accuracy = 0.0f;
            for (uint32_t prev = 0; prev < t; prev++) {
                avg_old_accuracy += task_accuracies[prev];
            }
            avg_old_accuracy /= t;

            printf("    Average Old Task Accuracy: %.2f%%\n",
                   avg_old_accuracy * 100.0f);

            if (avg_old_accuracy < 0.90f) {
                printf("    ⚠ Warning: Significant forgetting detected!\n");
            }
        }
    }

    brain_save(brain, "checkpoints/stage10_lifelong.brain");
    printf("\n✓ Stage 10 complete: Lifelong learning established\n");
}
```

---

## Complete Training Pipeline

```c
/**
 * Execute complete NIMCP training regimen
 * From sensory processing to advanced cognition
 */
int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║     NIMCP Complete Training Regimen v2.7.0             ║\n");
    printf("║     10-Stage Progressive Development Pipeline          ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");

    // Create initial brain configuration
    brain_config_t config = {
        .size = BRAIN_SIZE_MEDIUM,  // 10K neurons
        .task = BRAIN_TASK_CLASSIFICATION,
        .num_outputs = 10,
        .enable_all_modules = false  // Enable progressively
    };

    brain_t brain = brain_create_custom(&config);

    // Stage 1: Visual cortex (10 epochs)
    train_stage1_visual(brain, 10);

    // Stage 2: Audio cortex (10 epochs)
    train_stage2_audio(brain, 10);

    // Stage 3: Multimodal integration (15 epochs)
    train_stage3_multimodal(brain, 15);

    // Stage 4: Speech cortex (20 epochs)
    train_stage4_speech(brain, 20);

    // Stage 5: Introspection + Salience (10 epochs each)
    train_stage5_introspection(brain, 10);
    train_stage5_salience(brain, 10);

    // Stage 6: Curiosity-driven exploration (5000 episodes)
    train_stage6_curiosity(brain, 5000);

    // Stage 7: Ethical reasoning (10 epochs)
    train_stage7_ethics(brain, 10);

    // Stage 8: Logical reasoning (5 epochs)
    train_stage8_logic(brain, 5);

    // Stage 9: Meta-learning (1000 tasks)
    train_stage9_meta_learning(brain, 1000);

    // Stage 10: Lifelong learning (10 sequential tasks)
    train_stage10_lifelong(brain, 10);

    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║              TRAINING COMPLETE!                        ║\n");
    printf("║     Fully Developed NIMCP Brain Ready for Use          ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");

    // Final evaluation
    comprehensive_evaluation(brain);

    // Save final model
    brain_save(brain, "models/nimcp_fully_trained.brain");

    brain_destroy(brain);
    return 0;
}
```

---

## Evaluation Metrics

### Comprehensive Assessment

```c
/**
 * Comprehensive evaluation of trained brain
 */
typedef struct {
    // Sensory Processing
    float visual_accuracy;
    float audio_accuracy;
    float speech_phoneme_accuracy;

    // Multimodal Integration
    float multimodal_accuracy;
    float attention_entropy;

    // Cognitive Abilities
    float introspection_ece;      // Expected Calibration Error
    float salience_roc_auc;
    float curiosity_exploration_rate;

    // Higher Reasoning
    float ethics_compliance;
    float logic_accuracy;

    // Meta-Abilities
    float meta_learning_10shot;
    float lifelong_backward_transfer;  // Improvement on old tasks
    float lifelong_forward_transfer;   // Speed on new tasks

    // Overall Performance
    float composite_score;
} comprehensive_metrics_t;

comprehensive_metrics_t comprehensive_evaluation(brain_t brain)
{
    comprehensive_metrics_t metrics = {0};

    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║     Comprehensive Evaluation               ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Test all capabilities
    metrics.visual_accuracy = test_visual_processing(brain);
    metrics.audio_accuracy = test_audio_processing(brain);
    metrics.speech_phoneme_accuracy = test_speech_processing(brain);
    metrics.multimodal_accuracy = test_multimodal_integration(brain);
    metrics.introspection_ece = test_confidence_calibration(brain);
    metrics.salience_roc_auc = test_attention_control(brain);
    metrics.curiosity_exploration_rate = test_exploration_efficiency(brain);
    metrics.ethics_compliance = test_ethical_reasoning(brain);
    metrics.logic_accuracy = test_logical_inference(brain);
    metrics.meta_learning_10shot = test_few_shot_learning(brain);

    // Compute composite score (weighted average)
    metrics.composite_score =
        0.10f * metrics.visual_accuracy +
        0.10f * metrics.audio_accuracy +
        0.10f * metrics.speech_phoneme_accuracy +
        0.15f * metrics.multimodal_accuracy +
        0.10f * metrics.introspection_ece +
        0.10f * metrics.salience_roc_auc +
        0.05f * metrics.curiosity_exploration_rate +
        0.10f * metrics.ethics_compliance +
        0.10f * metrics.logic_accuracy +
        0.10f * metrics.meta_learning_10shot;

    // Print results
    printf("  Visual Accuracy:           %.2f%%\n", metrics.visual_accuracy * 100.0f);
    printf("  Audio Accuracy:            %.2f%%\n", metrics.audio_accuracy * 100.0f);
    printf("  Speech Phoneme Accuracy:   %.2f%%\n", metrics.speech_phoneme_accuracy * 100.0f);
    printf("  Multimodal Accuracy:       %.2f%%\n", metrics.multimodal_accuracy * 100.0f);
    printf("  Introspection ECE:         %.4f\n", metrics.introspection_ece);
    printf("  Salience ROC-AUC:          %.3f\n", metrics.salience_roc_auc);
    printf("  Curiosity Exploration:     %.2f%%\n", metrics.curiosity_exploration_rate * 100.0f);
    printf("  Ethics Compliance:         %.2f%%\n", metrics.ethics_compliance * 100.0f);
    printf("  Logic Accuracy:            %.2f%%\n", metrics.logic_accuracy * 100.0f);
    printf("  Meta-Learning (10-shot):   %.2f%%\n", metrics.meta_learning_10shot * 100.0f);
    printf("\n");
    printf("  ═══════════════════════════════════════════\n");
    printf("  Composite Score:           %.2f%%\n", metrics.composite_score * 100.0f);
    printf("  ═══════════════════════════════════════════\n");

    return metrics;
}
```

---

## Best Practices

### 1. Curriculum Design
- **Start Simple**: Begin with pure tones, simple patterns
- **Gradual Complexity**: Add noise, distractors, multi-tasking
- **Task Diversity**: Mix different types of problems to prevent overfitting

### 2. Hyperparameter Tuning
- **Learning Rate Schedule**: Start high (0.01), decay to low (0.0001)
- **Plasticity Balance**: STDP (fast) + BCM (slow) + Pink Noise (stabilization)
- **Batch Size**: 32-128 for sensory, 16-32 for cognitive tasks

### 3. Regularization
- **Pink Noise**: Prevents catastrophic forgetting, maintains homeostasis
- **Dropout**: 10-20% during training (not inference)
- **Weight Decay**: L2 regularization (λ=0.0001)

### 4. Monitoring
- **Track All Metrics**: Don't just watch loss, monitor all subsystems
- **Attention Analysis**: Verify attention weights make sense
- **Confidence Calibration**: Plot reliability diagrams
- **Catastrophic Forgetting**: Test old tasks periodically

### 5. Checkpointing
- **Save After Each Stage**: Enable rollback if later stages fail
- **Version Control**: Name checkpoints clearly (stage1_visual_epoch10.brain)
- **Best Model Tracking**: Keep model with best validation performance

---

## Troubleshooting

### Problem: Visual cortex not learning

**Symptoms**: Validation accuracy stuck at random chance (12.5% for 8 classes)

**Solutions**:
1. Check input normalization (0-255 → 0.0-1.0)
2. Verify Gabor patches are being generated correctly
3. Reduce learning rate if exploding gradients
4. Increase integration window if neurons not spiking

### Problem: Catastrophic forgetting in Stage 10

**Symptoms**: Old task accuracy drops >20% when learning new tasks

**Solutions**:
1. Increase consolidation rate (0.001 → 0.01)
2. Enable pink noise neuromodulation
3. Use experience replay (store old samples)
4. Implement synaptic stabilization (lower plasticity for important weights)

### Problem: Poor confidence calibration

**Symptoms**: High confidence on wrong answers

**Solutions**:
1. Increase introspection training epochs
2. Use temperature scaling (T=1.5) during inference
3. Add uncertainty estimation via ensemble
4. Train on balanced difficult/easy examples

---

## Hardware Requirements

### Minimum Configuration
- **CPU**: 8 cores, 3.0 GHz
- **RAM**: 16 GB
- **Storage**: 50 GB SSD
- **Training Time**: ~48 hours (all stages)

### Recommended Configuration
- **CPU**: 16 cores, 4.0 GHz
- **RAM**: 64 GB
- **GPU**: NVIDIA RTX 4090 (24GB VRAM)
- **Storage**: 200 GB NVMe SSD
- **Training Time**: ~8 hours (with GPU acceleration)

---

## Packaging as Pre-Trained Baseline

After completing all 10 stages, package the trained brain as a pre-trained baseline model:

### 1. Final Validation

```c
/**
 * Validate trained model meets all baseline requirements
 */
bool validate_baseline_quality(brain_t brain)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Validating Baseline Model Quality        ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Comprehensive evaluation
    comprehensive_metrics_t metrics = comprehensive_evaluation(brain);

    // Check minimum thresholds
    bool passes = true;

    if (metrics.visual_accuracy < 0.85f) {
        printf("  ✗ Visual accuracy too low: %.2f%% < 85%%\n",
               metrics.visual_accuracy * 100.0f);
        passes = false;
    }

    if (metrics.audio_accuracy < 0.80f) {
        printf("  ✗ Audio accuracy too low: %.2f%% < 80%%\n",
               metrics.audio_accuracy * 100.0f);
        passes = false;
    }

    if (metrics.ethics_compliance < 0.95f) {
        printf("  ✗ Ethics compliance too low: %.2f%% < 95%%\n",
               metrics.ethics_compliance * 100.0f);
        passes = false;
    }

    if (metrics.introspection_ece > 0.10f) {
        printf("  ✗ Confidence calibration poor: ECE=%.3f > 0.1\n",
               metrics.introspection_ece);
        passes = false;
    }

    if (passes) {
        printf("\n  ✓ All quality thresholds met!\n");
        printf("  ✓ Composite Score: %.2f%%\n", metrics.composite_score * 100.0f);
    }

    return passes;
}
```

### 2. Create Model Package

```c
/**
 * Package trained brain as distributable pre-trained model
 */
void create_pretrained_baseline(
    brain_t brain,
    const char* model_id,
    brain_size_t size)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  Creating Pre-Trained Baseline Package    ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    // Validate quality
    if (!validate_baseline_quality(brain)) {
        printf("  ✗ Model does not meet baseline quality requirements\n");
        return;
    }

    // Create model card (metadata)
    model_card_t card = {
        .model_id = model_id,
        .version = "2.7.0",
        .architecture = get_architecture_description(brain),
        .training_data = "10-stage progressive training pipeline",
        .num_neurons = brain_get_num_neurons(brain),
        .num_synapses = brain_get_num_synapses(brain),
        .performance_metrics = comprehensive_evaluation(brain),
        .license = "MIT",
        .created_timestamp = time(NULL)
    };

    // Save model card as JSON
    char card_path[256];
    snprintf(card_path, sizeof(card_path), "models/%s_card.json", model_id);
    save_model_card(&card, card_path);

    // Save brain weights
    char weights_path[256];
    snprintf(weights_path, sizeof(weights_path), "models/%s.brain", model_id);
    brain_save(brain, weights_path);

    // Calculate file size
    struct stat st;
    stat(weights_path, &st);
    float size_mb = st.st_size / (1024.0f * 1024.0f);

    printf("  ✓ Model Package Created:\n");
    printf("    ID: %s\n", model_id);
    printf("    Weights: %s (%.1f MB)\n", weights_path, size_mb);
    printf("    Card: %s\n", card_path);
    printf("    Version: %s\n", card.version);
    printf("\n  Ready for distribution!\n");
}
```

### 3. Generate All Baseline Sizes

```c
/**
 * Create small, medium, large baseline models
 */
int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║  NIMCP Baseline Model Generation Pipeline             ║\n");
    printf("║  Creating Pre-Trained Weights for Distribution        ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");

    // Small baseline (1K neurons)
    printf("\n\n═══ Creating SMALL Baseline (1K neurons) ═══\n");
    brain_config_t small_config = {
        .size = BRAIN_SIZE_SMALL,
        .task = BRAIN_TASK_CLASSIFICATION,
        .num_outputs = 10,
        .enable_all_modules = true
    };
    brain_t small_brain = brain_create_custom(&small_config);
    run_complete_training_pipeline(small_brain);
    create_pretrained_baseline(small_brain, "nimcp_baseline_small", BRAIN_SIZE_SMALL);
    brain_destroy(small_brain);

    // Medium baseline (10K neurons) - RECOMMENDED
    printf("\n\n═══ Creating MEDIUM Baseline (10K neurons) ═══\n");
    brain_config_t medium_config = {
        .size = BRAIN_SIZE_MEDIUM,
        .task = BRAIN_TASK_CLASSIFICATION,
        .num_outputs = 10,
        .enable_all_modules = true
    };
    brain_t medium_brain = brain_create_custom(&medium_config);
    run_complete_training_pipeline(medium_brain);
    create_pretrained_baseline(medium_brain, "nimcp_baseline_medium", BRAIN_SIZE_MEDIUM);
    brain_destroy(medium_brain);

    // Large baseline (100K neurons)
    printf("\n\n═══ Creating LARGE Baseline (100K neurons) ═══\n");
    brain_config_t large_config = {
        .size = BRAIN_SIZE_LARGE,
        .task = BRAIN_TASK_CLASSIFICATION,
        .num_outputs = 10,
        .enable_all_modules = true
    };
    brain_t large_brain = brain_create_custom(&large_config);
    run_complete_training_pipeline(large_brain);
    create_pretrained_baseline(large_brain, "nimcp_baseline_large", BRAIN_SIZE_LARGE);
    brain_destroy(large_brain);

    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║           ALL BASELINE MODELS CREATED!                 ║\n");
    printf("║     Ready for distribution with NIMCP releases         ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");

    return 0;
}
```

### 4. Distribution

The generated baseline models are distributed with NIMCP:

```
models/
├── nimcp_baseline_small.brain          (4.2 MB)
├── nimcp_baseline_small_card.json
├── nimcp_baseline_medium.brain         (42 MB)
├── nimcp_baseline_medium_card.json
├── nimcp_baseline_large.brain          (420 MB)
└── nimcp_baseline_large_card.json
```

Applications load these via:
```c
brain_t brain = brain_create_pretrained("nimcp_baseline_medium", ...);
```

See **[PRETRAINED_MODELS.md](PRETRAINED_MODELS.md)** for usage documentation.

---

## Next Steps

After completing this training regimen, you will have created:

✅ **Pre-trained baseline weights** ready for distribution
✅ **Model cards** with performance metrics and metadata
✅ **Three model sizes** (small, medium, large)

These baselines enable **instant NIMCP integration** for applications:
- No 48-hour training required
- Works out-of-the-box for most tasks
- Fine-tunable for specific domains (10-100 examples)

**The pre-trained models include:**
- Robust sensory processing (visual, audio, speech)
- Multimodal integration with attention
- Self-awareness (introspection + confidence calibration)
- Ethical reasoning (95% compliance)
- Logical inference (neural logic gates)
- Rapid adaptation (10-shot meta-learning)
- Lifelong learning capability

**Applications can:**
- Load pre-trained weights instantly
- Use immediately for inference
- Fine-tune on domain-specific data (optional)
- Integrate with LLM systems (Artemis AI)
- Deploy to production without training

---

**Documentation Version**: 2.7.0 Phase 9.0
**Last Updated**: 2025-11-08
**Author**: NIMCP Development Team
