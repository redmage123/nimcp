# Cognitive Modules Async Decoupling Analysis

## Current Coupling Patterns Identified

### 1. **Global Workspace - Direct Synchronous Coupling**
**Location**: `src/cognitive/global_workspace/nimcp_global_workspace.c`

**Current Pattern**:
- Modules call `global_workspace_compete()` synchronously
- Competition resolves immediately via `resolve_winner_take_all()`, `resolve_priority_based()`, etc.
- Subscribers poll via `global_workspace_read_broadcast()` - tight coupling

**Issues**:
- Synchronous competition blocks calling module
- No async notification to subscribers
- Direct function calls create compile-time dependencies
- Winner-take-all happens immediately without allowing batched submissions

**Async Solution**:
- Submit broadcasts via **DOPAMINE channel** (reward/completion semantics)
- Subscribers register bio-futures to wait for broadcasts
- Use **GAMMA oscillation** phase sync for consciousness binding
- Global workspace becomes async message hub

---

### 2. **Memory Systems - Direct Read/Write Coupling**
**Modules**: `working_memory/`, `memory/` (episodic/semantic)

**Current Pattern**:
- Synchronous `memory_store()`, `memory_retrieve()` calls
- Direct pointer passing between modules
- No async query support

**Issues**:
- Memory access blocks calling thread
- No support for concurrent queries
- Tight coupling between memory consumers and providers

**Async Solution**:
- Memory queries return **bio-futures** (ACETYLCHOLINE channel for fast retrieval)
- Memory consolidation uses **DELTA/THETA oscillations** (slow background process)
- Storage operations via **SEROTONIN channel** (sustained state changes)

---

### 3. **Emotional Systems - Direct State Access**
**Modules**: `emotions/`, `emotional_tagging/`, `emotion_recognition/`

**Current Pattern**:
- Direct emotional state variable access
- Synchronous emotion updates
- No event-driven emotion changes

**Issues**:
- Emotional state changes don't propagate asynchronously
- No neuromodulator-based signaling (should use DA, 5-HT, NE naturally!)
- Missing biological realism in affect

dynamics

**Async Solution**:
- **DOPAMINE**: Positive valence emotions (joy, excitement)
- **SEROTONIN**: Mood regulation, wellbeing
- **NOREPINEPHRINE**: Arousal, alertness, stress
- Emotional tags use predictive coding callbacks (surprise-driven)

---

### 4. **Ethics/Executive - Synchronous Decision Making**
**Modules**: `ethics/`, `executive/`

**Current Pattern**:
- Synchronous ethical reasoning
- Direct executive function calls
- No async deliberation

**Issues**:
- Ethical decisions block execution
- No support for deliberative vs reactive ethics
- Executive control tightly coupled to modules

**Async Solution**:
- Ethics queries return futures with confidence decay
- Executive uses **BETA oscillations** for working memory coordination
- Priority-based competition via NOREPINEPHRINE levels

---

### 5. **Knowledge/Introspection - Synchronous Queries**
**Modules**: `knowledge/`, `introspection/`, `curiosity/`

**Current Pattern**:
- Synchronous knowledge lookups
- Direct introspection queries
- Curiosity signals synchronously

**Issues**:
- Knowledge retrieval blocks processing
- Introspection requires complete system scan
- Curiosity drive not event-driven

**Async Solution**:
- Knowledge queries via ACETYLCHOLINE (attention-driven retrieval)
- Introspection uses glial waves (slow system-wide scan)
- Curiosity signals via DOPAMINE (exploration reward)

---

### 6. **Mirror Neurons/Theory of Mind - Direct Empathy**
**Modules**: `mirror_neurons/`, `theory_of_mind/`, `empathetic_response/`

**Current Pattern**:
- Synchronous mirroring
- Direct ToM inference
- Immediate empathy response

**Issues**:
- Social cognition should be gradual, not instant
- No resonance dynamics
- Missing temporal aspects of understanding others

**Async Solution**:
- Mirror resonance via **phase coupling** (GAMMA for binding)
- ToM inference returns futures with decay
- Empathy builds over time via SEROTONIN modulation

---

### 7. **Consolidation/Predictive - Batch Processing**
**Modules**: `consolidation/`, `predictive/`, `meta_learning/`

**Current Pattern**:
- Synchronous consolidation
- Immediate prediction error handling
- No background learning

**Issues**:
- Consolidation should be slow background process
- Prediction errors should trigger callbacks, not block
- Meta-learning needs temporal integration

**Async Solution**:
- Consolidation via **DELTA oscillations** (sleep-like states)
- Prediction errors use predictive coding callbacks
- Meta-learning uses **THETA oscillations** (sequence learning)

---

## Async Integration Strategy

### Phase 1: Global Workspace Async Hub
Convert global workspace to async broadcast system using bio-async.

### Phase 2: Neuromodulator-Based Emotions
Map emotional systems to appropriate neuromodulator channels.

### Phase 3: Memory with Futures
Add async query support to all memory systems.

### Phase 4: Social Cognition Phase Sync
Use oscillation coupling for mirror neurons and ToM.

### Phase 5: Background Consolidation
Slow-timescale processes via DELTA/THETA oscillations.

---

## Expected Benefits

1. **Decoupling**: Modules communicate via messages, not direct calls
2. **Biological Realism**: Neuromodulator semantics match brain function
3. **Concurrency**: Multiple async operations in flight
4. **Temporal Dynamics**: Confidence decay, phase synchronization
5. **Event-Driven**: Callbacks fire on prediction errors, not polling
6. **Scalability**: Modules can be added without recompiling others

---

## Implementation Priority

1. Global workspace (affects all modules)
2. Emotions (natural neuromodulator mapping)
3. Memory (high-frequency operations)
4. Ethics/Executive (decision-making)
5. Social cognition (complex interactions)
6. Learning modules (background processes)

