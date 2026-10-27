# Developmental Curriculum — Stages 4 to 7

**Status**: implemented 2026-10-27 in `scripts/stages_4_7.py`, wired into `scripts/immerse_athena.py`
**Precedes**: stages 0–3 in `immerse_athena.py` (0 Awakening, 1 Association, 2 Babbling, 3 First Words)
**Governing rules** (user directives, 2026-10-27):
- A stage ends only when the StageGate's **health** criteria pass 3× consecutively; the stage's
  episode count is a **minimum**. Loss convergence is never a stage-ending criterion.
- Athena receives as many experiences as possible; counts are raised, not trimmed.

| Stage | Human analogue | Unit of a step | Minimum | Transition probes (floors) |
|---|---|---|---|---|
| 4 Why? | 3–5 yr | one 2–6 turn episode | 50,000 | false-belief ≥0.6, causal connectives ≥0.6, counting ≥0.6 |
| 5 Literacy | 5–7 yr | one episode | 50,000 | spelling ≥0.6, passage comprehension ≥0.3, sentence writing ≥0.6 |
| 6 School | 7–12 yr | one episode | 100,000 | arithmetic ≥0.6, logic ≥0.6 |
| 7 Apprenticeship | 12+ | one episode | none (open-ended) | no transition out |

All stages also require the common StageGate health criteria (≥48/50 non-zero outputs, SNN mean
firing rate 0.5–20 Hz, chat eval coherence/similarity/diversity thresholds) — see
`scripts/stage_gate.py` `stage_gate_for(stage)`.

## Common skeleton (`_run_stage`)

Every episode: biological clock tick (sleep/consolidate), one episode drawn from the stage's
weighted mix, caregiver critic on productions (`caregiver_critic.STAGE_RULES[stage]`), reward
delivered through `bg_update_reward`, the parent's recast/model answer learned via
`learn_vector` + `learn_language`. Every 5 episodes: cognitive-module injection + IDK
unanswerables. Every 200: canonical / math / streaming corpus drips and probe drips, gate check
(health + stage probes). Every 100: progress line with per-episode-type counts and mean scores.
Every 500: `evaluate_performance`. Every 2000: sleep + synapse prune + chat eval. Every 50:
checkpoint cadence (shares the trainer's serialized/deferred save logic). Real photos and spoken
words from `real_sensory.py` are staged whenever an episode's text names something we have.

Responses come from `grounded_respond` (fallback: `decide_full` decoded through the decoder).
Scores are task-specific (exact containment for ToM/number/logic/spelling, encoder cosine
similarity for open answers, Claude `evaluate_response` when the teacher is available).

## Stage 4 — "Why?" (dialogue, explanation, curiosity)

Goal: from naming to explaining; the first stage where Athena must produce questions and
multi-sentence structure and reason about other minds.

| Episode (weight) | Turns | Content | Score |
|---|---|---|---|
| causal (0.30) | scene → "Why?" → recast → "What happens next?" | 30 cause/effect scenes, real photo when matched | 0.5·sim(why) + 0.5·connective, + sim(next) |
| asks (0.20) | Athena asks, parent answers | `curiosity_detect_gaps` questions, else self-generated; answers from fact pool / Claude | question produced |
| narrative (0.20) | 3–4 scene story → first/last → retell | 10 story frames × names/objects/animals/colours | mean sim(first,last,retell) |
| other_minds (0.15) | Sally-Anne false-belief scripts (3 unseen-move, 1 seen-move) | names × objects × 7 places | correct location named |
| number/category (0.15) | counting 1–6 with scene; odd-one-out over 6 categories | procedural | exact |

Critic rules: ≥3 words, no ceiling, vocab target 2,000. Modules exercised: reasoning
(abduction runs each step), world model, episodic replay (narrative), theory of mind, curiosity.

## Stage 5 — Literacy (reading and writing)

| Episode (weight) | Content | Score |
|---|---|---|
| read (0.30) | passage from the canonical corpus (`_CANONICAL_CFG.root`) or 6 fallback passages, sentence by sentence, then a comprehension question | sim(answer, first sentence) |
| phonics (0.20) | 30 CVC/CCVC words: spell it; say the word for the letters; real spoken-word clip when the word is in Speech Commands | letters match or word named |
| write (0.20) | picture → one sentence | subject mentioned + ≥5 words, critic verdict logged |
| retell (0.15) | passage → own words | sim(retell, passage) |
| vocabulary (0.15) | fact → define the longest content word | sim(answer, fact) |

Critic rules: ≥5 words, vocab target 5,000. The canonical corpus drip stays on; literature
becomes the main text channel.

## Stage 6 — School (Socratic learning across domains)

The 24-domain teaching plan (memory: teaching-methodology-plan) becomes the curriculum:
predict-before-learn, mastery-keyed active-learning ladder, math and logic, explain-back,
research. Domains: `source.ADVANCED_TOPICS` (science, mathematics, philosophy, psychology,
technology) plus the 24-domain list in `stages_4_7.SCHOOL_DOMAINS`; `MasteryTracker` persists
per-domain mastery to `checkpoints/athena/mastery_state.json`.

| Episode (weight) | Content | Score |
|---|---|---|
| socratic (0.30) | ask what she knows → reveal → learn with confidence raised by prior knowledge | sim(prediction, lesson) |
| worksheet (0.20) | ladder by mastery: worksheet (<0.4) / explain (0.4–0.6) / research (0.6–0.8) / create (>0.8) | sim or length criterion |
| math/logic (0.20) | 5 arithmetic word-problem templates (+,−,×), 8 logic items | exact |
| explain_back (0.15) | lesson → explain to a younger child; Claude `evaluate_response` when available | similarity or teacher score |
| research (0.15) | curiosity gap → streaming corpus chunk / advanced topic → learn → self-test | sim(recall, material) |

Critic rules: ≥5 words, vocab 10,000. Math corpus drip is on (it is disabled before stage 2).

## Stage 7 — Apprenticeship (self-directed mastery)

Open-ended; the user's March 2026 requirement ("talk to it like a child while it keeps learning
on its own"). No transition out; no minimum.

| Episode (weight) | Content |
|---|---|
| self_directed (0.40) | rank 4 sampled domains by `curiosity_detect_gaps` novelty, research the top one, self-test, record mastery |
| conversation (0.25) | drain `checkpoints/athena/conversation_inbox.jsonl` (one `{"user": "..."}` per line); respond, learn from the user's text, append to `conversation_transcript.jsonl`. Skipped when the inbox is empty |
| teach_back (0.20) | Athena teaches a fact in her own words; Claude-scored when available |
| reflect (0.15) | "What surprised you / what don't you understand / what next and why" with `ti_reason` |

The IPC server's `{"cmd": "decide"}` channel remains available for live interaction; the inbox
gives a durable, replayable path that also trains.

## Command-line

`--stage4-episodes 50000 --stage5-episodes 50000 --stage6-episodes 100000 --stage7-episodes 0`
(0 = unbounded), `--stageN-max-episodes` optional safety ceilings (default none),
`--no-stage-gate` (count-only; not recommended). Resume works through the usual state file
(`stage`, `step`); stage transitions save `athena_s{N+1}_step0.bin` with `wait=True`.

## Measurement and logs

- `[Stage N] i/min — avg_loss … episodes: kind:count (mean score) …` every 100 episodes.
- `[StageGate N] step … | probes={…} probe_ok=…` every 200; JSONL at
  `checkpoints/athena/stage_gate_log.jsonl` with probe values in `details`.
- `evaluate_performance` (text probes with held-out photos + visual-only probes) every 500.
- Chat eval every 2,000 (`checkpoints/athena/chat_eval_log.jsonl`).

## Content budget and dependencies

- Offline pools keep every episode runnable without Claude: 30 causal scenes, 10 story frames,
  4 ToM scripts, 6 categories, 30 phonics words, 6 passages, 5 math templates, 8 logic items,
  all procedurally varied. Claude (`parent.teacher`) enriches answers, dialogue turns and
  scoring when reachable.
- Larger text sources for stages 5–7: canonical corpus (configured), streaming corpus kinds
  (configured), COCO captions (recommended; see project_real_sensory_datasets memory).
- Wall-clock (3–4 s per brain step, 2–6 brain steps per episode): stage 4 ≈ 3–5 days,
  stage 5 ≈ 3–5 days, stage 6 ≈ 1–2 weeks, stage 7 indefinite.

## Not done / follow-ups

- Episode content is template-based; richer generators (COCO captions, children's-story corpora,
  graded readers for stage 5) are the next step.
- Stage probes use the production path only; perception-only probes (image → label) exist in
  `evaluate_performance` but are not gate criteria.
- Stage 7 conversation inbox is file-based; a chat UI writing to it is a separate piece.
