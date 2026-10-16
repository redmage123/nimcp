# Phase 10.3: Emotional Tagging System

**Duration**: 3 weeks
**Status**: In Progress
**Started**: 2025-11-09

## Overview

**WHAT**: Tag cognitive representations with emotional states (valence + arousal)
**WHY**: Emotions enhance memory, guide attention, and influence decision-making
**HOW**: Integrate 2D emotional space (Russell's circumplex model) with working memory

## Theoretical Foundation

### Russell's Circumplex Model of Affect (1980)
```
        High Arousal
             |
    Tense    |    Excited
    Anxious  |    Alert
             |
Negative ----+---- Positive  (Valence)
             |
    Sad      |    Calm
    Bored    |    Relaxed
             |
        Low Arousal
```

**Two Dimensions**:
1. **Valence**: Negative (-1.0) ↔ Positive (+1.0)
2. **Arousal**: Low (0.0) ↔ High (1.0)

**Examples**:
- Fear: Valence=-0.8, Arousal=0.9 (high arousal negative)
- Joy: Valence=+0.9, Arousal=0.8 (high arousal positive)
- Sadness: Valence=-0.7, Arousal=0.2 (low arousal negative)
- Calm: Valence=+0.3, Arousal=0.1 (low arousal positive)

### Biological Basis

**Amygdala**: Emotional tagging and memory enhancement
- Tags salient events with emotional significance
- Enhances memory consolidation for emotional events
- Modulates attention based on emotional relevance

**Working Memory + Emotion**:
- Emotional items receive priority in working memory
- High arousal increases salience and retention
- Emotional context aids retrieval and reasoning

## Design Specifications

### 1. Emotional Tag Structure

```c
/**
 * @brief Emotional tag for cognitive representation
 *
 * Based on Russell's circumplex model (valence + arousal)
 */
typedef struct {
    float valence;        // Emotional valence: -1.0 (negative) to +1.0 (positive)
    float arousal;        // Emotional arousal: 0.0 (calm) to 1.0 (excited)
    uint64_t timestamp;   // When emotion was tagged (ms)
    float intensity;      // Overall emotional intensity [0.0, 1.0]

    // Derived emotional state (computed from valence+arousal)
    emotion_category_t category;  // e.g., JOY, FEAR, SADNESS, CALM
} emotional_tag_t;
```

### 2. Emotion Categories (Derived)

```c
typedef enum {
    EMOTION_NEUTRAL,      // valence ≈ 0, arousal < 0.3
    EMOTION_JOY,          // valence > 0.5, arousal > 0.5
    EMOTION_EXCITEMENT,   // valence > 0.3, arousal > 0.7
    EMOTION_CALM,         // valence > 0.2, arousal < 0.3
    EMOTION_FEAR,         // valence < -0.3, arousal > 0.6
    EMOTION_ANGER,        // valence < -0.4, arousal > 0.6
    EMOTION_SADNESS,      // valence < -0.3, arousal < 0.4
    EMOTION_ANXIETY,      // valence < -0.2, arousal > 0.5
    EMOTION_BOREDOM       // valence < 0, arousal < 0.2
} emotion_category_t;
```

### 3. Integration with Working Memory

**Extend working memory item**:
```c
typedef struct {
    float* data;              // Feature vector
    uint32_t size;            // Vector size
    float salience;           // Base salience
    uint64_t timestamp;       // Creation time
    bool attention_refreshed; // Rehearsal flag

    // Phase 10.3: Emotional tagging
    emotional_tag_t emotion;  // Emotional state
    float emotional_salience; // Salience boost from emotion
} working_memory_item_t;
```

### 4. Emotional Salience Modulation

**Formula**:
```
emotional_salience = base_salience × (1.0 + arousal × 0.5 + |valence| × 0.3)
```

**Rationale**:
- High arousal emotions boost salience (attention-grabbing)
- Strong valence (positive OR negative) increases importance
- Neutral, low-arousal items have minimal boost

**Examples**:
- Fear (valence=-0.8, arousal=0.9): boost = 1 + 0.45 + 0.24 = 1.69× (69% increase)
- Joy (valence=+0.9, arousal=0.8): boost = 1 + 0.40 + 0.27 = 1.67× (67% increase)
- Neutral (valence=0, arousal=0.1): boost = 1 + 0.05 + 0 = 1.05× (5% increase)

### 5. Emotion Detection

**Sources**:
1. **Introspection uncertainty** → Arousal
   - High uncertainty = higher arousal
   - `arousal = min(1.0, uncertainty × 1.2)`

2. **Ethical violations** → Negative valence
   - Ethical rejection = strong negative valence
   - `valence = -0.8 if ethical_approved == false`

3. **Novelty/curiosity** → Positive valence + arousal
   - Novel patterns = curiosity (positive, aroused)
   - `valence = +0.5, arousal = novelty_score`

4. **Confidence** → Valence
   - High confidence = positive valence
   - Low confidence = negative valence
   - `valence = (confidence - 0.5) × 2.0`

## Implementation Plan

### Week 1: Core Emotional Tagging System

**Files to create**:
- `src/include/cognitive/nimcp_emotional_tagging.h` - Public API
- `src/cognitive/emotional_tagging/nimcp_emotional_tagging.c` - Implementation

**Functions**:
```c
// Create/destroy emotional tag
emotional_tag_t emotional_tag_create(float valence, float arousal);
emotion_category_t emotional_tag_classify(const emotional_tag_t* tag);
float emotional_tag_intensity(const emotional_tag_t* tag);

// Compute emotional salience boost
float emotional_compute_salience_boost(const emotional_tag_t* tag);

// Detect emotions from cognitive state
emotional_tag_t emotional_tag_from_cognitive_state(
    float confidence,
    float uncertainty,
    float novelty,
    bool ethical_approved
);
```

**Tests**:
- Valence-arousal space classification
- Emotion category detection
- Salience boost computation
- Edge cases (extreme values)

### Week 2: Working Memory Integration

**Modifications**:
- Extend `working_memory_item_t` with emotional_tag_t
- Update `working_memory_add()` to accept emotional tag
- Update salience computation to include emotional boost
- Update eviction to consider emotional salience

**Files to modify**:
- `src/include/cognitive/nimcp_working_memory.h` - Add emotion parameter
- `src/cognitive/working_memory/nimcp_working_memory.c` - Integration

**New functions**:
```c
bool working_memory_add_with_emotion(
    working_memory_t* wm,
    const float* item,
    uint32_t item_size,
    float base_salience,
    const emotional_tag_t* emotion
);

bool working_memory_get_emotion(
    const working_memory_t* wm,
    uint32_t index,
    emotional_tag_t* emotion_out
);
```

### Week 3: Brain Integration & Testing

**Brain Integration**:
- Extract emotional signals from cognitive assessment
- Tag working memory items with emotions during `brain_process_multimodal()`
- Update public API

**Files to modify**:
- `src/core/brain/nimcp_brain.c` - Emotion detection and tagging
- `src/include/nimcp.h` - Public API extensions

**New public API**:
```c
nimcp_status_t nimcp_brain_working_memory_add_with_emotion(
    nimcp_brain_t brain,
    const float* data,
    uint32_t size,
    float base_salience,
    float valence,
    float arousal
);

nimcp_status_t nimcp_brain_working_memory_get_emotion(
    nimcp_brain_t brain,
    uint32_t index,
    float* valence_out,
    float* arousal_out,
    emotion_category_t* category_out
);
```

**Comprehensive Tests**:
- Unit tests for emotional tagging
- Integration tests with working memory
- End-to-end test with brain processing
- Emotion-based salience ranking

**Demo**:
- `examples/emotional_tagging_demo.c`
- Show emotion detection from cognitive state
- Demonstrate emotional salience boosting
- Visualize emotion categories

## Success Criteria

- [x] Emotional tag structure defined
- [ ] Emotion category classification working
- [ ] Salience boost formula validated
- [ ] Working memory accepts emotional tags
- [ ] Emotions detected from cognitive state
- [ ] Integration with brain processing
- [ ] Public API implemented
- [ ] 15+ unit tests passing
- [ ] Integration demo working

## Validation Tests

### Test 1: Emotion Classification
```c
emotional_tag_t fear = {.valence = -0.8f, .arousal = 0.9f};
EXPECT_EQ(emotional_tag_classify(&fear), EMOTION_FEAR);

emotional_tag_t joy = {.valence = 0.9f, .arousal = 0.8f};
EXPECT_EQ(emotional_tag_classify(&joy), EMOTION_JOY);
```

### Test 2: Salience Boost
```c
// High arousal should boost salience
emotional_tag_t excited = {.valence = 0.5f, .arousal = 0.9f};
float boost = emotional_compute_salience_boost(&excited);
EXPECT_GT(boost, 1.4f);  // Expect >40% boost
```

### Test 3: Working Memory Priority
```c
// Emotional items should have higher effective salience
working_memory_add_with_emotion(wm, data1, size, 0.5f, &fear_tag);
working_memory_add_with_emotion(wm, data2, size, 0.5f, &neutral_tag);

// Fear item should rank higher despite same base salience
uint32_t fear_idx = working_memory_find_highest_salience(wm, NULL);
EXPECT_EQ(fear_idx, 0);  // Fear item should be first
```

## Biological Plausibility

**Amygdala-Hippocampus Interaction**:
- Amygdala tags emotional events → Emotional tagging system
- Hippocampus encodes with emotional context → Working memory with tags
- High arousal enhances consolidation → Salience boost

**Neurochemical Basis**:
- Norepinephrine (arousal) → Arousal dimension
- Dopamine (reward/valence) → Valence dimension
- Emotional tagging strengthens synaptic traces → Higher salience = longer retention

## Next Phases (Dependencies)

**Phase 10.4: Executive Functions** (4 weeks)
- Will use emotional tags for goal prioritization
- Emotional regulation (down-regulation of negative emotions)

**Phase 10.5: Sleep-Wake Cycle** (6 weeks)
- Emotional memory consolidation during "sleep"
- REM-like processing of emotional events

**Phase 10.9: Mental Health Monitoring** (8 weeks)
- Track emotional patterns over time
- Detect dysregulation (persistent negative valence)

## References

1. Russell, J. A. (1980). "A circumplex model of affect". Journal of Personality and Social Psychology.
2. LaBar, K. S., & Cabeza, R. (2006). "Cognitive neuroscience of emotional memory". Nature Reviews Neuroscience.
3. Phelps, E. A. (2004). "Human emotion and memory: interactions of the amygdala and hippocampal complex". Current Opinion in Neurobiology.
4. Cahill, L., & McGaugh, J. L. (1998). "Mechanisms of emotional arousal and lasting declarative memory". Trends in Neurosciences.
