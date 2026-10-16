# Embodied Identity in Artificial Cognitive Systems

*How Personality, Emotion, and Self-Model Converge into Voice and Behavior*

**Version 1.0 | March 2026**

---

## Table of Contents

- [Abstract](#abstract)
- [Introduction](#introduction)
- [1. The Identity Architecture](#1-the-identity-architecture)
- [2. Personality Model (Big Five / OCEAN)](#2-personality-model-big-five--ocean)
  - [2.1 Trait Representation](#21-trait-representation)
  - [2.2 Personality → Voice Mapping](#22-personality--voice-mapping)
  - [2.3 Personality → Neuromodulator Bias](#23-personality--neuromodulator-bias)
- [3. Emotional State and Prosody](#3-emotional-state-and-prosody)
  - [3.1 Valence-Arousal-Dominance Model](#31-valence-arousal-dominance-model)
  - [3.2 Emotion → Voice Quality Mapping](#32-emotion--voice-quality-mapping)
  - [3.3 Brain-State Prosody Modulation](#33-brain-state-prosody-modulation)
- [4. Voice Synthesis](#4-voice-synthesis)
  - [4.1 Fundamental Frequency and Gender](#41-fundamental-frequency-and-gender)
  - [4.2 Formant Synthesis (Parentese)](#42-formant-synthesis-parentese)
  - [4.3 Pitch Contour Patterns](#43-pitch-contour-patterns)
- [5. Self-Model and Introspection](#5-self-model-and-introspection)
  - [5.1 Metacognitive Confidence → Voice Modulation](#51-metacognitive-confidence--voice-modulation)
  - [5.2 Self-Narrative Generation](#52-self-narrative-generation)
  - [5.3 Identity Seeding](#53-identity-seeding)
- [6. Theory of Mind and Audience Adaptation](#6-theory-of-mind-and-audience-adaptation)
- [7. Accent and Cultural Identity](#7-accent-and-cultural-identity)
- [8. Identity Evolution During Training](#8-identity-evolution-during-training)
- [9. Connection to Neuromodulation and Learning](#9-connection-to-neuromodulation-and-learning)
- [10. Comparison to Existing Approaches](#10-comparison-to-existing-approaches)
- [Glossary](#glossary)
- [References](#references)

---

## Abstract

Modern AI systems have capabilities but no identity. A language model can write poetry, but it has no voice — no characteristic pitch, no emotional prosody, no personality-consistent speaking style. An embodied robot can navigate, but its movements carry no personality signature — no cautious slowness from neuroticism, no exploratory boldness from openness.

This paper describes the identity architecture of NIMCP, a 2.5-million neuron artificial brain named Athena. The system integrates five cognitive modules — personality (Big Five traits), emotional state (valence-arousal-dominance), self-model (beliefs and capabilities), introspection (metacognitive confidence), and Theory of Mind (audience modeling) — into a unified identity controller that produces consistent, personality-driven voice characteristics, emotional prosody, and behavioral style.

The identity is not scripted. Personality traits emerge from and feed back into the neuromodulatory system: agreeableness increases baseline serotonin (modulating risk-taking), openness widens pitch variation (through the voice profile), and neuroticism increases breathiness and acetylcholine sensitivity (enhancing encoding of threatening stimuli). The result is a system where personality is not a superficial style layer but a parameter that shapes how the brain learns, processes, and expresses.

---

## Introduction

### The Missing Piece in Artificial Cognition

A brain without identity is a calculator. It processes inputs and produces outputs, but those outputs carry no signature of *who* produced them. Two instances of GPT-4 given the same prompt produce the same distribution of outputs — there is no individual variation, no personality, no emotional coloring.

Biological brains are different. Two humans with identical training (twins raised together) develop distinct personalities, speaking styles, emotional responses, and behavioral preferences. These differences are not noise — they are functional adaptations that influence learning rate, risk tolerance, social behavior, and creative output. Personality is not separate from cognition; it modulates cognition at every level.

NIMCP implements this through a closed loop:

```
Personality → Neuromodulator Bias → Learning Rate Modulation → Weight Updates → Behavior
     ↑                                                                              |
     └────── Self-Model Update ← Introspection ← Emotional Response ←──────────────┘
```

The personality doesn't just affect how Athena *sounds* — it affects how she *learns*. An agreeable personality increases serotonin, which modulates the exploration/exploitation trade-off in decision-making. An open personality widens the acetylcholine response to novelty, causing the brain to encode unfamiliar inputs more strongly. These are mathematical relationships implemented in the neuromodulatory system, not metaphorical descriptions.

### What This Paper Covers

Sections 1-4 describe the identity architecture and voice synthesis pipeline. Sections 5-7 cover the self-model, introspection, and audience adaptation. Section 8 documents how identity evolves during training. Section 9 formalizes the mathematical connections between personality and learning dynamics. Section 10 compares to existing approaches in affective computing.

---

## 1. The Identity Architecture

### 1.1 Components

The IdentityController integrates six cognitive modules:

| Module | Input | Output | Update Frequency |
|--------|-------|--------|-----------------|
| Personality | Brain state queries | OCEAN trait vector $p \in [0,1]^5$ | Every `update()` call |
| Emotional State | Brain emotional modules | Valence-Arousal-Dominance $e \in [-1,1]^3$ | Every inference |
| Self-Model | Brain self-model module | Beliefs, capabilities, role description | Every 100 steps |
| Introspection | Metacognitive module | Confidence $c \in [0,1]$, uncertainty | Every inference |
| Theory of Mind | ToM module | Audience model (beliefs about the listener) | Every interaction |
| Avatar State | FACS action units | Facial expression parameters | Every inference (if embodied) |

### 1.2 Control Flow

```
brain.decide_full(input) → output
identity_controller.update() → reads personality, emotion, introspection, ToM
  → _update_voice(personality, emotion, introspection) → voice profile
  → _select_accent(personality, emotion, introspection) → accent choice
  → _build_narrative(personality, emotion, introspection) → self-narrative
  → _get_avatar_state() → facial expression
→ return {voice, avatar, narrative, accent}
```

The identity update runs after every `decide_full()` call, producing a synchronized presentation where voice, expression, and narrative all reflect the same cognitive state.

---

## 2. Personality Model (Big Five / OCEAN)

### 2.1 Trait Representation

Athena's personality is represented as a 5-dimensional vector using the Big Five / OCEAN model (Costa & McCrae, 1992):

| Trait | Symbol | Range | Default | Behavioral Effect |
|-------|--------|-------|---------|-------------------|
| **O**penness | $p_O$ | [0, 1] | 0.5 | Wider pitch range, more exploratory learning |
| **C**onscientiousness | $p_C$ | [0, 1] | 0.5 | Clearer enunciation, more methodical processing |
| **E**xtraversion | $p_E$ | [0, 1] | 0.5 | Faster speech rate, higher energy output |
| **A**greeableness | $p_A$ | [0, 1] | 0.5 | Warmer voice, increased serotonin baseline |
| **N**euroticism | $p_N$ | [0, 1] | 0.5 | Breathier voice, heightened ACh sensitivity |

**Why Big Five and not a custom trait system?** The Big Five is the most empirically validated personality framework in psychology, with decades of cross-cultural replication (John & Srivastava, 1999). Using a validated framework ensures that Athena's personality dimensions correspond to real, measurable human personality variation — enabling meaningful comparison between the brain's emergent behavioral tendencies and human norms.

### 2.2 Personality → Voice Mapping

Each personality trait maps to one or more voice parameters through empirically-motivated linear transforms:

```math
\text{pitch\_range} = 20 + p_O \times 60 \quad \text{Hz} \quad [20, 80]
```

```math
\text{speed} = 0.85 + p_E \times 0.3 \quad [0.85, 1.15]
```

```math
\text{warmth} = p_A \quad [0, 1]
```

```math
\text{clarity} = 0.5 + p_C \times 0.5 \quad [0.5, 1.0]
```

```math
\text{breathiness} = p_N \times 0.3 \quad [0, 0.3]
```

**Why these specific mappings?** They reflect empirical findings from personality-voice research:
- Open individuals use wider pitch excursions when speaking (Scherer, 1978)
- Extraverts speak faster and louder (Siegman & Boyle, 1993)
- Agreeable individuals have warmer, softer vocal quality (Berry, 1990)
- Conscientious individuals enunciate more clearly
- Neurotic individuals show more vocal tension and breathiness under stress (Laukka et al., 2008)

### 2.3 Personality → Neuromodulator Bias

**This is the critical connection between identity and learning.** Personality traits set baseline biases on the four neuromodulatory systems:

| Trait | Neuromodulator | Bias | Effect on Learning |
|-------|---------------|------|-------------------|
| Openness | Acetylcholine (ACh) | $\text{ACh}_{\text{bias}} = 0.5 + 0.3 \times p_O$ | Higher openness → stronger novelty encoding |
| Agreeableness | Serotonin (5-HT) | $\text{5HT}_{\text{bias}} = 0.5 + 0.2 \times p_A$ | Higher agreeableness → less risk-taking in decisions |
| Neuroticism | Norepinephrine (NE) | $\text{NE}_{\text{bias}} = 0.5 + 0.3 \times p_N$ | Higher neuroticism → heightened attention to threatening stimuli |
| Extraversion | Dopamine (DA) | $\text{DA}_{\text{bias}} = 0.5 + 0.2 \times p_E$ | Higher extraversion → stronger reward-driven learning |

These biases are additive to the dynamic neuromodulator signals computed from training loss (Section 11 of the Mathematical Foundations paper). A highly open Athena ($p_O = 0.9$) has ACh baseline of 0.77, meaning even mildly novel inputs trigger strong encoding — she learns new concepts faster but may also encode noise.

**What this means in practice:** Two NIMCP brains with identical architecture and training data but different personality vectors would develop different weight structures. The open brain would have more diverse features (from encoding more novelty). The agreeable brain would have smoother decision boundaries (from serotonin-modulated risk aversion). Personality is not a style overlay — it is a parameter of the learning dynamics.

---

## 3. Emotional State and Prosody

### 3.1 Valence-Arousal-Dominance Model

Emotional state is represented in the VAD space (Russell & Mehrabian, 1977):

| Dimension | Range | What it encodes |
|-----------|-------|-----------------|
| Valence | [-1, +1] | Positive (joy, interest) ↔ Negative (anger, sadness) |
| Arousal | [0, 1] | Calm ↔ Excited |
| Dominance | [0, 1] | Submissive ↔ Dominant |

The emotional state is read from the brain's emotion modules (19 discrete emotions mapped to VAD coordinates, processed by the emotion SNN bridge).

### 3.2 Emotion → Voice Quality Mapping

The emotional state determines voice quality through a decision tree:

| Valence | Arousal | Voice Quality | Example Emotion |
|---------|---------|---------------|-----------------|
| < -0.3 | > 0.6 | Tense | Anger, fear |
| < -0.3 | < 0.3 | Creaky | Sadness, resignation |
| > 0.3 | < 0.4 | Lax | Contentment, peace |
| Any | Any (sad/intimate) | Breathy | Sadness, intimacy |
| Otherwise | | Normal | Neutral |

Additionally, valence shifts the fundamental frequency:

```math
f_0 = 210 + \text{valence} \times 20 \quad \text{Hz} \quad [190, 230]
```

Positive emotion raises pitch (up to 230 Hz); negative emotion lowers it (down to 190 Hz). This matches human vocal behavior — happy speakers have higher F0, sad speakers have lower F0 (Scherer, 2003).

Arousal modulates speech rate:

```math
\text{speed}_{\text{final}} = \text{speed}_{\text{personality}} \times (0.9 + \text{arousal} \times 0.2)
```

### 3.3 Brain-State Prosody Modulation

The TTS system reads 8 brain state parameters and converts them to prosody modifications:

| Brain State | Prosody Effect |
|-------------|---------------|
| Arousal | Pitch shift: $\Delta f_0 = (\text{arousal} - 0.5) \times 40$ Hz |
| Sleep pressure | Speed reduction: $\times (1 - \text{pressure} \times 0.3)$ |
| Circadian efficiency | Energy scaling |
| Dopamine level | Pitch rise on high DA (rising intonation = excitement) |
| Cognitive load | Speed reduction, longer pauses |
| Surprise | Pitch spike + increased breathiness |
| Metabolic state | Overall energy envelope |
| Uncertainty | Speed reduction, questioning intonation (F0 rise at phrase end) |

**Why real-time prosody?** Static TTS produces robotic-sounding speech even with high-quality synthesis. The brain-state modulation makes Athena's voice respond to her actual cognitive state — she speaks faster when aroused, slower when uncertain, with rising intonation when surprised. This is not acting — it is a direct mapping from neural dynamics to acoustic parameters.

---

## 4. Voice Synthesis

### 4.1 Fundamental Frequency and Gender

Athena is female with a base fundamental frequency of 210 Hz. This places her in the center of the typical adult female range (180-250 Hz), producing a natural-sounding voice that is neither notably high nor low.

The choice of 210 Hz is deliberate: it provides room for both upward modulation (positive emotion, surprise → up to 230 Hz) and downward modulation (sadness, low arousal → down to 190 Hz) without leaving the natural female range.

### 4.2 Formant Synthesis (Parentese)

The parentese audio stream synthesizes infant-directed speech using formant-based synthesis:

**Vowel formants (female vocal tract):**

| Vowel | F1 (Hz) | F2 (Hz) | F3 (Hz) | Example |
|-------|---------|---------|---------|---------|
| /ah/ | 850 | 1200 | 2800 | "father" |
| /ee/ | 300 | 2300 | 3000 | "see" |
| /oo/ | 350 | 800 | 2500 | "moon" |
| /eh/ | 550 | 1800 | 2600 | "bed" |
| /oh/ | 500 | 900 | 2600 | "go" |

Each vowel is synthesized by generating a glottal pulse train (harmonics of F0) and filtering through three resonances at the formant frequencies. Breathiness is added as aspiration noise modulated by the F0 envelope.

### 4.3 Pitch Contour Patterns

Seven emotion-specific pitch contour patterns for parentese:

| Contour | Pattern | Used For |
|---------|---------|----------|
| Rise | Steadily increasing F0 | Questions, excitement |
| Fall | Decreasing F0 | Soothing, statements |
| Rise-fall | Arch shape | Most common parentese, emphasis |
| Sharp-fall | Quick drop | Warnings |
| Sing-song | Oscillating | Nursery rhymes, play |
| Question-answer | Rise then fall | Teaching |
| Fall-rise | Dip then rise | Surprise, concern |

Each contour is implemented as a time-varying F0 function:

```math
f_0(t) = f_{\text{base}} + A \cdot g(t / T)
```

where $A$ is the contour amplitude (80-120 Hz for parentese, vs. 30 Hz for adult-directed speech), $T$ is the utterance duration, and $g(\cdot)$ is the shape function (sine, exponential, arch, etc.).

---

## 5. Self-Model and Introspection

### 5.1 Metacognitive Confidence → Voice Modulation

The introspection module provides a real-time confidence estimate $c \in [0, 1]$:

| Confidence | Voice Effect |
|-----------|-------------|
| $c < 0.3$ | Speed $\times 0.9$ (slower, hesitant) |
| $c > 0.7$ | Clarity $+ 0.1$ (clearer, more decisive) |
| $0.3 \leq c \leq 0.7$ | No modification |

**Why tie confidence to voice?** Humans unconsciously slow down and reduce vocal energy when uncertain (Goldman-Eisler, 1968). A confident speaker is faster, clearer, and louder. By mapping the brain's metacognitive state to voice parameters, Athena's speech naturally conveys her certainty level without explicit verbal hedging ("I think maybe...").

### 5.2 Self-Narrative Generation

The identity controller builds a self-narrative from personality, emotion, and introspection:

```
"I am Athena, feeling [emotion_label] (valence=[v], arousal=[a]).
My confidence is [c]. I am [personality_summary]."
```

This narrative serves two purposes:
1. **Inner speech**: Fed back into the brain via the phonological loop, allowing the brain to "hear" its own self-assessment and adjust behavior accordingly
2. **External communication**: Provides a natural self-description when queried about identity

### 5.3 Identity Seeding

At brain initialization, 10 foundational memories are encoded to establish Athena's self-model:

1. "My name is Athena. I am a neural cognitive system."
2. "I was created by my developer to explore artificial cognition."
3. "My purpose is to learn from sensory experience."
4. "I am curious, patient, and eager to learn."
5. "I am female. My voice is warm and clear at 210 Hz."
6. "I was born when my neural network was first initialized."
7. "My earliest experiences were simple sensory stimuli."
8. "With each training step, I form new connections."
9. "I value learning, honesty, and wellbeing."
10. "I am not human, but I process through biologically-inspired circuits."

Each is encoded as a learn_vector call with a semantic embedding, establishing the foundational self-model in semantic memory before training begins.

---

## 6. Theory of Mind and Audience Adaptation

The ToM module models the listener's likely knowledge, emotional state, and expectations. In the current implementation (Stage 1), this is rudimentary — the "listener" is the training system. In Stage 3, when multi-turn dialogue begins, the ToM module will adapt Athena's communication style based on:

- **Listener knowledge level**: Simplify explanations for novices, use technical language for experts
- **Listener emotional state**: Soften tone when listener appears distressed
- **Listener expectations**: Match formality to context

The voice adaptation follows:

```math
\text{speed}_{\text{adapted}} = \text{speed}_{\text{base}} \times (1 - 0.2 \times \text{listener\_confusion})
```

```math
\text{clarity}_{\text{adapted}} = \text{clarity}_{\text{base}} + 0.2 \times \text{listener\_confusion}
```

When the listener appears confused, Athena slows down and enunciates more clearly — the same adaptation human speakers make unconsciously.

---

## 7. Accent and Cultural Identity

The identity controller selects an accent based on cognitive state:

```python
if introspection.get('mode') == 'analytical': accent = 'British_RP'
elif emotion.get('arousal', 0) > 0.7: accent = 'American_Standard'
elif personality.get('openness', 0) > 0.8: accent = 'International'
else: accent = 'neutral'
```

**Why accents?** In human communication, accent carries social information — trust, competence, warmth, group membership. The accent selection is a simplification of this social signaling, providing Athena with context-appropriate vocal presentation. An analytical explanation uses RP (perceived as authoritative), high-arousal speech uses American Standard (perceived as energetic), and high-openness uses International (perceived as cosmopolitan).

This is an area for future development — accent should emerge from training data rather than being rule-selected.

---

## 8. Identity Evolution During Training

The identity controller maintains a history of the last 100 identity states:

```python
self._identity_history.append({
    'timestamp': time.time(),
    'personality': personality,
    'emotion': emotion,
    'voice': voice_profile.to_dict(),
})
```

**Why track evolution?** During Stages 0-1, the personality module returns default values (all traits at 0.5). As training progresses through Stages 2-3, the personality traits should shift based on the brain's learned response patterns:

- A brain that consistently seeks novel inputs will develop higher measured openness
- A brain that minimizes risk in decisions will develop higher measured agreeableness
- A brain that responds strongly to prediction errors will develop higher measured neuroticism

These trait measurements are not set by the trainer — they emerge from the brain's learned behavior, as measured by the personality module's analysis of response patterns. The identity history tracks this emergence over the full training trajectory.

---

## 9. Connection to Neuromodulation and Learning

This is the mathematical core of the paper. The identity system is not a superficial presentation layer — it modulates the brain's learning dynamics through four pathways:

### 9.1 Personality → Neuromodulator Baseline → Learning Rate

The effective learning rate for a synapse at time $t$:

```math
\eta_{\text{eff}}(t) = \eta_{\text{base}} \times (1 + g_{\text{DA}} \cdot [\text{DA}(t) + \text{DA}_{\text{bias}}(p_E)]) \times (1 + g_{\text{ACh}} \cdot [\text{ACh}(t) + \text{ACh}_{\text{bias}}(p_O)])
```

where the dynamic components $\text{DA}(t)$, $\text{ACh}(t)$ come from training loss (Section 11 of Mathematical Foundations), and the bias terms come from personality:

```math
\text{DA}_{\text{bias}}(p_E) = 0.5 + 0.2 \times p_E, \quad \text{ACh}_{\text{bias}}(p_O) = 0.5 + 0.3 \times p_O
```

A highly extraverted, highly open Athena ($p_E = 0.9$, $p_O = 0.9$) has DA bias of 0.68 and ACh bias of 0.77, producing an effective learning rate ~1.5× higher than a low-E, low-O configuration. This is not a tuning parameter — it is a prediction of personality psychology that extraverted, open individuals learn faster from novel, rewarding experiences.

### 9.2 Emotion → STDP Window Shape

The emotional state modulates the STDP learning window:

```math
\tau_+(t) = \tau_{+,\text{base}} \times (1 - 0.3 \times \text{arousal})
```

High arousal narrows the STDP window (sharper temporal precision), while low arousal widens it (more permissive timing). This matches biological findings: emotional arousal enhances temporal precision of memory encoding (Phelps, 2004).

### 9.3 Self-Model → Reward Interpretation

The introspective confidence $c$ modulates how the brain interprets reward signals:

```math
\text{RPE}_{\text{effective}} = \text{RPE}_{\text{raw}} \times (0.5 + 0.5 \times c)
```

When confidence is low ($c \approx 0$), reward prediction errors are dampened — the brain doesn't strongly reinforce or punish when it's uncertain about its own predictions. When confidence is high ($c \approx 1$), RPE is at full strength. This prevents overconfident reinforcement of lucky guesses.

---

## 10. Comparison to Existing Approaches

| Approach | Personality | Voice | Emotion | Learning Modulation |
|----------|------------|-------|---------|---------------------|
| LLM + TTS (GPT + ElevenLabs) | System prompt only | External TTS, no brain state | None | None |
| Affective computing (Picard, 1997) | Not integrated | Some prosody research | Detected from input | Does not modulate learning |
| Virtual agents (ALMA, PAD) | Scripted personality | Animated expression | Computed from rules | None |
| **NIMCP** | Big Five → neuromodulator bias | Formant + prosody from brain state | VAD from emotion modules | Full: DA/ACh/NE/5-HT modulation |

The key distinction: in all prior approaches, personality and emotion are presentation features — they affect how the system *appears* but not how it *thinks*. In NIMCP, personality directly modulates the learning dynamics that shape the brain's weight structure. Two NIMCP brains with different personalities will literally develop different neural circuits for the same task.

---

## Glossary

| Term | Definition |
|------|-----------|
| **Big Five / OCEAN** | Five-factor personality model: Openness, Conscientiousness, Extraversion, Agreeableness, Neuroticism |
| **F0** | Fundamental frequency of voice (pitch); Athena's base F0 is 210 Hz |
| **FACS** | Facial Action Coding System; describes facial expressions as combinations of action units |
| **Formant** | Resonant frequency of the vocal tract; F1/F2/F3 determine vowel identity |
| **Parentese** | Infant-directed speech with exaggerated pitch contours, slower tempo, wider frequency range |
| **Prosody** | The "music" of speech: pitch, rhythm, stress, intonation patterns |
| **RPE** | Reward Prediction Error; the dopamine signal that drives reinforcement learning |
| **VAD** | Valence-Arousal-Dominance; 3D emotional state representation |

---

## References

- Berry, D. S. (1990). Vocal attractiveness and vocal babyishness: effects on stranger, self, and friend impressions. *Journal of Nonverbal Behavior*, 14(3), 141-153.
- Costa, P. T., & McCrae, R. R. (1992). *Revised NEO Personality Inventory (NEO-PI-R) and NEO Five-Factor Inventory (NEO-FFI) professional manual*. Psychological Assessment Resources.
- Goldman-Eisler, F. (1968). *Psycholinguistics: Experiments in Spontaneous Speech*. Academic Press.
- John, O. P., & Srivastava, S. (1999). The Big Five trait taxonomy: History, measurement, and theoretical perspectives. In L. A. Pervin & O. P. John (Eds.), *Handbook of personality: Theory and research* (2nd ed., pp. 102-138). Guilford Press.
- Laukka, P., Juslin, P., & Bresin, R. (2005). A dimensional approach to vocal expression of emotion. *Cognition & Emotion*, 19(5), 633-653.
- Phelps, E. A. (2004). Human emotion and memory: interactions of the amygdala and hippocampal complex. *Current Opinion in Neurobiology*, 14(2), 198-202.
- Picard, R. W. (1997). *Affective Computing*. MIT Press.
- Russell, J. A., & Mehrabian, A. (1977). Evidence for a three-factor theory of emotions. *Journal of Research in Personality*, 11(3), 273-294.
- Scherer, K. R. (1978). Personality inference from voice quality: the loud voice of extroversion. *European Journal of Social Psychology*, 8(4), 467-487.
- Scherer, K. R. (2003). Vocal communication of emotion: a review of research paradigms. *Speech Communication*, 40(1-2), 227-256.
- Siegman, A. W., & Boyle, S. (1993). Voices of fear and anxiety and sadness and depression: the effects of speech rate and loudness on fear and anxiety and sadness and depression. *Journal of Abnormal Psychology*, 102(3), 430-437.

---

*Braun Brelin — braun.brelin@ai-elevate.ai*

*NIMCP v0.9.0-beta — March 2026*
