# 01 snn/plasticity/glial
- Alg 2: NOTHING works. (VERIFIED) pc_hierarchy (plasticity/predictive/nimcp_predictive_coding.c): error = input - prior (:256), W -= lr*err*mu^T (:493) = gradient ASCENT (sign inverted); bias same; level 0 never computes error (:717 condition) so W[0] never learns; higher levels get no bottom-up W^T e. Live only in brain_learn_example (learning.c:5507,5978). (VERIFIED) e-prop (snn_training.c): update_eligibility only decays (:1189-1190) -> trace 0; both callers pass learning_signal 0 (training_dispatch.c:549, snn_training_bridge.c:316) -> returns :1206; writes neural_net not CSR; lr hardcoded .001; global scalar. Surrogate apply_gradients = stub return (:1172).
- R-STDP live (entry->weight += delta :913). Homeostatic scaling live, CSR+GPU mirrored (snn_homeostatic_apply :1339) — keep as stabiliser.
- Wave H two-compartment (snn_network.c:1471-1511): basal+apical V, NMDA plateau; off by default (snn_training.c:216), on for pyramidal tiers (snn_hierarchical.c:430-445); no learning rule. Substrate for burst-dependent plasticity.
- Triplet STDP (nimcp_triplet_stdp.c:344,445) clean standalone per-synapse, not used on SNN. BCM + eligibility 3-factor wired to the ANN (neuralnet.c:2462-2469), not SNN.
- Plasticity coordinator: logs "Registered 9 mechanisms" but 8 have NULL update_fn (coordinator init :144-167, skipped :555) — statue.
- 36/42 snn/bridges statues (stdp/bcm/homeostatic/hippocampus/free_energy bridges never called externally).
- SNN language bridge learn_pair = EMA copy of lexicon weights (:921) — SNN doesn't learn; word pops hash-collide (learning.c:414).
- snn_fno backprop (live via learning.c:601). Glial modulation applied to ANN only.
- Shortest route to alg 2: fix pc_hierarchy (sign, bottom error, bottom-up term), port spiking onto Wave H (error -> apical, plateau-gated plasticity); or rebuild e-prop properly on CSR with per-neuron feedback.
