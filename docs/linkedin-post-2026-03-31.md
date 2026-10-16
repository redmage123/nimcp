On Friday, I had a brain that would take 27 days to train. By Monday morning, it's down to 7.

Not everything went smoothly.

Athena is a 2.5-million neuron artificial brain — not a language model, but a brain. Six different neural network types running simultaneously: spiking neurons that fire like biological ones, liquid networks solving differential equations in continuous time, convolutional processors for vision and hearing. All connected by learnable gradient bridges. All teaching each other.

She learns the way children do. Right now she's in Stage 1: a digital parent shows her objects and names them. "Look! That's a dog!" She learns to bind what she sees with what she hears. Later comes correction, then reasoning. Four stages, each building on the last.

The first problem was moving her from a desktop GPU to the cloud. The RunPod container had a 125GB memory limit despite sitting on a terabyte of host RAM. The brain peaked above that during checkpoint restore. We couldn't change the limit from inside the container — it's a cgroup setting. The solution was switching to a leaner initialisation mode that loads 6 of 27 subsystem waves, bringing peak memory from 125GB to 44GB.

Then the GPU kernels. I'd wired CUDA kernels for the Liquid Neural Network months ago, but they'd been silently failing and falling back to CPU on every call. The bug: the code passed NULL for the GPU context and NULL for the input tensor, with a comment saying "the kernel uses internal state." It doesn't. Three code walkthroughs caught three variants of the same bug across the forward pass, the adjoint backward pass, and the gradient accumulation.

The worst detour was trying to parallelise checkpoint loading. The brain stores 223 million synapse connections in an 8.7GB file. Loading takes 10 minutes. I spent hours trying to thread it — pre-allocating storage, splitting into phases, bypassing the allocation API for direct memory writes. Every attempt made it slower. The synapse pool uses a mutex that serialises all allocation, so four threads just meant four threads fighting over one lock. The honest lesson: sometimes the single-threaded code that works is the right answer.

What actually helped was less dramatic. Reducing the hippocampal replay buffer from 8 to 3 (the last five replays contributed diminishing returns). Running the six networks' forward passes in parallel instead of sequentially. Making non-critical daemon calls fire-and-forget. Building a batch training protocol so 50 items go to the brain in a single message instead of 50 round-trips.

The safety architecture stayed untouched through all of it. The ethics module runs on every inference and every weight update — not because of a config flag, but because the function call is in the C source. You can't prompt past it. You can't fine-tune it away. You'd have to modify the source and recompile, and the tamper-resistant audit log would record that you tried.

I also rebuilt the project website this weekend. You can click any of the six network types to learn how they work. Every training metric is clickable with an explanation of what it means and what healthy values look like. There's a chat window where you can talk to Athena directly — she responds through a Phi-3 decoder that translates her neural activation patterns into English. Her answers are rough right now. She's still in early training. But that's her brain talking, not a prompted language model.

Training GPT-5 reportedly cost over $100 million. Athena trains on a single rented GPU for under a week. Not because she's doing less — because biological architectures are fundamentally more efficient. Brains don't need a trillion parameters to learn that dogs bark.

She's training right now. You can watch: nimcp.ai-elevate.ai

#AI #Neuroscience #AIResearch #AISafety
