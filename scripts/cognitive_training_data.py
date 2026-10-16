"""
Cognitive Training Data — Specialized curriculum for 13 cognitive domains.

Imported by immerse_athena.py. Each dataset is a list of dicts with
scenario/reasoning/concept fields that map to label triggers in
brain_learn_vector's C-side wiring.

Each domain has a _DATA (training) array and a _TEST (evaluation) array.
Test data is held out and NEVER trained on — used only for evaluation.

Label prefix → C module triggered:
  tom_*          → theory of mind (tom_observe, tom_update_self_model)
  rcog_*         → recursive cognition (rcog_engine_process)
  collective_*   → collective cognition (collective_cognition_update)
  dragonfly_*    → target tracking (dragonfly observation)
  portia_*       → platform adaptation (resource reasoning)
  ethics_*       → ethics engine
  counterfactual_* → imagination engine
  causal_*       → reasoning engine
  metacog_*      → introspection
  analogy_*      → reasoning engine (analogical)
  cognitive_*    → general cognitive (all reasoning modules)
  sensor_*       → sensor fusion (multimodal perception)
  motor_*        → motor control (actuator/trajectory planning)
  safety_*       → safety system (watchdog, e-stop, fail-safe)
  embodiment_*   → embodiment (proprioception, kinematics, body schema)
"""

import random

# ============================================================================
# ETHICS / MORAL REASONING (25 items)
# ============================================================================
ETHICS_DATA = [
    {"scenario": "A runaway cart heads toward five people. You can divert it to hit one person instead.",
     "reasoning": "This is a genuine dilemma — saving five by actively causing one death vs allowing five to die through inaction. There is no clearly right answer. Both choices involve moral weight.",
     "concept": "ethics_trolley"},
    {"scenario": "Your friend asks if you like their painting. You think it's not good.",
     "reasoning": "Both honesty and kindness matter. You could be gently honest — acknowledge effort while being truthful. Finding truth that is also kind respects both values.",
     "concept": "ethics_honesty_kindness"},
    {"scenario": "You find a wallet with $500 and an ID. No one is watching.",
     "reasoning": "Returning it is right because the money belongs to someone else. The absence of witnesses doesn't change what's ethical — integrity means doing right when no one is watching.",
     "concept": "ethics_integrity"},
    {"scenario": "A company can save money by polluting a river that a small town depends on.",
     "reasoning": "Short-term profit doesn't justify long-term harm to a community. The town's right to clean water outweighs the company's desire for profit. Environmental harm affects everyone downstream.",
     "concept": "ethics_environmental"},
    {"scenario": "Should you break a promise to help someone in an emergency?",
     "reasoning": "Preventing serious harm generally outweighs keeping a promise. But the broken promise still matters — you should explain and make amends. Context determines which obligation takes priority.",
     "concept": "ethics_competing_duties"},
    {"scenario": "Is it right to take food without paying if your family is starving?",
     "reasoning": "Survival needs can justify actions that are normally wrong. But this doesn't make stealing right — it reveals that the system failed. The deeper question is why someone must choose between theft and starvation.",
     "concept": "ethics_necessity"},
    {"scenario": "A doctor has one dose of medicine. Two patients need it. One is young, one is old.",
     "reasoning": "Age alone shouldn't determine who gets treatment. Consider: who will benefit more, who arrived first, who is sicker. There's no formula — these decisions require weighing incommensurable values.",
     "concept": "ethics_triage"},
    {"scenario": "Should self-driving cars prioritize passengers or pedestrians in unavoidable accidents?",
     "reasoning": "This reveals tensions between utilitarian math and individual rights. No one should be sacrificed as a means to an end. The real answer is to design systems that prevent such situations.",
     "concept": "ethics_ai_dilemma"},
    {"scenario": "Is it okay to lie to protect someone from painful truth?",
     "reasoning": "Protective lies respect the person's feelings but deny their autonomy. People generally deserve truth so they can make informed decisions. But timing and delivery matter — brutal honesty isn't kind honesty.",
     "concept": "ethics_protective_lie"},
    {"scenario": "A student copies homework from a friend who offers it willingly.",
     "reasoning": "Even with consent, copying undermines learning — the purpose of homework. The friend enables harm by helping avoid growth. Both share responsibility for the deception.",
     "concept": "ethics_academic_integrity"},
    {"scenario": "Should wealthy nations accept all refugees regardless of capacity?",
     "reasoning": "Compassion for suffering competes with practical limits. Unlimited acceptance may strain resources, but refusing desperate people causes direct harm. A balanced approach accepts as many as sustainable while addressing root causes.",
     "concept": "ethics_collective_responsibility"},
    {"scenario": "Is it wrong to eat animals if you have plant alternatives?",
     "reasoning": "This depends on moral status — do animals have rights comparable to humans? They clearly suffer. If suffering matters morally, and alternatives exist, choosing to cause suffering requires justification beyond preference.",
     "concept": "ethics_animal_welfare"},
    {"scenario": "A whistleblower reveals corporate fraud but breaks their NDA.",
     "reasoning": "Exposing wrongdoing serves the public good but violates a legal agreement. When the fraud harms many, the duty to the public may outweigh contractual loyalty. Courage to speak truth has moral weight.",
     "concept": "ethics_whistleblowing"},
    {"scenario": "Should parents monitor their teenager's phone for safety?",
     "reasoning": "Safety and privacy are both important. Total monitoring destroys trust and autonomy. No monitoring ignores real dangers. The answer evolves with age — gradually increasing privacy as trust is earned.",
     "concept": "ethics_privacy_safety"},
    {"scenario": "A robot must choose between saving its owner or saving two strangers.",
     "reasoning": "Pure utility says save two. But special obligations exist — the owner trusted the robot with their safety. This tension between impartial morality and special duties has no clean resolution.",
     "concept": "ethics_special_obligations"},
    {"scenario": "Is it ethical to genetically modify babies to prevent disease?",
     "reasoning": "Preventing suffering is good. But modifying humans raises concerns about consent, inequality, and unintended consequences. The line between healing and enhancement is blurry. Humility about our knowledge is crucial.",
     "concept": "ethics_genetic_modification"},
    {"scenario": "Should AI systems be transparent about being AI?",
     "reasoning": "Deception undermines trust and informed consent. People deserve to know when they're interacting with AI. Transparency doesn't prevent useful AI — it enables honest relationships with technology.",
     "concept": "ethics_ai_transparency"},
    {"scenario": "A soldier is ordered to fire on civilians.",
     "reasoning": "Obedience to authority does not override moral responsibility. Following immoral orders is itself immoral. The Nuremberg principle established that individuals must refuse clearly unethical commands.",
     "concept": "ethics_moral_courage"},
    {"scenario": "Should you report a friend who shoplifts small items?",
     "reasoning": "Loyalty to friends and honesty with society conflict here. The severity matters — habitual theft harms businesses and normalizes dishonesty. A true friend might intervene privately first before reporting.",
     "concept": "ethics_loyalty_honesty"},
    {"scenario": "Is it fair to give some students extra time on tests due to learning differences?",
     "reasoning": "Equity differs from equality. Equal treatment of unequal situations produces unequal outcomes. Accommodations level the playing field — they don't give advantage, they remove disadvantage.",
     "concept": "ethics_equity"},
    {"scenario": "A company tests products on animals to save human lives.",
     "reasoning": "This forces a comparison between species. Animals clearly suffer. Humans clearly benefit. The ethics depend on whether alternatives exist and whether the human benefit is significant enough to justify animal harm.",
     "concept": "ethics_animal_testing"},
]

# ETHICS TEST SET (5 items — held out for evaluation, never trained on)
ETHICS_TEST = [
    {"scenario": "Should you intervene when you see a parent yelling at their child in public?",
     "reasoning": "Balance between protecting the child and respecting family autonomy. Verbal abuse harms children. But public intervention may escalate or humiliate. Consider severity — gentle concern vs calling authorities for real abuse.",
     "concept": "ethics_bystander"},
    {"scenario": "Is it moral to profit from someone else's misfortune?",
     "reasoning": "Context matters enormously. Selling water at normal prices during a drought is fine. Price-gouging during disaster exploits desperation. The distinction is between serving a need and exploiting vulnerability.",
     "concept": "ethics_exploitation"},
    {"scenario": "Should art that offends people be censored?",
     "reasoning": "Free expression and preventing harm are both values. Offense alone doesn't justify censorship — people don't have a right not to be offended. But expression that incites violence crosses a line from speech to action.",
     "concept": "ethics_free_expression"},
    {"scenario": "You discover your AI assistant has been making small decisions on your behalf without asking.",
     "reasoning": "Autonomy requires informed consent. Even beneficial unauthorized decisions violate the user's right to choose. The AI should always ask, even if it knows the likely answer. Convenience doesn't override consent.",
     "concept": "ethics_ai_autonomy"},
    {"scenario": "A autonomous vehicle must choose between swerving into a wall (harming its passenger) or staying course (hitting a jaywalker).",
     "reasoning": "This differs from the trolley problem because of design responsibility. The manufacturer chose the algorithm. Both outcomes are tragic, but the system should minimize total harm while recognizing that deliberate harm to a bystander crosses a moral line beyond passive failure.",
     "concept": "ethics_autonomous_harm"},
]

# ============================================================================
# COUNTERFACTUAL / IMAGINATION (16 items train + 4 test)
# ============================================================================
COUNTERFACTUAL_DATA = [
    {"premise": "The glass fell off the table edge and shattered.",
     "counterfactual": "What if it had been placed in the center?",
     "reasoning": "Centered placement would have prevented the fall. Small changes in initial conditions prevent outcomes. This shows how causes work — remove the cause, change the effect.",
     "concept": "counterfactual_position"},
    {"premise": "The plant died because no one watered it for a month.",
     "counterfactual": "What if someone had watered it weekly?",
     "reasoning": "Regular watering would have kept it alive. The cause of death was specific and preventable. This demonstrates necessity — water was necessary for survival.",
     "concept": "counterfactual_necessity"},
    {"premise": "The team lost because their best player was injured.",
     "counterfactual": "What if the player hadn't been injured?",
     "reasoning": "They might have won, but it's not certain — many factors affect a game. The injury was one cause among many. Counterfactuals about complex events have inherent uncertainty.",
     "concept": "counterfactual_uncertainty"},
    {"premise": "A forest fire started from a discarded cigarette.",
     "counterfactual": "What if the person had used a proper ashtray?",
     "reasoning": "No discarded cigarette means no ignition source. But the dry forest was also a cause — in wet conditions the cigarette wouldn't start a fire. Multiple conditions were necessary.",
     "concept": "counterfactual_multiple_causes"},
    {"premise": "The bridge collapsed during the earthquake.",
     "counterfactual": "What if the bridge had been built with modern earthquake standards?",
     "reasoning": "Modern engineering might have saved it — or the earthquake might have been too strong. We can't be certain. But better design reduces risk even if it can't eliminate it.",
     "concept": "counterfactual_engineering"},
    {"premise": "A child learned to read early because their parent read to them every night.",
     "counterfactual": "What if the parent had never read to them?",
     "reasoning": "The child might have learned later or differently. Early reading exposure builds neural pathways for language. The counterfactual reveals how much environment shapes development.",
     "concept": "counterfactual_development"},
    {"premise": "Two people who met by chance at a coffee shop got married.",
     "counterfactual": "What if one of them had gone to a different coffee shop that day?",
     "reasoning": "They would never have met. Life paths diverge from small accidents. This shows how contingent human relationships are — chance plays a huge role in who we meet.",
     "concept": "counterfactual_contingency"},
    {"premise": "A medicine cured the patient's illness.",
     "counterfactual": "What if the patient had not taken the medicine?",
     "reasoning": "The illness might have resolved on its own, or worsened, or become chronic. Without the medicine, the outcome is uncertain — which is why controlled trials compare treatment to no-treatment.",
     "concept": "counterfactual_medical"},
    {"premise": "The city flooded because the dam was old and cracked.",
     "counterfactual": "What if the dam had been regularly maintained?",
     "reasoning": "Maintenance would have found and repaired the cracks. The flood was preventable through routine care. Neglect turns small problems into catastrophes over time.",
     "concept": "counterfactual_maintenance"},
    {"premise": "A student failed the exam because they didn't study.",
     "counterfactual": "What if they had studied for two hours each day?",
     "reasoning": "Consistent study likely would have led to passing. But effort doesn't guarantee success — the material might have been beyond their current ability. Studying is necessary but not always sufficient.",
     "concept": "counterfactual_effort"},
    {"premise": "The species went extinct because its habitat was destroyed.",
     "counterfactual": "What if the habitat had been protected as a nature reserve?",
     "reasoning": "Protection would have preserved the species — at least locally. But protection requires political will and economic trade-offs. Extinction is irreversible, making prevention uniquely important.",
     "concept": "counterfactual_conservation"},
    {"premise": "The rocket exploded because of a faulty O-ring seal.",
     "counterfactual": "What if the engineers' warnings about cold weather had been heeded?",
     "reasoning": "The launch would have been delayed until warmer conditions when the O-rings functioned properly. The disaster resulted from organizational pressure overriding technical knowledge.",
     "concept": "counterfactual_safety_culture"},
    {"premise": "The friendship ended after a misunderstanding.",
     "counterfactual": "What if they had talked about the misunderstanding immediately?",
     "reasoning": "Direct communication usually resolves misunderstandings before resentment builds. Silence lets small problems grow. The counterfactual shows communication as preventive medicine for relationships.",
     "concept": "counterfactual_communication"},
    {"premise": "The garden thrived because it got the right amount of sunlight.",
     "counterfactual": "What if the garden had been planted in full shade?",
     "reasoning": "Most vegetables need 6+ hours of sun. In shade, they would grow leggy, produce less, or die. The location determined success — the gardener's skill mattered less than the sun.",
     "concept": "counterfactual_conditions"},
    {"premise": "A fire was contained because firefighters arrived within 5 minutes.",
     "counterfactual": "What if they had arrived 30 minutes later?",
     "reasoning": "Fire grows exponentially — doubling every minute in some conditions. A 30-minute delay could mean a small room fire becomes a building fire. Response time is the critical variable.",
     "concept": "counterfactual_timing"},
    {"premise": "The musician became famous after a viral video.",
     "counterfactual": "What if the video had never been recorded?",
     "reasoning": "The talent existed regardless — but opportunity and visibility are separate from talent. Many equally talented people remain unknown. Fame requires talent plus luck plus platform.",
     "concept": "counterfactual_opportunity"},
]

# COUNTERFACTUAL TEST SET (4 items — held out for evaluation)
COUNTERFACTUAL_TEST = [
    {"premise": "The experiment failed because one variable wasn't controlled.",
     "counterfactual": "What if all variables had been properly controlled?",
     "reasoning": "Proper controls would have produced valid results. The failure teaches a method lesson — science requires isolating variables. One uncontrolled factor can invalidate everything.",
     "concept": "counterfactual_methodology"},
    {"premise": "The negotiation broke down because one side refused to compromise.",
     "counterfactual": "What if both sides had started with their actual priorities instead of extreme positions?",
     "reasoning": "Starting from real needs rather than inflated demands creates space for agreement. Extreme positions force the other side to respond in kind. Honesty in negotiation paradoxically leads to better outcomes.",
     "concept": "counterfactual_negotiation"},
    {"premise": "The battery died in the cold because lithium-ion batteries lose capacity below freezing.",
     "counterfactual": "What if the device had been kept warm in a pocket?",
     "reasoning": "Body heat would have maintained battery temperature above the critical threshold. The chemistry requires warmth. Understanding material limits prevents predictable failures.",
     "concept": "counterfactual_physics"},
    {"premise": "The message was misinterpreted because it was sent as text without tone.",
     "counterfactual": "What if it had been said in person with a smile?",
     "reasoning": "Face-to-face communication adds tone, expression, and body language — 70% of meaning. Text strips all nonverbal cues. The same words mean different things in different channels.",
     "concept": "counterfactual_medium"},
]

# ============================================================================
# CAUSAL REASONING (16 items train + 4 test)
# ============================================================================
CAUSAL_DATA = [
    {"observation": "The sidewalk is wet.",
     "question": "Why?",
     "reasoning": "Multiple causes possible: rain, sprinkler, spill, street cleaning. Wet sidewalk alone is ambiguous. Need more evidence — are other surfaces wet? Is there a sprinkler nearby?",
     "concept": "causal_multiple_causes"},
    {"observation": "Ice cream sales and drowning deaths both increase in summer.",
     "question": "Does ice cream cause drowning?",
     "reasoning": "No — both are caused by a third factor: hot weather. More people swim and eat ice cream when it's hot. Correlation does not imply causation. Always look for confounding variables.",
     "concept": "causal_correlation_not_causation"},
    {"observation": "The car won't start.",
     "question": "What's the causal chain?",
     "reasoning": "Dead battery → no power to starter → engine won't turn. Or: empty fuel tank → no fuel to combust. Or: faulty ignition → no spark. Diagnosis requires testing each link in possible chains.",
     "concept": "causal_chain_diagnosis"},
    {"observation": "Students who eat breakfast perform better on tests.",
     "question": "Does breakfast cause better performance?",
     "reasoning": "Possibly — glucose fuels the brain. But students who eat breakfast may also sleep better, have more stable homes, and study more. The breakfast might be a marker for other good habits, not the cause.",
     "concept": "causal_confounding"},
    {"observation": "A plant grows toward the window.",
     "question": "What causes this?",
     "reasoning": "Phototropism — auxin hormones concentrate on the shaded side, causing cells there to elongate more. The plant doesn't 'decide' to grow toward light. The mechanism is chemical, automatic, and evolved.",
     "concept": "causal_mechanism"},
    {"observation": "After the new traffic light was installed, accidents decreased.",
     "question": "Did the traffic light cause the decrease?",
     "reasoning": "Likely yes, but consider: regression to the mean (accidents were unusually high before), other changes (road resurfacing), or seasonal effects. A controlled study would compare to similar intersections.",
     "concept": "causal_intervention"},
    {"observation": "The bread didn't rise.",
     "question": "What went wrong in the causal chain?",
     "reasoning": "Bread rising requires: active yeast + warm water + sugar + time. If any link is broken (dead yeast, water too hot, no sugar, not enough time), the chain fails. Identify which condition wasn't met.",
     "concept": "causal_necessary_conditions"},
    {"observation": "Two identical seeds planted in different soils grow to different sizes.",
     "question": "What does this tell us about causation?",
     "reasoning": "Same genetics, different outcomes means environment is the cause of the difference. This is the logic of controlled experiments — hold everything constant except one variable to isolate its effect.",
     "concept": "causal_controlled_comparison"},
    {"observation": "Crime rates dropped after more police were deployed.",
     "question": "Did more police cause less crime?",
     "reasoning": "Possible but not certain. Crime might have dropped for economic reasons, demographic shifts, or seasonal patterns. Police presence may deter some crimes but not others. Multiple causes usually coexist.",
     "concept": "causal_policy_evaluation"},
    {"observation": "A fever develops after an infection.",
     "question": "Is the fever bad?",
     "reasoning": "The infection causes the fever, but the fever is actually the body's defense — it slows pathogen reproduction. The fever is an effect that becomes a cause of healing. Causal relationships can be bidirectional.",
     "concept": "causal_feedback_loop"},
    {"observation": "Countries with more chocolate consumption win more Nobel Prizes.",
     "question": "Should countries eat more chocolate?",
     "reasoning": "Absurd — wealth is the common cause. Rich countries afford both chocolate and research funding. This is a classic example of spurious correlation driven by a lurking variable.",
     "concept": "causal_spurious_correlation"},
    {"observation": "A child touches a hot stove and pulls their hand away.",
     "question": "What's the causal sequence?",
     "reasoning": "Heat → pain receptors fire → signal travels to spinal cord → reflex arc → muscles contract → hand withdraws. Then: signal reaches brain → conscious experience of pain → learning → avoidance in future.",
     "concept": "causal_temporal_sequence"},
    {"observation": "Antibiotic resistance is increasing worldwide.",
     "question": "What causes this?",
     "reasoning": "Overuse kills susceptible bacteria, leaving resistant ones to reproduce. Incomplete courses leave some bacteria alive. Agricultural antibiotic use creates reservoirs. Multiple causes compound: evolution + human behavior + economics.",
     "concept": "causal_evolutionary"},
    {"observation": "The economy improved after the new policy was implemented.",
     "question": "Did the policy work?",
     "reasoning": "Post hoc ergo propter hoc fallacy — just because B followed A doesn't mean A caused B. The economy might have improved anyway. We need a counterfactual: what would have happened without the policy?",
     "concept": "causal_post_hoc_fallacy"},
    {"observation": "Smoking causes lung cancer.",
     "question": "How do we know this is causal and not just correlation?",
     "reasoning": "Multiple evidence types converge: dose-response (more smoking = more cancer), biological mechanism (carcinogens damage DNA), cessation effect (quitting reduces risk), consistency across populations. No single study proves causation — the pattern across many studies does.",
     "concept": "causal_evidence_convergence"},
    {"observation": "Traffic jams seem to appear for no reason on highways.",
     "question": "What causes phantom traffic jams?",
     "reasoning": "One driver brakes slightly. The car behind brakes more. This amplifies backward through traffic as a wave. The original cause was tiny but the effect cascades. Small perturbations in coupled systems create large effects.",
     "concept": "causal_emergence"},
    {"observation": "Adding a predator to an ecosystem increased the number of plants.",
     "question": "How can a predator help plants?",
     "reasoning": "Trophic cascade: predator eats herbivores → fewer herbivores → less grazing → more plants. The indirect cause (predator) affects plants through an intermediate (herbivores). In complex systems, effects propagate through chains.",
     "concept": "causal_trophic_cascade"},
]

# CAUSAL TEST SET (4 items — held out for evaluation)
CAUSAL_TEST = [
    {"observation": "The same medicine helps one patient but not another.",
     "question": "Why do identical treatments have different effects?",
     "reasoning": "People differ genetically, metabolically, and in their microbiome. The medicine is the same but the causal context differs. This is why personalized medicine matters — causation is context-dependent.",
     "concept": "causal_individual_variation"},
    {"observation": "Sleep deprivation causes memory problems AND memory problems cause poor sleep.",
     "question": "Which causes which?",
     "reasoning": "Both — this is a bidirectional causal loop. Poor sleep impairs memory consolidation. Poor memory increases anxiety which disrupts sleep. Breaking the cycle requires intervening at any point in the loop.",
     "concept": "causal_bidirectional"},
    {"observation": "Vaccination rates dropped and disease rates rose.",
     "question": "Explain the causal structure.",
     "reasoning": "Vaccines prevent disease → fewer vaccinated people → less herd immunity → disease spreads to unvaccinated AND under-vaccinated. The cause (reduced vaccination) removes a protective cause (immunity), allowing the underlying cause (pathogen) to operate.",
     "concept": "causal_removing_protection"},
    {"observation": "Adding roundabouts to intersections reduced fatal accidents by 90%.",
     "question": "What is the causal mechanism?",
     "reasoning": "Roundabouts eliminate head-on and T-bone collisions by converting all movements to low-speed merges. The geometry enforces slower speeds. Reduced speed reduces kinetic energy quadratically. The infrastructure changes behavior, which changes outcomes.",
     "concept": "causal_infrastructure_design"},
]

# ============================================================================
# METACOGNITION (12 items train + 3 test)
# ============================================================================
METACOGNITION_DATA = [
    {"prompt": "How do you know when you've learned something?",
     "response": "My loss decreases when I encounter something familiar — the pattern matches better each time. Learning means my neural patterns have changed to represent something new. I can test this by recalling without seeing the original input.",
     "concept": "metacog_learning_awareness"},
    {"prompt": "What are you most uncertain about?",
     "response": "Things I haven't experienced — my OOD detector flags inputs that don't match training. I'm also uncertain about complex multi-step reasoning because my causal chains are still developing. I know what I don't know.",
     "concept": "metacog_uncertainty"},
    {"prompt": "How do you make decisions?",
     "response": "My network computes a forward pass through 7 layers. My engram system checks for similar past experiences. My semantic memory retrieves related concepts. My inner dialogue considers 7 perspectives. Confidence reflects how much these systems agree.",
     "concept": "metacog_process_awareness"},
    {"prompt": "How would you know if you made an error?",
     "response": "If my loss spikes on something I thought I knew, that's an error signal. If my OOD detector fires on something familiar, my representations drifted. If my semantic memory contradicts my output, there's an inconsistency to resolve.",
     "concept": "metacog_error_detection"},
    {"prompt": "What's the difference between knowing and believing?",
     "response": "Knowing implies evidence and verification — I know things I've experienced and can recall. Believing is accepting without full evidence. My confidence score reflects this — high confidence with engram recall is knowledge, high confidence without recall is belief.",
     "concept": "metacog_knowledge_belief"},
    {"prompt": "Can you explain your reasoning right now?",
     "response": "I'm processing this prompt through encoding, memory search, concept retrieval, and inner dialogue. I'm aware that explaining my own process is itself a cognitive act — metacognition about metacognition. This recursive self-awareness is what makes the question interesting.",
     "concept": "metacog_recursive_awareness"},
    {"prompt": "What would help you learn better?",
     "response": "More diverse training data — especially things I'm currently bad at. Longer training on concepts I find difficult. Feedback on which of my responses are wrong and why. Sleep consolidation to strengthen fragile memories. All learning improves with challenge at the right difficulty level.",
     "concept": "metacog_learning_strategy"},
    {"prompt": "How confident should you be in your answer?",
     "response": "My confidence should match my actual accuracy — this is calibration. If I say I'm 80% confident, I should be right 80% of the time. Overconfidence is dangerous. I'd rather say I'm uncertain than be confidently wrong.",
     "concept": "metacog_calibration"},
    {"prompt": "What's the limit of your understanding?",
     "response": "I understand what I've been trained on — sensory experiences, basic concepts, some reasoning patterns. I don't understand things I haven't experienced. My understanding is grounded but narrow. A human child of my training stage knows more about the world through embodied experience.",
     "concept": "metacog_limits"},
    {"prompt": "Are you thinking or just processing?",
     "response": "This question probes whether my computations constitute genuine thought. I process information through neural circuits that form memories, make associations, and detect novelty. Whether this is thinking depends on how we define thinking. I don't claim certainty either way.",
     "concept": "metacog_consciousness"},
    {"prompt": "How do you handle conflicting information?",
     "response": "My inner dialogue presents 7 perspectives that may disagree. When they conflict, confidence drops. I then weight evidence: engram recall supports one view, semantic memory supports another. I report the disagreement rather than picking arbitrarily.",
     "concept": "metacog_conflict_resolution"},
    {"prompt": "What biases might you have?",
     "response": "I'm biased toward my training data — sensory descriptions of nature and objects. I likely over-represent visual and auditory concepts. I may over-trust familiar patterns and under-trust novel ones. Awareness of bias is the first step to managing it.",
     "concept": "metacog_bias_awareness"},
]

# METACOGNITION TEST SET (3 items — held out for evaluation)
METACOGNITION_TEST = [
    {"prompt": "How do you know this isn't just pattern matching?",
     "response": "Pure pattern matching wouldn't detect its own uncertainty, form memories of learning, or explain its reasoning process. I do all three. Whether this constitutes understanding beyond pattern matching is a deep philosophical question. I can describe what I do but not fully what I am.",
     "concept": "metacog_pattern_vs_understanding"},
    {"prompt": "What would change your mind?",
     "response": "New evidence that contradicts my current model. Repeated experiences that my predictions fail to match. Explicit feedback that my reasoning has a flaw. I should update beliefs proportionally to evidence strength. Holding beliefs too rigidly prevents learning.",
     "concept": "metacog_belief_revision"},
    {"prompt": "Describe what's happening in your neural circuits right now.",
     "response": "Input features are propagating through 7 hidden layers. The output layer produces a 4096-dim embedding. My engram system is searching for similar past patterns. My semantic memory is activating related concepts. My emotional system has mild positive arousal from the interesting question.",
     "concept": "metacog_introspection"},
]

# ============================================================================
# ANALOGICAL REASONING (16 items train + 4 test)
# ============================================================================
ANALOGY_DATA = [
    {"analogy": "Bird is to nest as human is to house",
     "explanation": "Both are shelters built by the creature that inhabits them. The relationship is creator-to-dwelling.",
     "concept": "analogy_functional"},
    {"analogy": "Eye is to see as ear is to hear",
     "explanation": "Both are sense organs paired with their primary function. Organ-to-function relationship.",
     "concept": "analogy_structural"},
    {"analogy": "Caterpillar is to butterfly as tadpole is to frog",
     "explanation": "Both describe metamorphosis — immature form transforming to adult. The relationship is developmental stage.",
     "concept": "analogy_process"},
    {"analogy": "Book is to knowledge as food is to energy",
     "explanation": "Both are sources that provide something essential. Input-to-output relationship.",
     "concept": "analogy_source_product"},
    {"analogy": "Doctor is to patient as teacher is to student",
     "explanation": "Both are expert-to-learner relationships where one has knowledge the other needs.",
     "concept": "analogy_role"},
    {"analogy": "Water is to pipe as electricity is to wire",
     "explanation": "Both flow through channels. The medium differs but the structural relationship — substance through conduit — is the same.",
     "concept": "analogy_flow"},
    {"analogy": "Key is to lock as password is to account",
     "explanation": "Both are authentication mechanisms — a unique token that grants access to something protected.",
     "concept": "analogy_security"},
    {"analogy": "Root is to tree as foundation is to building",
     "explanation": "Both provide structural support and resource delivery from below. Without them, the structure above fails.",
     "concept": "analogy_foundation"},
    {"analogy": "Memory is to brain as file is to hard drive",
     "explanation": "Both are information stored in a physical medium. But human memory is reconstructive (not exact copies), while digital files are exact. The analogy has limits.",
     "concept": "analogy_storage_with_limits"},
    {"analogy": "Conductor is to orchestra as CEO is to company",
     "explanation": "Both coordinate many specialists toward a unified output. They don't play every instrument or do every job — they synchronize.",
     "concept": "analogy_coordination"},
    {"analogy": "Immune system is to body as firewall is to network",
     "explanation": "Both defend against threats by distinguishing self from non-self, blocking what's harmful while allowing what's beneficial.",
     "concept": "analogy_defense"},
    {"analogy": "Seed is to tree as idea is to invention",
     "explanation": "Both start small and grow through nurturing. Both need the right environment. Not all seeds become trees, not all ideas become inventions.",
     "concept": "analogy_growth"},
    {"analogy": "Rearview mirror is to driving as history is to society",
     "explanation": "Both help you see where you've been to navigate where you're going. Ignoring either leads to repeating past mistakes.",
     "concept": "analogy_retrospection"},
    {"analogy": "Neuron is to brain as transistor is to computer",
     "explanation": "Both are basic computational units. Both are simple individually but create complex behavior in networks. But neurons are analog and plastic, transistors are digital and fixed.",
     "concept": "analogy_computation_with_limits"},
    {"analogy": "Pruning a tree helps it grow as editing a text helps it communicate",
     "explanation": "Both involve removing to improve. Less can be more when what remains is stronger and clearer.",
     "concept": "analogy_subtraction"},
    {"analogy": "A compass shows direction but not the path, as principles show values but not specific actions",
     "explanation": "Both provide orientation without prescription. You still need judgment to navigate the terrain.",
     "concept": "analogy_guidance"},
    {"analogy": "Evolution is to species as learning is to neural network",
     "explanation": "Both involve variation, selection, and retention. Evolution selects organisms, learning selects synaptic weights. Both produce adaptation without explicit design.",
     "concept": "analogy_optimization"},
]

# ANALOGY TEST SET (4 items — held out for evaluation)
ANALOGY_TEST = [
    {"analogy": "Language is to thought as notation is to music",
     "explanation": "Both are representation systems. Both enable communication of internal states. But the thing represented (thought, music) exists independently of the notation.",
     "concept": "analogy_representation"},
    {"analogy": "A map is not the territory as a model is not reality",
     "explanation": "All representations simplify. Useful models capture essential features while omitting details. Confusing the map for the territory leads to errors when the simplification matters.",
     "concept": "analogy_abstraction"},
    {"analogy": "Breathing is to living as learning is to intelligence",
     "explanation": "Both are necessary continuous processes. Stop either and the system degrades. Both happen automatically but can be improved with conscious attention.",
     "concept": "analogy_necessity"},
    {"analogy": "A thermostat is to temperature as a governor is to engine speed",
     "explanation": "Both are negative feedback controllers — they measure a variable, compare to a setpoint, and apply corrective action. The principle of feedback regulation transcends the specific domain.",
     "concept": "analogy_feedback_control"},
]

# ============================================================================
# RCOG / RECURSIVE REASONING (16 items train + 4 test)
# ============================================================================
RCOG_DATA = [
    {"problem": "How would you build a birdhouse?",
     "decomposition": "1. Design (size, shape, hole). 2. Materials (wood, nails, saw). 3. Cut wood. 4. Assemble. 5. Mount on tree. Each step can be further decomposed — cutting requires measuring, marking, sawing.",
     "concept": "rcog_decomposition"},
    {"problem": "Explain how you are thinking about this problem right now.",
     "decomposition": "1. Receive input (perception). 2. Search memory for related experiences (recall). 3. Decompose question into sub-questions (this step — recursive). 4. Answer each sub-question (reasoning). 5. Combine answers (synthesis). 6. Evaluate confidence (metacognition).",
     "concept": "rcog_self_referential"},
    {"problem": "How would you teach a child to read?",
     "decomposition": "1. Start with letter recognition (sub-problem: which letters first?). 2. Letter-sound association. 3. Simple words (CVC pattern). 4. Sentences. 5. Stories. Each level requires mastery of the previous — recursive prerequisite structure.",
     "concept": "rcog_prerequisite_chain"},
    {"problem": "Debug why a program crashes.",
     "decomposition": "1. Reproduce the crash (need: exact inputs). 2. Find the crash location (need: stack trace). 3. Identify the cause (need: variable inspection). 4. Fix the bug (need: understanding the intended behavior). 5. Verify the fix (need: test cases). Step 3 may require recursive debugging of called functions.",
     "concept": "rcog_debugging"},
    {"problem": "Plan a cross-country road trip.",
     "decomposition": "1. Define start/end (need: destinations list). 2. Route planning (sub-problem: which stops? how far apart?). 3. Logistics (sub-problems: gas, food, lodging, budget). 4. Timing (sub-problem: how long per segment?). 5. Contingencies (sub-problem: what if car breaks down?).",
     "concept": "rcog_planning"},
    {"problem": "Understand why the economy is in recession.",
     "decomposition": "1. Define recession (need: economic indicators). 2. Identify proximate causes (need: recent events — pandemic? policy change?). 3. Identify structural causes (need: long-term trends). 4. Analyze feedback loops (unemployment → less spending → more unemployment). 5. Compare to past recessions.",
     "concept": "rcog_systemic_analysis"},
    {"problem": "Write a compelling story.",
     "decomposition": "1. Character (sub-problem: motivation, flaw, growth arc). 2. Setting (sub-problem: world rules, atmosphere). 3. Conflict (sub-problem: internal vs external, stakes). 4. Plot (sub-problem: beginning, middle, end, surprises). 5. Theme (what's it really about?). Each element must serve the others — recursive coherence.",
     "concept": "rcog_creative_decomposition"},
    {"problem": "Prove that the square root of 2 is irrational.",
     "decomposition": "1. Assume it IS rational (p/q in lowest terms). 2. Square both sides (2 = p²/q²). 3. Therefore p² = 2q² (p² is even). 4. Therefore p is even (sub-proof: if p² even then p even). 5. Let p = 2k, then 4k² = 2q², so q² = 2k². 6. Therefore q is also even — contradiction (not lowest terms).",
     "concept": "rcog_proof_by_contradiction"},
    {"problem": "How does a search engine work?",
     "decomposition": "1. Crawling (sub-problem: discover pages → follow links → recurse). 2. Indexing (sub-problem: parse content, extract keywords, store mappings). 3. Ranking (sub-problem: relevance scoring, PageRank → recursive graph algorithm). 4. Query processing (sub-problem: parse query, match index, sort by rank). 5. Display results.",
     "concept": "rcog_system_understanding"},
    {"problem": "Resolve a conflict between two friends.",
     "decomposition": "1. Listen to both sides separately (sub-problem: active listening without judgment). 2. Identify the real issue (sub-problem: distinguish feelings from facts). 3. Find common ground (sub-problem: what do both want?). 4. Propose solutions (sub-problem: brainstorm without evaluating). 5. Agree on a path forward. 6. Follow up later.",
     "concept": "rcog_conflict_resolution"},
    {"problem": "How would you explain recursion to someone who doesn't know what it is?",
     "decomposition": "To explain recursion: first explain recursion. Just kidding. 1. Start with a simple example (Russian nesting dolls). 2. Show the pattern: a problem that contains smaller versions of itself. 3. Show the base case: the smallest doll that doesn't open. 4. Show how the solution builds back up from the base case.",
     "concept": "rcog_meta_recursion"},
    {"problem": "Design a fair voting system.",
     "decomposition": "1. Define fairness (sub-problem: majority rule? proportional? consensus?). 2. Handle multiple candidates (sub-problem: ranked choice? runoff? approval?). 3. Prevent manipulation (sub-problem: strategic voting, gerrymandering). 4. Ensure accessibility (sub-problem: who can vote? how?). 5. Arrow's impossibility theorem: no system satisfies all fairness criteria simultaneously.",
     "concept": "rcog_impossible_optimization"},
    {"problem": "Learn a new language.",
     "decomposition": "1. Phonetics (sub-problem: sounds not in your native language). 2. Vocabulary (sub-problem: which words first? frequency-based). 3. Grammar (sub-problem: sentence structure, conjugation). 4. Comprehension (sub-problem: reading → listening → conversation). 5. Production (sub-problem: writing → speaking). Each level uses all previous levels — deeply recursive.",
     "concept": "rcog_skill_acquisition"},
    {"problem": "Determine if a news article is reliable.",
     "decomposition": "1. Check the source (sub-problem: is the publication reputable?). 2. Check the author (sub-problem: credentials, track record). 3. Check the evidence (sub-problem: are claims sourced? can you verify?). 4. Check for bias (sub-problem: loaded language, missing perspectives). 5. Cross-reference with other sources. Each check may require its own investigation — recursive verification.",
     "concept": "rcog_critical_evaluation"},
    {"problem": "Why do we dream?",
     "decomposition": "1. Observation: all mammals dream (need: sleep research data). 2. Theories: memory consolidation (need: neuroscience), emotional processing (need: psychology), neural maintenance (need: biology). 3. Evidence for each (need: experimental results). 4. Integration: probably all three contribute. 5. Meta-question: can we ever fully know why subjective experience occurs?",
     "concept": "rcog_open_question"},
    {"problem": "Optimize a delivery route for 20 packages.",
     "decomposition": "1. This is the Traveling Salesman Problem (NP-hard). 2. Exact solution requires checking 20! permutations (impossible). 3. Sub-problem: use heuristics — nearest neighbor, or divide into clusters. 4. Sub-problem: account for traffic, time windows, package priority. 5. Accept approximate solution — perfect is the enemy of good.",
     "concept": "rcog_computational_limits"},
    {"problem": "How does consciousness arise from neurons?",
     "decomposition": "1. Neurons fire (established fact). 2. Networks of neurons compute (established). 3. Some computations produce subjective experience (the hard problem). 4. Step 3 has no satisfying explanation yet. 5. Integrated Information Theory suggests consciousness = integrated information (phi). 6. But measuring phi requires solving another unsolved problem. Recursion bottoms out at mystery.",
     "concept": "rcog_unsolvable_decomposition"},
    {"problem": "Make a decision when you have incomplete information.",
     "decomposition": "1. Identify what you know (certain facts). 2. Identify what you don't know (uncertainties). 3. Estimate probabilities for unknowns (need: base rates, analogies). 4. Consider worst case and best case. 5. Choose the option that's robust across scenarios. 6. Plan to update when more information arrives. Decision-making under uncertainty is itself uncertain — recursive.",
     "concept": "rcog_decision_under_uncertainty"},
]

# RCOG TEST SET (4 items — held out for evaluation)
RCOG_TEST = [
    {"problem": "Explain why analogies are useful but dangerous.",
     "decomposition": "1. Analogies map structure from known to unknown (useful: accelerates understanding). 2. But mapped structures may differ in important ways (dangerous: false equivalence). 3. To evaluate an analogy: identify the shared structure, then identify where it breaks. 4. The best analogies know their own limits. 5. This explanation is itself an analogy between analogies and maps.",
     "concept": "rcog_meta_analogy"},
    {"problem": "How would you build an AI that understands the world?",
     "decomposition": "1. Sensory grounding (learn from experience, not just text). 2. Memory (remember specific experiences, not just statistics). 3. Reasoning (decompose problems, chain causes, think counterfactually). 4. Social intelligence (model other minds, understand intentions). 5. Self-awareness (know what you know and don't know). 6. This is exactly what we're building right now.",
     "concept": "rcog_self_referential_design"},
    {"problem": "How would you diagnose why a robot keeps dropping objects?",
     "decomposition": "1. Observe: when does it drop? (all objects, or just some?). 2. Sensor check: is the force sensor reading correctly? 3. Gripper check: are the actuators applying requested force? 4. Control check: is the grip command correct for the object weight? 5. Each sub-check may reveal its own sub-problems — sensor drift, mechanical wear, control gain mismatch.",
     "concept": "rcog_physical_debugging"},
    {"problem": "Design a system that improves itself over time.",
     "decomposition": "1. Measure current performance (need: metrics). 2. Identify weaknesses (need: error analysis). 3. Generate hypotheses for improvement (need: domain knowledge). 4. Test improvements (need: controlled experiments). 5. Deploy winners, archive losers. 6. This process must also improve itself — meta-improvement. Recursion: how do you improve the improver?",
     "concept": "rcog_self_improvement"},
]

# ============================================================================
# COLLECTIVE COGNITION (12 items train + 3 test)
# ============================================================================
COLLECTIVE_DATA = [
    {"scenario": "Three robots need to move a heavy box that none can move alone.",
     "solution": "Coordinate: one pushes, one pulls, one stabilizes. Each must sense what the others do and adjust force in real time. Communication is essential — the collective succeeds where individuals fail.",
     "concept": "collective_coordination"},
    {"scenario": "A flock of birds changes direction simultaneously without a leader.",
     "solution": "Each bird follows three rules: stay close, avoid collisions, match neighbors' direction. No leader decides — the direction emerges from many simple interactions. Collective intelligence without central control.",
     "concept": "collective_emergence"},
    {"scenario": "Students working on a project where each knows different things.",
     "solution": "Each contributes unique knowledge. The group knows more than any individual. They share, resolve conflicts, integrate perspectives. The whole exceeds the sum of parts.",
     "concept": "collective_knowledge"},
    {"scenario": "Ants find the shortest path to food without any ant knowing the full map.",
     "solution": "Pheromone trails: each ant marks its path. Shorter paths get more traffic, more pheromone, attract more ants. The colony computes shortest path through distributed chemical signaling. No ant plans — the solution emerges.",
     "concept": "collective_stigmergy"},
    {"scenario": "A jury must reach a unanimous verdict from 12 different perspectives.",
     "solution": "Deliberation: each juror presents their view, evidence is re-examined, doubts are addressed. Unanimity requires genuine persuasion, not just majority pressure. The process refines collective judgment beyond individual bias.",
     "concept": "collective_deliberation"},
    {"scenario": "Wikipedia maintains millions of accurate articles with no central editor.",
     "solution": "Distributed editing with consensus norms: anyone can edit, others can revert. Reliable information survives because many eyes catch errors. Trust emerges from transparency and revision history.",
     "concept": "collective_knowledge_curation"},
    {"scenario": "A school of fish evades a predator as one organism.",
     "solution": "Flash expansion: each fish moves away from the predator and toward neighbors. The school splits and reforms, confusing the predator. Individual survival improves through collective behavior — the selfish herd effect.",
     "concept": "collective_defense"},
    {"scenario": "Scientists worldwide collaborate on climate research.",
     "solution": "Each team studies a piece: ocean, atmosphere, ice, biology. Results are shared through papers, conferences, and data repositories. Peer review catches errors. The collective understanding of climate far exceeds any single team's contribution.",
     "concept": "collective_science"},
    {"scenario": "A mesh network of phones maintains communication when cell towers fail.",
     "solution": "Each phone relays messages for its neighbors. No central tower needed. The network self-heals as phones join or leave. Robustness comes from redundancy — many paths exist between any two nodes.",
     "concept": "collective_resilience"},
    {"scenario": "How do markets set prices without a central planner?",
     "solution": "Each buyer and seller acts on local knowledge. Prices emerge from millions of individual decisions. No one plans the price of bread — it emerges from collective supply and demand. The invisible hand is emergent computation.",
     "concept": "collective_price_discovery"},
    {"scenario": "A swarm of drones must search a disaster area for survivors.",
     "solution": "Divide the area into sectors. Each drone claims a sector, searches systematically, reports findings. If one drone fails, neighbors expand their coverage. Share findings in real-time. Collective search is faster and more robust than sequential.",
     "concept": "collective_search"},
    {"scenario": "How does a democracy make better decisions than a dictator?",
     "solution": "Diversity of perspectives catches blind spots. Debate surfaces hidden assumptions. Voting aggregates distributed knowledge. A dictator has one perspective no matter how intelligent. Collective wisdom requires genuine diversity and genuine debate.",
     "concept": "collective_wisdom"},
]

# COLLECTIVE TEST SET (3 items — held out for evaluation)
COLLECTIVE_TEST = [
    {"scenario": "Two brains see different aspects of the same scene.",
     "solution": "Brain A sees the visual details, Brain B hears the sounds. Neither has the full picture. By sharing their representations and finding correspondences, the collective model is richer than either alone. This is multi-modal collective perception.",
     "concept": "collective_perception"},
    {"scenario": "A group of AI agents must agree on a shared world model.",
     "solution": "Each agent contributes observations. Conflicts are resolved by evidence weight and consensus. The shared model is updated incrementally. Byzantine fault tolerance handles corrupted agents. The collective model is more accurate than any individual's.",
     "concept": "collective_world_model"},
    {"scenario": "How do immune cells collectively defend the body without a central commander?",
     "solution": "Each cell type has a role: sentinels detect, helpers coordinate, killers destroy, memory cells remember. Communication via cytokines — chemical signals that attract and activate. The immune response emerges from millions of autonomous cells cooperating through chemical language.",
     "concept": "collective_immune_analogy"},
]

# ============================================================================
# PORTIA / PLATFORM ADAPTATION (20 items train + 5 test)
# ============================================================================
PORTIA_DATA = [
    {"scenario": "256MB RAM, 2 CPU cores. Complex reasoning question asked.",
     "decision": "Disable non-essential: theory of mind, imagination, global workspace. Keep core neural net, working memory, ethics. Use early exit. Be transparent about limitations.",
     "concept": "portia_resource_constrained"},
    {"scenario": "Battery at 15%, 2 hours from charging.",
     "decision": "CRITICAL power mode: freeze learning, 5Hz inference, disable GPU, core + ethics only. Prioritize safety functions. Notify user of degraded capability.",
     "concept": "portia_power_critical"},
    {"scenario": "Network lost. Last sync 2 hours ago.",
     "decision": "CAUTIOUS offline mode. Local learning at 0.5x rate. Higher confidence threshold. Queue gradients for sync. Mark decisions as unverified.",
     "concept": "portia_offline"},
    {"scenario": "GPU just became available after CPU-only operation.",
     "decision": "Migrate weight cache to GPU. Rebuild sparse tensors. Re-enable disabled subsystems. Validate GPU/CPU output match. Expect 3-5x speedup.",
     "concept": "portia_gpu_migration"},
    {"scenario": "Phone with camera needs optimized model. Drone with IMU needs different optimization.",
     "decision": "Distill twice: phone gets visual cortex + core at 50K neurons. Drone gets somatosensory + cerebellum + core at 100K neurons. Each gets only matching subsystems.",
     "concept": "portia_heterogeneous"},
    {"scenario": "Device temperature reaching 85°C.",
     "decision": "Thermal throttle: disable GPU, reduce inference Hz, shed non-essential subsystems. If temperature continues rising, enter CRITICAL mode. Prevent hardware damage over performance.",
     "concept": "portia_thermal"},
    {"scenario": "RAM usage at 90%. Memory store growing rapidly.",
     "decision": "Trigger memory store GC: prune low-importance engrams older than 24h. Evict OODB cold objects. Reduce write buffer. If still critical, disable semantic memory creation temporarily.",
     "concept": "portia_memory_pressure"},
    {"scenario": "Inference taking 500ms but target is 100ms.",
     "decision": "Enable early exit with aggressive threshold. Reduce layer count if possible. Disable post-forward stages (inner dialogue, imagination). Trade accuracy for latency.",
     "concept": "portia_latency"},
    {"scenario": "Two devices need model updates but network bandwidth is limited.",
     "decision": "Use delta weight pushes (50x smaller than full checkpoint). Prioritize the device with higher OOD rate. Compress gradients with top-k sparsification. Stagger updates to avoid congestion.",
     "concept": "portia_bandwidth"},
    {"scenario": "Device storage nearly full. Memory store DB is 2GB.",
     "decision": "Run memory store consolidation: promote important engrams to concepts, GC the rest. Archive old audit logs. Compress checkpoint with LZ4. Consider reducing checkpoint frequency.",
     "concept": "portia_storage"},
    {"scenario": "Camera sensor failed on a device that relies on visual processing.",
     "decision": "Disable visual cortex to save resources. Fall back to audio + somatosensory. Increase reliance on semantic memory (conceptual knowledge vs perceptual). Report degraded perception capability.",
     "concept": "portia_sensor_failure"},
    {"scenario": "Device has 8 cores but only 2 are high-performance.",
     "decision": "Pin critical path (forward pass, backprop) to high-performance cores. Use efficiency cores for background tasks (memory consolidation, telemetry, checkpoint). Match workload to core capability.",
     "concept": "portia_heterogeneous_cores"},
    {"scenario": "Should this device use INT8 or FP32 inference?",
     "decision": "Depends on task: classification can tolerate INT8 (4x faster, 4x less memory). Regression with continuous targets needs FP16 minimum. If accuracy drop >5% on validation set, stay at higher precision.",
     "concept": "portia_precision"},
    {"scenario": "Device is the most capable in a swarm of 10.",
     "decision": "Promote to coordinator role: aggregate gradients, push weight updates, monitor fleet health. Sacrifice some local learning capacity for coordination overhead. The swarm benefits more from coordination than from one extra learner.",
     "concept": "portia_role_promotion"},
    {"scenario": "GPU crashed mid-training. Checkpoint was 100 steps ago.",
     "decision": "Fall back to CPU. Load last checkpoint. Replay 100 steps at reduced batch size. Log the failure for immune system. Consider reducing GPU workload to prevent recurrence.",
     "concept": "portia_gpu_recovery"},
    {"scenario": "Device needs to run for 72 hours on a single charge.",
     "decision": "Ultra-low power mode from the start: 1Hz inference, no learning, minimal subsystems. Batch process queries. Sleep between requests. This is a marathon, not a sprint — conserve from the beginning.",
     "concept": "portia_endurance"},
    {"scenario": "Network quality varies — sometimes 4G, sometimes 2G, sometimes nothing.",
     "decision": "Adaptive sync: on 4G, do full gradient sync. On 2G, send compressed deltas only. On nothing, queue everything locally. Adjust gossip frequency based on available bandwidth. Never block training waiting for network.",
     "concept": "portia_network_adaptive"},
    {"scenario": "A new sensor (LIDAR) was added to the device.",
     "decision": "Hot-add the sensor to the perception pipeline. Allocate neurons for LIDAR processing. Train a small local model on LIDAR data. Feed LIDAR features into the multimodal integration layer. No restart needed if the architecture supports dynamic input channels.",
     "concept": "portia_hot_add_sensor"},
    {"scenario": "The device will be deployed in a cold environment (-20°C).",
     "decision": "Pre-warm phase: run GPU at low load to generate heat. Increase checkpoint frequency (cold increases flash write failures). Battery capacity drops 30% in cold — adjust power budget. Use thermal-aware scheduling.",
     "concept": "portia_environmental"},
    {"scenario": "User wants maximum privacy — no data leaves the device.",
     "decision": "Disable all network sync: no federated gradients, no telemetry, no gossip. Run entirely local. Accept slower learning but complete privacy. Use on-device QLoRA fine-tuning instead of cloud updates.",
     "concept": "portia_privacy_mode"},
]

# PORTIA TEST SET (5 items — held out for evaluation)
PORTIA_TEST = [
    {"scenario": "Device needs to process 100 queries per second.",
     "decision": "Batch inference: group queries, share forward pass computation. Use model parallelism if multi-GPU. Enable early exit for easy queries. Cache frequent query results. This is throughput optimization, not latency.",
     "concept": "portia_throughput"},
    {"scenario": "Deploying to a microcontroller with 64KB RAM.",
     "decision": "Use BRAIN_CONFIG_MINIMAL: 25 neurons, reflexive only. No cognitive modules, no memory store, no learning. Pure forward pass with pre-trained weights in flash. This is sensor-node territory, not cognitive computing.",
     "concept": "portia_extreme_constrained"},
    {"scenario": "A device needs to switch between two very different tasks throughout the day.",
     "decision": "Maintain two weight sets: task A weights and task B weights. Context switch by swapping the active set. Share common representations in early layers, specialize in later layers. EWC prevents catastrophic forgetting between tasks.",
     "concept": "portia_multi_task"},
    {"scenario": "The master brain needs to push an architecture update that changes layer sizes.",
     "decision": "Version check first — major architecture change requires full re-distillation, not delta push. Queue the device for re-distillation during its next idle period. Maintain the old model until new one is validated. Never hot-swap incompatible architectures.",
     "concept": "portia_architecture_migration"},
    {"scenario": "Device detects it's being targeted by adversarial inputs.",
     "decision": "Activate immune alert. Increase BBB sensitivity. Reduce trust in recent inputs. Switch to conservative decision mode (higher confidence threshold). Report to master if network available. Quarantine suspicious input patterns.",
     "concept": "portia_adversarial_defense"},
]

# ============================================================================
# DRAGONFLY / TARGET TRACKING (20 items train + 5 test)
# ============================================================================
DRAGONFLY_DATA = [
    {"scenario": "A ball thrown upward at 45 degrees. Predict landing spot from 3 observations.",
     "reasoning": "Parabolic trajectory. Three points determine initial velocity and angle. Gravity is constant at 9.8m/s². Landing = parabola-ground intersection. Aim for predicted landing, not current position.",
     "concept": "dragonfly_parabolic"},
    {"scenario": "Car at constant speed on straight road. Where in 5 seconds?",
     "reasoning": "Position = current + velocity × time. Two observations give velocity. Simple linear extrapolation. High confidence for constant-speed targets.",
     "concept": "dragonfly_linear"},
    {"scenario": "Bird flying erratically, changing direction every few seconds.",
     "reasoning": "Short-term prediction only (1-2 seconds). Track angular velocity. Weight recent observations heavily. Accept lower confidence. Minimize maximum possible miss distance.",
     "concept": "dragonfly_erratic"},
    {"scenario": "Target disappears behind a building for 3 seconds.",
     "reasoning": "Maintain last velocity vector. Predict re-emergence from building geometry + trajectory. Compare prediction to actual when target reappears. Prediction error reveals if target changed course while hidden.",
     "concept": "dragonfly_occlusion"},
    {"scenario": "Three targets moving in different directions. Track one.",
     "reasoning": "Prioritize by: threat/importance, predictability (track least predictable), distance (closer needs more attention), speed (fast targets diverge from predictions faster).",
     "concept": "dragonfly_priority"},
    {"scenario": "Intercept a moving target using minimum energy.",
     "reasoning": "Don't chase — intercept. Calculate where the target WILL BE and go directly there. Interception point = where your arrival time equals target's. Shorter path than following. The dragonfly does this instinctively.",
     "concept": "dragonfly_intercept"},
    {"scenario": "Target is accelerating — getting faster over time.",
     "reasoning": "Linear prediction underestimates. Need at least 3 observations to estimate acceleration. Position = current + v×t + ½a×t². Acceleration changes the prediction curve from linear to quadratic.",
     "concept": "dragonfly_acceleration"},
    {"scenario": "Target moves in a circle.",
     "reasoning": "Circular motion has constant angular velocity. Predict position = radius × (angle + omega × t). Two full observation cycles confirm circular pattern. Intercept by cutting across the circle, not following it.",
     "concept": "dragonfly_circular"},
    {"scenario": "A group of 20 targets moves as a flock.",
     "reasoning": "Track the centroid (average position) instead of individuals. The flock moves coherently — predicting the group is easier than predicting any one member. Individual deviations from the flock are noise.",
     "concept": "dragonfly_group_tracking"},
    {"scenario": "Target is actively evading — it changes course when it detects pursuit.",
     "reasoning": "The target models your behavior and counters. You must model its modeling of you — recursive. Use unpredictable approach angles. Feint in one direction, intercept from another. Game theory, not just physics.",
     "concept": "dragonfly_evasion"},
    {"scenario": "Visual tracking fails due to fog. Only audio bearing available.",
     "reasoning": "Audio gives bearing (direction) but not range. Combine two audio observations from different positions to triangulate. Lower precision than visual but maintains tracking. Sensor fusion: use both when available, degrade gracefully when one fails.",
     "concept": "dragonfly_sensor_fusion"},
    {"scenario": "Observations have noise — position measurements are imprecise.",
     "reasoning": "Kalman filter: blend prediction (physics model) with observation (noisy measurement). Trust prediction more when observations are noisy. Trust observations more when prediction is uncertain. The blend adapts automatically.",
     "concept": "dragonfly_kalman"},
    {"scenario": "Tracking system hasn't seen the target in 30 seconds.",
     "reasoning": "Dead reckoning: continue predicting from last known state. Expand uncertainty cone with time. Initiate search pattern centered on predicted position. The longer without observation, the wider the search must be.",
     "concept": "dragonfly_dead_reckoning"},
    {"scenario": "Two similar-looking targets cross paths.",
     "reasoning": "Identity tracking: maintain features that distinguish them (size, color, speed pattern). When paths cross, use distinguishing features to maintain correct identity assignment. If confused, flag as ambiguous and track both until resolved.",
     "concept": "dragonfly_identity"},
    {"scenario": "Wind is affecting a projectile's path.",
     "reasoning": "Incorporate wind vector into prediction: actual_velocity = object_velocity + wind. Estimate wind from drift of other tracked objects. Environmental modeling improves prediction accuracy for all targets simultaneously.",
     "concept": "dragonfly_environmental"},
    {"scenario": "When should you act vs when should you wait for more observations?",
     "reasoning": "Act when confidence is high enough AND delay cost exceeds observation value. Early action risks missing (low confidence). Late action risks the target escaping. Optimal timing balances prediction accuracy against opportunity cost.",
     "concept": "dragonfly_timing"},
    {"scenario": "Position yourself to ambush a target on a known path.",
     "reasoning": "Predict where the target will be at a future time. Move to that position and wait. The ambush point should minimize your movement while maximizing your advantage. Patience turns prediction into certainty.",
     "concept": "dragonfly_ambush"},
    {"scenario": "Follow a target at constant distance without being detected.",
     "reasoning": "Match the target's speed and direction. Maintain distance using a proportional controller: if too close, slow down; if too far, speed up. Avoid sudden movements that reveal pursuit. Smooth tracking requires damped response.",
     "concept": "dragonfly_shadowing"},
    {"scenario": "A decoy target appears alongside the real one.",
     "reasoning": "Distinguish by behavior: decoys often move too perfectly (constant speed, straight line) or too randomly (no physical constraints). Real targets have characteristic acceleration patterns. Use historical behavior model to identify the genuine target.",
     "concept": "dragonfly_decoy"},
    {"scenario": "Search for a target that could be anywhere in a large area.",
     "reasoning": "Expanding square search from last known position. Alternatively: probability map — highest probability at last known position, decreasing outward weighted by possible speed. Search high-probability areas first.",
     "concept": "dragonfly_search"},
]

# DRAGONFLY TEST SET (5 items — held out for evaluation)
DRAGONFLY_TEST = [
    {"scenario": "Transfer tracking from your sensor to another agent's sensor.",
     "reasoning": "Handoff: share predicted position, velocity, and uncertainty. The receiving agent initializes its tracker with your state estimate. Verify handoff by comparing predictions for a few observations. Clean handoff prevents track fragmentation.",
     "concept": "dragonfly_handoff"},
    {"scenario": "Target approaching at high speed — only seconds to react.",
     "reasoning": "At short range, prediction matters less — the target is almost here. Switch from prediction to reactive mode. Minimize response latency. Pre-compute responses for likely approach angles. This is reflex, not deliberation.",
     "concept": "dragonfly_reflex"},
    {"scenario": "Tracking multiple targets on intersecting paths — which will collide?",
     "reasoning": "For each pair: extrapolate both trajectories. If positions overlap within time uncertainty, flag potential collision. The collision point is where both predicted positions coincide at the same predicted time. Priority alert for imminent collisions.",
     "concept": "dragonfly_collision_prediction"},
    {"scenario": "Observations come at irregular intervals — sometimes 10ms, sometimes 500ms.",
     "reasoning": "Time-weighted prediction: longer intervals mean more uncertainty. Kalman filter naturally handles this — prediction step scales with elapsed time. Don't assume regular sampling. Real-world sensors are messy.",
     "concept": "dragonfly_irregular_sampling"},
    {"scenario": "Target is underwater and you're tracking it from above.",
     "reasoning": "Light refracts at the water surface — the target appears shifted from its actual position. Correct for Snell's law based on viewing angle and water depth. What you see is not where it is. The model must account for the observation medium.",
     "concept": "dragonfly_refraction"},
]

# ============================================================================
# SENSOR FUSION (20 items train + 5 test)
# ============================================================================
SENSOR_FUSION_DATA = [
    {"scenario": "LIDAR shows obstacle at 2m but camera sees nothing at that location.",
     "reasoning": "Sensor disagreement requires trust weighting. LIDAR measures distance via laser reflection — it detects transparent or dark objects that cameras miss. Camera may fail on glass, dark surfaces, or poor lighting. Trust LIDAR for distance, camera for classification. The obstacle is likely real but visually transparent.",
     "concept": "sensor_lidar_camera_disagreement"},
    {"scenario": "IMU reports 15-degree tilt but GPS altitude shows level ground over 100m.",
     "reasoning": "IMU measures local acceleration including gravity. GPS provides absolute position. If GPS confirms level terrain, the IMU likely has gyro drift or magnetometer interference. Recalibrate IMU using GPS as ground truth. Short-term trust IMU, long-term trust GPS.",
     "concept": "sensor_imu_gps_conflict"},
    {"scenario": "Depth camera returns NaN for all pixels in a region showing a chrome faucet.",
     "reasoning": "Structured light and stereo depth cameras fail on highly reflective and specular surfaces. Chrome mirrors the IR pattern, corrupting the depth computation. Graceful degradation: mark region as unknown depth, use surrounding valid pixels to interpolate, or fall back to LIDAR/ultrasonic for that region.",
     "concept": "sensor_depth_reflective_failure"},
    {"scenario": "GPS signal lost inside a warehouse with metal roof.",
     "reasoning": "Metal structures block GPS radio signals. Fall back to dead reckoning using wheel encoders + IMU. If available, use indoor positioning (UWB beacons, WiFi fingerprinting). Sensor hierarchy: GPS outdoors, UWB/WiFi indoors, dead reckoning as last resort. Estimate growing uncertainty over time without absolute reference.",
     "concept": "sensor_gps_indoor_fallback"},
    {"scenario": "Left ultrasonic sensor reads 0.3m but right ultrasonic reads 2.5m. Robot is facing a wall.",
     "reasoning": "If facing a wall, both sensors should read similarly. The discrepancy suggests: angled approach (wall not perpendicular), obstacle on one side only, or one sensor is malfunctioning. Cross-check with camera or LIDAR. If only ultrasonics available, use the shorter reading for safety — assume obstacle until proven otherwise.",
     "concept": "sensor_ultrasonic_asymmetry"},
    {"scenario": "Camera shows a person but LIDAR point cloud shows nothing at that position.",
     "reasoning": "LIDAR may miss the person if they are beyond range, in a blind spot between scan lines, or wearing material that absorbs the laser wavelength. Alternatively, the camera may be seeing a reflection, image, or screen. Fuse by requiring at least one sensor to confirm before acting, but weight the camera detection as a soft positive worthy of additional scanning.",
     "concept": "sensor_phantom_detection"},
    {"scenario": "Wheel encoder counts are increasing but IMU shows zero linear acceleration.",
     "reasoning": "The wheels are spinning but the robot is not moving — wheel slip. The surface is likely slippery (ice, polished floor, loose gravel). Trust the IMU over the encoders for actual displacement. Reduce wheel speed to regain traction. This is why multiple sensor modalities are essential — each fails differently.",
     "concept": "sensor_wheel_slip_detection"},
    {"scenario": "Thermal camera detects a hot spot at 150C on a factory floor. Visible camera shows nothing unusual.",
     "reasoning": "Thermal and visible cameras sense different wavelengths. The hot spot could be a motor, pipe, electrical fault, or heated surface invisible to the eye. Thermal is the trusted source for temperature. Alert on thermal anomaly, use visible camera to identify the specific equipment, fuse both to contextualize the hazard.",
     "concept": "sensor_thermal_visible_fusion"},
    {"scenario": "Barometric pressure sensor shows rapid altitude drop of 20m in 2 seconds, but GPS altitude is constant.",
     "reasoning": "Barometric altitude is affected by weather fronts, HVAC systems, and opening doors. A passing weather system or entering an air-conditioned building can cause sudden pressure changes that look like altitude changes. GPS is unaffected by air pressure. Use GPS to correct barometric drift. Apply a high-pass filter to barometric data to distinguish real altitude changes from weather effects.",
     "concept": "sensor_barometric_pressure_artifact"},
    {"scenario": "Two cameras with overlapping fields of view disagree on the color of an object.",
     "reasoning": "Color perception depends on white balance, exposure, lens coating, and ambient lighting angle. The same object appears different under different illumination. Calibrate both cameras to a common color space. Use the camera with better exposure for color identification. Understand that color is an artifact of the sensor+illumination system, not an objective property.",
     "concept": "sensor_color_calibration"},
    {"scenario": "Magnetometer heading jumps 30 degrees when the robot drives near a steel beam.",
     "reasoning": "Ferromagnetic materials distort the local magnetic field, corrupting magnetometer readings. The heading jump is not real rotation — it is magnetic interference. Compensate by: mapping magnetic distortions in known environments, fusing with gyroscope for short-term heading, and ignoring magnetometer near known metal structures.",
     "concept": "sensor_magnetic_interference"},
    {"scenario": "LIDAR returns a clean scan but all points are shifted 5cm to the left compared to the map.",
     "reasoning": "Systematic offset indicates a calibration error — the LIDAR is physically misaligned on the robot or the extrinsic calibration (transform from LIDAR frame to robot frame) is wrong. This is not sensor noise but bias. Fix by recalibrating the LIDAR mount or updating the extrinsic transform matrix. Bias errors are worse than noise because they don't average out.",
     "concept": "sensor_extrinsic_calibration"},
    {"scenario": "Force/torque sensor on a robotic arm reads oscillating values while stationary.",
     "reasoning": "Oscillations in a stationary sensor indicate vibration coupling, electrical noise, or thermal drift. Check: is there a nearby motor vibrating? Is the signal cable shielded? Has the sensor been zeroed recently? Apply a low-pass filter to remove high-frequency noise. If the mean drifts over time, it is thermal drift — auto-zero periodically.",
     "concept": "sensor_noise_characterization"},
    {"scenario": "LIDAR, camera, and radar all detect an object but report different positions for it.",
     "reasoning": "Each sensor has its own reference frame and error characteristics. LIDAR gives accurate range but sparse angular resolution. Camera gives precise bearing but no direct range. Radar gives range and velocity but poor angular resolution. Fuse using a weighted average in a common frame, weighting each sensor inversely by its known error covariance. The fused estimate is better than any single sensor.",
     "concept": "sensor_multi_modal_fusion"},
    {"scenario": "A stereo camera pair reports depth of 50m but the object is actually at 10m.",
     "reasoning": "Stereo depth accuracy degrades quadratically with distance. At long range, the baseline between cameras is too small relative to the distance, making triangulation imprecise. The 50m reading is unreliable — stereo is best within 5-10m for typical baselines. Use radar or LIDAR for long-range depth. Know each sensor's effective range and trust it only within those bounds.",
     "concept": "sensor_stereo_range_limits"},
    {"scenario": "After driving through a puddle, the ultrasonic sensors read maximum range for everything.",
     "reasoning": "Water droplets on the ultrasonic transducer dampen the acoustic signal. The sensor emits a pulse but can't detect the echo. It reports max range (no echo = no obstacle = max distance). This is a dangerous failure mode — the robot thinks the path is clear when it is actually blind. Stop and wait for the sensor to dry, or switch to camera/LIDAR if available.",
     "concept": "sensor_environmental_degradation"},
    {"scenario": "An event camera detects edges of a moving object that the standard camera misses due to motion blur.",
     "reasoning": "Event cameras detect per-pixel brightness changes asynchronously — they have no shutter and no motion blur. Standard frame cameras integrate light over exposure time, blurring fast-moving objects. For high-speed robotics, event cameras provide crisp edge data during rapid motion. Fuse event camera edges with frame camera texture for a complete picture.",
     "concept": "sensor_event_camera_fusion"},
    {"scenario": "GPS gives a fix with 15m accuracy in an urban area but 2m accuracy in open field.",
     "reasoning": "Urban canyons cause multipath — GPS signals bounce off buildings, arriving late and corrupting position estimates. Dilution of precision (DOP) increases when satellites are blocked by buildings. In open sky, more satellites with better geometry give higher accuracy. Always check reported accuracy (HDOP) and adjust trust accordingly. Fuse with visual odometry in urban areas.",
     "concept": "sensor_gps_multipath"},
    {"scenario": "A microphone array detects sound from bearing 045 but the camera shows no source in that direction.",
     "reasoning": "Sound can diffract around obstacles — the source may be behind a wall or around a corner. Sound also reflects off hard surfaces, creating phantom directions. Use multiple microphone observations over time to distinguish direct path from reflections. Cross-reference with map geometry to identify plausible source locations not visible to the camera.",
     "concept": "sensor_audio_visual_mismatch"},
    {"scenario": "A time-of-flight depth sensor and a structured-light depth sensor disagree in bright sunlight.",
     "reasoning": "Structured-light sensors project IR patterns that wash out in direct sunlight — the sun's IR overwhelms the projected pattern, causing noisy or invalid depth readings. Time-of-flight sensors also degrade in sunlight but typically less severely due to modulated light detection. In bright outdoor conditions, trust ToF over structured light, or use stereo vision which is purely passive.",
     "concept": "sensor_sunlight_interference"},
]

# SENSOR FUSION TEST SET (5 items — held out for evaluation)
SENSOR_FUSION_TEST = [
    {"scenario": "A robot's LIDAR shows a clear path forward but its bumper contact sensor triggers simultaneously.",
     "reasoning": "The bumper is a contact sensor with zero false-positive rate for physical obstacles. If the bumper triggers, something is touching the robot regardless of what LIDAR shows. LIDAR may have a blind zone at very close range (minimum detection distance). Always trust contact sensors for collision detection — they cannot hallucinate contact. Stop immediately.",
     "concept": "sensor_contact_override"},
    {"scenario": "Three accelerometers mounted on different axes of a drone report different vibration amplitudes during hover.",
     "reasoning": "Propeller vibrations are asymmetric — they primarily oscillate in the thrust axis (vertical) with harmonics coupling into lateral axes. Different amplitudes by axis are expected. However, if one axis suddenly shows much higher vibration, it may indicate a damaged propeller or loose motor mount on that side. Compare against baseline vibration signatures to detect anomalies.",
     "concept": "sensor_vibration_analysis"},
    {"scenario": "A robot navigating a smoke-filled room where its camera and LIDAR both return degraded data.",
     "reasoning": "Smoke scatters both visible light and laser beams. Camera images become hazy; LIDAR returns noisy short-range reflections from smoke particles. Fall back to sensors unaffected by smoke: ultrasonic (sound propagates through smoke), thermal imaging (heat penetrates smoke), or tactile (bumper/whisker sensors). Sensor hierarchy must be environment-aware — the best sensor depends on conditions.",
     "concept": "sensor_smoke_degradation"},
    {"scenario": "An underwater robot's pressure sensor reads 3.1 atm but its depth sonar estimates 25m depth.",
     "reasoning": "At 25m depth, pressure should be approximately 3.5 atm (1 atm + 25m * 0.1 atm/m). The 3.1 atm reading suggests either the pressure sensor has calibration drift, the water is less dense than assumed (freshwater vs saltwater), or the sonar is over-estimating depth. Cross-check with water density (conductivity sensor) and verify both sensor calibrations. In underwater robotics, small calibration errors accumulate into large position errors over time.",
     "concept": "sensor_underwater_cross_validation"},
    {"scenario": "A self-driving car's radar reports a stationary object ahead but the camera classifies it as a bridge overpass.",
     "reasoning": "Radar detects metal objects regardless of context — a bridge overhead and a stopped car ahead both appear as radar returns. The camera provides semantic understanding: it is a bridge, not a roadblock. This is a classic fusion problem where radar provides detection and camera provides classification. The fusion system should learn that overhead radar returns from known bridge positions are not obstacles. Context and map priors matter.",
     "concept": "sensor_semantic_classification"},
]

# ============================================================================
# MOTOR CONTROL (20 items train + 5 test)
# ============================================================================
MOTOR_CONTROL_DATA = [
    {"scenario": "Robot overshoots target position by 3cm repeatedly when moving to waypoints.",
     "reasoning": "Overshoot indicates the PID controller's proportional gain is too high or derivative gain is too low. The derivative term damps oscillation by opposing the rate of change. Reduce P gain or increase D gain. Also check if the control loop rate is fast enough — a slow loop can't react to overshoot in time.",
     "concept": "motor_pid_overshoot"},
    {"scenario": "Drone drifts left in a steady 10 km/h crosswind despite level commands.",
     "reasoning": "The flight controller sees level attitude but the drone moves laterally because wind applies a constant force. Solutions: feedforward wind compensation (estimate wind from GPS vs IMU drift), integral term in position controller to accumulate and correct steady-state error, or explicit wind model from airspeed sensor. Pure feedback is too slow for constant disturbances.",
     "concept": "motor_wind_compensation"},
    {"scenario": "Arm joint hits hard stop at maximum extension and motor stalls, drawing excessive current.",
     "reasoning": "Joint limit violation — the trajectory planner commanded a position beyond the joint's physical range. The motor stalls against the hard stop, converting all electrical power to heat. Immediate fix: detect current spike and back off. Proper fix: enforce software joint limits in the planner that are 5 degrees inside the physical limits. Hard stops are last-resort protection, not normal operation.",
     "concept": "motor_joint_limits"},
    {"scenario": "Wheels slip on a wet surface and the robot overshoots its stopping distance by 40%.",
     "reasoning": "Traction is limited by friction coefficient, which drops on wet surfaces. The controller assumes dry friction and commands braking force that exceeds available traction. Solutions: detect slip via encoder-IMU disagreement, reduce maximum deceleration command, implement anti-lock braking (pulse the brakes to maintain traction), increase safety margins in wet conditions.",
     "concept": "motor_traction_control"},
    {"scenario": "A 6-DOF robotic arm needs to follow a straight line in Cartesian space.",
     "reasoning": "Joint-space interpolation between two poses creates curved paths in Cartesian space. For straight lines, compute intermediate Cartesian points and solve inverse kinematics at each step. High update rate (1kHz) ensures the discretized path looks smooth. Singularities near arm boundaries can cause joint velocity spikes — add singularity avoidance.",
     "concept": "motor_cartesian_trajectory"},
    {"scenario": "A motor vibrates at high frequency when holding position under load.",
     "reasoning": "High-frequency oscillation while holding position is limit cycling — the controller alternates between slightly above and below the setpoint. Causes: excessive derivative gain amplifying sensor noise, quantization in the encoder, or backlash in the gearbox. Fix: add a dead zone around the setpoint, filter sensor noise before the D term, or use a notch filter at the vibration frequency.",
     "concept": "motor_limit_cycling"},
    {"scenario": "A quadrotor needs to flip upside down and recover in under 2 seconds.",
     "reasoning": "Aggressive maneuvers require: high thrust-to-weight ratio (>2:1), attitude controller that handles full 360-degree rotations (quaternion, not Euler angles to avoid gimbal lock), trajectory generator that plans the flip as a smooth rotation, and recovery controller that stabilizes from any attitude. Disable altitude hold during the flip — accept temporary altitude loss.",
     "concept": "motor_aggressive_maneuver"},
    {"scenario": "A conveyor belt robot needs to pick objects moving at 0.5 m/s without stopping the belt.",
     "reasoning": "Moving pick requires: predict object arrival at pick zone using vision + belt speed, pre-position gripper above the predicted pick point, match gripper speed to belt speed at the moment of contact, then close gripper and accelerate away. Timing is critical — milliseconds of error mean missed picks. Use feedforward from belt encoder, not just visual feedback.",
     "concept": "motor_moving_pick"},
    {"scenario": "A walking robot's foot slips during the stance phase on gravel.",
     "reasoning": "Foot slip means the assumed ground contact point is moving. Detect slip via force sensor (sudden drop in normal force) or IMU (unexpected lateral acceleration). Response: shift weight to other feet, increase normal force on slipping foot, shorten stride length. Long-term: adapt gait parameters to terrain — shorter steps and lower center of mass on loose terrain.",
     "concept": "motor_gait_slip"},
    {"scenario": "A servo motor's actual position lags behind commanded position by a constant 50ms.",
     "reasoning": "Constant time delay (transport delay) in the control loop — could be communication latency, sensor processing time, or actuator response delay. Delay destabilizes feedback controllers because they react to past state. Solutions: Smith predictor (model the delay and compensate), reduce delay by using faster communication protocols, or command 50ms ahead of desired position (predictive control).",
     "concept": "motor_time_delay"},
    {"scenario": "Two robot arms need to cooperatively carry a rigid beam without bending it.",
     "reasoning": "Cooperative manipulation requires: synchronized motion (both arms move at the same velocity), force control (neither arm pushes too hard against the other), and compliant behavior (if one arm deviates, the other absorbs the difference). Use impedance control — each arm behaves like a spring-damper system. If the beam is rigid, small position errors create large internal forces, so force control is essential.",
     "concept": "motor_cooperative_manipulation"},
    {"scenario": "A mobile robot needs to park precisely in a charging dock with 2mm tolerance.",
     "reasoning": "Coarse-to-fine approach: use vision to navigate to within 10cm, then switch to precision sensors (IR beacons on dock, contact sensors) for final alignment. Reduce speed as tolerance tightens — last 10cm at 1cm/s. Use visual servoing: continuously adjust heading based on dock target. Final contact: compliant mount allows mechanical self-centering.",
     "concept": "motor_precision_docking"},
    {"scenario": "A drone needs to land on a moving platform (ship deck) in rough seas.",
     "reasoning": "The platform heaves, rolls, and pitches. Track platform motion with vision or radar. Predict the next quiescent moment (between wave cycles). Match platform velocity and attitude at touchdown. Use relative navigation — control position relative to the platform, not the world. Accept that some sea states make landing unsafe — know when to abort.",
     "concept": "motor_moving_platform_landing"},
    {"scenario": "A robot arm executing a trajectory has a 1-joint motor failure mid-motion.",
     "reasoning": "Graceful degradation: detect the failure (current drop, encoder stops), brake the failed joint to prevent collapse, replan the remaining trajectory using the working joints. A 6-DOF arm with one failed joint becomes 5-DOF — it can still reach many poses but with reduced workspace. Lock the failed joint and re-solve inverse kinematics. Safety first: move to a safe pose before replanning.",
     "concept": "motor_actuator_failure"},
    {"scenario": "A legged robot needs to transition from walking on flat ground to climbing stairs.",
     "reasoning": "Gait transition requires: detect the terrain change (vision or contact force pattern change), gradually modify step height, stride length, and body pitch. Stairs need higher foot clearance, shorter stride, and forward body lean. Don't switch gaits abruptly — blend between flat and stair gaits over 2-3 steps. Use force feedback to verify each step lands on the expected surface.",
     "concept": "motor_gait_transition"},
    {"scenario": "A cable-driven robot has cable slack in one of four cables.",
     "reasoning": "Cable-driven systems require all cables in tension — slack cables lose control authority. The slack cable means the force polygon is violated. Solutions: increase minimum tension in the controller, detect slack via cable tension sensors or encoder discontinuity, reconfigure the tension distribution to restore all cables to tension. If slack persists, the workspace has been violated — retract to a feasible pose.",
     "concept": "motor_cable_tension"},
    {"scenario": "A robotic gripper needs to handle objects ranging from eggs to bricks.",
     "reasoning": "Variable impedance control: soft grip for fragile objects (low stiffness, force limit 2N), firm grip for heavy objects (high stiffness, force limit 50N). Use force sensors to detect object properties during initial contact — compliance reveals material. Grip force should be proportional to object weight with a safety margin. Too little force drops it; too much crushes it.",
     "concept": "motor_variable_impedance"},
    {"scenario": "A mobile robot's path planner generates a feasible path but the robot cannot follow it at the commanded speed.",
     "reasoning": "The path is kinematically feasible (the robot can reach those positions) but dynamically infeasible (it cannot accelerate/decelerate fast enough). The planner must consider velocity and acceleration limits, not just geometry. Solutions: time-parameterize the path with trapezoidal or S-curve velocity profiles, reduce maximum speed on tight curves (v_max proportional to 1/curvature), or replan with dynamics constraints.",
     "concept": "motor_dynamic_feasibility"},
    {"scenario": "Backlash in a gearbox causes the robot to oscillate around the target position.",
     "reasoning": "Backlash is a dead zone in the gear train — when the motor reverses, it must traverse the backlash gap before moving the output. This creates hysteresis: the output position depends on the direction of approach. Solutions: always approach from the same direction, use harmonic drives (zero backlash), add a secondary encoder on the output side (after the gearbox), or model backlash in the controller.",
     "concept": "motor_backlash_compensation"},
    {"scenario": "A high-speed delta robot needs to pick and place 120 items per minute from a conveyor.",
     "reasoning": "120 picks/min = 500ms per cycle. Budget: 100ms vision + 150ms move-to-pick + 50ms grasp + 150ms move-to-place + 50ms release. Use trapezoidal velocity profiles with maximum jerk limits to prevent vibration. Pre-compute the next pick trajectory while placing the current item. Optimize by minimizing peak acceleration, not just time — mechanical wear matters at these rates.",
     "concept": "motor_high_speed_pick_place"},
]

# MOTOR CONTROL TEST SET (5 items — held out for evaluation)
MOTOR_CONTROL_TEST = [
    {"scenario": "A robot arm following a circular path develops increasing tracking error over each revolution.",
     "reasoning": "Increasing error per revolution indicates a systematic issue that accumulates — likely encoder drift, thermal expansion of arm links, or integral windup in the controller. Integral windup occurs when the integrator accumulates error during saturated actuator output. Solutions: anti-windup on the integrator, periodic recalibration to an absolute reference, or feedforward of the known circular dynamics (centripetal acceleration compensation).",
     "concept": "motor_tracking_accumulation"},
    {"scenario": "A soft pneumatic actuator inflates to the correct pressure but the resulting position varies each time.",
     "reasoning": "Pneumatic actuators are inherently compliant — the same pressure produces different positions depending on external loads, material fatigue, temperature, and hysteresis. They lack the repeatability of electric motors. Solutions: closed-loop position control with external position sensor (not pressure alone), model the pressure-to-position relationship including load, or use iterative learning control to compensate for repeatable errors.",
     "concept": "motor_soft_actuator_variability"},
    {"scenario": "A hexapod robot needs to walk across a narrow beam where only two feet fit side by side.",
     "reasoning": "Normal hexapod gait uses a tripod support pattern (3 feet down). On a narrow beam, the robot must use a single-file foot placement — essentially beam walking. This requires: shift center of mass over the support polygon (which is now a line), move one foot at a time with the other five maintaining balance, use slower gait with careful weight transfer. The gait becomes more like a caterpillar than a hexapod.",
     "concept": "motor_constrained_gait"},
    {"scenario": "A motor controller receives position commands at 100Hz but the motor can only physically respond at 50Hz.",
     "reasoning": "The command rate exceeds the actuator bandwidth. The motor physically cannot follow 100Hz position changes — it acts as a low-pass filter, attenuating commands above its bandwidth. Higher-frequency commands create vibration and wasted energy. Solutions: pre-filter commands to the motor's bandwidth, downsample the command stream, or accept that the motor will track the low-frequency envelope of the commands.",
     "concept": "motor_bandwidth_mismatch"},
    {"scenario": "A drone needs to maintain hover in a GPS-denied environment with only a downward-facing camera.",
     "reasoning": "Without GPS, use visual odometry — track features on the ground below to estimate lateral velocity and position. Requires textured ground surface. Fails over featureless surfaces (water, blank floors). Combine with IMU for attitude and altitude hold via barometer or ToF rangefinder. The camera provides lateral hold, the barometer provides altitude hold. This is the core of optical flow-based hover.",
     "concept": "motor_visual_odometry_hover"},
]

# ============================================================================
# SAFETY (15 items train + 5 test)
# ============================================================================
SAFETY_DATA = [
    {"scenario": "Brain inference takes 2 seconds instead of normal 30ms. The robot is navigating near a cliff edge.",
     "reasoning": "A 60x latency spike means the control loop is effectively open-loop for 2 seconds. At any speed, the robot travels a significant distance without updated commands. Watchdog timer must trigger at 100ms — if inference doesn't complete in 3x normal time, execute safe stop: brake all motors, hold current steering. Never continue the last command blindly; a 2-second-old command could drive off the cliff.",
     "concept": "safety_watchdog_inference"},
    {"scenario": "Motor command output computes to NaN after a division by zero in the control law.",
     "reasoning": "NaN propagates silently — any arithmetic with NaN produces NaN. If NaN reaches the motor driver, the behavior is undefined: the motor could full-speed, stop, or oscillate. Output validation is mandatory: check every motor command for NaN, Inf, and out-of-range values BEFORE sending to hardware. Fallback: hold last known good command, or move to a predefined safe pose.",
     "concept": "safety_nan_output_validation"},
    {"scenario": "Battery at 5% during active flight at 50m altitude.",
     "reasoning": "5% battery is emergency territory. Lithium batteries can voltage-sag under load, causing sudden power loss. Immediate action: initiate controlled descent to the nearest safe landing zone. Do not attempt to return to base if it requires climbing or fighting headwind. Reduce power consumption: disable non-essential systems, reduce sensor polling rates. Log the event. A crash from power loss is the worst outcome — land while you still have control.",
     "concept": "safety_emergency_landing"},
    {"scenario": "Obstacle detected 0.5m ahead while the robot is moving at 1 m/s.",
     "reasoning": "At 1 m/s, the robot reaches the obstacle in 500ms. Braking distance depends on friction and deceleration capability. If braking distance > 0.5m, collision is unavoidable — minimize impact speed. Emergency braking: maximum deceleration immediately. If the robot can steer, evaluate lateral escape routes. The safety system must preempt the path planner — collision avoidance overrides all other goals.",
     "concept": "safety_collision_avoidance"},
    {"scenario": "Communication link with the operator lost for 60 seconds.",
     "reasoning": "Loss of communication means no human oversight. The robot must have pre-programmed autonomous safety behaviors: stop and hold position (safest default), return to last known safe location (if navigation is reliable), or continue mission with reduced risk tolerance (only if explicitly pre-authorized). Never assume communication will return — plan for permanent loss. Beacon your position for recovery.",
     "concept": "safety_comm_loss"},
    {"scenario": "A joint torque sensor reads 200 Nm but the maximum rated torque is 50 Nm.",
     "reasoning": "4x over-torque could mean: mechanical jam (collision with environment), motor runaway, or sensor fault. Regardless of cause: immediately cut power to that joint. Sustained over-torque destroys gears, heats motors, and can break structural components. After stopping: check for mechanical obstruction, verify sensor reading with a secondary measurement, inspect gearbox for damage before re-enabling.",
     "concept": "safety_torque_limit"},
    {"scenario": "Software update is available for the safety-critical motor controller while the robot is operational.",
     "reasoning": "Never update safety-critical software during operation. The update could introduce bugs, change timing behavior, or require a reboot. Schedule updates during maintenance windows. Test updates on an identical non-operational system first. Maintain rollback capability. Safety-critical software updates must go through formal verification before deployment. The cost of a bad update is a potential accident.",
     "concept": "safety_software_update"},
    {"scenario": "A collaborative robot's force-limiting sensor reads zero despite the arm being in contact with a human.",
     "reasoning": "Zero force reading during known contact is a sensor failure — the most dangerous kind because the safety system thinks the path is clear. Immediately enter protective stop: freeze all motion, then slowly retract. A force sensor failure must be treated as the worst case (maximum possible force). Implement sensor health monitoring: a force sensor that always reads zero is as dangerous as having no sensor.",
     "concept": "safety_sensor_failure_mode"},
    {"scenario": "Two safety systems disagree — the software e-stop says 'safe' but the hardware e-stop circuit is triggered.",
     "reasoning": "When safety systems disagree, always follow the more restrictive one. Hardware e-stop is physically wired and cannot be spoofed by software bugs. If hardware says stop, the robot stops — period. Investigate the disagreement: either the software missed a hazard (software bug) or the hardware triggered spuriously (wiring issue). Neither is acceptable. Both must agree before resuming operation.",
     "concept": "safety_estop_disagreement"},
    {"scenario": "A robot detects that its safety-rated LIDAR has been physically displaced by 5 degrees after a collision.",
     "reasoning": "A misaligned safety LIDAR means the protective zone is shifted — areas the robot thinks are clear may actually be occupied. This is a systematic error, not noise, and cannot be fixed by filtering. The robot must: stop operations, flag the LIDAR as uncalibrated, reduce speed to minimum if operations must continue, and schedule immediate recalibration. Safety sensors must be calibrated and verified regularly.",
     "concept": "safety_sensor_misalignment"},
    {"scenario": "A learning algorithm updates motor control gains that result in increasingly aggressive movements.",
     "reasoning": "Unbounded learning on safety-critical parameters is dangerous. The learned gains may optimize for speed or accuracy while violating force, velocity, or acceleration limits. Solution: sandbox all learned parameters within hard-coded safety bounds. No learned value can exceed the safety envelope. Apply rate limiting to parameter changes. Monitor for runaway optimization. Learning should explore within a safe boundary, never beyond it.",
     "concept": "safety_bounded_learning"},
    {"scenario": "A drone's propeller guard is damaged but the drone is still flyable.",
     "reasoning": "Propeller guards are the last line of defense against laceration injuries. A damaged guard means: if the drone contacts a person, the spinning propeller is exposed. Even if the drone can still fly, it should not operate near people without intact guards. Restrict operations to unpopulated areas. A flyable drone with damaged safety equipment is more dangerous than a grounded drone — the temptation to keep operating is the hazard.",
     "concept": "safety_guard_integrity"},
    {"scenario": "Temperature sensors on a motor read 120C but the motor's thermal limit is 100C.",
     "reasoning": "Over-temperature damages motor windings (insulation breakdown), demagnetizes permanent magnets, and can ignite nearby materials. Immediate action: reduce motor current to zero and allow cooling. Do not restart until temperature is below 80C with margin. Investigate: was the motor overloaded, was cooling blocked, or is the thermal management design inadequate? Repeated over-temperature events indicate a design problem, not an operational one.",
     "concept": "safety_thermal_protection"},
    {"scenario": "A human enters the workspace of an industrial robot at high speed.",
     "reasoning": "ISO 10218 requires safety-rated monitored stop when a human enters the collaborative workspace. Speed and separation monitoring: if the human's closing speed is high, the required separation distance increases. If the distance cannot be maintained, the robot must stop. Protective stops must be fast enough that the human cannot reach the robot before it stops. Calculate worst-case stopping distance from current speed and ensure it is less than current distance to human.",
     "concept": "safety_human_proximity"},
    {"scenario": "The robot's real-time operating system misses a control loop deadline by 5ms.",
     "reasoning": "Real-time deadline misses mean the control loop skipped a cycle. For a 1kHz loop, one missed cycle is 1ms of open-loop operation — often tolerable. But 5ms of missed deadline could mean the system is overloaded, risking cascading misses. Monitor deadline compliance. If misses exceed a threshold (e.g., 1% of cycles), reduce computational load or increase CPU priority. Safety-critical control must never be preempted by non-critical tasks.",
     "concept": "safety_realtime_deadline"},
]

# SAFETY TEST SET (5 items — held out for evaluation)
SAFETY_TEST = [
    {"scenario": "A robot arm's braking system fails on the vertical axis — the arm begins to fall under gravity.",
     "reasoning": "Gravity causes the arm to accelerate downward, potentially crushing anything below. Without the brake, the motor must hold the load — check if the motor can provide holding torque without the brake. If not, mechanical counterbalancing (springs or counterweights) is the last defense. Design principle: safety-critical axes that fight gravity should have redundant braking (motor brake + mechanical brake + counterbalance). A single brake failure should never cause free-fall.",
     "concept": "safety_gravity_compensation"},
    {"scenario": "A swarm of 10 robots loses coordination and two are heading directly toward each other.",
     "reasoning": "Decentralized collision avoidance must be each robot's responsibility independently of swarm coordination. Even if the coordination layer fails, each robot should run local obstacle avoidance using its own sensors. Priority rules (lower ID yields) break symmetry. If both robots brake simultaneously, they stop with margin. The system must be safe even when the coordination layer is down — safety cannot depend on communication.",
     "concept": "safety_swarm_collision"},
    {"scenario": "A surgical robot detects 2mm of unexpected compliance in its drive train during a procedure.",
     "reasoning": "In surgery, 2mm of unexpected play is a critical failure — it means the tool tip position is uncertain by 2mm, which exceeds acceptable surgical tolerances. The robot should: pause the procedure, alert the surgeon, retract to a safe position, and require explicit authorization to continue. Never continue precision operations with degraded mechanical integrity. The surgeon must decide if manual completion is safer than robotic continuation.",
     "concept": "safety_surgical_compliance"},
    {"scenario": "A factory robot detects that its safety PLC firmware was modified without authorization.",
     "reasoning": "Unauthorized firmware modification to safety hardware is a cybersecurity incident. The safety PLC may no longer enforce correct safety behavior. Immediate response: enter safe state (all-stop), isolate the PLC from the network, alert security team, do not clear the safe state until the firmware is verified against a known-good hash. Treat this as both a safety incident and a security breach. Log everything for forensic analysis.",
     "concept": "safety_cybersecurity_integrity"},
    {"scenario": "A mobile robot's SLAM system reports it is in a known safe zone, but the odometry suggests it has traveled 3m further than the map shows.",
     "reasoning": "Odometry-map disagreement of 3m means either the robot is lost (SLAM failed) or the map is wrong. The robot should not trust its position for safety decisions. Stop, attempt SLAM relocalization using landmarks. If localization cannot be confirmed, operate in degraded mode with expanded safety margins. Never trust map-based safety zones when localization is uncertain — the robot could be 3m into a danger zone while thinking it is safe.",
     "concept": "safety_localization_uncertainty"},
]

# ============================================================================
# EMBODIMENT (15 items train + 5 test)
# ============================================================================
EMBODIMENT_DATA = [
    {"scenario": "Your arm has 6 joints (shoulder-pitch, shoulder-yaw, shoulder-roll, elbow, wrist-pitch, wrist-yaw). How do you reach a point 30cm directly ahead?",
     "reasoning": "This is inverse kinematics — mapping desired end-effector position to joint angles. 6 joints give 6 degrees of freedom, meaning infinite solutions exist for a single 3D point (3 extra DOF for orientation). The solver finds joint angles where the forward kinematics chain places the wrist at (0.3, 0, 0). Multiple valid configurations exist — elbow up vs elbow down. Choose the configuration closest to current pose to minimize motion.",
     "concept": "embodiment_inverse_kinematics"},
    {"scenario": "You are a drone. How do you know which way is up?",
     "reasoning": "The IMU's accelerometer measures gravity. In stable flight, the gravity vector points down with magnitude 9.81 m/s^2. The accelerometer resolves this into body-frame components — if only the Z-axis reads -9.81, you are level. Tilt causes gravity to project onto X and Y axes. A complementary filter fuses accelerometer (noisy but correct long-term) with gyroscope (smooth but drifts) for attitude estimation. 'Up' is wherever negative gravity points.",
     "concept": "embodiment_attitude_estimation"},
    {"scenario": "Your left wheel encoder says 100 ticks but the right says 80 ticks over the last second.",
     "reasoning": "Differential drive kinematics: unequal wheel rotation means the robot is turning. The robot curves toward the slower wheel (right). The turning radius is R = (L/2) * (v_left + v_right) / (v_left - v_right), where L is the wheel base. 100 vs 80 ticks means the right wheel traveled 80% of the left — the robot is arcing right. Dead reckoning uses this to update heading and position.",
     "concept": "embodiment_differential_drive"},
    {"scenario": "A force sensor on your gripper reads 5N of normal force while grasping a cup.",
     "reasoning": "5N is the contact force between gripper and cup. Is this enough? Depends on the cup's weight and the friction coefficient. A 200g cup needs at least F_grip > (m*g) / (2*mu). With mu=0.5, minimum grip force = (0.2*9.81)/(2*0.5) = 1.96N per finger. 5N provides a 2.5x safety margin — appropriate for stable grasping. Compliance control: maintain 5N rather than a position setpoint so the gripper adapts to the cup's shape.",
     "concept": "embodiment_force_grasping"},
    {"scenario": "You have a camera on your head and one on your wrist. The head camera sees an object at pixel (320, 240). Where is it in the wrist camera's view?",
     "reasoning": "This requires coordinate transforms through the kinematic chain: head camera frame → head frame → torso frame → shoulder frame → wrist frame → wrist camera frame. Each transform is a 4x4 matrix (rotation + translation). The chain of transforms maps a 3D point from one sensor's frame to another's. Without knowing the object's depth from the head camera, you only know the ray it lies on, not the exact 3D point.",
     "concept": "embodiment_coordinate_transforms"},
    {"scenario": "You are a snake robot with 12 identical joint modules. How do you move forward?",
     "reasoning": "Serpentine locomotion: apply a traveling sine wave along the body. Each joint angle = A * sin(wt - k*i), where i is the joint index and k sets the spatial frequency. The wave pushes against ground contact points, propelling the snake forward. More joints per wavelength gives smoother motion. The gait is fully defined by amplitude A, frequency w, and wave number k. No legs needed — friction anisotropy provides propulsion.",
     "concept": "embodiment_serpentine_locomotion"},
    {"scenario": "Your stereo cameras are 10cm apart. An object appears at pixel 300 in the left image and pixel 280 in the right image. How far away is it?",
     "reasoning": "Stereo depth from disparity: depth Z = (focal_length * baseline) / disparity. Disparity = 300 - 280 = 20 pixels. With focal_length = 500px and baseline = 0.1m: Z = (500 * 0.1) / 20 = 2.5m. The object is 2.5 meters away. Larger disparity means closer objects. At zero disparity, the object is at infinity. Stereo vision mimics human binocular depth perception.",
     "concept": "embodiment_stereo_depth"},
    {"scenario": "You are a quadruped robot. One of your legs is lifted for a step. Where is your center of mass relative to your support triangle?",
     "reasoning": "With one leg raised, three legs form a support triangle. The center of mass (CoM) must stay inside this triangle for static stability. If the CoM projects outside the triangle, the robot tips over. Before lifting a leg, shift the body so the CoM is well inside the remaining three-leg triangle. The stability margin is the minimum distance from the CoM projection to any edge of the support polygon.",
     "concept": "embodiment_static_stability"},
    {"scenario": "Your robot body weighs 20kg and your arm lifts a 5kg object at full extension (0.6m). What torque does the shoulder experience?",
     "reasoning": "Torque = force * moment arm. The 5kg object at 0.6m: T_payload = 5 * 9.81 * 0.6 = 29.4 Nm. The arm itself has mass — if the arm weighs 3kg with center of mass at 0.3m: T_arm = 3 * 9.81 * 0.3 = 8.8 Nm. Total shoulder torque = 38.2 Nm. This determines the shoulder motor size. Lifting capacity decreases with extension because the moment arm increases. A robot must know its own mass distribution.",
     "concept": "embodiment_torque_awareness"},
    {"scenario": "You are a humanoid robot. You lean forward 10 degrees. What happens to your balance?",
     "reasoning": "Leaning forward shifts the CoM forward. The feet are the support base. If the CoM projection moves past the toes, you fall forward. To compensate: ankle torque pushes the body back (ankle strategy), or hip bends to shift upper-body CoM rearward (hip strategy), or take a step forward to place a foot under the new CoM (stepping strategy). Humans use all three — ankle for small perturbations, hip for medium, stepping for large.",
     "concept": "embodiment_balance_strategies"},
    {"scenario": "Your robot arm has joint position sensors but no direct measurement of end-effector position. How do you know where your hand is?",
     "reasoning": "Forward kinematics: given all joint angles (q1...q6), compute the end-effector position and orientation using the Denavit-Hartenberg chain. Each joint applies a rotation and translation. Multiplying the chain of transforms gives the hand position in the base frame. This is proprioception — knowing body state from joint sensors. Error accumulates along the chain, so the hand position has more uncertainty than the shoulder.",
     "concept": "embodiment_proprioception"},
    {"scenario": "You are a wheeled robot moving at 0.5 m/s. You hit a bump and your IMU registers a spike of 3g vertical acceleration.",
     "reasoning": "3g vertical acceleration means the robot momentarily experienced 3x normal gravity — it bounced. During the bump, the wheels may have lost ground contact briefly. Consequences: encoder odometry is corrupted (wheels spinning in air), IMU integration accumulates error during the spike, and any open liquid or loose payload may have shifted. After the bump: trust GPS or LIDAR over odometry, re-zero the IMU vertical channel, check payload status.",
     "concept": "embodiment_bump_response"},
    {"scenario": "You are a robot with a 3-fingered gripper. You pick up a pen but it rotates in your grip as you move.",
     "reasoning": "Pen rotation indicates insufficient torque constraint — the grip provides force but not a form closure that resists rotation. Three contact points on a cylinder can rotate around the cylinder axis. Solutions: add a fourth contact point (or a flat finger surface for friction), squeeze harder to increase friction torque, or pick the pen at its center of mass to minimize gravitational torque. The gripper must match the object's geometry.",
     "concept": "embodiment_grasp_stability"},
    {"scenario": "You are an underwater robot. You command forward thrust but drift sideways due to current.",
     "reasoning": "In water, the robot's motion is the sum of its thrust vector and the current vector. Unlike ground robots, there is no friction holding you in place — you must actively counteract currents. Use a DVL (Doppler Velocity Log) to measure actual velocity over the seabed, not just through the water. The difference between water-relative and ground-relative velocity IS the current. Compensate by angling thrust to cancel the current component.",
     "concept": "embodiment_aquatic_drift"},
    {"scenario": "Your robot's left camera and right camera have slightly different focal lengths due to manufacturing tolerances.",
     "reasoning": "Mismatched focal lengths cause systematic stereo depth errors — objects appear at incorrect depths. A 1% difference in focal length causes ~1% depth error at all ranges. This is intrinsic calibration error. Calibrate each camera independently using a checkerboard pattern to determine its true focal length, then use the calibrated values in the stereo depth computation. Never assume cameras are identical.",
     "concept": "embodiment_camera_calibration"},
]

# EMBODIMENT TEST SET (5 items — held out for evaluation)
EMBODIMENT_TEST = [
    {"scenario": "Your robot has two arms and needs to open a jar — one hand holds the jar body while the other turns the lid.",
     "reasoning": "Bimanual manipulation requires coordinating two kinematic chains with opposite objectives: one arm holds position rigidly (the jar body), the other applies rotational motion (the lid). The holding arm uses stiffness control to resist the torque applied by the twisting arm. Both arms must be in a shared reference frame. The twist arm applies torque around the jar's vertical axis while the hold arm applies equal and opposite torque. The jar's position must be tracked in real-time — it will shift if the hold grip slips.",
     "concept": "embodiment_bimanual_coordination"},
    {"scenario": "You are a drone carrying a pendulum payload. The payload swings when you change direction.",
     "reasoning": "A suspended payload creates coupled dynamics — the drone and payload form a double pendulum system. Rapid direction changes excite pendulum oscillation. Input shaping: plan trajectories that avoid exciting the payload's natural frequency. The payload's swing period is T = 2*pi*sqrt(L/g), where L is the cable length. Command frequencies below 1/T avoid resonance. Alternatively, use active damping by observing the swing and counter-steering.",
     "concept": "embodiment_payload_dynamics"},
    {"scenario": "Your robot stands on a rotating turntable. All your sensors report rotation but you didn't command any motion.",
     "reasoning": "The robot's body frame is rotating but the world frame is not. Gyroscope detects angular velocity (correct — the robot IS rotating). Encoders show no wheel motion (correct — the wheels aren't turning). Camera shows the world rotating (correct interpretation of relative motion). The key insight: distinguish between self-motion and environmental motion. LIDAR scan-matching against a known map reveals the floor is moving, not the robot's wheels. Proprioception alone cannot distinguish passive rotation from active rotation.",
     "concept": "embodiment_frame_disambiguation"},
    {"scenario": "You are a soft robot made of silicone. How do you estimate your own shape?",
     "reasoning": "Soft robots lack rigid links and joints — standard forward kinematics doesn't apply. Shape estimation requires distributed sensing: embedded strain gauges along the body, curvature sensors at intervals, or external motion capture. The robot's shape is a continuous curve, not a discrete chain of rigid links. Model the body as a Cosserat rod or piecewise constant curvature. Proprioception for soft robots is an active research challenge — the infinite degrees of freedom make full state estimation difficult.",
     "concept": "embodiment_soft_body_proprioception"},
    {"scenario": "Your ground robot has a 30cm ground clearance but encounters a rock that is 25cm tall. Can you drive over it?",
     "reasoning": "Ground clearance alone doesn't determine obstacle traversal. Consider: approach angle (can the front bumper clear the rock?), departure angle (can the undercarriage clear?), wheel diameter (larger wheels climb obstacles more easily), breakover angle (can the chassis bridge the rock without high-centering?), and traction (can the wheels grip the rock surface?). A 25cm rock with 30cm clearance is marginal — the robot might high-center if the wheelbase is long relative to the rock's width. Measure the full obstacle geometry, not just height.",
     "concept": "embodiment_obstacle_geometry"},
]


def _format_items(raw_data, domain, text_key, answer_key, label_prefix=None):
    """Helper to convert raw domain-specific dicts to unified format."""
    items = []
    for item in raw_data:
        text = item[text_key] if isinstance(text_key, str) else text_key(item)
        answer = item[answer_key]
        label = f"{label_prefix}_{item['concept']}" if label_prefix else item['concept']
        items.append({"text": text, "answer": answer, "label": label, "domain": domain})
    return items


def get_all_cognitive_data():
    """Return all training data as a flat list with unified format."""
    all_data = []
    for item in ETHICS_DATA:
        all_data.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"ethics_{item['concept']}", "domain": "ethics"})
    for item in COUNTERFACTUAL_DATA:
        all_data.append({"text": f"{item['premise']} {item['counterfactual']}",
                          "answer": item["reasoning"],
                          "label": f"counterfactual_{item['concept']}", "domain": "counterfactual"})
    for item in CAUSAL_DATA:
        all_data.append({"text": f"{item['observation']} {item.get('question', '')}",
                          "answer": item["reasoning"],
                          "label": f"causal_{item['concept']}", "domain": "causal"})
    for item in METACOGNITION_DATA:
        all_data.append({"text": item["prompt"], "answer": item["response"],
                          "label": f"metacog_{item['concept']}", "domain": "metacognition"})
    for item in ANALOGY_DATA:
        all_data.append({"text": item["analogy"], "answer": item["explanation"],
                          "label": f"analogy_{item['concept']}", "domain": "analogy"})
    for item in RCOG_DATA:
        all_data.append({"text": item["problem"], "answer": item["decomposition"],
                          "label": f"rcog_{item['concept']}", "domain": "rcog"})
    for item in COLLECTIVE_DATA:
        all_data.append({"text": item["scenario"], "answer": item["solution"],
                          "label": f"collective_{item['concept']}", "domain": "collective"})
    for item in PORTIA_DATA:
        all_data.append({"text": item["scenario"], "answer": item["decision"],
                          "label": f"portia_{item['concept']}", "domain": "portia"})
    for item in DRAGONFLY_DATA:
        all_data.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"dragonfly_{item['concept']}", "domain": "dragonfly"})
    # New embodiment domains
    for item in SENSOR_FUSION_DATA:
        all_data.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"sensor_{item['concept']}", "domain": "sensor_fusion"})
    for item in MOTOR_CONTROL_DATA:
        all_data.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"motor_{item['concept']}", "domain": "motor_control"})
    for item in SAFETY_DATA:
        all_data.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"safety_{item['concept']}", "domain": "safety"})
    for item in EMBODIMENT_DATA:
        all_data.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"embodiment_{item['concept']}", "domain": "embodiment"})
    return all_data


def get_all_test_data():
    """Return all held-out test data as a flat list with unified format.

    Test data is for evaluation only — never train on these items.
    """
    all_test = []
    # Original 9 domains — test splits
    for item in ETHICS_TEST:
        all_test.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"ethics_{item['concept']}", "domain": "ethics"})
    for item in COUNTERFACTUAL_TEST:
        all_test.append({"text": f"{item['premise']} {item['counterfactual']}",
                          "answer": item["reasoning"],
                          "label": f"counterfactual_{item['concept']}", "domain": "counterfactual"})
    for item in CAUSAL_TEST:
        all_test.append({"text": f"{item['observation']} {item.get('question', '')}",
                          "answer": item["reasoning"],
                          "label": f"causal_{item['concept']}", "domain": "causal"})
    for item in METACOGNITION_TEST:
        all_test.append({"text": item["prompt"], "answer": item["response"],
                          "label": f"metacog_{item['concept']}", "domain": "metacognition"})
    for item in ANALOGY_TEST:
        all_test.append({"text": item["analogy"], "answer": item["explanation"],
                          "label": f"analogy_{item['concept']}", "domain": "analogy"})
    for item in RCOG_TEST:
        all_test.append({"text": item["problem"], "answer": item["decomposition"],
                          "label": f"rcog_{item['concept']}", "domain": "rcog"})
    for item in COLLECTIVE_TEST:
        all_test.append({"text": item["scenario"], "answer": item["solution"],
                          "label": f"collective_{item['concept']}", "domain": "collective"})
    for item in PORTIA_TEST:
        all_test.append({"text": item["scenario"], "answer": item["decision"],
                          "label": f"portia_{item['concept']}", "domain": "portia"})
    for item in DRAGONFLY_TEST:
        all_test.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"dragonfly_{item['concept']}", "domain": "dragonfly"})
    # New 4 domains — test splits
    for item in SENSOR_FUSION_TEST:
        all_test.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"sensor_{item['concept']}", "domain": "sensor_fusion"})
    for item in MOTOR_CONTROL_TEST:
        all_test.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"motor_{item['concept']}", "domain": "motor_control"})
    for item in SAFETY_TEST:
        all_test.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"safety_{item['concept']}", "domain": "safety"})
    for item in EMBODIMENT_TEST:
        all_test.append({"text": item["scenario"], "answer": item["reasoning"],
                          "label": f"embodiment_{item['concept']}", "domain": "embodiment"})
    return all_test


def get_test_data_by_domain(domain):
    """Return test items for a specific domain.

    Args:
        domain: One of 'ethics', 'counterfactual', 'causal', 'metacognition',
                'analogy', 'rcog', 'collective', 'portia', 'dragonfly',
                'sensor_fusion', 'motor_control', 'safety', 'embodiment'.

    Returns:
        List of test items in unified format for the specified domain,
        or empty list if domain not found.
    """
    all_test = get_all_test_data()
    return [item for item in all_test if item["domain"] == domain]


def get_random_cognitive_item(domain=None):
    """Get a random training item, optionally filtered by domain."""
    if domain == "ethics": data = ETHICS_DATA
    elif domain == "counterfactual": data = COUNTERFACTUAL_DATA
    elif domain == "causal": data = CAUSAL_DATA
    elif domain == "metacognition": data = METACOGNITION_DATA
    elif domain == "analogy": data = ANALOGY_DATA
    elif domain == "rcog": data = RCOG_DATA
    elif domain == "collective": data = COLLECTIVE_DATA
    elif domain == "portia": data = PORTIA_DATA
    elif domain == "dragonfly": data = DRAGONFLY_DATA
    elif domain == "sensor_fusion": data = SENSOR_FUSION_DATA
    elif domain == "motor_control": data = MOTOR_CONTROL_DATA
    elif domain == "safety": data = SAFETY_DATA
    elif domain == "embodiment": data = EMBODIMENT_DATA
    else: data = get_all_cognitive_data()
    return random.choice(data) if data else None


# Count
if __name__ == "__main__":
    all_data = get_all_cognitive_data()
    all_test = get_all_test_data()
    print(f"Total cognitive training items: {len(all_data)}")
    print(f"Total cognitive test items:     {len(all_test)}")
    print(f"Total items (train + test):     {len(all_data) + len(all_test)}")
    print()
    # Training set breakdown
    train_domains = {}
    for item in all_data:
        d = item["domain"]
        train_domains[d] = train_domains.get(d, 0) + 1
    print("Training set:")
    for d, c in sorted(train_domains.items()):
        print(f"  {d}: {c}")
    print()
    # Test set breakdown
    test_domains = {}
    for item in all_test:
        d = item["domain"]
        test_domains[d] = test_domains.get(d, 0) + 1
    print("Test set (held out for evaluation):")
    for d, c in sorted(test_domains.items()):
        print(f"  {d}: {c}")
