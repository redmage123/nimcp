# Logic Modules Wiring Audit

**Date:** 2025-11-11
**Auditor:** Claude Code

## Summary

**Total Logic Modules:** 2
- **Neural Logic** (GPU-accelerated spiking logic gates)
- **Symbolic Logic** (First-order logic, inference engine)

**Fully Wired & Active:** 0 ❌
**Partially Wired:** 2 ⚠️
**NOT Wired to Cognitive Modules:** 2 ❌

---

## Module Status

### ⚠️ Neural Logic (Spiking Logic Gates)

**Location:** `core/neuron_types/nimcp_neural_logic.h/c`
**Status:** ⚠️ INFRASTRUCTURE EXISTS, NOT USED BY COGNITIVE MODULES

**What It Does:**
- GPU-accelerated logic gates (AND, OR, NOT, XOR, IMPLIES)
- Spike-based computation (~0.1ms vs 100ms+ for symbolic)
- Biologically-inspired coincidence detection
- Variable binding for compositional reasoning

**Wiring:**
- ✅ Included: `brain.c` line 60
- ✅ Field: `brain->logic` (line 150)
- ✅ Initialized: `init_symbolic_logic_subsystem()` (line 1586-1595)
- ✅ Cleanup: `brain_destroy()` (line 2635)

**Usage:**
- ❌ ZERO cognitive module integration
- ❌ NOT used by Ethics (should validate moral rules)
- ❌ NOT used by Knowledge (should perform logical inference)
- ❌ NOT used by Executive (should do planning logic)
- ❌ NOT used by Theory of Mind (should infer beliefs)
- ❌ NOT used by Predictive (should compute conditionals)

**Critical Finding:** Neural logic network created but never executed. No `neural_logic_update()` calls in brain processing pipeline.

---

### ⚠️ Symbolic Logic (Inference Engine)

**Location:** `cognitive/logic/nimcp_symbolic_logic.h/c`
**Status:** ⚠️ INFRASTRUCTURE EXISTS, MINIMAL USE

**What It Does:**
- First-order logic with predicates and quantifiers
- Forward chaining (derive facts from rules)
- Backward chaining (prove goals)
- Resolution-based theorem proving
- Unification and variable binding
- Knowledge base with salience

**Wiring:**
- ✅ Included: `brain.c` line 54
- ✅ Field: `brain->symbolic_logic` (line 151)
- ✅ Initialized: `init_symbolic_logic_subsystem()` (line 1617-1643)
- ✅ Cleanup: `brain_destroy()` (line 2644-2645)

**Usage:**
- ⚠️ ONLY used by Explanations module (optional proof generation)
- ❌ NOT used by Knowledge (should store facts/rules)
- ❌ NOT used by Ethics (should encode moral principles)
- ❌ NOT used by Curiosity (should generate hypotheses)
- ❌ NOT used by Executive (should do goal reasoning)
- ❌ NOT used by Theory of Mind (should model beliefs)
- ❌ NOT used by Predictive (should infer consequences)

**Evidence:** Only 1 call site:
```c
// brain.c:3832
if (brain->symbolic_logic && nat_exp.has_symbolic_proof) {
    explain_with_symbolic_logic(brain->explanation_gen, brain, ...);
}
```

---

## Where Logic SHOULD Be Integrated

### 1. **Knowledge Module** ❌

**File:** `cognitive/knowledge/nimcp_knowledge.c`

**What's Missing:**
- Symbolic facts: `symbolic_logic_add_fact(logic, "IsA(cat, animal)", 0.9)`
- Inference rules: `symbolic_logic_add_rule(logic, "IsA(X, Y) ∧ IsA(Y, Z) → IsA(X, Z)")`
- Query answering: `symbolic_logic_query(logic, "IsA(cat, ?)")`
- Logical deduction from knowledge graph

**Why It Matters:**
Knowledge graphs are inherently symbolic. Current implementation stores concepts but can't perform logical reasoning over them.

**Example Use Case:**
```
Knowledge: "Socrates is a man", "All men are mortal"
Query: "Is Socrates mortal?"
Answer: YES (via symbolic inference)
```

---

### 2. **Ethics Module** ❌

**File:** `cognitive/ethics/nimcp_ethics.c`

**What's Missing:**
- Moral rules as logic: `∀x (HarmHuman(x) → Forbidden(x))`
- Deontological reasoning: IF action violates rule THEN unethical
- Consequentialist logic: IF outcome(action) = harm THEN reduce weight
- Conflict resolution via logic priorities

**Why It Matters:**
Ethics is fundamentally about rules and their logical application. Current implementation uses neural networks without explicit moral principles.

**Example Use Case:**
```
Rule: "Never harm humans"
Action: "Push person off cliff to save 5 others"
Logic: harm(action) = TRUE → Check exception rules → Trolley problem
```

---

### 3. **Executive Function Module** ❌

**File:** `cognitive/nimcp_executive.c`

**What's Missing:**
- Goal decomposition: `Achieve(X) → Subgoal(A) ∧ Subgoal(B)`
- Plan validation: Check preconditions via logic
- Conflict detection: `Goal(X) ∧ ¬Goal(X)` → resolve
- Causal reasoning: IF action A THEN state B

**Why It Matters:**
Planning requires logical decomposition of goals and validation of action sequences.

**Example Use Case:**
```
Goal: "Get coffee"
Decomposition:
  Subgoal: "Go to kitchen"
  Subgoal: "Find mug"
  Subgoal: "Pour coffee"
Logic: Check preconditions for each subgoal
```

---

### 4. **Theory of Mind Module** ❌

**File:** `cognitive/nimcp_theory_of_mind.c`

**What's Missing:**
- Belief representation: `Believes(Agent, Proposition)`
- Belief inference: `Sees(Agent, X) → Believes(Agent, Present(X))`
- Desire logic: `Desires(Agent, Goal) ∧ Believes(Agent, Action→Goal) → Intends(Agent, Action)`
- False belief reasoning: Track what agent believes vs reality

**Why It Matters:**
Theory of Mind is about modeling others' mental states as propositional attitudes (beliefs, desires, intentions).

**Example Use Case:**
```
Reality: "Ball is in box B"
Agent saw: "Ball in box A"
Logic: Believes(Agent, InBox(ball, A)) ∧ InBox(ball, B)
Conclusion: Agent has false belief
```

---

### 5. **Curiosity Module** ❌

**File:** `cognitive/curiosity/nimcp_curiosity.c`

**What's Missing:**
- Hypothesis generation: `symbolic_logic_explore()` to derive novel facts
- Gap detection: Find missing predicates in knowledge base
- Question generation: Convert gaps to queries
- Abductive reasoning: Find best explanation for observation

**Why It Matters:**
Curiosity is driven by knowledge gaps. Logic can systematically identify what's unknown.

**Example Use Case:**
```
Known: "Birds fly", "Penguins are birds"
Gap: Unknown(Fly(penguin))
Hypothesis: Fly(penguin) OR ¬Fly(penguin)
Curiosity: Explore to determine which
```

---

### 6. **Predictive Module** ❌

**File:** `cognitive/predictive/nimcp_predictive.c`

**What's Missing:**
- Conditional logic: `IF condition THEN consequence`
- Temporal logic: `NEXT(state) ← Action(current)`
- Probabilistic inference: Combine logic with uncertainty
- Counterfactual reasoning: `IF X had been true, THEN Y`

**Why It Matters:**
Prediction requires causal and temporal reasoning, which logic provides.

**Example Use Case:**
```
State: "It's cloudy"
Rule: "Cloudy → (Rain with 70% probability)"
Prediction: Probably(Rain)
```

---

### 7. **NLP Module** ❌

**File:** `nlp/nimcp_nlp.c`, `nlp/nimcp_multimodal_nlp_bridge.c`

**What's Missing:**
- Semantic parsing: Convert sentences to logical forms
- Natural logic: "All cats are animals" → `∀x (Cat(x) → Animal(x))`
- Question answering: Parse question → Logic query → Answer
- Textual entailment: Does sentence A entail sentence B?

**Why It Matters:**
Language understanding requires mapping natural language to logical meaning representations.

**Example Use Case:**
```
Input: "All dogs bark. Fido is a dog."
Parse: ∀x (Dog(x) → Barks(x)), Dog(Fido)
Inference: Barks(Fido)
Output: "Yes, Fido barks."
```

---

### 8. **Explanations Module** ⚠️

**File:** `cognitive/explanations/nimcp_explanations.c`

**Current Status:** PARTIALLY WIRED
- ✅ Has optional symbolic proof generation
- ⚠️ Only used if `has_symbolic_proof` flag set
- ❌ No integration with knowledge/ethics for deeper explanations

**What's Missing:**
- Logical proof trees for all decisions
- Step-by-step inference chains
- Counterfactual explanations via logic

---

## Neural Logic Integration Points

### Where Neural Logic Should Execute

**Currently:** Neural logic network is created but never stepped/updated.

**Should Be Integrated Into:**

1. **Brain Forward Pass** (`brain_process_multimodal`)
   - Add logic evaluation stage
   - Evaluate logical constraints on network output
   - Use logic gates to validate decisions

2. **Ethics Validation**
   - Fast neural logic checks for common rules
   - 0.1ms logic validation vs 100ms symbolic

3. **Executive Planning**
   - Logical gate circuits for plan validation
   - Precondition checking via AND gates
   - Conflict detection via XOR gates

4. **Working Memory**
   - Logic gates for memory retrieval conditions
   - "Retrieve IF condition AND salience > threshold"

---

## Recommended Integration Strategy

### High Priority (Critical Gaps)

1. **Wire Neural Logic into Brain Processing**
   ```c
   // Add to brain_process_multimodal() after Stage 3
   if (brain->logic) {
       neural_logic_update(brain->logic, timestamp, 100);  // Update logic neurons
   }
   ```

2. **Integrate Symbolic Logic with Knowledge Module**
   ```c
   // In knowledge_add_concept()
   if (brain->symbolic_logic) {
       logic_clause_t* fact = create_isa_clause(concept, parent);
       symbolic_logic_add_fact(brain->symbolic_logic, fact, salience);
   }
   ```

3. **Wire Logic into Ethics for Rule Checking**
   ```c
   // In ethics_evaluate()
   if (brain->symbolic_logic) {
       logic_clause_t* action_clause = encode_action(action);
       bool* violates_rule = NULL;
       symbolic_logic_query(brain->symbolic_logic, action_clause, &violates_rule);
       if (violates_rule) return ETHICAL_FORBIDDEN;
   }
   ```

### Medium Priority

4. **Add Logic to Executive Function**
   - Goal decomposition via inference rules
   - Plan validation via theorem proving

5. **Theory of Mind Belief Modeling**
   - Represent beliefs as logical propositions
   - Infer mental states via symbolic reasoning

6. **Curiosity-Driven Exploration**
   - Use `symbolic_logic_explore()` to find knowledge gaps
   - Generate hypotheses via abduction

### Low Priority

7. **NLP Semantic Parsing**
   - Convert sentences to logical forms
   - Question answering via logic queries

8. **Predictive Causal Reasoning**
   - Encode causal rules in logic
   - Predict via forward chaining

---

## Performance Considerations

### Neural Logic (Fast)
- **GPU:** ~0.1ms for 1000 operations
- **CPU:** ~10ms for 1000 operations
- **Use for:** Real-time constraints, fast validation

### Symbolic Logic (Slower)
- **Inference:** ~100ms+ for complex proofs
- **Use for:** Deep reasoning, explanations, offline planning

### Hybrid Approach (Recommended)
1. Neural logic for real-time checks (ethics, constraints)
2. Symbolic logic for deep reasoning (planning, explanations)
3. Neural logic can "compile" frequently-used symbolic rules

---

## Action Items

### Immediate (Phase 1)
- [ ] Add `neural_logic_update()` to brain processing loop
- [ ] Wire symbolic logic into Knowledge module (fact storage)
- [ ] Wire symbolic logic into Ethics module (rule checking)

### Short-term (Phase 2)
- [ ] Integrate logic with Executive function (planning)
- [ ] Add logic to Theory of Mind (belief modeling)
- [ ] Connect Curiosity to symbolic exploration

### Long-term (Phase 3)
- [ ] NLP semantic parsing to logic
- [ ] Predictive causal reasoning
- [ ] Neural-symbolic hybrid learning

---

## Conclusion

**Both logic modules are initialized but dormant:**
- Neural logic network created but never executed (no `neural_logic_update()` calls)
- Symbolic logic engine created but only used optionally in Explanations

**Critical Gap:** Knowledge, Ethics, Executive, Theory of Mind, Curiosity, and Predictive modules perform NO logical reasoning despite having access to both neural and symbolic logic engines.

**Impact:**
- Knowledge graph can't perform inference
- Ethics can't check logical consistency of moral rules
- Executive can't do logical planning
- Theory of Mind can't model beliefs symbolically
- Curiosity can't systematically explore knowledge gaps
- NLP can't map language to logical meaning

**Recommendation:** Logic modules are well-designed but need 5-10 integration points across cognitive modules to be useful.
