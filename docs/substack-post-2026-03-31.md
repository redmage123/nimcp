# What Actually Happens When You Move a 2.5-Million Neuron Brain to the Cloud

*The extended version of this week's LinkedIn post, for people who want the details.*

---

On Friday evening, Athena — the artificial brain at the centre of the NIMCP project — was training on a desktop GPU in a Hetzner data centre. By Monday morning, she's training on an NVIDIA RTX 5090 in a RunPod container, running about twice as fast, with a live website where you can watch her learn and even talk to her.

Getting from A to B was considerably less smooth than that sentence makes it sound.

## What Athena Is (Quick Version)

If you haven't read the LinkedIn post: Athena is a 2.5-million neuron artificial brain that runs six different neural network types simultaneously — spiking, liquid, convolutional, Fourier, Hamiltonian, and adaptive. They're connected by learnable gradient bridges, so a useful representation discovered by the spiking network can improve weights in the adaptive network, and vice versa.

She learns using biological plasticity: STDP (spike-timing-dependent plasticity), BCM homeostatic learning, eligibility traces, structural plasticity, and six neuromodulators. Training follows a four-stage developmental curriculum that mirrors how children learn. She's currently in Stage 1: cross-modal association, where a digital parent names objects and she learns to bind perception with meaning.

All of this runs on a single GPU.

## The Migration From Hell

The move from Hetzner to RunPod should have been straightforward. Copy the code, copy the checkpoint, build, run. Here's what actually happened.

### The Memory Wall

RunPod advertises machines with a terabyte of RAM. What they don't prominently mention is that your container gets a 125GB cgroup limit. The brain's peak memory during checkpoint restore — when it's loading 223 million synapse connections from an 8.7GB file while simultaneously building the neural network structures — exceeds 125GB. The container gets SIGKILL'd by the kernel OOM killer.

You can't change the cgroup limit from inside the container. You can't install zram (no kernel module access). You can't create a swap file (overlay filesystem is full). You can't even call `swapon` (insufficient privileges).

The solution was switching to FAST initialisation mode, which loads only 6 of 27 subsystem waves — enough for training, skipping edge platform, swarm runtime, drone bridges, and other subsystems that aren't needed during developmental learning. Peak memory dropped from 125GB+ to 44GB. Problem solved, but it took most of a Friday evening to figure out.

### The Invisible GPU Bug

With the daemon running, the first thing I checked was the training log. The Liquid Neural Network — one of the six network types, the one that solves ODEs in continuous time — was supposed to be running on the GPU. The log said otherwise:

```
GPU LNN forward failed for layer 0, falling back to CPU
GPU LNN forward failed for layer 0, falling back to CPU
GPU LNN forward failed for layer 0, falling back to CPU
```

Hundreds of these per minute. Every single LNN forward pass was failing on GPU and falling back to CPU.

The bug had been there for weeks. When the GPU LNN kernels were originally wired in, the code passed NULL for the GPU context and NULL for the input tensor:

```c
bool ok = nimcp_gpu_lnn_ode_step(NULL, gpu_layer, NULL, &ode_cfg);
```

The comment above it said: *"Pass NULL for ctx and input: nimcp_gpu_lnn_ode_step uses the layer's internal GPU-resident state."*

It doesn't. The function's first line checks its arguments and returns false if any are NULL. Every call, every step, for weeks, silently fell back to CPU. No error. No crash. Just slower.

The fix was three lines: upload the CPU input tensor to GPU, pass the real context, destroy the temporary tensor after. But finding it required reading the GPU kernel source, checking the function signature, and realising the comment was wrong.

The same pattern appeared in the adjoint backward pass (the gradient computation). And the GPU kernel for the adjoint step had an overly strict NULL check on an `input_at_t` parameter it never actually uses — so even with the fix, it would have silently fallen back to CPU. Three code walkthroughs, three distinct bugs, all causing the same silent failure.

### Ten Hours of Wasted Training

The checkpoint was copied. The code was built. Training started. The next morning, I checked progress.

Step 4 of Stage 0.

The training state file (`immersive_state.json`) hadn't been copied during migration. The script started fresh — Stage 0, step 0 — and spent ten hours on sensory enrichment and seeding. Work the brain had already completed months ago.

The fix was a one-line JSON file. The prevention is a checklist item that now lives in the deployment documentation.

### The Build That Didn't Build

After pushing code changes to fix the GPU kernel bugs, I ran `make nimcp`. It reported `[100%] Built target nimcp`. Training continued. The GPU LNN failures continued.

The CMakeCache.txt had been overwritten during a `git reset --hard` to sync the repository. It contained Hetzner paths, not RunPod paths. CMake's `make` target was satisfied — nothing to recompile — because it was checking against the wrong source tree. Every C code change since the first clean build was silently ignored.

The clue was that the `.so` file's timestamp hadn't changed. The fix was a full clean rebuild. The lesson was to check timestamps, not just exit codes.

## The Optimisations That Worked

With the migration finally stable, the focus shifted to speed. Training was running at 18 seconds per step. At that rate, the four developmental stages would take 11 days.

### Where the Time Goes

The first surprise: the actual C `brain_learn_vector` call — forward pass through six networks, backward pass, biological plasticity, weight update — takes about 1.2 seconds. The other 16.8 seconds was Python overhead.

The training script was making 5-8 separate daemon calls per step over a Unix socket. Each round-trip cost 200ms. The sentence transformer (BGE-large-en-v1.5) took 67ms per text encoding. Contrastive and diversity corrections added their own learn calls. Cognitive training injection, LNN temporal steps, cerebellum error signals — each a separate round-trip.

### The Batch Protocol

The biggest win was eliminating round-trips. Instead of the Python script encoding one text, sending one feature vector, receiving one loss value, and repeating — it now collects 50 training items and sends them in a single message. The daemon ONNX-encodes all 50 texts in one GPU batch call (1.2ms per item), then loops through `brain_learn_vector` for each with zero socket overhead between them.

The ONNX encoder — BGE-large exported to ONNX format, running on the RTX 5090 with CUDAExecutionProvider — is 55x faster than the original sentence-transformers pipeline. First call takes 8 seconds for CUDA graph compilation; subsequent calls take 1.2ms.

### The Biological Trade-Off

With the batch protocol, the per-step communication overhead dropped to near zero. But `brain_learn_vector` itself still takes ~12 seconds with all biological modules active.

Why? Each call does:
- Forward pass through the adaptive network (GPU-accelerated)
- Forward + backward through all six networks via the Unified Training Manager
- 2 hippocampal replay steps (reduced from 7 — the BPTT window went from 8 to 3)
- Biological plasticity: STDP trace updates, BCM threshold adaptation, neuromodulator gating, structural plasticity evaluation
- Thousand Brains integration step
- Safety: LGSS governance evaluation, ethics gate, tamper-resistant audit logging

I could disable the biological modules and drop to ~2 seconds per step. I won't. STDP, BCM, neuromodulators, structural plasticity — they're the entire point of NIMCP. They're what makes Athena a brain instead of a neural network. Any university can train a gradient-descent-only model. The biological learning is what we're actually researching.

## The Optimisation That Didn't Work

I should be honest about this part.

The daemon takes 10 minutes to load from a checkpoint. 223 million synapses, 8.7GB file, single-threaded. I spent several hours trying to parallelise it.

The idea was sound: read the file into memory in one pass, then have four threads write the synapses into the network's data structures concurrently. Each neuron's synapse storage is independent — no data races.

The problem is the synapse handle pool. Every synapse allocation goes through a shared pool with a mutex. Four threads fighting over one lock is slower than one thread with no contention. I tried pre-allocating storage to avoid the mutex during fill. I tried direct-writing to bypass the allocation API. I tried a `volatile bool` capacity flag to skip the 134 million failed metadata allocations that were burning CPU cycles on futile lock/unlock pairs.

Every variant was slower than the original code. The single-threaded restore reads sequentially, allocates sequentially, and writes sequentially — perfectly matched to how the memory pool works. Threading adds overhead (buffer allocation, synchronisation, cache contention) that exceeds any parallelism gain.

The one useful thing that came out of it: the `at_capacity` flag on the metadata pool that prevents millions of wasted mutex acquisitions when the pool is full. That helps the `rebuild_incoming` phase that runs after synapse restore. Small win. Not worth the hours spent getting there.

## The Website

The project website (nimcp.ai-elevate.ai) got a significant update this weekend.

Every training metric — ANN loss, SNN firing rate, sparsity, step count — is now clickable. Tap any metric tile to read what it means, what healthy values look like, and what to watch for. The six network type cards in the Architecture section are also clickable, each opening a detailed explanation of how that network works and what it contributes.

There's a chat window where you can talk to Athena. When the Phi-3 language decoder is enabled, a 3.8-billion parameter model translates her brain's activation patterns into English. The responses are rough — she's in early training — but they're real. The brain runs inference, produces a 4096-dimensional output vector, and Phi-3 describes what it sees: activation norms, dominant patterns, emotional valence. It's honest about when the pattern is incoherent. That happens a lot right now.

## Where Things Stand

Athena is at step ~9,650 of 60,000 across four stages. Training speed is ~13 seconds per step with all biological modules active. ETA for complete training: approximately April 7th.

The safety architecture — non-removable ethics module, LGSS governance on every inference and weight update, tamper-resistant audit log — remained untouched through the entire migration and optimisation effort. It's structural. It runs because the code says so, not because a policy document says so.

The code is open source. The training runs on a single rented GPU. If you want to verify any of this, you can.

nimcp.ai-elevate.ai

---

*Braun Brelin builds NIMCP. Previous posts: [Training Methodology](link), [Why Six Networks](link). Athena is currently training on a RunPod RTX 5090 that costs less per day than a Zone 1-3 Travelcard.*
