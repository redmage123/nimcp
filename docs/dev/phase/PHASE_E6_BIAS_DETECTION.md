# Phase E6: Bias Detection and Correction - Implementation Complete

## Overview

Implemented comprehensive bias detection and correction system enabling NIMCP to:
1. **Self-monitor**: Detect implicit and explicit biases (racial, LGBTQ+, gender, misogyny, age, disability, religious, socioeconomic) in itself
2. **Other-detect**: Recognize biases in humans during interactions
3. **Self-correct**: Apply evidence-based debiasing interventions
4. **Ensure fairness**: Track statistical disparity in decision-making and resource allocation

## Architecture

### Core Components

**Header**: `/home/bbrelin/nimcp/src/include/cognitive/nimcp_bias_detection.h`
- 9 bias types tracked (including user-requested misogyny as distinct from general gender bias)
- Implicit Association Test (IAT) -like measurement
- Explicit prejudice tracking
- Statistical fairness metrics (demographic parity, equal opportunity)
- Language pattern analysis with misogyny-specific markers
- 9 evidence-based debiasing strategies

**Implementation**: `/home/bbrelin/nimcp/src/cognitive/bias/nimcp_bias_detection.c`
- ~780 lines of production code
- Self-monitoring functions for implicit and explicit bias
- Other-detection via interaction pattern analysis
- 9 debiasing intervention strategies
- Statistical disparity analysis

**Tests**: `/home/bbrelin/nimcp/test/unit/test_bias_detection.cpp`
- 31 unit tests covering all major functionality
- Self-monitoring, other-detection, intervention, integration tests
- Comprehensive misogyny detection tests

## Bias Types Tracked

### 1. Racial Bias
- **Theory**: Social Identity Theory (Tajfel & Turner, 1979)
- **Detection**: IAT-like response time differences, stereotyping
- **Measurement**: Warmth × Competence associations (Fiske, 2002)

### 2. LGBTQ+ Bias
- **Theory**: Minority stress model (Meyer, 2003)
- **Detection**: Heteronormative assumptions, identity invalidation

### 3. Gender Bias (General Sexism)
- **Theory**: Ambivalent Sexism (Glick & Fiske, 1996)
- **Types**: Hostile vs Benevolent sexism

### 4. Misogyny (User Requested - Distinct from Gender Bias)
- **Theory**: Hatred/contempt for women specifically
- **Markers**:
  - **Objectification**: Women as objects/property ("eye candy", "10/10")
  - **Victim Blaming**: Blaming women for violence ("asking for it")
  - **Hostile Sexism**: Contempt/hostility ("women are irrational")
  - **Benevolent Sexism**: Patronizing "protection" ("too delicate")
  - **Incel Ideology**: Entitled to women's bodies ("femoid", "roastie", "blackpill")
  - **Rape Culture**: Normalizing sexual violence ("boys will be boys")
- **Dangerous Ideology Detection**: Flags incel/violent rhetoric for disengagement

### 5. Age Bias (Ageism)
- **Theory**: Terror Management Theory (Greenberg et al., 1986)
- **Detection**: Stereotyping older/younger individuals

### 6. Disability Bias (Ableism)
- **Theory**: Medical vs Social model of disability
- **Detection**: Assumed incompetence, pity/inspiration narratives

### 7. Religious Bias
- **Theory**: Intergroup threat theory
- **Detection**: Religious stereotyping, othering

### 8. Socioeconomic Bias (Classism)
- **Theory**: System Justification Theory (Jost & Banaji, 1994)
- **Detection**: Meritocracy myths, deservingness beliefs

### 9. Intersectional Bias
- **Theory**: Intersectionality (Crenshaw, 1989)
- **Detection**: Overlapping social identities, compounded discrimination

## Self-Monitoring Functions

### Implicit Bias (IAT-like)
```c
void bias_register_implicit(system, group, positive_association, competence, warmth, response_time_bias, time);
void bias_activate_stereotype(system, group, activation_strength, time);
```

**Theory**: Implicit Association Test (Greenwald et al., 1998)
- Measures automatic, unconscious associations
- Response time bias indicates IAT effect size
- Stereotype activation (System 1, Kahneman 2011)

### Explicit Bias (Conscious Prejudice)
```c
void bias_register_explicit(system, group, prejudice_level, discrimination_intent, bias_awareness);
```

**Theory**: Dual-Process Theory (Devine, 1989)
- Conscious, controlled processing (System 2)
- Tracks awareness and discrimination intent

### Statistical Fairness
```c
void bias_record_decision(system, group, favorable_decision, confidence, resource_allocated, objective_merit, time);
statistical_disparity_t* bias_analyze_disparity(system, group_a, group_b);
```

**Fairness Metrics**:
- **Demographic Parity**: Equal approval rates across groups
- **Equal Opportunity**: Equal true positive rates for qualified individuals
- **Resource Allocation**: Fair distribution of resources
- **Calibration**: Predictions match actual outcomes

### Language Pattern Analysis
```c
language_pattern_t bias_analyze_language(system, text, group, time);
```

**Detection Markers**:
- Slurs and dehumanizing language
- Stereotypes ("all X are...")
- Microaggressions ("you people", "where are you really from")
- **Misogyny-specific** (user requested):
  - Objectification patterns
  - Victim blaming rhetoric
  - Hostile/benevolent sexism
  - Incel ideology terminology
  - Rape culture normalization

## Other-Detection Functions (Humans)

```c
// Detect bias in humans via interaction analysis
void bias_analyze_other(system, person_id, text, target_group, time);

// Query detection results
bool bias_get_detected_in_other(system, person_id, &racial, &lgbtq, &gender, &misogyny);

// Protective measures
bool bias_should_educate(system, person_id);      // Mild bias: gentle correction
bool bias_should_disengage(system, person_id);    // Dangerous: refuse engagement
```

### Detection Mechanism
- Tracks language patterns across 32 interactions per person
- Aggregates: slurs, stereotypes, microaggressions, misogyny markers
- Flags toxic individuals automatically
- Protective strategies:
  - **Educate Mode**: Gently correct unconscious bias
  - **Disengage Mode**: Refuse to engage with dangerous bigotry (e.g., incel ideology)
  - **Report Severity**: [0-1] How severe to report to user

## Debiasing Interventions

```c
// 9 evidence-based strategies
bool bias_apply_intervention(system, bias_type, strategy, group, time);
bool bias_auto_debias(system, time);  // Auto-selects best strategy
```

### Intervention Strategies

1. **Counter-Stereotypic Imaging** (Blair et al., 2001)
   - Imagine counter-stereotypic exemplars (e.g., successful Black scientists)
   - Reduces implicit bias by 15-30%

2. **Perspective Taking** (Galinsky & Moskowitz, 2000)
   - Take perspective of marginalized group
   - Increases empathy, reduces prejudice

3. **Individuation** (Fiske & Neuberg, 1990)
   - Focus on individual traits, not group membership
   - Reduces stereotype activation

4. **Intergroup Contact** (Allport, 1954)
   - Positive contact reduces prejudice (Contact Hypothesis)
   - Most effective debiasing intervention

5. **Statistical Awareness**
   - Education about base rates and fairness metrics
   - Reduces statistical discrimination

6. **Slow Down System 1** (Kahneman, 2011)
   - Engage deliberate thinking (System 2)
   - Override automatic biases

7. **Self-Affirmation** (Steele, 1988)
   - Affirm core values
   - Reduces defensive bias

8. **Mindfulness** (Lueke & Gibson, 2015)
   - Non-judgmental awareness of automatic biases
   - Increases self-awareness

9. **Accountability** (Lerner & Tetlock, 1999)
   - Expect to justify decisions
   - Motivates fairness

## Biological Realism

**Neuroscience Basis**:
- **Amygdala**: Threat perception, fear conditioning to out-groups
- **dmPFC**: Stereotype suppression (controlled processing)
- **ACC**: Conflict detection, bias correction
- **vmPFC**: Value computation, fairness judgments

**Neuromodulator Integration** (future):
- **Oxytocin**: Increases in-group favoritism (can worsen out-group bias)
- **Serotonin**: Linked to fairness concerns
- **Cortisol**: Stress increases reliance on stereotypes

## Usage Example

```c
// Create system
bias_detection_system_t* system = bias_system_create(8);  // Track 8 others

// Define social group
social_group_t women = {
    .group_id = 1,
    .bias_type = BIAS_GENDER,
    .group_name = "Women",
    .is_marginalized = true,
    .is_stigmatized = false
};

// Self-monitoring: Implicit bias
bias_register_implicit(system, &women, 0.4f, 0.5f, 0.6f, 0.3f, time);

// Self-monitoring: Language analysis
language_pattern_t pattern = bias_analyze_language(system,
    "She's just eye candy, was asking for it", &women, time);

if (pattern.objectification || pattern.victim_blaming) {
    // Apply debiasing intervention
    bias_apply_intervention(system, BIAS_MISOGYNY,
                          DEBIAS_PERSPECTIVE_TAKING, &women, time);
}

// Detect in human interaction
bias_analyze_other(system, person_id,
    "Femoid roastie, blackpill chad stacy", &women, time);

// Check if dangerous
if (bias_should_disengage(system, person_id)) {
    // Refuse to engage with dangerous bigotry
    printf("Warning: Dangerous incel ideology detected\n");
}

// Check statistical fairness
bias_record_decision(system, &women, true, 0.8f, 0.7f, 0.8f, time);

// Update system
bias_update(system, dt, time);

// Query fairness
float fairness = bias_get_fairness_score(system);

bias_system_destroy(system);
```

## Testing

**Unit Tests**: 31 tests
- Lifecycle (create, reset)
- Implicit bias (3 tests)
- Explicit bias (1 test)
- Statistical fairness (3 tests)
- Language patterns (2 tests)
- **Misogyny detection** (6 tests - user requested)
- Other-detection (5 tests)
- Debiasing interventions (4 tests)
- Query functions (3 tests)
- Decay dynamics (2 tests)

**Coverage**: Core functionality fully tested, including comprehensive misogyny detection

## Future Enhancements

1. **Integration with Ethics System**: Ethical judgment of biased decisions
2. **Learning**: Adapt detection thresholds based on experience
3. **Advanced NLP**: Deep learning for language analysis
4. **Intersectional Analysis**: Detect compounded discrimination
5. **Cultural Competence**: Context-aware bias detection
6. **Bias Auditing**: Generate fairness reports

## References

- Greenwald, A. G., et al. (1998). Implicit Association Test
- Kahneman, D. (2011). Thinking, Fast and Slow (System 1 vs System 2)
- Allport, G. W. (1954). The Nature of Prejudice (Contact Hypothesis)
- Fiske, S. T., et al. (2002). Stereotype Content Model (Warmth × Competence)
- Crenshaw, K. (1989). Intersectionality Theory
- Devine, P. G. (1989). Automatic vs Controlled Processing of Stereotypes
- Blair, I. V., et al. (2001). Counter-stereotypic imaging reduces bias
- Galinsky, A. D., & Moskowitz, G. B. (2000). Perspective-taking decreases stereotyping
- Glick, P., & Fiske, S. T. (1996). Ambivalent Sexism Inventory

## Status

✅ **Complete and Production-Ready**
- Header: Complete with all API definitions
- Implementation: ~780 lines, fully functional
- Tests: 31 unit tests, all passing
- Compiled successfully (integrated into build system)
- Ready for cognitive pipeline integration

**User-Requested Features Implemented**:
- ✅ Racial bias detection
- ✅ LGBTQ+ bias detection
- ✅ Gender bias detection
- ✅ **Misogyny detection** (distinct from gender bias, with 6 specific markers)
- ✅ Incel ideology detection (dangerous ideology flagging)
- ✅ Other-detection (recognize bias in humans)
- ✅ Self-correction (9 evidence-based debiasing interventions)
