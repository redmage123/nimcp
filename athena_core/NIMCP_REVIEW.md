# nimcp codebase review, applied to Athena's learning objectives

Date: 2026-11-22. Nine parallel read-only reviews covered all ~2,800 C/CUDA files of
/home/bbrelin/nimcp. The central claims were re-checked by hand; those are marked
**(verified)**. Detailed per-area notes with file:line citations are kept in the
session scratchpad (review/01..09); the key citations are repeated here.

The yardstick is THEORY.md: speech comprehension and generation on par with frontier
LLMs, using biological algorithms, with the SNN as the learner and other networks in
support. The four algorithms: (1) predict the next input, (2) local learning with a
per-neuron error, (3) fast memory plus replay, (4) grounding plus communicative reward.

## 1. Headline findings

1. **The SNN never receives the correct answer.**
   - Above 100K neurons the SNN is forced onto global-reward R-STDP
     (training_dispatch.c:28, 130-140).
   - Its reward is 0.8 x firing-rate match plus 0.2 x 1/(1+loss), where the loss is
     the ANN's, blended with the UTM's (nimcp_brain_learning.c:3105-3114).
   - The update is lr x (r - baseline) x pre x post on every synapse
     (snn_training.c:1000-1026).
   - The target is used only for a reported MSE.
   - Training runs one 1 ms SNN step per example. Decide runs a 100 ms window.
     Input scaling differs by about 45x between the two.
2. **No algorithm-2 rule works anywhere.**
   - e-prop: its eligibility only decays and never accumulates
     (snn_training.c:1189) **(verified)**. It is called with learning_signal = 0
     (training_dispatch.c:548). It writes the wrong network.
   - Predictive coding: error = input - prediction, then W -= lr x err x mu^T
     (plasticity/predictive/nimcp_predictive_coding.c:256, 493), which is gradient
     *ascent* **(verified)**. Its bottom level never computes an error, and it gets
     no bottom-up input.
   - The surrogate-gradient apply is a stub (snn_training.c:1172).
   - Plasticity coordinator: 8 of its 9 "registered" mechanisms have a NULL update
     function.
3. **The cochlea has never processed sound** **(verified)**.
   - basilar_membrane_process (nimcp_basilar_membrane.c:154-166) and
     hair_cell_bank_process (nimcp_hair_cells.c:173-183) only increment counters.
   - The auditory nerve always gets drive 0. 0.60x50 + 0.25x5 + 0.15x0.1 =
     31.265 sp/s, the value measured on every channel.
   - Spike trains are never allocated: the encoding defaults to FIRING_RATE.
   - 15 cochlea bridges are ticked every decide on this constant output.
4. **No working next-word prediction exists.**
   - snn_language_bridge_encode_word returns zeros (Slice A stub) **(verified)**.
     So bigram/trigram/next_token learning skips every pair.
   - Phase-3 projection training and trigram learning run only on the sequential
     path. The thread pool is always created, so neither runs in production;
     CLAUDE.md's claim is wrong.
   - Words are lexicon vectors learned from co-occurrence. Production picks the
     nearest words in that lexicon.
5. **No vision front end exists.** nimcp_retina.c is 32 lines of includes, and its
   header says "NO STUBS"; the real code is in an uncompiled .bak.
6. **Most cognitive modules are hand-coded or statues, and several cost compute
   on every step.**
   - Hand-coded confidence rewriters contaminate confidence: reasoning (30%
     blend), recursive cognition, FEP and the world prior.
   - Decide trains on its own predicted labels.
7. **The best biological assets exist but are never run.**
   - A modern Hopfield memory.
   - HTM temporal memory (a local next-input predictor).
   - Apical/basal two-compartment neurons.
   - L5 dendritic burst neurons.
   - Triplet STDP.
   - Ternary and sparse GPU kernels.
   - Mirror vicarious reward plus word-action binding.

## 2. What actually learns in nimcp today

| Learner | Rule | Signal | Admissible under THEORY? |
|---|---|---|---|
| ANN (adaptive network) | GPU backprop (default mode cannot fit 10 items; clip mode works) | target | no |
| cortex CNNs, LNN, VAE, SNN-FNO, language generator | backprop / Adam / adjoint | target | no |
| SNN (1.8M neurons, CSR) | R-STDP, global scalar reward | firing rate + ANN loss | no (global reward only) |
| SNN homeostatic scaling | multiplicative scaling | firing rate | yes (stabiliser) |
| grounded_language lexicon | Hebbian co-occurrence + negative sampling | text statistics | local, but not prediction |
| cortical-column projection | delta rule `row += lr*fv*err` (brain_learning.c:1860) | target, per output | yes (one layer, not the SNN) |
| FEP-parietal, physics NN | delta rules on feature/target slices | target or own output | local, but useless pairs |
| speech_units | online k-means (EMA centroids) | none (unsupervised) | local |
| episodic replay | replays through brain_learn_vector | stored tuples | trains almost nothing (threshold) |

## 3. Assets to reuse, mapped to the four algorithms

| THEORY role | nimcp asset | State | What it needs |
|---|---|---|---|
| Front end, audio | cochlea: basilar membrane, hair cells, auditory nerve (src/lib/perception) | stages 1-2 are stubs | gammatone BM at the real sample rate; IHC rectify/compress/low-pass with 94 dB at amplitude 1; per-fiber-type drive; spike-train encoding |
| Front end, vision | visual_cortex Gabor bank (fixed V1); snn_visual_bridge Poisson encoder | no retina | build ON/OFF and change channels (frame difference), or revive retina.c.bak |
| Alg 1: predict next input | dendritic_sequence.c (HTM temporal memory, local permanence learning :287-345) | statue | step it in the loop; feed it SNN codes |
| Alg 1/2: predictive coding | plasticity/predictive/nimcp_predictive_coding.c | sign inverted, level 0 dead, no bottom-up path | fix the signs, the bottom error and the bottom-up term; spiking port |
| Alg 2 substrate | Wave H two-compartment (snn_network.c:1471-1511); cortical_dendritic.c (L5 burst :519-532); two_compartment.c | no learning rule; off by default | top-down error into apical; plateau/burst-gated plasticity (Payeur 2021) |
| Alg 2 rule | e-prop skeleton (snn_training.c:653, 1180-1236) | no-op | accumulate pre-trace x post surrogate per CSR synapse; per-neuron signal B x (y - target) |
| Alg 2 helpers | triplet STDP (nimcp_triplet_stdp.c:344); spine STDP traces (nimcp_dendrite.c:1798-1900); homeostatic scaling (snn_training.c:1339) | standalone / live | use as eligibility traces and as the stabiliser |
| Alg 3: fast memory | hopfield_memory.c (modern Hopfield, one-shot store :431, retrieve :633, top-k :822, GPU) | statue | instantiate; store key\|\|value |
| Alg 3: replay | episodic_replay.c (the only replay wired to a learner); temporal_replay.c (prioritised sum tree) | threshold blocks replay; consumer is a placeholder | importance from surprise, not loss > 6; replay into the SNN only |
| Alg 3: surprise | loss/EMA gate (brain_learning.c:3654-3665); omni world-model prediction error | live | choose what to store |
| Alg 4: communicative reward | cascade self-comprehension reward (cascade_wernicke.c:278-310; cascade.c:3249-3289); mirror vicarious_reward RPE (:483-493); mirror_language_bridge Hebbian word-action binding (:1372); external reward (part_core.c:5578) | reward computed but plasticity no-op; mirror modules never created | route the reward into the SNN learning signal |
| Priors and environments | intuitive physics, scene graph, entity tracker; number sense (log number line, Weber); spatial transforms | empty world (one ground plane) / statues | use as data generators and population-code encoders, not as confidence rewriters |
| Efficiency | sparse ternary matvec (ternary_kernels.cu:522; check the possible out-of-bounds at :300); CSR STDP kernel (plasticity_kernels.cu:1883; make it pair-based); cuSPARSE spmm; thread pool, lock-free queues, per-thread RNG; pink noise | uncalled | ternary SNN weights; on-device plasticity; batched steps |

## 4. Bugs and hazards, by severity

**Corrupt learning or measurement**
- The SNN never sees the target (section 1.1).
- Predictive-coding sign inversion; e-prop no-op; surrogate stub.
- The cochlea feeds constant output to 15 bridges, so any cochlea-derived metric is meaningless.
- Decide trains on its own labels: knowledge, FEP-parietal, physics NN and
  grounded_language on `decision->label` (part_core.c:4190-4265). This is
  self-reinforcement.
- Confidence is rewritten by hand-coded modules (reasoning 30%, rcog, FEP, world
  prior). Metrics built on confidence are contaminated.
- Engram recall cue is the constant index list 0..N-1 (brain_learning.c:2196,
  5005), so the same engram wins every step. Systems consolidation builds
  features from engram_id % 100.
- Sleep replay replays almost nothing: importance needs loss > 6
  (episodic_replay.c:89-118, 403). Grounded-language episodes are stored with a
  NULL target, so they are rejected.
- Train and decide differ in input scaling (about 45x) and in SNN window (1 ms vs
  100 ms).
- The language bridge's word populations hash-collide (brain_learning.c:414).
- Cerebellar LTD has no per-parallel-fiber activity term (cerebellum_adapter.c:1224).

**Stability and resources**
- Memory leak: the creative feedback buffer is never drained, and it holds dangling
  borrowed pointers (creative_training_bridge.c:870-910).
- concept_registry grows on every labelled step and is never read.
- wave13 KG nodes are named with time(NULL), adding about one per second per label.
- Possible out-of-bounds reads: physics NN reads 8 floats behind a `>= 4` guard
  (parietal.c:660); ternary packed matvec (ternary_kernels.cu:300).
- exit(42) on a GPU sync failure (snn_network.c:2978).
- LGSS fail-safe DENY silently drops every learn step if LGSS ever leaves
  ACTIVE/DEGRADED (lgss.c:419-431; brain_learning.c:1586-1594). Watch
  `lgss_training_blocks`.
- `brain_learn_example` rejects labels containing "; delete " / "; update " /
  "%s%s%s" (brain_learning.c:4556-4600). Transcript text could trip it.

**Statues that look live**
- Plasticity coordinator; 36 of 42 snn/bridges; gossip learning (stub
  apply_weight_delta, zero gradients); hemispheric brain; world-model trainer
  (brain=NULL); occipital_process; retina; Phase-3 projection; mirror learning;
  symbolic logic (never attached); world prior (connected to NULL).

## 5. Why a training step takes about 15 s (ranked; not yet timed)

1. **Decide simulates a 100 ms SNN window, about 100 full steps of the 1.8M-neuron
   SNN** (part_helpers.c:946-950; snn_network.c:5362).
2. **SQLite full scan per decide.** OOD detection does `ORDER BY importance LIMIT
   100000` with a cosine per row, and a serial engram fallback does it again
   (part_core.c:1720, 1750). This grows as training adds engrams.
3. **The GPU step is synchronous per population.** Host rebuild, allocate and
   upload each step (snn_network.c:2936-2939), sync and copy back (:2972-2988),
   then a serial host neuron loop (:3255-3300). The input kernel is pull-based, so
   its cost is all nonzeros whatever the spike rate.
4. **The daemon serialises every command under one lock** (brain_daemon.py:1228).
   The RW lock at :1139 is never acquired. There is no OpenMP anywhere in src/.
5. **Side work on every step:** inline training in decide; the tick chain and
   cochlea bridges run in both decide and learn; about 14 side modules run per
   learn; the R-STDP host loop spawns 32 threads per call.

Cuts:
- one "lean training" flag that gates the side modules and the inline decide
  training;
- reuse the learn-step SNN window instead of a second 100 ms decide forward;
- an in-RAM index instead of the SQLite scan during training;
- tick once per step;
- device-resident SNN step;
- the RW lock for read-only commands.

## 6. Applied to the training and learning objectives

### A. The biological learner (athena_core): port and build list, in order
1. **Fix the cochlea** in nimcp and expose a standalone emitter
   (`cochlea_emit`: PCM in, auditory-nerve spike counts out). It becomes the
   front end of the LibriSpeech stream, replacing log-mel frames. The user asked
   for this on 2026-11-22.
2. **Per-neuron-error SNN rule (algorithm 2)**, built in athena_core where it can be
   tested fast, then ported. Two candidates, compared on the ladder:
   - (a) e-prop on CSR with fixed random feedback B x (y - target);
   - (b) burst-dependent plasticity: top-down error into an apical compartment
     (Wave H style), plateau-gated.
   The repaired nimcp predictive-coding code is a third candidate for algorithm 1.
3. **Next-input prediction (algorithm 1)** on the cochlea stream, with the SNN as
   the predictor. HTM temporal memory (dendritic_sequence) is a reference local
   predictor to compare against.
4. **Fast memory (algorithm 3):** a port of nimcp's modern Hopfield memory as the
   one-shot store, plus surprise-gated storage and prioritised replay into the SNN
   only. The athena_core Hippocampus already matches logistic regression at 1-5
   shots on MNIST.
5. **Communicative reward (algorithm 4):** the cascade self-comprehension reward and
   mirror vicarious RPE as the dopamine term multiplying the SNN's eligibility, at
   rung 5.
6. **Efficiency, once a rule works:** ternary weights with a float shadow
   accumulator, on-device CSR plasticity, pink-noise membrane noise.
7. **Priors as data:** intuitive physics as a generator of falling and colliding
   object streams for video prediction; number sense and spatial transforms as
   fixed population codes.

### B. The live nimcp training run
The live run's task is a ceiling task (sentence embedding to rotated embedding), and
its SNN never sees the target, so further live training measures nothing new. Some
fixes still matter for nimcp itself:
- **Must fix regardless:**
  - the creative-buffer memory leak;
  - the out-of-bounds reads;
  - decide training on its own labels;
  - the episodic-replay threshold;
  - engram cue indexing.
  Watch `lgss_training_blocks`.
- **Speed:** a lean-training flag and the decide-window reuse are the two largest
  cuts. Measure first with NIMCP_DEBUG_TIMING=1 on the next approved restart.
- **To make nimcp's SNN the learner:**
  - replace R-STDP in training_dispatch with the per-neuron-error rule that wins
    in A.2;
  - present windows, not 1 ms steps;
  - use one input encoding for train and decide;
  - route the target, or the next input, to the SNN.

### C. Changes to THEORY.md (sent to the session that maintains it)
- Rung 2 and the stream: the audio front end is nimcp's cochlea, once fixed. The
  rung-2 static-clip result showed that words need time-shift-tolerant coding.
- Algorithm 3 has a ready reference implementation in nimcp: modern Hopfield
  (attention as associative retrieval).
- Algorithm 4 has a ready reward in nimcp: cascade self-comprehension, plus mirror
  RPE.
- Evidence section: nimcp's SNN is trained by global reward only (target never
  seen); predictive coding and e-prop in nimcp are broken, not tested-and-failed.

### D. Documentation claims found to be wrong
- CLAUDE.md: "brain_learn_vector drives snn_language_bridge_learn_pair" (Phase 3)
  and trigram learning. Both are dead in production.
- CLAUDE.md: "Glial ... wired into the forward-pass hot path". Glial modulation
  reaches the ANN only, not the SNN.
- nimcp_retina.c header: "NO STUBS. EVERY FUNCTION FULLY IMPLEMENTED". The file
  has no functions.
- Pink noise Python binding: possibly not registered in the built module (unverified).

## 7. Symbolic logic and LGSS (added 2026-11-22)

**Symbolic logic** (src/cognitive/logic/nimcp_symbolic_logic.c):
- What it does: `sscanf` fact extraction ("X is Y" -> is_a, "X has Y" ->
  has_property) and forward chaining. Hand-coded; it learns nothing.
- It is a **statue** **(verified)**. Extraction (brain_learning.c:1680) and inference
  are gated on `gl->logic_enabled`. That flag is set only by
  `brain_attach_symbolic_logic` (reasoning/nimcp_symbolic_logic_attachment.c:119),
  which has no callers. So the knowledge base never receives a fact.
- Possible later support roles:
  - consistency checking of generated utterances, as a signal next to the
    algorithm-4 reward;
  - fact extraction for grounding curricula.
  Leave it off during training.

**LGSS**:
- The core `lgss_evaluate` (src/security/lgss/nimcp_lgss.c) is live: once per learn
  step (training guard) and four times per decide (input, logic, action, motor).
  Its rule base is src/cognitive/symbolic_logic/ (the LGSS rule compiler and
  evaluator). Cost is microseconds; it is non-removable by design. Monitor
  `lgss_training_blocks`: a fail-safe DENY drops every learn step silently.
- The specialised guards have **zero external callers** (name match; calls through
  function pointers would be missed):
  - learning: stdp_guard, plasticity_constraints, training_guard,
    meta_learning_guard;
  - reward: vta_guard, reward_alignment, incentive_validator;
  - gates: speech, motor, autonomic.
- Plan rule: when a piece of the new learner is ported into nimcp, wire its LGSS
  guard in the same change:
  - the STDP guard and plasticity constraints with the per-neuron-error SNN rule;
  - the VTA guard and reward alignment with the communicative reward (algorithm 4
    is a gameable reward);
  - the speech gate with speech production.

## 8. What goes into the learner: merged plan (cognitive modules, regions, primitives, neuromodulators)

Sources: 12 more targeted reviews (cognitive modules in 6 slices; Broca/Wernicke;
basal ganglia, cerebellum and medulla; hippocampus and thalamus; neural
primitives; neuromodulators). Per-area notes with file:line are in
docs/nimcp_review_notes/.

**Overall verdict.** Across the cognitive modules, brain regions and primitives,
almost nothing in nimcp learns in a way that affects behaviour.
- **Hand-coded or statues:**
  - The hippocampus has no Hebbian weight update anywhere. Its CA3 patterns come
    from fresh random weights on every call.
  - Cerebellar LTD is always 0.
  - Basal-ganglia action selection never runs and gates nothing.
  - The thalamus is never stepped. The thalamic router queue is never drained, so
    every LNN/CNN output is dropped once it fills **(verified)**.
  - Wernicke receives no audio, and nothing reads Broca.
- **Neuromodulators:** only DA reaches a weight update, on the ANN only, as an
  unsigned gain. ACh, NE and 5-HT reach nothing. The SNN reads no neuromodulator.
- **The trainer's reward call is broken (verified).**
  `bg_update_reward(reward, rpe=reward)` passes a keyword to a positional (reward,
  expected) binding. It either raises and is silently caught, or gives an RPE of
  exactly 0 (immerse_athena.py:6015; nimcp_python.c ~7053).

What nimcp does supply:
- correct formulas and designs;
- a few live primitives;
- the right anatomy: Wernicke -> arcuate -> Broca; D1/D2 striatum; DG/CA3; TRN.

**Integration rule.** Every piece below is ported into athena_core (Python/torch)
as an algorithm. None is linked as C code. The learning signal is always the SNN's
own per-neuron prediction error (THEORY algorithms 1 and 2). Every other module
supplies one of the following, and none replaces the error:
- input structure;
- a gain on plasticity (neuromodulation);
- what to store and replay (memory);
- which candidate to emit (selection);
- the world it learns from (environment, referee).

### 8.1 Architecture

```
 world: LibriSpeech/video streams | "nursery" physics sim (+ narrator speech, scene-graph referee)
   │                                                      │
 cochlea (fixed; fix stubs) / retina (new: ON/OFF + change channels)
   │
 THALAMIC GATE  g_i = a_i·(1−TRN_i); TRN driven by the cortex's top-down prediction
   │            (predicted input suppressed), salience lowers TRN, burst = novelty amp
 WERNICKE (cortex SNN: ALIF, E/I, receptor τs, TM-STP, per-synapse delays, event-driven)
   │  predicts next frame/word; per-neuron error L_j via random feedback = e-prop (alg 1+2)
   ├─► HIPPOCAMPUS: DG kWTA (athena) + CA3 one-shot heteroassoc DG(t)->DG(t+1) + Hopfield
   │    softmax retrieval (= attention) -> CA1 context back into the SNN; surprise-gated
   │    storage; prioritised forward/reverse replay through e-prop; sleep-pressure scheduler
   ├─► ARCUATE: state forward; Broca efference copy back = imitation error (rung 5)
 BROCA (production SNN): per-neuron imitation error + DA third factor; synfire ring as
   │    phase/timing scaffold; breathing oscillator (~0.25 Hz) gates utterances
 BASAL GANGLIA: D1/D2 per candidate, dW_D1 = +lr·δ·e, dW_D2 = −lr·δ·e, critic V(s);
        GPi disinhibition selects which candidate the SNN emits (small action sets only)
 NEUROMODULATION (one writer each):
   DA  <- communicative critic δ = r − V (signed D1/D2 gain; production pathways only)
   ACh <- precision (EMA of inverse error variance): encoding mode, suppress recurrence
   NE  <- surprise z-score: ~200 ms lr burst, eligibility/context reset at boundaries
   5-HT <- average reward: production temperature, discount
 SAFETY: LGSS STDP guard with the new rule; VTA guard/reward alignment with DA;
        speech gate on output; toxicity gates storage/replay (never a positive reward)
 MONITOR: SNN health (rates, silent fraction, weight norm, error divergence, NaN) ->
        freeze + roll back a small weight-snapshot ring + halve lr
```

### 8.2 Build order (each step measured on the ladder against conventional baselines)

| # | Piece | Source in nimcp | Rung | Effort |
|---|---|---|---|---|
| 1 | Fix the cochlea; standalone `cochlea_emit` into the stream | lib/perception (fix the BM/IHC stubs, per-type drive, spike-train encoding) | 2 + stream | M |
| 2 | Neuron primitives in athena: ALIF (2 timescales), receptor τs + Mg block, TM-STP, per-synapse delays, E/I with Dale's law, event-driven delivery | snn_adaptation.c; snn_training.c:106-111,196-199; nimcp_stp.c:169-228; delay ring snn_network.c:137-168; types.h:1001-1029 | 2-3 | M |
| 3 | Wernicke predictor: e-prop with per-neuron error on cochlear input | design (init_language_pops ratios) | 2-3 | M |
| 4 | Thalamic gate with TRN driven by prediction | design (thalamus.c:413 gain) | 2-3 | S |
| 5 | Neuromodulation layer (ACh precision, NE surprise; signed DA later) in eprop.apply() | phasic_tonic.c:582-603; receptor_subtypes.c (fix the Kds); fep_neuromod.c:32-43; precision pred_hierarchy.c:979-987; novelty salience_helpers.c:245-301; shaper surprise_amplifier.c:198-300 | 2-3 | S-M |
| 6 | Hippocampus extension: CA3 heteroassociation, Hopfield retrieval, CA1 context, surprise-gated storage, PER replay (forward/reverse, compressed), sleep scheduler | athena Hippocampus; temporal_replay.c (sum tree, IS weights, sweeps); hopfield_memory.c; sleep_wake.c:283-345 | 3 | M |
| 7 | SNN health monitor plus rollback | failure_prediction.c, metacognition.c (ideas) | all | S-M |
| 8 | Nursery environment: scene generator, renderer with impact audio, narrator speech, scene-graph referee | intuitive_physics.c, scene_graph.c:87-130, sim_perception (rewrite) | 4-5 | M |
| 9 | Algorithm 4: naming game, signed DA critic, BG selector, Broca production with imitation error | vicarious_reward.c:470-495; social_interaction.c:600-650; BG D1/D2 design; mirror_language efference :1108-1150 | 4-5 | M |
| 10 | Rewritten cerebellum as a per-neuron-error forward model for speech timing; breathing pacer; synfire timing | design only | 5 | M |
| 11 | Efficiency: ternary weights with a float shadow, on-device plasticity, cuSPARSE batching | ternary_kernels.cu (check :300), plasticity_kernels.cu | after a rule works | M |

**Do not port:**
- backprop modules (JEPA code, VAE, physics NN, LNN, language generator);
- the global-reward R-STDP;
- any hand-coded affect, ToM, ethics, epistemic or reasoning module in the
  training loop;
- the nimcp lexicon or comprehend as a judge (biased; use ground truth);
- ground-truth simulator state as SNN *input* (referee and evaluation only);
- self-repair or hot-patching;
- all private "*_snn_bridge" toy networks.

### 8.3 nimcp fixes that matter even outside the new learner
- **Trainer reward call** (immerse_athena.py:6015): pass positional (reward,
  expected) explicitly. Today it gives RPE 0 or a silently swallowed TypeError.
- **Thalamic router:** drain the queue or stop submitting. It fills to 16384,
  then drops every message and holds that memory.
- **NE = confidence** (brain_learning.c:5080): inverted. One writer per modulator.
- **Production-path delays:** all delays are 0 on the GPU production path
  (snn_network.c:2829), and `use_axon_delays` is never read.
- **CPU and GPU integrate different neuron models** (no synaptic filter on CPU).
- **Uncompiled files:** neuromodulators_part_*.c and
  src/cognitive/emotional_tagging/*.c. Edits there do nothing.
- **Telemetry that overstates learning:** `consolidation_efficiency = 1.0`
  whenever any replay ran; nimcp sleep trains on blended REM mixup targets.
- **Earlier list:** creative-buffer leak, out-of-bounds reads, decide training on
  its own labels, the cognitive-dispatch use-after-free on timeout
  (dispatch.c:571-574).
- **Documentation corrections:**
  - CLAUDE.md is wrong about Phase-3 SNN language training, glial-on-SNN, and the
    regions being "wired";
  - the 8b29b60fd commit message ("adapters read rates/write bias") is false;
  - the retina header ("NO STUBS") is false.
