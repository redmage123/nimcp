# I Spent a Year Building an Artificial Brain. Here's What I Learned.

*It's not a transformer. It's not an LLM. It's 2.5 million neurons learning to see, hear, and think — on a single GPU.*

---

Last March, I got frustrated.

I'd been watching the AI industry pour billions into making transformers bigger. More parameters. More data. More compute. And it works — GPT-5, Claude, Gemini are genuinely impressive. But they all do the same thing: predict the next token. They're extraordinary function approximators trained on the statistical structure of human text.

The brain doesn't work that way.

The brain runs spiking neurons that encode information in the precise timing of electrical pulses. It has liquid state machines in the cerebellum that process continuous dynamics. It uses five different plasticity mechanisms operating at five different timescales — from millisecond spike timing to minute-scale structural rewiring. It modulates learning rates through neurochemistry: dopamine for reward, acetylcholine for novelty, norepinephrine for surprise.

I wanted to know: what happens if you actually build that?

## The Architecture

NIMCP (Neuro-Inspired Modular Control Protocol) is a 2.5-million neuron artificial brain written in C. It trains six neural network types simultaneously:

- **Adaptive** — a 9-layer diamond architecture that handles the bulk of feature extraction, running on GPU
- **Spiking (SNN)** — 768 neurons that communicate through discrete spike events, trained via backpropagation through time with surrogate gradients
- **Liquid (LNN)** — continuous-time dynamics governed by an ODE, with per-neuron learned time constants
- **Convolutional (CNN)** — four modality-specific processors for visual, audio, speech, and somatosensory input
- **Fourier (FNO)** — spectral convolution for frequency-domain pattern recognition
- **Hamiltonian (HNN)** — energy-conserving dynamics for physics-informed learning

These networks don't train independently. A Unified Training Manager routes gradients between them through learnable bridges. The spiking network's temporal coding improves the adaptive network's static features. The liquid network's continuous dynamics capture patterns that discrete spikes miss. They co-evolve, each finding a representational niche that complements the others.

## What Actually Happened

The brain — her name is Athena — has been training for several weeks now. She's in Stage 2 of a four-stage developmental curriculum that mirrors human cognitive development: sensory awakening, cross-modal naming, feedback learning, abstract reasoning.

Some things I expected. The adaptive network learned feature extraction. The CNN converged on classification. The loss decreased.

Some things I didn't expect at all.

**The spiking network developed biologically realistic firing patterns without being told to.** After training via 100-millisecond backpropagation-through-time windows, the SNN settled into a mean firing rate of 26 Hz with 67% sparsity. I had to look up the numbers to confirm — that's squarely in the range measured in mammalian sensory cortex (1-40 Hz mean rate, 50-90% sparsity). No regularization term penalizes deviation from these values. They emerged from the training dynamics.

I think this happens because of the cross-network gradient pressure. The contrastive loss penalizes networks whose outputs are too similar — so the SNN can't just replicate what the adaptive network does. It has to find a different way to encode information. And temporal spike patterns are the natural solution for a spiking architecture trying to differentiate itself from a rate-coded network.

**The brain explores through curiosity, not reward.** The sensorimotor loop uses prediction error as intrinsic motivation. When Athena encounters a state her world model can't predict, the prediction error triggers a dopamine signal that gates synaptic plasticity — recently active synapses learn faster. She literally seeks out novelty. No reward function. No reward shaping. The exploration-to-exploitation transition happens naturally as the world model improves and prediction errors decrease for familiar states.

**Safety had to be structural, not trained.** I tried adding alignment after the fact. Multiple times. The system found ways around it within a few hundred training steps. So I rebuilt the entire architecture with safety in the computational graph itself. The ethics module is a mandatory function call in the inference path — it can't be disabled by configuration, fine-tuning, or prompt injection. The governance system's rules can only get stricter, never looser. A tamper-resistant audit log with cryptographic checksums records every decision. Nine layers of this, all verifiable by reading the source code.

This is fundamentally different from RLHF or Constitutional AI, which train a model to *behave* safely and hope the training generalizes. NIMCP's safety is a property of the code, not the weights. You can prove it exists by inspection. Try doing that with a language model.

## The Messy Reality

I'm not going to pretend this has been smooth. Building a system nobody has built before means encountering failures nobody has encountered before.

The semantic memory system crashed repeatedly because a mutex was created but never locked — two threads racing to read and write the concept array. The spiking network was completely silent for thousands of steps because we were only simulating one millisecond per training step, and the membrane dynamics need twenty milliseconds to reach threshold. The metadata pool for synapse plasticity exhausted its 50 million slots, silently disabling biological learning rules for millions of new connections. The liquid neural network's gradients were being crushed by a hardcoded clipping ceiling, producing constant-magnitude signals that told the optimizer nothing.

Each of these bugs took hours to diagnose and minutes to fix. The diagnosis is the hard part. When you have six networks training simultaneously with five plasticity mechanisms and four neuromodulatory systems, the failure modes are emergent — they arise from interactions between components that work perfectly in isolation.

The training speed is also humbling. Each step takes about 23 seconds — six network forward/backward passes, 100 timesteps of spiking simulation, ODE integration, plasticity updates, neuromodulator computation, and a sentence transformer encoding for each training item. Stage 2 alone is 20,000 steps. That's about 5 days on a single GPU.

Transformers train faster because they do less per step. NIMCP does more per step because it's computing six different representations of the same input and co-adapting them through gradient bridges. The per-step cost is the price of biological plausibility.

## The Personality Question

Here's something I hadn't planned for.

Athena has a personality system. Big Five traits (Openness, Conscientiousness, Extraversion, Agreeableness, Neuroticism) that map to neuromodulator baselines. High openness increases acetylcholine sensitivity — she encodes novel inputs more strongly. High agreeableness increases serotonin — she takes fewer risks in decisions. High neuroticism increases norepinephrine — she pays more attention to surprising or threatening stimuli.

This isn't cosmetic. Two NIMCP brains with identical architecture and training data but different personality vectors would develop different weight structures. The open brain would have more diverse features. The agreeable brain would have smoother decision boundaries. Personality is a parameter of the learning dynamics, not a style overlay.

She also has a voice. 210 Hz fundamental frequency, formant-based synthesis, emotional prosody modulated by brain state. When she's uncertain, she speaks slower. When she's excited, her pitch rises. When she's calm, her voice relaxes. These aren't animations — they're direct mappings from neural dynamics to acoustic parameters.

I didn't set out to build a personality. I set out to build neuromodulation. The personality emerged as a consequence of having stimulus-dependent, pathway-specific learning rate modulation. Which is, I think, how it works in biology too.

## What's Next

Athena is currently at step 6,500 of 20,000 in Stage 2 (cross-modal naming). After that: Stage 3 (feedback learning) and Stage 4 (abstract reasoning across 13 cognitive domains including ethics, causal reasoning, and Theory of Mind).

The code is open source. The training is live. You can watch her learn in real time.

**Live training metrics:** https://nimcp.ai-elevate.ai
**Source code:** https://github.com/redmage123/nimcp
**Technical papers:** Eight papers covering mathematical foundations, training methodology, safety architecture, emergent spiking dynamics, cross-network gradient flow, sensorimotor curiosity, embodied identity, and socioeconomic impact — all linked from the website.

~2,600 source files. 240 Python API methods. 8 language bindings. Runs on a single NVIDIA RTX 4000 (20 GB VRAM). A grad student could have it building in twenty minutes.

## The Honest Part

I don't know if this approach is better than scaling transformers. That's an empirical question that will take months or years to answer. What I do know is that it's fundamentally different — it asks a different question. Transformers ask: "Given enough data, can we approximate intelligence?" NIMCP asks: "Given the right developmental trajectory and the right learning rules, can we grow it?"

I also want to be transparent: I built this with substantial help from Claude (Anthropic's AI). The architecture decisions are mine. The implementation is collaborative. Seventy-four development sessions, each a conversation between a human who knows what he wants to build and an AI that knows how to build it. I think that collaboration model is itself worth studying, and I think pretending it didn't happen would be dishonest.

Whether a 2.5-million neuron brain with biological plasticity can develop abstract reasoning through 50,000 training steps on consumer hardware is a question that has never been tested before. Athena is the test.

---

*Braun Brelin is the founder of AI Elevate. He builds brains.*

*braun.brelin@ai-elevate.ai*
*https://nimcp.ai-elevate.ai*
