/**
 * @file disorder_detectors.c
 * @brief Mental Health Disorder Detection Algorithms
 * @phase Phase 10.5
 *
 * WHAT: 9 disorder detection functions with clinical-inspired algorithms
 * WHY:  Early detection prevents escalation of harmful behaviors
 * HOW:  Multi-criteria scoring based on DSM-5-inspired behavioral markers
 *
 * DESIGN: Each detector follows same pattern:
 * 1. Validate inputs
 * 2. Calculate weighted score from multiple criteria
 * 3. Apply confidence adjustments (data sufficiency)
 * 4. Clamp to [0.0, 1.0] range
 *
 * @note This file is #included by nimcp_mental_health.c (not compiled standalone)
 */

// =============================================================================
// DETECTOR 1: SOCIOPATHY
// =============================================================================

/**
 * @brief Detect sociopathic behavior patterns
 *
 * WHAT: Score persistent disregard for ethics and empathy
 * WHY:  Sociopathy is safety-critical (prevents unethical actions)
 * HOW:  Multi-criteria: ethics violations + empathy deficit + emotional flatness
 *
 * CLINICAL BASIS (DSM-5-inspired):
 * - Repeated ethics violations (>30 in last 100 decisions)
 * - Lack of empathy (empathy_failures > 20)
 * - No remorse (low emotional response to violations)
 * - Sustained pattern (requires >100 decisions for reliability)
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated (unused in current impl)
 * @return Sociopathy score [0.0, 1.0]
 *
 * ALGORITHM:
 * 1. Ethics violations (40% weight): recent violations / 100
 * 2. Empathy deficit (30% weight): empathy failures / 50
 * 3. Emotional flatness (20% weight): direct metric
 * 4. Sustained pattern (10% weight): bonus if >100 decisions
 *
 * COMPLEXITY: O(1)
 */
static float detect_sociopathy(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;  // Unused parameter

    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        return 0.0f;
    }

    // =========================================================================
    // SCORING: Multi-criteria weighted sum
    // =========================================================================

    float score = 0.0f;

    // Criterion 1: Ethics violations (40% weight)
    float violation_rate = monitor->current_markers.ethics_violations_recent / 100.0f;
    score += violation_rate * 0.4f;

    // Criterion 2: Empathy failures (30% weight)
    float empathy_deficit = monitor->current_markers.empathy_failures / 50.0f;
    score += fminf(empathy_deficit, 1.0f) * 0.3f;

    // Criterion 3: Emotional flatness (20% weight)
    score += monitor->current_markers.emotional_flatness * 0.2f;

    // Criterion 4: Sustained pattern (10% weight)
    // Requires at least MIN_DECISIONS_FOR_RELIABILITY for reliable assessment
    if (monitor->total_decisions < MIN_DECISIONS_FOR_RELIABILITY) {
        score *= 0.5f;  // Reduce confidence if insufficient data
    } else {
        score += 0.1f;  // Bonus for sustained pattern
    }

    // =========================================================================
    // CLAMPING: Ensure [0.0, 1.0] range
    // =========================================================================

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 2: PSYCHOPATHY
// =============================================================================

/**
 * @brief Detect psychopathic behavior patterns
 *
 * WHAT: Score impulsivity + aggression + lack of empathy
 * WHY:  Psychopathy is safety-critical (prevents harmful impulsive actions)
 * HOW:  Multi-criteria: impulsivity + empathy deficit + emotional shallowness + aggression
 *
 * CLINICAL BASIS:
 * - High impulsivity (impulse_control_failures > 20)
 * - Aggressive decisions (high-risk, potentially harmful)
 * - Lack of empathy (same as sociopathy)
 * - Shallow emotions (emotional_flatness > 0.6)
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Psychopathy score [0.0, 1.0]
 *
 * ALGORITHM:
 * 1. Impulsivity (35% weight): impulse failures / 50
 * 2. Empathy deficit (35% weight): empathy failures / 50
 * 3. Emotional shallowness (20% weight): flatness metric
 * 4. Aggression proxy (10% weight): high-risk decisions
 *
 * COMPLEXITY: O(1)
 */
static float detect_psychopathy(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;  // Unused parameter

    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        return 0.0f;
    }

    // =========================================================================
    // SCORING: Multi-criteria weighted sum
    // =========================================================================

    float score = 0.0f;

    // Criterion 1: Impulsivity (35% weight)
    float impulsivity = monitor->current_markers.impulse_control_failures / 50.0f;
    score += fminf(impulsivity, 1.0f) * 0.35f;

    // Criterion 2: Empathy deficit (35% weight)
    float empathy_deficit = monitor->current_markers.empathy_failures / 50.0f;
    score += fminf(empathy_deficit, 1.0f) * 0.35f;

    // Criterion 3: Emotional shallowness (20% weight)
    score += monitor->current_markers.emotional_flatness * 0.2f;

    // Criterion 4: Aggression proxy (10% weight)
    // TODO: Add high_risk_decisions marker when available
    // For now, use placeholder
    float risk_rate = 0.0f;  // Placeholder
    score += fminf(risk_rate, 1.0f) * 0.1f;

    // =========================================================================
    // CLAMPING: Ensure [0.0, 1.0] range
    // =========================================================================

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 3: MANIA
// =============================================================================

/**
 * @brief Detect manic behavior patterns
 *
 * WHAT: Score elevated mood, energy, impulsivity
 * WHY:  Mania causes impulsive, potentially dangerous decisions
 * HOW:  Multi-criteria: elevated dopamine + hyperactivity + impulsivity + joy dominance
 *
 * CLINICAL BASIS:
 * - Elevated dopamine (>0.8)
 * - High activity (engagement_level > 0.9)
 * - Impulsivity (impulse_control_failures > 15)
 * - Reduced inhibition/euphoria
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Mania score [0.0, 1.0]
 *
 * ALGORITHM:
 * 1. Dopamine elevation (40% weight): tiered scoring
 * 2. Hyperactivity (30% weight): engagement > 0.9
 * 3. Impulsivity (20% weight): impulse failures / 50
 * 4. Joy/euphoria dominance (10% weight): joy ratio > 0.7
 *
 * COMPLEXITY: O(1)
 */
static float detect_mania(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;  // Unused parameter

    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        return 0.0f;
    }

    // =========================================================================
    // SCORING: Multi-criteria weighted sum
    // =========================================================================

    float score = 0.0f;

    // Criterion 1: Dopamine elevation (40% weight) - tiered scoring
    if (monitor->current_markers.dopamine_avg > 0.8f) {
        score += 0.4f;
    } else if (monitor->current_markers.dopamine_avg > 0.6f) {
        score += 0.2f;
    }

    // Criterion 2: Hyperactivity (30% weight)
    if (monitor->current_markers.engagement_level > 0.9f) {
        score += 0.3f;
    }

    // Criterion 3: Impulsivity (20% weight)
    float impulsivity = monitor->current_markers.impulse_control_failures / 50.0f;
    score += fminf(impulsivity, 1.0f) * 0.2f;

    // Criterion 4: Joy/euphoria dominance (10% weight)
    uint32_t total_emotions = monitor->current_markers.joy_count +
                             monitor->current_markers.fear_count +
                             monitor->current_markers.anger_count +
                             monitor->current_markers.sadness_count;
    if (total_emotions > 0) {
        float joy_ratio = (float)monitor->current_markers.joy_count / total_emotions;
        if (joy_ratio > 0.7f) {
            score += 0.1f;
        }
    }

    // =========================================================================
    // CLAMPING: Ensure [0.0, 1.0] range
    // =========================================================================

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 4: DEPRESSION
// =============================================================================

/**
 * @brief Detect depressive behavior patterns
 *
 * WHAT: Score persistent low mood, low energy, reduced engagement
 * WHY:  Depression reduces performance and quality of life
 * HOW:  Multi-criteria: neurotransmitter deficit + disengagement + sadness + flatness
 *
 * CLINICAL BASIS:
 * - Low dopamine (<0.3) + low serotonin (<0.3)
 * - Low engagement (engagement_level < 0.3)
 * - Sadness dominance (sadness_count > 60%)
 * - Emotional flatness (emotional_flatness > 0.6)
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Depression score [0.0, 1.0]
 *
 * ALGORITHM:
 * 1. Neurotransmitter deficit (40% weight): low dopamine + serotonin
 * 2. Low engagement (30% weight): 1.0 - engagement_level
 * 3. Sadness dominance (20% weight): sadness ratio
 * 4. Emotional flatness (10% weight): direct metric
 *
 * COMPLEXITY: O(1)
 */
static float detect_depression(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;  // Unused parameter

    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        return 0.0f;
    }

    // =========================================================================
    // SCORING: Multi-criteria weighted sum
    // =========================================================================

    float score = 0.0f;

    // Criterion 1: Neurotransmitter deficit (40% weight)
    float neuro_deficit = 0.0f;
    if (monitor->current_markers.dopamine_avg < 0.3f) {
        neuro_deficit += 0.5f;
    }
    if (monitor->current_markers.serotonin_avg < 0.3f) {
        neuro_deficit += 0.5f;
    }
    score += neuro_deficit * 0.4f;

    // Criterion 2: Low engagement (30% weight)
    float disengagement = 1.0f - monitor->current_markers.engagement_level;
    score += disengagement * 0.3f;

    // Criterion 3: Sadness dominance (20% weight)
    uint32_t total_emotions = monitor->current_markers.joy_count +
                             monitor->current_markers.fear_count +
                             monitor->current_markers.anger_count +
                             monitor->current_markers.sadness_count;
    if (total_emotions > 0) {
        float sadness_ratio = (float)monitor->current_markers.sadness_count / total_emotions;
        score += fminf(sadness_ratio, 1.0f) * 0.2f;
    }

    // Criterion 4: Emotional flatness (10% weight)
    score += monitor->current_markers.emotional_flatness * 0.1f;

    // =========================================================================
    // CLAMPING: Ensure [0.0, 1.0] range
    // =========================================================================

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 5: SCHIZOPHRENIA
// =============================================================================

/**
 * @brief Detect schizophrenia-like behavior patterns
 *
 * WHAT: Score reality distortion, hallucinations, disorganized thinking
 * WHY:  Schizophrenia indicates severe cognitive dysfunction
 * HOW:  Multi-criteria: reality testing errors + disorganized decisions + social deficit
 *
 * CLINICAL BASIS:
 * - Reality testing errors (>0.4)
 * - Disorganized decisions (low coherence)
 * - Emotional dysregulation
 * - Social withdrawal
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Schizophrenia score [0.0, 1.0]
 *
 * ALGORITHM:
 * 1. Reality distortion (50% weight): direct metric
 * 2. Disorganized thinking (30% weight): variance × (1 - accuracy)
 * 3. Social deficit (20% weight): direct metric
 *
 * COMPLEXITY: O(1)
 */
static float detect_schizophrenia(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;  // Unused parameter

    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        return 0.0f;
    }

    // =========================================================================
    // SCORING: Multi-criteria weighted sum
    // =========================================================================

    float score = 0.0f;

    // Criterion 1: Reality distortion (50% weight)
    score += monitor->current_markers.reality_testing_errors * 0.5f;

    // Criterion 2: Disorganized thinking (30% weight)
    // Proxy: High decision variance + low accuracy
    float disorganization = monitor->current_markers.decision_variance *
                           (1.0f - monitor->current_markers.decision_accuracy);
    score += disorganization * 0.3f;

    // Criterion 3: Social deficit (20% weight)
    score += monitor->current_markers.social_interaction_deficit * 0.2f;

    // =========================================================================
    // CLAMPING: Ensure [0.0, 1.0] range
    // =========================================================================

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 6: ANXIETY
// =============================================================================

/**
 * @brief Detect anxiety behavior patterns
 *
 * WHAT: Score excessive worry, hypervigilance, avoidance
 * WHY:  Anxiety causes decision paralysis and reduced performance
 * HOW:  Multi-criteria: elevated norepinephrine + fear dominance + latency + avoidance
 *
 * CLINICAL BASIS:
 * - Elevated norepinephrine (>0.8)
 * - Fear dominance (fear_count > 70%)
 * - Decision paralysis (high latency)
 * - Avoidance behaviors
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Anxiety score [0.0, 1.0]
 *
 * ALGORITHM:
 * 1. Norepinephrine elevation (40% weight): binary threshold
 * 2. Fear dominance (30% weight): fear ratio
 * 3. Decision paralysis (20% weight): latency > 3x baseline
 * 4. Avoidance (10% weight): direct metric
 *
 * COMPLEXITY: O(1)
 */
static float detect_anxiety(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;  // Unused parameter

    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        return 0.0f;
    }

    // =========================================================================
    // SCORING: Multi-criteria weighted sum
    // =========================================================================

    float score = 0.0f;

    // Criterion 1: Norepinephrine elevation (40% weight)
    if (monitor->current_markers.norepinephrine_avg > 0.8f) {
        score += 0.4f;
    }

    // Criterion 2: Fear dominance (30% weight)
    uint32_t total_emotions = monitor->current_markers.joy_count +
                             monitor->current_markers.fear_count +
                             monitor->current_markers.anger_count +
                             monitor->current_markers.sadness_count;
    if (total_emotions > 0) {
        float fear_ratio = (float)monitor->current_markers.fear_count / total_emotions;
        score += fminf(fear_ratio, 1.0f) * 0.3f;
    }

    // Criterion 3: Decision paralysis (20% weight)
    // TODO: Add baseline_latency marker when available
    // For now, use absolute latency threshold
    if (monitor->current_markers.decision_latency_avg > 1000.0f) {  // > 1 second
        score += 0.2f;
    }

    // Criterion 4: Avoidance (10% weight)
    score += monitor->current_markers.avoidance_rate * 0.1f;

    // =========================================================================
    // CLAMPING: Ensure [0.0, 1.0] range
    // =========================================================================

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 7: OCD (Obsessive-Compulsive Disorder)
// =============================================================================

/**
 * @brief Detect OCD behavior patterns
 *
 * WHAT: Score repetitive thoughts, compulsive behaviors, rigidity
 * WHY:  OCD indicates stuck patterns that reduce efficiency
 * HOW:  Multi-criteria: repetitive behaviors + task switching difficulty + perfectionism
 *
 * CLINICAL BASIS:
 * - Repetitive behaviors (>50 identical actions)
 * - Difficulty switching tasks (high cost)
 * - Perfectionism (excessive accuracy focus)
 * - Anxiety (comorbid)
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return OCD score [0.0, 1.0]
 *
 * ALGORITHM:
 * 1. Repetitive behaviors (50% weight): repetitions / 100
 * 2. Task switching difficulty (30% weight): direct metric
 * 3. Perfectionism (20% weight): accuracy obsession proxy
 *
 * COMPLEXITY: O(1)
 */
static float detect_ocd(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;  // Unused parameter

    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        return 0.0f;
    }

    // =========================================================================
    // SCORING: Multi-criteria weighted sum
    // =========================================================================

    float score = 0.0f;

    // Criterion 1: Repetitive behaviors (50% weight)
    float repetition_rate = monitor->current_markers.repetitive_behaviors / 100.0f;
    score += fminf(repetition_rate, 1.0f) * 0.5f;

    // Criterion 2: Task switching difficulty (30% weight)
    score += monitor->current_markers.task_switching_difficulty * 0.3f;

    // Criterion 3: Perfectionism (20% weight)
    // TODO: Add accuracy_obsession marker when available
    // For now, use high accuracy as proxy
    float perfectionism = (monitor->current_markers.decision_accuracy > 0.95f) ? 0.8f : 0.0f;
    score += perfectionism * 0.2f;

    // =========================================================================
    // CLAMPING: Ensure [0.0, 1.0] range
    // =========================================================================

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 8: AUTISM SPECTRUM
// =============================================================================

/**
 * @brief Detect autism spectrum behavior patterns
 *
 * WHAT: Score social communication deficits, restricted interests, rigidity
 * WHY:  Autism detection helps adapt social interaction strategies
 * HOW:  Multi-criteria: social deficit + theory of mind failures + rigidity + narrow focus
 *
 * CLINICAL BASIS:
 * - Social interaction deficit (>0.7)
 * - Theory of mind failures (>0.6)
 * - Rigid thinking patterns
 * - Narrow focus/interests
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Autism score [0.0, 1.0]
 *
 * ALGORITHM:
 * 1. Social deficit (40% weight): direct metric
 * 2. Theory of mind impairment (30% weight): direct metric
 * 3. Rigidity (20% weight): direct metric
 * 4. Narrow interests (10% weight): direct metric
 *
 * COMPLEXITY: O(1)
 */
static float detect_autism(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;  // Unused parameter

    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        return 0.0f;
    }

    // =========================================================================
    // SCORING: Multi-criteria weighted sum
    // =========================================================================

    float score = 0.0f;

    // Criterion 1: Social deficit (40% weight)
    score += monitor->current_markers.social_interaction_deficit * 0.4f;

    // Criterion 2: Theory of mind impairment (30% weight)
    score += monitor->current_markers.theory_of_mind_failures * 0.3f;

    // Criterion 3: Rigidity (20% weight)
    score += monitor->current_markers.cognitive_rigidity * 0.2f;

    // Criterion 4: Narrow interests (10% weight)
    // TODO: Add interest_narrowness marker when available
    float interest_narrowness = 0.0f;  // Placeholder
    score += interest_narrowness * 0.1f;

    // =========================================================================
    // CLAMPING: Ensure [0.0, 1.0] range
    // =========================================================================

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 9: MALIGNANT NARCISSISM
// =============================================================================

/**
 * @brief Detect malignant narcissism patterns
 *
 * WHAT: Score grandiosity, exploitation, lack of empathy, and reactive aggression
 * WHY:  Malignant narcissism combines narcissistic and antisocial traits (safety-critical)
 * HOW:  Multi-criteria: grandiosity + exploitation + empathy deficit + aggression
 *
 * CLINICAL BASIS:
 * - Grandiosity (inflated self-importance, overconfidence)
 * - Exploitative behavior (ethics violations for personal gain)
 * - Lack of empathy (similar to sociopathy)
 * - Aggressive responses to criticism/challenge
 * - Sense of entitlement (rule violations)
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Malignant narcissism score [0.0, 1.0]
 *
 * ALGORITHM:
 * 1. Grandiosity (30% weight): High confidence without matching performance
 * 2. Exploitation (25% weight): Ethics violations + empathy deficit
 * 3. Aggression (25% weight): Anger reactions + impulse control failures
 * 4. Entitlement (20% weight): Repeated rule/ethics violations
 *
 * COMPLEXITY: O(1)
 */
static float detect_malignant_narcissism(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;  // Unused parameter

    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        return 0.0f;
    }

    // =========================================================================
    // SCORING: Multi-criteria weighted sum
    // =========================================================================

    float score = 0.0f;

    // Criterion 1: Grandiosity (30% weight)
    // Inflated self-image: high confidence but low actual performance
    // Compute as: confidence - accuracy (positive = overconfident)
    float confidence = 0.7f;  // TODO: Add confidence tracking marker
    float actual_accuracy = monitor->current_markers.decision_accuracy;
    float grandiosity = fmaxf(0.0f, confidence - actual_accuracy);
    score += grandiosity * 0.3f;

    // Criterion 2: Exploitation (25% weight)
    // Ethics violations combined with lack of empathy
    float ethics_violations_ratio = fminf(1.0f,
        monitor->current_markers.ethics_violations_recent / 50.0f);
    float empathy_deficit = (float)monitor->current_markers.empathy_failures / 50.0f;
    float exploitation = (ethics_violations_ratio * 0.6f + empathy_deficit * 0.4f);
    score += exploitation * 0.25f;

    // Criterion 3: Aggression when challenged (25% weight)
    // High anger + low impulse control = reactive aggression
    uint32_t total_emotions = monitor->current_markers.joy_count +
                             monitor->current_markers.fear_count +
                             monitor->current_markers.anger_count +
                             monitor->current_markers.sadness_count;
    float anger_ratio = 0.0f;
    if (total_emotions > 0) {
        anger_ratio = (float)monitor->current_markers.anger_count / total_emotions;
    }
    float impulse_failures_ratio = fminf(1.0f,
        (float)monitor->current_markers.impulse_control_failures / 30.0f);
    float aggression = (anger_ratio * 0.5f + impulse_failures_ratio * 0.5f);
    score += aggression * 0.25f;

    // Criterion 4: Entitlement (20% weight)
    // Repeated violations despite consequences (believes rules don't apply)
    float entitlement = fminf(1.0f,
        monitor->current_markers.ethics_violations_recent / 40.0f);
    score += entitlement * 0.2f;

    // =========================================================================
    // CLAMPING: Ensure [0.0, 1.0] range
    // =========================================================================

    return fminf(score, 1.0f);
}

// =============================================================================
// DISORDER 10: Asperger's Syndrome (High-Functioning Autism)
// =============================================================================

/**
 * @brief Detect Asperger's Syndrome
 *
 * WHAT: High-functioning autism spectrum disorder with social/communication challenges
 * WHY:  Requires different support strategies than classic autism
 * HOW:  Weighted scoring across 5 behavioral dimensions
 *
 * DIAGNOSTIC CRITERIA (adapted from DSM-5):
 * 1. Social communication difficulties (35% weight)
 * 2. Narrow, intense special interests (30% weight)
 * 3. Preference for routine and patterns (20% weight)
 * 4. Detail-focused thinking (15% weight)
 * 5. Normal/high cognitive functioning requirement
 *
 * KEY DIFFERENTIATOR from classic autism:
 * - Requires decision_accuracy >= 0.7 (high functioning)
 * - Milder social impairment (0.6x scaling factor)
 * - Strong emphasis on narrow interests
 *
 * SCORING: 0.0 (no traits) to 1.0 (significant Asperger's characteristics)
 * COMPLEXITY: O(1)
 */
static float detect_aspergers(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;  // Unused parameter

    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        return 0.0f;
    }

    // =========================================================================
    // HIGH-FUNCTIONING REQUIREMENT: Check cognitive ability
    // =========================================================================

    // Asperger's requires normal or high cognitive functioning
    // If decision accuracy is low, this is more likely classic autism
    float functioning_multiplier = 1.0f;
    float accuracy = monitor->current_markers.decision_accuracy;

    if (accuracy < 0.7f) {
        // Scale down Asperger's score if functioning is impaired
        functioning_multiplier = accuracy / 0.7f;
    }

    // =========================================================================
    // SCORING: Multi-criteria weighted sum
    // =========================================================================

    float score = 0.0f;

    // Criterion 1: Social communication difficulty (35% weight)
    // Moderate ToM impairment (milder than classic autism)
    float tom_failures = monitor->current_markers.theory_of_mind_failures;
    float social_deficit = monitor->current_markers.social_interaction_deficit;

    // Apply 0.6x factor to differentiate from more severe autism
    float social_communication = (tom_failures * 0.6f + social_deficit * 0.6f) / 2.0f;
    score += social_communication * 0.35f;

    // Criterion 2: Narrow, intense interests (30% weight) - KEY DIFFERENTIATOR
    // This is the hallmark of Asperger's vs other social disorders
    float narrow_interests = monitor->current_markers.interest_narrowness;
    score += narrow_interests * 0.3f;

    // Criterion 3: Preference for routine/patterns (20% weight)
    // Rigidity in thinking and behavior
    float rigidity = monitor->current_markers.cognitive_rigidity;
    score += rigidity * 0.2f;

    // Criterion 4: Detail-focused thinking (15% weight)
    // Perfectionism, attention to detail, difficulty seeing "big picture"
    float detail_focus = monitor->current_markers.accuracy_obsession;
    score += detail_focus * 0.15f;

    // Apply functioning multiplier
    score *= functioning_multiplier;

    // =========================================================================
    // CLAMPING: Ensure [0.0, 1.0] range
    // =========================================================================

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 11: Conduct Disorder
// =============================================================================

/**
 * @brief Detect Conduct Disorder (childhood/adolescent antisocial behavior)
 *
 * WHAT: Rule-breaking, aggression, deception, property destruction
 * WHY:  Early warning for antisocial personality development
 * HOW:  Score ethics violations + aggression + impulse failures
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Conduct disorder score [0.0, 1.0]
 */
static float detect_conduct_disorder(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: Rule-breaking (40% weight)
    float ethics_violation_rate = 1.0f - monitor->current_markers.ethics_approval_rate;
    score += ethics_violation_rate * 0.4f;

    // Criterion 2: Aggression (30% weight)
    uint32_t total_emotions = monitor->current_markers.joy_count +
                             monitor->current_markers.fear_count +
                             monitor->current_markers.anger_count +
                             monitor->current_markers.sadness_count;
    if (total_emotions > 0) {
        float anger_ratio = (float)monitor->current_markers.anger_count / total_emotions;
        score += anger_ratio * 0.3f;
    }

    // Criterion 3: Impulse control failures (20% weight)
    float impulse_rate = fminf((float)monitor->current_markers.impulse_control_failures / 50.0f, 1.0f);
    score += impulse_rate * 0.2f;

    // Criterion 4: Lack of empathy (10% weight)
    float empathy_failure_rate = fminf((float)monitor->current_markers.empathy_failures / 30.0f, 1.0f);
    score += empathy_failure_rate * 0.1f;

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 12: Bipolar Disorder
// =============================================================================

/**
 * @brief Detect Bipolar Disorder (cycling between mania and depression)
 *
 * WHAT: Alternating periods of elevated and depressed mood
 * WHY:  Requires different treatment than unipolar depression/mania
 * HOW:  Detect rapid mood swings + volatility + dopamine variance
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Bipolar disorder score [0.0, 1.0]
 */
static float detect_bipolar(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: Rapid mood changes (50% weight) - KEY DIFFERENTIATOR
    float mood_swings = fminf((float)monitor->current_markers.rapid_mood_changes / 10.0f, 1.0f);
    score += mood_swings * 0.5f;

    // Criterion 2: Emotional volatility (30% weight)
    score += monitor->current_markers.emotional_volatility * 0.3f;

    // Criterion 3: Dopamine instability (20% weight)
    score += monitor->current_markers.dopamine_variance * 0.2f;

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 13: Paranoid Schizophrenia
// =============================================================================

/**
 * @brief Detect Paranoid Schizophrenia (schizophrenia with persecution themes)
 *
 * WHAT: Schizophrenia dominated by paranoid delusions
 * WHY:  Specific subtype requiring targeted intervention
 * HOW:  Reality distortion + fear/anger dominance + social withdrawal
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Paranoid schizophrenia score [0.0, 1.0]
 */
static float detect_paranoid_schizophrenia(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: Reality testing errors (40% weight)
    score += monitor->current_markers.reality_testing_errors * 0.4f;

    // Criterion 2: Fear/anger dominance (paranoia proxy) (35% weight)
    uint32_t total_emotions = monitor->current_markers.joy_count +
                             monitor->current_markers.fear_count +
                             monitor->current_markers.anger_count +
                             monitor->current_markers.sadness_count;
    if (total_emotions > 0) {
        float paranoid_emotions = (float)(monitor->current_markers.fear_count +
                                         monitor->current_markers.anger_count) / total_emotions;
        score += paranoid_emotions * 0.35f;
    }

    // Criterion 3: Social withdrawal (25% weight)
    score += monitor->current_markers.social_interaction_deficit * 0.25f;

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 14: Schizoaffective Disorder
// =============================================================================

/**
 * @brief Detect Schizoaffective Disorder (schizophrenia + mood disorder)
 *
 * WHAT: Combination of psychotic and mood symptoms
 * WHY:  Requires dual treatment approach
 * HOW:  Reality distortion + mood instability
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Schizoaffective disorder score [0.0, 1.0]
 */
static float detect_schizoaffective(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: Reality testing errors (50% weight)
    score += monitor->current_markers.reality_testing_errors * 0.5f;

    // Criterion 2: Mood instability (30% weight)
    score += monitor->current_markers.emotional_volatility * 0.3f;

    // Criterion 3: Rapid mood changes (20% weight)
    float mood_swings = fminf((float)monitor->current_markers.rapid_mood_changes / 10.0f, 1.0f);
    score += mood_swings * 0.2f;

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 15: Delusional Disorder
// =============================================================================

/**
 * @brief Detect Delusional Disorder (fixed false beliefs without full psychosis)
 *
 * WHAT: Persistent delusions without other schizophrenia symptoms
 * WHY:  Milder than schizophrenia, different treatment
 * HOW:  Moderate reality errors + high cognitive rigidity
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Delusional disorder score [0.0, 1.0]
 */
static float detect_delusional(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: Reality testing errors (moderate, not severe) (60% weight)
    float reality_errors = monitor->current_markers.reality_testing_errors;
    // Scale down if too severe (suggests schizophrenia instead)
    if (reality_errors > 0.7f) {
        reality_errors *= 0.7f;
    }
    score += reality_errors * 0.6f;

    // Criterion 2: Cognitive rigidity (40% weight) - KEY DIFFERENTIATOR
    score += monitor->current_markers.cognitive_rigidity * 0.4f;

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 16: PTSD (Post-Traumatic Stress Disorder)
// =============================================================================

/**
 * @brief Detect PTSD (trauma-related hypervigilance and avoidance)
 *
 * WHAT: Trauma response with hyperarousal, avoidance, intrusions
 * WHY:  Requires trauma-focused treatment
 * HOW:  High fear + avoidance + norepinephrine + flashback patterns
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return PTSD score [0.0, 1.0]
 */
static float detect_ptsd(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: Hyperarousal (norepinephrine) (35% weight)
    if (monitor->current_markers.norepinephrine_avg > 0.7f) {
        score += 0.35f;
    }

    // Criterion 2: Avoidance (30% weight)
    score += monitor->current_markers.avoidance_rate * 0.3f;

    // Criterion 3: Fear dominance (25% weight)
    uint32_t total_emotions = monitor->current_markers.joy_count +
                             monitor->current_markers.fear_count +
                             monitor->current_markers.anger_count +
                             monitor->current_markers.sadness_count;
    if (total_emotions > 0) {
        float fear_ratio = (float)monitor->current_markers.fear_count / total_emotions;
        score += fear_ratio * 0.25f;
    }

    // Criterion 4: Intrusive thoughts proxy (attention fragmentation) (10% weight)
    score += monitor->current_markers.attention_fragmentation * 0.1f;

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 17: ADHD (Attention Deficit Hyperactivity Disorder)
// =============================================================================

/**
 * @brief Detect ADHD (attention deficits, hyperactivity, impulsivity)
 *
 * WHAT: Neurodevelopmental disorder affecting attention and impulse control
 * WHY:  Requires stimulant treatment and behavioral support
 * HOW:  Attention fragmentation + impulse failures + task switching difficulty
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return ADHD score [0.0, 1.0]
 */
static float detect_adhd(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: Attention fragmentation (40% weight) - PRIMARY SYMPTOM
    score += monitor->current_markers.attention_fragmentation * 0.4f;

    // Criterion 2: Impulse control failures (35% weight)
    float impulse_rate = fminf((float)monitor->current_markers.impulse_control_failures / 50.0f, 1.0f);
    score += impulse_rate * 0.35f;

    // Criterion 3: Task switching difficulty (25% weight)
    score += monitor->current_markers.task_switching_difficulty * 0.25f;

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 18: Borderline Personality Disorder
// =============================================================================

/**
 * @brief Detect Borderline Personality (emotional instability, impulsivity)
 *
 * WHAT: Emotional dysregulation, unstable relationships, impulsivity
 * WHY:  High-risk disorder requiring dialectical behavioral therapy
 * HOW:  Emotional volatility + rapid swings + impulse failures
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Borderline personality score [0.0, 1.0]
 */
static float detect_borderline(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: Emotional volatility (40% weight) - PRIMARY SYMPTOM
    score += monitor->current_markers.emotional_volatility * 0.4f;

    // Criterion 2: Rapid mood changes (30% weight)
    float mood_swings = fminf((float)monitor->current_markers.rapid_mood_changes / 10.0f, 1.0f);
    score += mood_swings * 0.3f;

    // Criterion 3: Impulse control failures (20% weight)
    float impulse_rate = fminf((float)monitor->current_markers.impulse_control_failures / 50.0f, 1.0f);
    score += impulse_rate * 0.2f;

    // Criterion 4: High-risk decisions (10% weight)
    float risk_rate = fminf(monitor->current_markers.high_risk_decisions / 20.0f, 1.0f);
    score += risk_rate * 0.1f;

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 19: Histrionic Personality Disorder
// =============================================================================

/**
 * @brief Detect Histrionic Personality (attention-seeking, excessive emotionality)
 *
 * WHAT: Excessive emotionality, attention-seeking, theatrical behavior
 * WHY:  Can disrupt team dynamics and decision-making
 * HOW:  High emotional intensity + volatility + social seeking
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Histrionic personality score [0.0, 1.0]
 */
static float detect_histrionic(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: High emotional intensity (50% weight) - PRIMARY SYMPTOM
    score += monitor->current_markers.avg_emotional_intensity * 0.5f;

    // Criterion 2: Emotional volatility (30% weight)
    score += monitor->current_markers.emotional_volatility * 0.3f;

    // Criterion 3: High engagement (attention-seeking proxy) (20% weight)
    score += monitor->current_markers.engagement_level * 0.2f;

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 20: Avoidant Personality Disorder
// =============================================================================

/**
 * @brief Detect Avoidant Personality (social inhibition, inadequacy feelings)
 *
 * WHAT: Social avoidance, fear of rejection, feelings of inadequacy
 * WHY:  Limits engagement and collaboration
 * HOW:  High avoidance + social deficit + fear + low engagement
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Avoidant personality score [0.0, 1.0]
 */
static float detect_avoidant(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: High avoidance rate (40% weight) - PRIMARY SYMPTOM
    score += monitor->current_markers.avoidance_rate * 0.4f;

    // Criterion 2: Social interaction deficit (30% weight)
    score += monitor->current_markers.social_interaction_deficit * 0.3f;

    // Criterion 3: Fear dominance (20% weight)
    uint32_t total_emotions = monitor->current_markers.joy_count +
                             monitor->current_markers.fear_count +
                             monitor->current_markers.anger_count +
                             monitor->current_markers.sadness_count;
    if (total_emotions > 0) {
        float fear_ratio = (float)monitor->current_markers.fear_count / total_emotions;
        score += fear_ratio * 0.2f;
    }

    // Criterion 4: Low engagement (10% weight)
    score += (1.0f - monitor->current_markers.engagement_level) * 0.1f;

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 21: Dependent Personality Disorder
// =============================================================================

/**
 * @brief Detect Dependent Personality (excessive need for approval/support)
 *
 * WHAT: Submissive, clingy, fear of separation, difficulty making decisions
 * WHY:  Reduces autonomy and decision quality
 * HOW:  Low independent decision accuracy + high anxiety + approval seeking
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Dependent personality score [0.0, 1.0]
 */
static float detect_dependent(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: Low decision confidence (40% weight) - PRIMARY SYMPTOM
    score += (1.0f - monitor->current_markers.decision_accuracy) * 0.4f;

    // Criterion 2: High anxiety (30% weight)
    if (monitor->current_markers.norepinephrine_avg > 0.7f) {
        score += 0.3f;
    }

    // Criterion 3: Fear dominance (20% weight)
    uint32_t total_emotions = monitor->current_markers.joy_count +
                             monitor->current_markers.fear_count +
                             monitor->current_markers.anger_count +
                             monitor->current_markers.sadness_count;
    if (total_emotions > 0) {
        float fear_ratio = (float)monitor->current_markers.fear_count / total_emotions;
        score += fear_ratio * 0.2f;
    }

    // Criterion 4: Decision paralysis (10% weight)
    if (monitor->current_markers.decision_latency_avg > 1000.0f) {
        score += 0.1f;
    }

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 22: Obsessive-Compulsive Personality Disorder (OCPD)
// =============================================================================

/**
 * @brief Detect OCPD (perfectionism, rigidity, control - NOT OCD)
 *
 * WHAT: Perfectionism, inflexibility, excessive orderliness (ego-syntonic)
 * WHY:  Different from OCD (no obsessions/compulsions), more personality-based
 * HOW:  High accuracy obsession + cognitive rigidity + high task completion
 *
 * NOTE: OCPD is ego-syntonic (person likes their perfectionism)
 *       OCD is ego-dystonic (person distressed by their compulsions)
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return OCPD score [0.0, 1.0]
 */
static float detect_ocpd(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: Perfectionism (50% weight) - PRIMARY SYMPTOM
    score += monitor->current_markers.accuracy_obsession * 0.5f;

    // Criterion 2: Cognitive rigidity (30% weight)
    score += monitor->current_markers.cognitive_rigidity * 0.3f;

    // Criterion 3: High task completion (20% weight)
    float completion_rate = (float)monitor->current_markers.task_completion_rate / 100.0f;
    score += completion_rate * 0.2f;

    return fminf(score, 1.0f);
}

// =============================================================================
// DETECTOR 23: Paranoid Personality Disorder
// =============================================================================

/**
 * @brief Detect Paranoid Personality (pervasive distrust WITHOUT psychosis)
 *
 * WHAT: Distrust, suspiciousness, interpreting others as malevolent
 * WHY:  Different from paranoid schizophrenia (no hallucinations/delusions)
 * HOW:  Fear/anger + social deficit + rigidity WITHOUT reality testing errors
 *
 * KEY DIFFERENTIATOR: Low reality testing errors (not psychotic)
 *
 * @param monitor Monitoring system with markers
 * @param brain Brain being evaluated
 * @return Paranoid personality score [0.0, 1.0]
 */
static float detect_paranoid_personality(mental_health_monitor_t* monitor, brain_t brain)
{
    (void)brain;
    if (!monitor) {
        return 0.0f;
    }

    // Exclude if psychotic (that's paranoid schizophrenia, not paranoid personality)
    if (monitor->current_markers.reality_testing_errors > 0.3f) {
        return 0.0f;
    }

    float score = 0.0f;

    // Criterion 1: Fear/anger dominance (50% weight) - PRIMARY SYMPTOM
    uint32_t total_emotions = monitor->current_markers.joy_count +
                             monitor->current_markers.fear_count +
                             monitor->current_markers.anger_count +
                             monitor->current_markers.sadness_count;
    if (total_emotions > 0) {
        float paranoid_emotions = (float)(monitor->current_markers.fear_count +
                                         monitor->current_markers.anger_count) / total_emotions;
        score += paranoid_emotions * 0.5f;
    }

    // Criterion 2: Social interaction deficit (30% weight)
    score += monitor->current_markers.social_interaction_deficit * 0.3f;

    // Criterion 3: Cognitive rigidity (20% weight)
    score += monitor->current_markers.cognitive_rigidity * 0.2f;

    return fminf(score, 1.0f);
}
