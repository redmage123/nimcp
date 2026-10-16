# NIMCP

**A digital brain that grows up like a child — and learns the way real brains learn.**

NIMCP (Neuro-Inspired Modular Control Protocol) is an experimental AI system
built on a different bet from today's chatbots. Instead of training a giant
text-prediction model once and freezing it, NIMCP simulates the actual
machinery of a biological brain: ~2 million neurons connected by trillions of
synapses, organized into the same regions you'd find in a human brain
(prefrontal cortex, hippocampus, visual cortex, language areas), and trained
the same way a child learns — through a developmental curriculum that runs
from "look at the pretty colors!" up to abstract reasoning.

The goal is an AI that **keeps learning** after deployment, **remembers**
what happened to it, can **inhabit a body** (a robot or drone), and has
**safety baked into its architecture** — not bolted on after training.

> **Status: research prototype.** It can hold simple multimodal conversations,
> ground words to images and sounds, and run on a single GPU. It is not
> production-ready, not a chatbot replacement, and intentionally less verbally
> fluent than a large language model. It is a different kind of system.

---

## Table of contents

- [Why build this?](#why-build-this)
- [How does it work? (plain English)](#how-does-it-work-plain-english)
- [How is this different from ChatGPT or Claude?](#how-is-this-different-from-chatgpt-or-claude)
- [Inside the brain](#inside-the-brain)
- [How it learns: a developmental curriculum](#how-it-learns-a-developmental-curriculum)
- [Safety architecture](#safety-architecture)
- [Where this could go (and where it shouldn't)](#where-this-could-go-and-where-it-shouldnt)
- [Getting started](#getting-started)
- [Project status](#project-status)
- [License & acceptable use](#license--acceptable-use)
- [Credits](#credits)

---

## Why build this?

Today's leading AI systems (GPT, Claude, Gemini) are extraordinarily good at
one thing: predicting the next word in a sequence. That single trick, scaled
up, gives them most of their behavior. But it leaves out a lot of what minds
actually do:

- **They don't learn after they're trained.** Every conversation starts from
  amnesia.
- **They have no memory of you.** Their "context window" is a sliding scratch
  pad, not a life history.
- **They have no body.** They can describe a room but they've never been in
  one.
- **They didn't develop.** They appeared fully-formed from a training run.
  A child grows up; an LLM is shipped.
- **Their safety is post-hoc.** Guardrails are added by fine-tuning after the
  capabilities exist, which is a constant arms race.

NIMCP starts from a different premise: **build the mechanisms biology
uses, and see what emerges from them.** Real synapses, real neurotransmitters,
real sleep cycles, real developmental stages, real embodiment. With safety as
a structural property rather than a politeness layer.

This is not a claim that NIMCP is smarter than an LLM. It isn't. It's a
claim that biological-style cognition is worth investigating as an
alternative path — one with different trade-offs, different failure modes,
and different long-term ceilings.

---

## How does it work? (plain English)

Imagine a video game that simulates a tiny brain. The brain is made of about
two million simulated neurons — little dots that fire electrical pulses to
each other through connections called synapses. When neurons fire together
often, the connection between them gets stronger. When they don't, it
weakens. That single rule, applied across trillions of connections, is most
of how learning works in real biology, and it's what NIMCP runs at its core.

On top of the neurons, NIMCP adds the structures real brains have:

- **Specialized regions.** A part for vision, a part for hearing, a part for
  movement, a part for language, a part for emotional reactions, a part for
  long-term memory. They talk to each other the same way the corresponding
  human regions do.
- **Two hemispheres.** Like a human brain, NIMCP has a left side (more
  language and logic) and a right side (more spatial and emotional), connected
  by a "corpus callosum" that lets them coordinate.
- **Chemicals.** It simulates six neuromodulators — dopamine, serotonin,
  acetylcholine, norepinephrine, GABA, and glutamate. These act like
  hormones: dopamine spikes when something rewarding happens; norepinephrine
  rises when the system is alert; serotonin shapes mood. They change how the
  rest of the brain learns.
- **Sleep.** The brain runs sleep cycles where it replays recent experience
  and consolidates memories — the same trick neuroscientists believe real
  brains use.
- **A body (optional).** It can be hooked up to sensors (cameras, microphones,
  LIDAR) and motors (drones, robot arms) so it can interact with the physical
  world.

To **teach** the brain anything, you don't fine-tune a dataset. You **raise
it**. There's a curriculum that progresses through developmental stages, just
like a child:

1. Stage 0 — *Sensory awakening*. Just patterns of light and sound. No
   meaning yet.
2. Stage 1 — *"Look! That's a ___!"*. A simulated parent points at things
   and names them. The brain learns to associate a word with what it sees and
   hears.
3. Stage 2 — *Babbling and feedback*. The brain tries to produce its own
   language. The parent corrects it.
4. Stage 3 — *Reasoning and conversation*. Abstract topics, theory of mind,
   counterfactuals.

If you stop the curriculum at Stage 1, you get a brain that names things
like a toddler. If you let it run further, vocabulary, syntax, and reasoning
emerge gradually — not because we programmed grammar, but because the
biological mechanisms that produce grammar in real brains are running.

Because NIMCP is structurally a brain, you can also **damage** it (turn off
a region) and watch how it compensates, the way humans recover from strokes.
Or **wake it** in a different sensory environment and watch the curriculum
push it down a different developmental path.

---

## How is this different from ChatGPT or Claude?

|                  | LLMs (ChatGPT, Claude, etc.)                         | NIMCP                                                                                  |
|------------------|------------------------------------------------------|----------------------------------------------------------------------------------------|
| **What it is**   | A statistical model of text                          | A simulation of a biological brain                                                     |
| **How it learns**| One huge training run, then frozen                   | Continuously, the way you do — every experience changes it                             |
| **Memory**       | A scratch pad that resets each conversation          | Episodic memory (events), semantic memory (facts), autobiographical memory (its life)  |
| **Body**         | None                                                 | Optional — can run drones, robot arms, sensor rigs                                     |
| **Language**     | Predicts the next word                               | Words are grounded in what they refer to (sights, sounds, motor actions)               |
| **Development**  | Born at full capability                              | Grows up through 4 stages, like a child                                                |
| **Safety**       | Trained behavior; can be jailbroken                  | Structural — the ethics module cannot be turned off; the audit log cannot be erased    |
| **Hardware**     | Datacenter clusters                                  | One consumer-grade GPU                                                                 |
| **What it's good at** | Fluent text, broad knowledge                    | Embodied learning, continuous adaptation, transparent decision-making                  |
| **What it's bad at**  | Memory, embodiment, post-deployment learning     | Verbal fluency at LLM scale, broad world knowledge from text alone                     |

**Important**: NIMCP is **not** trying to be a better ChatGPT. They solve
different problems. An LLM is a brilliant text predictor. NIMCP is an attempt
at a *cognitive architecture* — the kind of system you'd want if you needed
something that could be deployed in a robot, keep learning from new
experiences for years, and have its decisions auditable down to the synapse.

---

## Inside the brain

Under the hood, NIMCP is a C library (≈500K lines of source) with Python
bindings. Six different kinds of neural network run in parallel inside the
simulated brain, each playing a role real brain tissue does:

| Network | Biological analogue | What it does |
|---|---|---|
| **Adaptive** | Cortex (general)         | Standard learned function approximation                |
| **SNN** (spiking)  | Real cortical neurons    | Time-coded patterns; spikes carry information         |
| **LNN** (liquid)   | Recurrent dynamics       | Continuous-time temporal processing                   |
| **CNN** (visual / auditory) | Visual & auditory cortex | Edge / feature detection in images and sound  |
| **FNO** (Fourier)  | Spectral processing      | Frequency-domain pattern recognition                  |
| **HNN** (Hamiltonian) | Energy-conserving dynamics | Stable dynamical systems for motor control       |

Around them sit ~60 cognitive modules covering things like theory of mind,
moral reasoning, planning, memory consolidation, language production, and
introspection. There are 33+ named brain regions including hippocampus,
prefrontal cortex, amygdala, cerebellum, and the basal ganglia. There's a
glial layer (astrocytes, oligodendrocytes, microglia) that modulates the
neurons the way real glia do. There's a metabolic substrate that tracks
something analogous to ATP and temperature, so the brain can run "tired" or
"alert."

For deeper architecture documentation:

- [`docs/EXTERNAL_API_GUIDE.md`](docs/EXTERNAL_API_GUIDE.md) — the public API
- [`docs/INDEX.md`](docs/INDEX.md) — full documentation map
- [`docs/claude/`](docs/claude/) — modular technical references
- [`CHANGELOG.md`](CHANGELOG.md) — what shipped when

---

## How it learns: a developmental curriculum

The training script is `scripts/immerse_athena.py` (Athena is the project's
working brain). It runs four stages, in order, optionally pausing at any
stage. Each stage uses different teaching material:

| Stage | What it's like | What the brain experiences |
|---|---|---|
| **0 — Sensory awakening** | Newborn | Raw images, sounds, and proprioception. No labels. The brain learns the *shape* of its sensory world before the words for it. |
| **1 — Association** | Toddler | A simulated parent narrates: "Look at the cat. The cat is orange. Listen — it purrs." The brain binds words to grounded experience. |
| **2 — Babbling and correction** | Pre-school | The brain produces utterances; the parent and feedback loop correct them. Grammar starts to emerge from statistics, not from rules we wrote. |
| **3 — Reasoning** | Older child / adult | Conversation, theory of mind, math, science, ethics, counterfactuals ("what if X had happened instead?"). |

The curriculum is fed by real corpora (Project Gutenberg classics, Wikipedia,
arXiv, code) and by a "sibling" / "teacher" loop where another AI plays
playmate or instructor. Memory consolidation runs during simulated sleep
between sessions.

You can pause at any stage and inspect what the brain has learned. You can
freeze it, copy it, fork it — try the same brain raised on two different
curricula and compare them.

---

## Safety architecture

Because NIMCP is intended for embodied deployment (it can drive a drone),
safety isn't optional. Nine architectural layers run on every inference and
every training step. None of them can be turned off through configuration.

| Layer | What it does |
|---|---|
| **Input validator** | Catches corrupted, NaN, or adversarial inputs before they touch the brain |
| **Ethics module** | Evaluates every decision and every learning step — non-removable |
| **Action interceptor** | Governance rules check every output |
| **Safety watchdog** | Heartbeat-based dead-man's switch; if the brain stops updating, motors freeze |
| **Motor gate** | Validates anything destined for a physical actuator |
| **Training guard** | Blocks data poisoning attempts |
| **Reward alignment** | Detects and prevents reward-hacking |
| **Tamper-evident audit log** | Append-only, CRC-checked, with monotonic sequence numbers — gaps or hash mismatches indicate tampering |
| **LGSS enhanced** | Monotonic tightening (rules can only get *stricter*), formal verification hooks (rules exportable as SMT-LIB v2 for Z3/CVC5 proof), multi-stakeholder governance |

Two of these — the ethics module and the audit log — are **physically
impossible to disable** through any config flag. Removing them requires
recompiling the source.

For embodied deployment (drones, robots, vehicles), the project's license
**requires** a hardware emergency stop, an active safety watchdog, audit
logging enabled and monitored, and a human able to intervene within
five seconds.

See [`SAFETY.md`](SAFETY.md) for the full safety policy.

---

## Where this could go (and where it shouldn't)

This software has dual-use risk. We document it openly so users go in with
their eyes open.

| Capability | Beneficial uses | Misuse to refuse |
|---|---|---|
| Drone flight bridges | Search & rescue, agriculture, mapping | Autonomous weapons, mass surveillance |
| Swarm coordination | Distributed sensing, multi-robot cooperation | Coordinated autonomous attack |
| Sensorimotor learning | Prosthetics, robotic assistance | Autonomous pursuit / interception of humans |
| Theory of mind | Assistive communication, education | Social manipulation / deception |
| Emergent language | Cognitive-science research | Opaque agent-to-agent communication evading oversight |

The license (below) explicitly prohibits autonomous weapons, mass
surveillance, and deceptive AI applications.

If you find a misuse risk, vulnerability, or unintended capability, please
report it privately to <braun.brelin@ai-elevate.ai> before any public
disclosure.

---

## Getting started

NIMCP is a research prototype. Setup assumes Linux (Ubuntu / Debian
tested), comfort with C build systems, and a CUDA-capable GPU for full
performance (CPU works, but slowly).

```bash
# 1. System dependencies (Ubuntu / Debian)
sudo apt-get install build-essential cmake python3-dev \
    libjansson-dev liblz4-dev libsqlite3-dev libsodium-dev

# 2. Optional: CUDA for GPU acceleration
sudo apt-get install nvidia-cuda-toolkit

# 3. Build
git clone https://github.com/redmage123/nimcp.git
cd nimcp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 4. Smoke test
./build/test/unit/edge/unit_edge_test_sensor_hub
./build/test/unit/cognitive/unit_cognitive_test_enhancements

# 5. Start a fresh training run from Stage 0
python3 scripts/immerse_athena.py --stage 0
```

For a deeper guide:

- [`docs/EXTERNAL_API_GUIDE.md`](docs/EXTERNAL_API_GUIDE.md) — embedding NIMCP in your own code
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — how to contribute
- [`CLAUDE.md`](CLAUDE.md) — internal development reference

---

## Project status

**Version 0.9.0-beta.** This is a research prototype, not a product. Expect:

- Fast-moving APIs.
- Capabilities advancing as the curriculum runs.
- A working brain you can train, save, restore, and embed in robots.
- A multi-month training timeline to reach reasoning-stage competence.

Active research areas:
- Recursive syntax (Chomsky-style Merge) — see [`docs/claude/ce-20-recursive-syntax-plan.md`](docs/claude/ce-20-recursive-syntax-plan.md)
- Sleep-cycle memory consolidation
- Cross-species behavioural priors (dragonfly target tracking, octopus distributed cognition)
- Federated swarm learning across multiple physical brains

---

## License & acceptable use

NIMCP is distributed under a modified open-source license with explicit
acceptable-use restrictions:

**Prohibited uses** (non-exhaustive — see `LICENSE`):
- Autonomous weapons of any kind, including lethal targeting, autonomous
  pursuit of human targets, and military-offensive swarm coordination
  without continuous human-in-the-loop control.
- Mass surveillance or monitoring of individuals without informed consent,
  except where required by law with judicial oversight.
- Use of the theory-of-mind or social-interaction modules to deliberately
  deceive or manipulate humans without their knowledge.

**Required for any physical deployment** (robots, drones, vehicles):
- Hardware emergency stop (not software-only).
- Active safety watchdog with validated timeout.
- Audit logging enabled and monitored.
- Human operator able to intervene within 5 seconds.

For licensing questions or commercial enquiries, contact
<braun.brelin@ai-elevate.ai>.

---

## Credits

Built by [Braun Brelin](mailto:braun.brelin@ai-elevate.ai) with substantial
collaboration from Claude (Anthropic). The project is, by design, a long
co-development between a human architect and an AI assistant — we believe
that kind of collaboration is going to produce a lot of future research
software, and we'd rather document how it works in the open.

For research collaboration, security disclosures, or licensing:
<braun.brelin@ai-elevate.ai>.
