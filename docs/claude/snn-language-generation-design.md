# SNN Language Generation — Architecture & Build Plan

**Status**: design locked 2026-06-12. Supersedes the bag-of-words decode
(`decode_spikes_cached`), which ranks Broca word-pops by spike count and emits
a *set* — not how a brain generates language.

## Goal (user directive, crystallized)

1. **The SNN learns language.** Grammar / word order / POS live in *learned
   synapses* (STDP from the language curriculum, with POS/NER as auxiliary
   training signals), not hand-coded rules.
2. **It produces like a human.** Sequential / autoregressive: one word at a
   time, each word conditioned on the words already emitted, until a learned
   end-of-utterance signal. No bag-of-words.
3. **It integrates the whole brain before speaking.** The intent that drives
   generation is a fusion of the other networks (LNN/CNN/FNO/HNN/ANN) plus the
   language and cognitive modules (comprehension, discourse, working memory,
   reasoning, imagination, emotion, attention). Form the thought, then speak it.

## Why the current path fails

`snn_language_bridge_decode_spikes_cached`: seed concept(s) into Wernicke, step
the FULL 2M-neuron SNN ~16 ticks, sum each Broca word-pop's spikes over the
window, emit top-K as a set. Three independent failures:

- **Ambient domination** — the full net's ~15k spikes/step swamp the seed's
  ~8-neuron projection; a fixed set of high-ambient pops ("thoriated",
  "urbanized", "anisotropy") wins regardless of prompt. (Confirmed by eval +
  the failed differential-decode experiment, commit 5392e50c0 → reverted.)
- **No sequencing** — output is an unordered set; no autoregression, no
  feedback of the emitted word into the next step, no syntactic state.
- **No learned grammar** — word order is not encoded anywhere; the warmstart
  only sets concept→word weights, never word→word transitions.

## Architecture

### A. Intent fusion (pre-generation "form the thought")
Assemble one intent/thought vector fusing:
- **Other NNs**: LNN (temporal), CNN (visual/audio), FNO, HNN, main ANN — via
  the neural substrate / thalamic router already wired to the SNN.
- **Language modules**: grounded-language comprehension of the prompt, discourse
  state.
- **Cognitive modules**: working memory, reasoning, imagination, emotion,
  attention.
Much exists in the cascade `stage_content` (`content_intent` already blends WM,
discourse, imagination, reasoning). Output: the fused intent is projected into
**Wernicke** as the stable "what I want to say", held for the whole utterance.

### B. Isolated generation room (the clean room)
A dedicated SNN — Wernicke (concept/intent) + Broca (word output) + a sequencing
population — with NO ambient firing. Cannot run or train sequential dynamics
inside the 2M-neuron net (ambient swamps signal), so generation gets its own
isolated `snn_network_t` (`bridge->gen_net` repointed here; routing/plasticity
on the full brain are untouched — `gen_net` is generation-only, verified).

### C. Autoregressive readout (human-like production)
Replace the bag-of-words decode:
- **WTA / lateral inhibition** in Broca → exactly ONE word ensemble wins per
  step.
- **Efference copy** — the emitted word feeds back into the net, conditioning
  the next step's competition (this is the "conditioned on what I just said").
- **Next-word loop** until a learned end-of-utterance signal (learned stop, not
  a fixed length cap).

### D. Learned language (training)
Run the language curriculum through the isolated room so STDP shapes:
- **Concept→word** (Wernicke→Broca) — partly exists (warmstart); refined online.
- **Word→word transitions** (Broca→sequencing→Broca) — the grammar / word order
  / POS, *learned*. POS/NER are auxiliary training signals that bias transitions.

## Build phases (each independently verifiable via scripts/lang_eval.py)

1. **Isolated subnet** — build the clean Wernicke→Broca room, repoint
   `gen_net`, warmstart the projection there. Verify: selectivity probe recovers
   (correct word ranks #1, no "thoriated"). *[substrate]*
2. **Autoregressive readout** — WTA one-word-per-step + efference feedback +
   learned stop. Verify: ordered output, not a set. *[production mechanism]*
3. **Intent fusion** — route the cascade `content_intent` (all-NN + cognitive)
   into Wernicke seeding. Verify: prompt-relevant content words. *[pre-gen feedback]*
4. **Training loop** — STDP on word→word transitions from the curriculum
   (+POS/NER aux). Verify: grammaticality / order improves over training.
   *[learn to speak]*
5. **Eval + iterate.**

## Reuse (not from scratch)
- Cascade `content_intent` fusion (WM/discourse/imagination/reasoning).
- Neural substrate + thalamic router (LNN/CNN/FNO/HNN → SNN).
- Sensorymotor synfire ring (sequencing primitive), lateral inhibition.
- STDP, concept→word warmstart, `generate_step`/decode plumbing,
  `scripts/lang_eval.py` harness, `snn_probe_selectivity`.

## Key constants / APIs (verified 2026-06-12)
- Pops: `LANG_WERNICKE_POP_NEURONS`=`LANG_BROCA_POP_NEURONS`=64000; ensembles
  `SNN_LANG_NEURONS_PER_POP`=8 → 8000 word/concept pops; `SNN_LANG_PROJ_W_MAX`=50.
- Build: `snn_network_create(snn_config_t*)` (auto-makes input/output pops — give
  them 1 neuron, inert), `snn_network_add_population_lightweight(net,n,NEURON_GENERIC_LIF,name)`,
  `snn_network_connect_populations(net,src,dst,SNN_TOPO_RANDOM,conn,SYNAPSE_AMPA,w_mean,w_std)`,
  `snn_network_set_pop_exclude_from_plasticity`, `snn_network_finalize_connections`,
  `snn_network_destroy`.
- Bridge: `gen_net`/`gen_wernicke_pop_id`/`gen_broca_pop_id` (bridge.c:143-145),
  generation-only; `connect_network`, `warmstart_projection`, `build_projection`,
  `generate_and_decode`, `probe_selectivity`.
- Boot hook chain: `brain_daemon._activate_snn_language_generator` →
  `nimcp_brain_build_lang_projection` / `nimcp_brain_warmstart_lang_projection`
  (part_core.c:5033/5052) → bridge. These must target the isolated `gen_net`.
