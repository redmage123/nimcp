# Athena core: design

A rewrite of the learning core behind Athena, built to fix the reasons the
nimcp brain could not be shown to learn (see "Why" below). nimcp stays as it
is; pieces of it come over only when an ablation shows they help.

## Why

Measured on nimcp's live stage-1 training (2026-11-15 to 11-18):

- The training task was "sentence embedding -> rotated embedding of almost the
  same sentence". A ridge regression reached held-out top-1 36%; the ANN
  reached 35-37%. The learner sat at the ceiling of a task with nothing in it
  to learn.
- The trained output was not connected to anything Athena does (the decoder
  vocabulary was empty; speech ran through a separate path).
- Competence was not measured for months; each new metric showed the last one
  was meaningless.
- Many interacting modules failed silently (frozen layers, unsaved GPU
  weights, a spiking blend that drowned the answer, flags ignored on resume).
- Many learning rules wrote to the same system toward different goals.
- One experiment per day (22 GB checkpoints, 15-minute restarts).
- A few hundred hand-written sentences of data.

## Rules

1. **The objective is the capability.** Every task head has a loss on the
   output we actually want (the word, the next token, the next frame). No
   proxy targets built from the input.
2. **Measure first.** Every task has a fixed held-out split and baselines
   (chance, majority, linear, nearest neighbour). A number is reported only
   next to its baselines. Every run appends one record to the scoreboard.
3. **Small surface, everything ablatable.** Each module is switched by config,
   and the ablation runner reports the score with and without it. A module
   that does not move the score is removed, not kept "for later".
4. **One loss, one optimizer per run.** Nothing writes weights outside the
   optimizer step unless it is a module under ablation.
5. **No statues.** Every module has a test showing that switching it changes
   the output. The overfit test (memorise a few items to 100%) runs in the
   test suite; if it fails, nothing else matters.
6. **Fast loop.** A useful experiment takes minutes on one GPU. Checkpoints
   are megabytes, not gigabytes.
7. **Real data.** Public datasets and corpora, not hand-written lists.

## Capabilities, in order

All three share one latent space: modality encoders feed a shared workspace,
and task heads read from it. The word space is shared too, so the word the
naming head picks is the same token the language model uses.

### Phase 1: name what it sees (in progress)

- Data: CIFAR-100 (100 object words), then Caltech-256 (256), then
  Speech Commands (audio -> the same word space).
- Model: vision encoder -> workspace -> naming head over the vocabulary.
- Metrics: held-out top-1 / top-5.
- Baselines: chance 1%, majority, ridge on pixels, k-NN on pixels.
- Exit: well above the baselines (a small ResNet should reach 60%+ top-1 on
  CIFAR-100 where pixel baselines sit near 15-20%), audio and vision naming
  through one head.

Status 2026-11-18 (CIFAR-100, held-out test, top-1):

| What | top-1 | top-5 |
|---|---|---|
| chance / majority | 0.010 | 0.050 |
| ridge on pixels (lambda chosen on validation) | 0.152 | 0.354 |
| 1-NN on pixels | 0.204 | 0.235 |
| 10-NN on pixels | 0.179 | 0.369 |
| **small model, 30 epochs (0.8M params, 99 s on one GPU)** | **0.688** | 0.91 |

Ablation (small, 15 epochs, one seed): augmentation is worth +0.087,
residual connections +0.026, the workspace MLP +0.003 (within noise; it has
not earned its place for naming alone and is kept only because the audio
encoder will share it; re-test then).

Status 2026-11-18, phase 1b (one model, seen + heard, shared word table;
small, 30 epochs, 19 min; each task scored on its own words):

| Task | joint model | best baseline |
|---|---|---|
| CIFAR-100 | 0.703 | 0.204 (1-NN) |
| CIFAR-10 | 0.918 | 0.422 (10-NN) |
| Speech Commands (35 words, unseen speakers) | 0.975 | 0.529 (10-NN) |

Grounding test (5 words both seen and heard; chance 0.20): a heard word's
workspace vector lies nearest the pictures of the same word 97.9% of the
time, and a picture nearest the right spoken word 87.7%. Ablation: with a
separate word table per sense, naming is unchanged but grounding falls to
chance (0.21 / 0.31). The shared word table is what links the senses.

Lesson recorded on the way: training each dataset with a softmax over the
joint vocabulary set them against each other (a CIFAR-100 "pickup truck" is
also CIFAR-10's "truck") and cut CIFAR-100 to 0.29; each task now uses a
softmax over its own words.

### Phase 2: continue a sentence

- Data: public-domain books (the canonical corpus list: Melville, Tolstoy,
  Shakespeare, ...), then wider text.
- Model: a language head (next-token prediction) on the same workspace, with
  token embeddings tied to the naming head's word space.
- Grounding test: from an image, complete "this is a ___".
- Metrics: held-out perplexity vs an n-gram baseline; grounded completion
  accuracy.

Status 2026-11-18: corpus = 29 public-domain books from the canonical list
(6.4M training tokens, 16,384-word vocabulary, 4% of held-out tokens unknown).
Held-out test perplexity: unigram 533.6, bigram 207.1, **transformer (small,
text only, 162 s) 80.7**. Captions (input -> "there is a ___", a phrasing
never trained on, scored against the same model with the input blanked)
pass on synthetic data. Real run (one model: 3 naming tasks + books +
captions, small, 10 epochs AdamW, 41 min, 8.9M params), held-out test:

| Captions ("there is a ___" / "it sounded like ___") | with input | input blanked | chance |
|---|---|---|---|
| CIFAR-100 | 0.651 | 0.008 | 0.010 |
| CIFAR-10 | 0.900 | 0.096 | 0.100 |
| Speech | 0.714 | 0.039 | 0.029 |

Text perplexity 85.7 in the joint model (80.7 text-only, bigram 207.1):
grounding costs the language model a little. Naming and cross-modal
grounding held (CIFAR-100 0.675, speech 0.974, heard->seen 0.957).

More text (2026-11-19): PG-19 shard 0 (2,488 public-domain books, the 9
that are also canonical excluded; 182.6M tokens) added to the canonical
books, same vocabulary, 60k steps. Canonical held-out perplexity:

| Model | canonical test ppl | PG-19 held-out ppl |
|---|---|---|
| bigram baseline | 207.1 | |
| small, canonical books only | 80.7 | |
| small, + PG-19 (8.2M params, 22 min) | 64.2 | 48.0 |
| base, + PG-19 (30.6M params, 33 min) | 51.8 | 39.2 |

More data and a bigger model both help, and neither has levelled off. The
base model writes locally coherent prose with dialogue and punctuation
("she opened the letter and | read , her <unk> eyes blazing with
curiosity . " my dear , " she said ..."); the 16k word vocabulary leaves
many <unk>s (7.6% of PG-19 tokens), the next thing to fix (subwords).

Subwords (2026-11-19, `--subwords`): byte-pair pieces learned on the
canonical training words, 16k tokens, naming words kept whole; 1.07 tokens
per word on canonical text, 1.12 on PG-19, no <unk> at all. Perplexity is
now per word (summed token loss / words) so word and subword runs share a
scale. The word-level numbers above were flattered by <unk>, a cheap
catch-all prediction: per word with subwords the bigram baseline is 412.4,
not 207.1. Base model, PG-19 shard 0, same 60k steps: canonical ppl 76.5
(18.6% of its bigram, vs 25% for the word model), and the samples have no
<unk> ("she opened the letter and | handed it to the visitor . " have you
read ? " she asked .").

All six PG-19 shards (1.12B training tokens) at the same 60k steps were
worse: canonical 84.3, shard-0 held-out 76.1 vs 74.5. The shard-0 run never
overfit (validation still falling at the end), so the model is short of
compute, not data, and the extra shards only diluted the canonical books.
They pay off only in a much longer run.

Grounded model at base size (2026-11-19, `grounded_sw_base`: naming,
captions, video and language in one model; subwords, PG-19 shard 0, 25k
steps, 37.8M params, 3.3 h), against the small word-level model above:

| | small, words | base, subwords |
|---|---|---|
| CIFAR-100 naming / caption | .675 / .651 | .687 / .675 |
| CIFAR-10 naming / caption | .902 / .900 | .909 / .911 |
| speech naming / caption | .974 / .714 | .977 / .355 |
| heard->seen / seen->heard | .957 / .853 | .980 / .871 |
| video skill kubric / home movies | (video-only .310 / .175) | .170 / .152 |
| text ppl (per word) | 85.7 (words) | 90.2 (subwords; LM-only 76.5) |

The speech caption drop is the language prior, not hearing. The test
prompt "it sounded like" is a strong book collocation ("sounded like one
..."): 2,266 of the wrong answers are "one". On other unseen phrasings
the captions match naming:

| unseen prompt | speech caption top-1 |
|---|---|
| "someone said" | .976 |
| "what i heard was" | .975 |
| "it sounded like" | .362 (says "one") |
| "the sound was the word" | .229 (says "on") |

So a stronger language model makes captions more fragile wherever the
prompt carries its own expectation: the workspace evidence has to outweigh
the collocation, and caption training on four templates does not teach
that. Next: evaluate on several held-out prompts (report mean and worst),
and train captions with many more phrasings so the input always wins.
Video skill is also lower than the video-only model (.170 vs .310 on
kubric; validation peaked at epoch 10), so video needs its own share of
steps in the joint run.

Both follow-ups ran (2026-11-19/20, same config as `grounded_sw_base`):

`grounded_sw_tpl`: 18 caption phrasings per sense, scored on 4 unseen
prompts (mean / worst):

| | base (4 phrasings) | tpl (18 phrasings) |
|---|---|---|
| CIFAR-100 caption | .667 / .655 | .684 / .682 |
| CIFAR-10 caption | .909 / .905 | .896 / .895 |
| speech caption | .633 / .229 | .774 / .181 |
| text ppl | 90.2 | 90.2 |

More phrasings lift the speech mean by 14 points but not the worst prompt:
"it sounded like" is still captured by the prior (.181). It was .467 at
epoch 4 and fell as perplexity improved (150 -> 73), so the collocation wins
as the language model strengthens. Phrasing variety is not enough; the
caption loss needs to make the input necessary, e.g. train against the
language model's own prediction for the slot (score the input's gain over
the prior), or weight captions up as text improves.

Video: the video-only base run trained for 600 epochs peaks early and then
overfits (kubric val .375 at epoch 50 -> .242 at 600; home movies .190 at
epoch 7 -> .062). But rationing video in the joint run to one pass per
epoch (`--video-passes 1`, `grounded_sw_vp1`, the same 1,550 video steps
the video-only 25-epoch run used to reach .314) made it worse: kubric
.13-.16 vs .19-.25 with video on every step. Overfitting is not what limits
video in the joint model; interference in the shared encoder is. Next:
give video its own encoder path (or stop its gradient into the shared
trunk) rather than fewer steps.

### Phase 3: predict what happens next

- Data: the video sets already collected (kubric, home movies).
- Model: next-frame prediction in latent space.
- Metric: beat the "next frame = this frame" baseline.

Status 2026-11-18: Kubric (571 clips x 24 frames) and home movies (2077 x 8),
32x32, split by clip. Held-out MSE per pixel: copy-last 0.057 / 0.153,
linear extrapolation 0.131 / 0.381 (worse). The model predicts the change
from the last frame and starts exactly at copy-last; skill = 1 - mse/copy.
It learns a synthetic moving square (skill > 0.3 in the tests).

First version predicted from the pooled workspace vector: skill exactly
0.000 on real video (pooling discards where things are). Now a ConvGRU runs
on the encoder's 8x8 feature map and decodes the change. Held-out skill
(small, 20 epochs): Kubric 0.310 (MSE 0.039 vs copy 0.057), home movies
0.175 (0.126 vs 0.153). Ablation: last frame only -> Kubric 0.099, home
movies 0.136: half the skill comes from using motion over time. Residual
connections +0.008 (noise).

### Phase 4 results so far

CIFAR-100, small, 30 epochs, three seeds each (mean +/- sd top-1):

| Candidate | top-1 | vs conventional | verdict |
|---|---|---|---|
| conventional workspace | 0.693 +/- 0.003 | | |
| spiking workspace (LIF, 8 steps, surrogate gradients) | 0.689 +/- 0.001 | -0.4 pts, 5.5% of neurons spiking per step | passes: kept as an option (`--on spiking_workspace`); becomes the default for the edge port, where sparse activity saves energy |
| feedback alignment (workspace errors sent back through fixed random weights) | 0.650 +/- 0.008 | -4.3 pts | rejected |
| surprise-scaled learning (per-example loss weight = loss / running mean, clipped) | 0.682 +/- 0.002 (8 epochs: 0.546 vs 0.563) | -1.1 pts, and no faster | rejected |

Rejected candidates are removed from the code (rule 3); both live in commit
52f5886 if they need another look.

### Phase 4: biological components earn their place

Spiking layers (surrogate gradients), local plasticity rules, neuromodulated
learning rates, sparsity. Each goes in as a swap for a conventional part and
stays only if the ablation shows it matches the score with a real gain
(energy, sparsity, sample efficiency, continual learning).

### Phase 5: port to C/CUDA

Port the proven design for edge and robot deployment, reusing nimcp
infrastructure where it fits (GPU kernels, safety layers, swarm runtime).
The safety layers (ethics gate, LGSS) attach at the agent level when Athena
acts; this core is the learner.

## Layout

```
athena/
  data/       datasets -> tensors, vocabularies
  models/     encoders, workspace, heads, the Brain that wires them
  baselines.py
  train.py    one training loop for every task
  scoreboard.py
  ablate.py   runs a config with and without each module
  run.py      command line
tests/
```

## Running

```
python3 -m athena.run baselines naming
python3 -m athena.run train naming --size small --epochs 30
python3 -m athena.run ablate naming --size tiny --epochs 5
python3 -m pytest -q
```

## Phase B1: spiking learners, few-shot (started 2026-11-21)

Goal (user): speech comprehension and generation on par with frontier LLMs
using biological algorithms, not transformers; the SNN does the learning,
the other networks support it. The project's thesis is that biological
learning is far more data-efficient than backprop. Step 1 tests that thesis
directly at small scale: spiking networks trained only by local rules,
against conventional learners at equal labelled data.

Code: athena/bio/ (snn.py primitives, learners.py, bench.py); tests/test_bio.py.
Learners: STDPCortex (LIF + winner-take-all + STDP + adaptive threshold,
after Diehl & Cook 2015), Hippocampus (sparse dentate expansion + one-shot
Hebbian word association), RSTDPReadout (reward-modulated STDP, three-factor).
Pass criterion (THEORY.md, rung 1): match logistic regression within seed
noise at 1-5 labelled examples per class.

First result (sklearn digits, 797 held out, 3 seeds, `python3 -m athena.bio.bench`):

| learner | k=1 | k=2 | k=5 | k=10 |
|---|---|---|---|---|
| logreg | .667 | .748 | .866 | .906 |
| 1-NN | .663 | .752 | .850 | .908 |
| k-means(pool)+logreg | .714 | .793 | .890 | .921 |
| snn hippocampus (pixels) | .645 | .729 | .801 | .850 |
| snn cortex(pool)+hippocampus | .586 | .666 | .762 | .791 |
| snn R-STDP readout | .140 | .128 | .185 | .264 |

Not passed. The one-shot hippocampus is near 1-NN at k=1-2 (it is a
prototype memory) and falls behind with more data (no error-driven
refinement). The STDP cortex lowers accuracy: its features are worse than
pixels, while conventional unsupervised features (k-means on the same pool)
add +.05 at k=1. Reward-only R-STDP learns a 3-class toy (test) but not 10
digit classes from pixels. Next: MNIST (60k unlabelled pool, where
unsupervised experience can matter), a cortex whose code beats pixels, and
R-STDP on the sparse hippocampal code.

MNIST rung (2026-11-21, `python3 -m athena.bio.bench_mnist --n 400,1600`): the
60k training images are the unlabelled pool, 10k test images held out, 3 seeds.
The STDP cortex learns from the pool once (1 pass, no labels); word neurons
then get one Hebbian increment per labelled example (`hebb`).

| learner | k=1 | k=2 | k=5 | k=10 | k=100 |
|---|---|---|---|---|---|
| logreg (pixels) | .450 | .554 | .658 | .758 | .877 |
| 1-NN (pixels) | .433 | .511 | .636 | .713 | .887 |
| k-means400(pool) + logreg | .522 | .597 | .691 | .802 | .916 |
| snn hippocampus (pixels) | .462 | .550 | .646 | .708 | .787 |
| snn STDP cortex400(pool) + Hebbian words | .501 | .575 | .694 | .768 | .853 |
| snn STDP cortex1600(pool) + Hebbian words | .465 | .559 | .681 | .751 | .852 |
| diag: cortex400 counts + logreg | .427 | .492 | .666 | .761 | .884 |

Seed std is .02-.06 at k<=5. With unlabelled experience the spiking cortex
plus one-shot Hebbian words beats the pixel learners at k<=10 and matches the
same-pool conventional baseline (k-means + logreg) at k<=5 within seed noise:
rung 1 passes on MNIST. It falls behind from k=10 (no error-driven
refinement: the target for e-prop + replay, THEORY.md algorithms 2-3). 1600
neurons did not beat 400 after one pass (25% of neurons never fired).
