# Athena: theoretical core

This document says what we are trying to build and which algorithms define
it. Every piece of work is judged against it. DESIGN.md says how we work
(measure first, everything ablatable, fast loop); this says what we are
testing.

## Goal

In the user's words (2026-11-21): **comprehend and generate speech as well as
frontier LLMs, but using biological algorithms rather than transformers.**

Conventional networks (logistic regression, k-NN, MLPs, LSTMs, transformers)
are used only as baselines on the same data, or as support around the
biological learner. They are never the learner being claimed.

## Thesis

A system built from the four algorithms below, trained in a grounded,
interactive setting, learns language from far less data than a transformer
needs. A child gets to fluent language from roughly 10^8 words; a frontier
LLM trains on roughly 10^13 tokens.

The data efficiency is expected to come mainly from fast memory with replay
(algorithm 3) and grounding (algorithm 4). Spikes and local learning rules
(algorithm 2) are what make the system biological; on their own they are not
expected to beat backprop.

The thesis is falsifiable. At each rung of the ladder below, the biological
learner is compared with conventional learners trained on the same data. If
it is consistently worse at every data budget, the thesis is wrong in its
current form, and that is a result to report, not to hide.

## The four algorithms

### 1. Objective: predict the next input

The system learns by predicting what it will perceive next (the next word,
the next sound, the next frame) at several timescales, and reducing its own
prediction error. This is predictive processing.

- LLM counterpart: next-token prediction, the single objective behind every
  frontier model.
- Evidence: brain responses during language processing are predicted by how
  well a model predicts the next word (Schrimpf et al. 2021; Goldstein et
  al. 2022), and the brain predicts at a hierarchy of timescales longer than
  current LLMs use (Caucheteux, Gramfort & King 2023).
- Consequence: the main training signal is unlabelled. Labels (a word for an
  image) are just one more stream to predict.

### 2. Credit assignment: local learning with a per-neuron error

Each synapse changes using only quantities available at the synapse or its
neuron (presynaptic activity, the postsynaptic neuron's state) plus a
learning signal sent to that neuron. Together these approximate the gradient
of algorithm 1.

Admissible rules:

- e-prop: eligibility traces times a learning signal sent through fixed
  random feedback weights (Bellec et al. 2020). Implemented in
  `athena/bio/eprop.py`.
- Burst-dependent or dendritic plasticity: the error arrives on apical
  dendrites and is multiplexed with the feedforward signal (Payeur et al.
  2021).
- Predictive-coding networks: each layer predicts the layer below and learns
  from its own prediction error; they approximate backprop (Whittington &
  Bogacz 2017).

Not admissible for the learner: backprop through time, autograd, weight
transport (feedback must not copy the forward weights, except as a test of
exactness), and global scalar reward as the only signal for a many-class
mapping. That last one is what nimcp's SNN does, and it cannot assign credit
(see Evidence).

- LLM counterpart: backpropagation.
- Spiking neurons (LIF or adaptive LIF) are the default unit.
- Stepping stone, not the end state: unsupervised Hebbian feature learning
  (STDP with winner-take-all and homeostasis, after Diehl & Cook 2015; the
  `STDPCortex` in Phase B1). It is local and biological, but it neither
  predicts the next input nor carries a per-neuron error, so it is a step
  toward a predictive cortex (algorithm 1), not an instance of algorithm 2.

### 3. Context and fast memory

Two systems, as in the brain (complementary learning systems, McClelland,
McNaughton & O'Reilly 1995):

- A fast memory stores an episode in one exposure: a sparse,
  pattern-separated code (dentate-gyrus-like expansion) bound to its context
  by Hebbian plasticity. It answers "what followed this before?" and holds
  the recent context of a sentence or conversation.
- The slow learner (algorithm 2) learns general structure. Replay of stored
  episodes trains it offline, so one experience is learned from many times.

- LLM counterpart: attention over the context window. A transformer's
  attention step is mathematically the retrieval step of a modern Hopfield
  associative memory (Ramsauer et al. 2020), and models of the hippocampal
  formation are closely related to transformers (Whittington, Warren &
  Behrens 2022). A biological associative memory is therefore a principled
  replacement for attention, not a loose analogy.
- This is the main expected source of data efficiency.

### 4. Grounding and social feedback

Words are learned together with what they refer to: what is seen and heard
at the same time, and the effect of saying something. Communicative success
(being understood, getting the thing asked for) gives a reward that
modulates learning, as dopamine does.

- LLM counterpart: pretraining on huge text plus reinforcement learning from
  human feedback.
- Consequence: data includes paired speech, images and video, and
  interaction, not text alone.

## How they fit together

```
 sound / image / text ─► sensory spiking layers ──► recurrent spiking cortex ──► prediction of next input
                              ▲   (alg. 2)              ▲     │ (alg. 1, 2)          │
                              │                         │     ▼                      ▼
                              └──── prediction error ◄──┴── fast memory ◄─────── error signal
                                                           (alg. 3, replay)
                         reward from communicative success modulates plasticity (alg. 4)
```

The cortex predicts the next input. The difference between prediction and
input is the error that drives local plasticity (algorithm 2). The fast
memory supplies context and stores episodes for replay (algorithm 3). Reward
from successful communication scales plasticity (algorithm 4). Production
(speaking) is the same prediction run forward: the system says what it
predicts comes next.

## The ladder

Each rung compares the biological learner with conventional learners on the
same data and the same held-out split, at several data budgets. A rung passes
when the biological learner matches the best conventional learner of similar
size at the smallest budgets, within seed noise.

| rung | task | algorithms tested | data | pass criterion |
|---|---|---|---|---|
| 1 | name a digit from 1-10 labelled examples per class | 2, 3 | sklearn digits, then MNIST | at k=1-5, match logistic regression within seed noise |
| 2 | name a spoken word | 1, 2, 3 | Speech Commands | at k=1-10 per word, match the same-size conventional net |
| 3 | predict the next word of child-directed speech | 1, 2, 3 | CHILDES transcripts | held-out per-word perplexity no worse than a same-size LSTM or transformer trained on the same words |
| 4 | the BabyLM benchmark | 1-4 | 10M- and 100M-word budgets | grammar (BLiMP) and comprehension scores against the BabyLM baselines |
| 5 | grounded speech in and out | 1-4 | speech, images, video, interaction | comprehension and spoken answers on held-out situations |

Rungs 1 and 2 are checks that the parts work. **Rung 3 is the first decisive
test**: algorithms 1 and 2 together, on language, against conventional nets
at equal data.

## How to judge any piece of work

Before starting, ask: does this move a biological learner up the ladder,
measured on held-out data against conventional learners? If not, it is
housekeeping: keep it small, or do not do it. Repairs to nimcp's daemon,
readout modules and plumbing do not count unless they serve a rung.

## Evidence so far (2026-11-21)

- nimcp's training task (stage 1) mapped a sentence embedding to a rotated
  embedding of nearly the same sentence. A ridge regression reached the same
  held-out top-1 as the network (about 36%), so the task carried no
  information to learn.
- nimcp's spiking network never receives the correct answer. Its live
  populations take the R-STDP path, which ignores the target. Its reward is
  80% firing-rate match and 20% the ANN's error. Its weight change is one
  global scalar times pre/post co-activity on every synapse. One of its two
  update loops indexes the eligibility with the wrong neuron numbering.
- nimcp's backprop ANN on digits (fresh 4096-neuron brain): held-out .20,
  .38, .54, .69 at 1, 2, 5, 10 examples per class, and only with the
  full-gradient GPU mode; the default mode does not learn 10 examples.
- Phase B1 (DESIGN.md), digits, 3 seeds, k = 1/2/5/10:

| learner | k=1 | k=2 | k=5 | k=10 |
|---|---|---|---|---|
| logistic regression | .667 | .748 | .866 | .906 |
| k-means (pool) + logreg | .714 | .793 | .890 | .921 |
| one-shot hippocampus (alg. 3) | .645 | .729 | .801 | .850 |
| STDP cortex + hippocampus | .586 | .666 | .762 | .791 |
| R-STDP readout (global reward) | .140 | .128 | .185 | .264 |
| e-prop, recurrent LIF (alg. 2) | .281 | | | |

- MNIST (60k training images as the unlabelled pool, 10k test held out,
  3 seeds, seed std .02-.06 at k <= 5; Phase B1, athena_core cab3e85),
  k = 1/2/5/10/100:

| learner | k=1 | k=2 | k=5 | k=10 | k=100 |
|---|---|---|---|---|---|
| logistic regression (pixels) | .450 | .554 | .658 | .758 | .877 |
| 1-NN (pixels) | .433 | .511 | .636 | .713 | .887 |
| k-means400 (pool) + logreg | .522 | .597 | .691 | .802 | .916 |
| k-means1600 (pool) + logreg | .516 | .591 | .686 | .804 | .919 |
| one-shot hippocampus (pixels; alg. 3) | .462 | .550 | .646 | .708 | .787 |
| STDP cortex400 (pool, 1 pass) + one-shot Hebbian words | .501 | .575 | .694 | .768 | .853 |
| STDP cortex1600 + Hebbian words | .465 | .559 | .681 | .751 | .852 |
| cortex400 counts + logreg (diagnostic) | .427 | .492 | .666 | .761 | .884 |

- With unlabelled experience, the spiking cortex plus one-shot Hebbian
  words beats the pixel learners at k <= 10 and matches the fair same-pool
  conventional baseline (k-means + logreg) at k <= 5 within seed noise:
  rung 1 passes on MNIST for algorithm 2's stepping stone plus algorithm 3.
  It falls behind from k = 10 (no error-driven refinement). 1600 neurons did
  not beat 400 after one pass (25% never fired).
- So far the best biological result comes from fast memory (algorithm 3), as
  the thesis predicts, and error-driven local learning alone (e-prop) is
  weak at tiny budgets. The next step on rung 1 is to combine them: e-prop
  refining the hippocampal code, with replay, aimed at k >= 10.

- Rung 3, next-word prediction on AO-CHILDES (vocabulary 2000; 2026-11-21/22;
  `athena/bio/bench_curve.py`, test perplexity, lower is better; INTERIM:
  one seed, and both the LSTM size and the slow learner's decay chose the
  edge of their grids).
  **The slow learner here is not yet the SNN.** `stack_lm.py` is a residual
  stack of per-neuron-recurrence layers of firing-rate (ReLU) neurons,
  trained by a local rule (traces exact within a layer, learned
  Kolen-Pollack feedback) with Adam as the optimiser (an engineering choice,
  not biological). Rung 3 has to be shown by a spiking version (LIF/ALIF,
  e.g. `eprop_lm.py` or a spiking `stack_lm`), and a plain local update rate
  (no Adam) must be one of the reported conditions.
  Like-for-like pairs (the memory helps any slow learner, so it is compared
  on both sides):

| train words | tuned LSTM | rate-based local slow learner | memory + LSTM | memory + local slow learner | fast memory alone | trigram |
|---|---|---|---|---|---|---|
| 3k | 239.0 | 210.9 (-11.8%) | 180.6 | 187.7 (+3.9%) | 196.7 | 213.6 |
| 10k | 154.5 | 146.5 (-5.2%) | 132.8 | 136.2 (+2.6%) | 149.7 | 159.4 |

  Percentages compare each local learner with its conventional counterpart
  (negative = local better). Alone, the rate-based local learner is ahead
  of the tuned LSTM at 3k and 10k words; with the memory on both sides, the
  LSTM pairing is slightly ahead. The memory's gain (algorithm 3) is real
  but belongs to the memory, not to the learning rule. At 30k words (dev)
  the local learner tied a tuned LSTM. With symmetric feedback its updates
  equal backprop pass for pass.

## Open problems and risks

- Local learning rules have not yet been shown to train language models at
  scale. That is the central research risk, and rung 3 is where it gets
  tested.
- Random-feedback rules learn more slowly than backprop as depth grows. The
  candidates in algorithm 2 differ in how well they scale, and the ladder
  should compare them.
- Spiking simulation is expensive on GPUs. Run-time cost is measured but
  is not a pass criterion until rung 4.

## References

- Bellec, G. et al. (2020). A solution to the learning dilemma for recurrent
  networks of spiking neurons. Nature Communications 11, 3625.
- Caucheteux, C., Gramfort, A. & King, J.-R. (2023). Evidence of a predictive
  coding hierarchy in the human brain listening to speech. Nature Human
  Behaviour 7, 430-441.
- Diehl, P. U. & Cook, M. (2015). Unsupervised learning of digit recognition
  using spike-timing-dependent plasticity. Frontiers in Computational
  Neuroscience 9, 99.
- Goldstein, A. et al. (2022). Shared computational principles for language
  processing in humans and deep language models. Nature Neuroscience 25,
  369-380.
- McClelland, J. L., McNaughton, B. L. & O'Reilly, R. C. (1995). Why there
  are complementary learning systems in the hippocampus and neocortex.
  Psychological Review 102, 419-457.
- Payeur, A., Guerguiev, J., Zenke, F., Richards, B. A. & Naud, R. (2021).
  Burst-dependent synaptic plasticity can coordinate learning in
  hierarchical circuits. Nature Neuroscience 24, 1010-1019.
- Ramsauer, H. et al. (2020). Hopfield networks is all you need.
  arXiv:2008.02217 (ICLR 2021).
- Schrimpf, M. et al. (2021). The neural architecture of language:
  integrative modeling converges on predictive processing. PNAS 118,
  e2105646118.
- Warstadt, A. et al. (2023). Findings of the BabyLM Challenge:
  sample-efficient pretraining on developmentally plausible corpora.
  CoNLL-CMCL 2023.
- Whittington, J. C. R. & Bogacz, R. (2017). An approximation of the error
  backpropagation algorithm in a predictive coding network with local
  Hebbian synaptic plasticity. Neural Computation 29, 1229-1262.
- Whittington, J. C. R., Warren, J. & Behrens, T. E. J. (2022). Relating
  transformers to models and neural representations of the hippocampal
  formation. ICLR 2022.
