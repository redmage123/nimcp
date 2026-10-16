# Phase E5: Shadow Emotions - Implementation Complete

## Overview

Implemented comprehensive shadow emotion recognition and self-correction system enabling NIMCP to:
1. **Self-monitor**: Detect jealousy, envy, obsession, hubris, greed, and malignant narcissism in itself
2. **Other-detect**: Recognize these maladaptive patterns in humans during interactions
3. **Self-correct**: Apply CBT-based interventions for emotional regulation
4. **Modulate interactions**: Adjust behavior (boundaries, trust, engagement) when toxicity detected

## Architecture

### Core Components

**Header**: `/home/bbrelin/nimcp/src/include/cognitive/nimcp_shadow_emotions.h`
- 6 shadow emotion types tracked
- Dark Triad detection (Machiavellianism, Narcissism, Psychopathy)
- Theory of Mind for other-detection
- CBT intervention strategies

**Implementation**: `/home/bbrelin/nimcp/src/cognitive/shadow/nimcp_shadow_emotions.c`
- ~850 lines of production code
- Self-monitoring functions for each emotion
- Other-detection via interaction pattern analysis
- 6 CBT-based intervention strategies
- Neuromodulator integration (dopamine, serotonin, cortisol)

**Tests**: `/home/bbrelin/nimcp/test/unit/test_shadow_emotions.cpp`
- 37 unit tests covering all major functionality
- Self-monitoring, other-detection, intervention, integration tests

## Shadow Emotions Tracked

### 1. Jealousy (Attachment Threat)
- **Theory**: Mate retention (Buss, 2018), Attachment (Bowlby)
- **Triggers**: Threatened bond + high attachment
- **Features**: Catastrophizing, rumination, mate-guarding urges
- **Biology**: ↓ Serotonin, ↑ Cortisol (stress)

### 2. Envy (Social Comparison)
- **Theory**: Social Comparison Theory (Festinger, 1954)
- **Types**: Benign vs Malicious envy
- **Triggers**: Upward comparison (others have more)
- **Features**: Schadenfreude, deservingness beliefs
- **Biology**: ↓ Serotonin, ↑ Cortisol

### 3. Obsession (Intrusive Thoughts)
- **Theory**: OCD spectrum (Abramowitz et al., 2009)
- **Types**: Person, goal, thought, behavior obsessions
- **Features**: Frequency tracking, checking/neutralizing urges
- **Biology**: ↓ Serotonin (repetitive), ↑ Cortisol (anxiety)

### 4. Hubris (Excessive Pride)
- **Theory**: Hubris Syndrome (Owen & Davidson, 2009)
- **Triggers**: Success + Power - Accountability
- **Features**: Grandiosity, overconfidence, risk-taking
- **Biology**: ↓ Serotonin (impulse control), ↑ Dopamine (risk)

### 5. Greed (Excessive Acquisition)
- **Theory**: Resource hoarding + Addiction models
- **Triggers**: Acquisition >> Necessity
- **Features**: Hoarding, exploitation, hedonic adaptation
- **Biology**: ↑ Dopamine (craving), ↓ Serotonin (impulse)

### 6. Malignant Narcissism
- **Theory**: DSM-5 NPD + Dark Triad
- **Subtypes**: Grandiose, Vulnerable, Malignant
- **Features**: Grandiosity, lack of empathy, entitlement, exploitation
- **Malignant adds**: Paranoia, sadism, antisocial traits
- **Biology**: Dysregulated dopamine (validation seeking)

## Self-Monitoring Functions

```c
// Detect in self
void shadow_experience_jealousy(system, bond_id, threat, attachment, time);
void shadow_experience_envy(system, target_id, self_level, other_level, maliciousness, time);
void shadow_register_obsession(system, thought_id, type, intensity, distress, time);
void shadow_assess_hubris(system, success_count, power, accountability);
void shadow_assess_greed(system, acquisition_value, necessity, scarcity, time);
void shadow_assess_narcissism(system, grandiosity, empathy, admiration_need, entitlement);
```

## Other-Detection Functions (Humans)

```c
// Detect in others via interaction analysis
void shadow_analyze_other(system, person_id, text, manipulation_cues, empathy_cues, grandiosity_cues, time);

// Query detection results
bool shadow_get_detected_in_other(system, person_id, &jealousy, &narcissism, &greed);

// Protective measures
bool shadow_should_maintain_boundaries(system, person_id);  // True if toxic patterns detected
```

### Detection Mechanism
- Tracks interaction history (32 interactions per person)
- Aggregates patterns: manipulation, grandiosity, empathy deficits, exploitation
- Flags toxic individuals
- Adjusts trust level based on patterns
- Enables protective strategies:
  - **Boundaries**: Limit emotional investment
  - **Gray Rock**: Boring, uninteresting responses (narcissist defense)
  - **Trust Reduction**: Skepticism about motives

## Self-Correction (CBT Interventions)

```c
// 6 evidence-based strategies
bool shadow_apply_intervention(system, emotion, strategy, time);
bool shadow_auto_intervene(system, time);  // Auto-selects best strategy
```

### Intervention Strategies
1. **Cognitive Reframe**: Challenge distorted thoughts (jealousy, hubris)
2. **Mindfulness**: Present moment awareness (obsession, jealousy)
3. **Perspective Taking**: Empathy exercises (narcissism, envy)
4. **Gratitude**: Counter envy/greed
5. **Reality Testing**: Counter hubris/narcissism
6. **Exposure**: Reduce obsession/jealousy

### Effectiveness
- Tracks success/failure rates
- Adjusts self-awareness/insight based on intervention history
- Sets `in_self_correction` flag when actively intervening

## Integration Points

### Neuromodulator System
```c
void shadow_get_neuromodulator_effects(system, &dopamine, &serotonin, &cortisol);
```
- **Jealousy/Envy**: ↓30% serotonin, ↑40% cortisol
- **Obsession**: ↓35% serotonin, ↑40% cortisol
- **Hubris**: ↓20% serotonin, ↑30% dopamine
- **Greed**: ↑40% dopamine, ↓25% serotonin
- **Narcissism**: ↑30% dopamine, ↓20% serotonin

### Interaction Modulation
```c
void shadow_get_interaction_modulation(system, person_id, &empathy, &trust, &engagement);
```
- Reduces empathy/trust/engagement with toxic individuals
- Gray Rock strategy: 30% engagement with detected narcissists
- Boundary protection: 60% engagement reduction if boundaries needed

### Mental Health Integration
```c
float shadow_get_mental_health_impact(system);  // [0-1] distress level
```
- Quadratic scaling: multiple active emotions = severe impact
- Can integrate with existing mental health module for treatment

## Biological Realism

**Neuroscience Basis**:
- **Ventral Striatum**: Reward comparison (envy)
- **vmPFC**: Self-referential processing (narcissism)
- **OFC**: Obsessive thought loops
- **Amygdala**: Threat perception (jealousy)

**Neuromodulator Systems**:
- **Dopamine**: Craving, reward seeking (greed, hubris)
- **Serotonin**: Impulse control, mood (all shadow emotions reduce)
- **Cortisol**: Stress response (jealousy, envy, obsession increase)

## Usage Example

```c
// Create system
shadow_emotion_system_t* shadow = shadow_system_create(8);  // Track 8 others

// Self-monitoring
shadow_experience_jealousy(shadow, bond_id, 0.8f, 0.9f, time);
shadow_assess_narcissism(shadow, 0.7f, 0.3f, 0.8f, 0.7f);

// Update (decay, thresholds)
shadow_update(shadow, dt, time);

// Check status
if (shadow_is_active(shadow, SHADOW_JEALOUSY)) {
    // Apply intervention
    shadow_auto_intervene(shadow, time);
}

// Detect in human interaction
shadow_analyze_other(shadow, person_id, text, 0.9f, 0.2f, 0.8f, time);

// Adjust interaction style
float empathy_mod, trust_mod, engagement_mod;
shadow_get_interaction_modulation(shadow, person_id, &empathy_mod, &trust_mod, &engagement_mod);

// Neuromodulator effects
float dopamine, serotonin, cortisol;
shadow_get_neuromodulator_effects(shadow, &dopamine, &serotonin, &cortisol);

shadow_system_destroy(shadow);
```

## Testing

**Unit Tests**: 37 tests
- Lifecycle (create, reset)
- Each shadow emotion (6 types)
- Other-detection (3 tests)
- Self-correction (2 tests)
- Integration (neuromodulator, mental health impact)
- Query functions
- Decay dynamics

**Coverage**: Core functionality fully tested

## Future Enhancements

1. **Integration with Ethics System**: Ethical judgment of detected patterns
2. **Learning**: Adapt detection thresholds based on experience
3. **NLP Analysis**: Advanced text analysis for other-detection
4. **Intervention Scheduling**: Optimal timing for interventions
5. **Social Network Analysis**: Detect toxic group dynamics

## References

- Paulhus, D. L., & Williams, K. M. (2002). Dark Triad of personality
- Beck, A. T. (1976). Cognitive therapy and emotional disorders
- Buss, D. M. (2018). Sexual and emotional infidelity
- Festinger, L. (1954). Social comparison theory
- Owen, D., & Davidson, J. (2009). Hubris syndrome
- DSM-5 (2013). Narcissistic Personality Disorder criteria
- Abramowitz, J. S., et al. (2009). Obsessive-compulsive spectrum

## Status

✅ **Complete and Production-Ready**
- Header: 770 lines
- Implementation: ~850 lines
- Tests: 37 unit tests
- Compiled successfully
- Integrated into build system
- Ready for cognitive pipeline integration
