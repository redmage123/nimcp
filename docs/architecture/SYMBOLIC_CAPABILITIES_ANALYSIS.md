# NIMCP Symbolic Engine & Logic Capabilities Analysis

## Executive Summary

NIMCP includes a comprehensive **symbolic logic engine** with first-order logic support, inference mechanisms, and brain-integrated knowledge representation. This document outlines the symbolic capabilities available for incorporation into training datasets.

---

## 1. CORE SYMBOLIC LOGIC ENGINE

### Location
- **Header**: `/home/bbrelin/nimcp/src/include/cognitive/nimcp_symbolic_logic.h`
- **Implementation**: `/home/bbrelin/nimcp/src/cognitive/logic/nimcp_symbolic_logic.c`
- **Tests**: `/home/bbrelin/nimcp/src/tests/test_symbolic_logic.cpp`

### What It Does

A biologically-inspired symbolic reasoning system that mimics logical reasoning processes across brain regions:
- **Prefrontal Cortex**: Abstract reasoning and planning
- **Hippocampus**: Fact storage and retrieval
- **Working Memory**: Active inference and unification

### Key Components

#### 1.1 Logical Terms
```c
typedef enum {
    TERM_VARIABLE,    // Variables (e.g., x, y)
    TERM_CONSTANT,    // Constants (e.g., john, 5)
    TERM_FUNCTION     // Functions (e.g., f(x))
} term_type_t;
```

**Capabilities**:
- Create variables for quantification
- Handle constants for specific entities
- Support function terms for complex expressions
- Maximum 26 variables (a-z)
- Maximum arity of 4 arguments per function

#### 1.2 Atomic Formulas (Predicates)
```c
typedef struct {
    char name[LOGIC_MAX_NAME_LENGTH];
    logical_term_t** terms;    // Predicate arguments
    uint8_t arity;             // Number of arguments
    bool negated;              // True if ¬P (negation)
} atomic_formula_t;
```

**Capabilities**:
- Define predicates with variable arity
- Support negation for negative facts
- Maximum 1000 predicates in a system
- Examples: `Human(socrates)`, `Mortal(x)`, `Raining`

#### 1.3 Logical Operators
```c
typedef enum {
    OP_AND,        // Conjunction (∧)
    OP_OR,         // Disjunction (∨)
    OP_NOT,        // Negation (¬)
    OP_IMPLIES,    // Implication (→)
    OP_IFF,        // Biconditional (↔)
    OP_FORALL,     // Universal quantifier (∀)
    OP_EXISTS      // Existential quantifier (∃)
} logical_operator_t;
```

**Symbolic Expression Examples**:
- `Human(socrates) & Mortal(socrates)` - Conjunction
- `Raining | Snowing` - Disjunction
- `Human(x) -> Mortal(x)` - Implication
- `∀x Human(x) -> Mortal(x)` - Universal quantification
- `∃x Human(x)` - Existential quantification

#### 1.4 Formulas in CNF
```c
typedef struct {
    atomic_formula_t** literals;
    uint32_t num_literals;
    float confidence;          // Confidence [0,1]
} logic_clause_t;
```

**Capabilities**:
- Convert arbitrary formulas to Conjunctive Normal Form (CNF)
- Support confidence-weighted clauses
- Maximum 16 literals per clause
- Fundamental for resolution-based proving

---

## 2. INFERENCE ENGINE

### 2.1 Forward Chaining

**What**: Derive new facts from existing facts and rules
**How**: Data-driven reasoning (bottom-up)

```c
bool symbolic_logic_forward_chain(
    symbolic_logic_t* logic,
    uint32_t max_iterations,
    logic_clause_t*** new_facts,
    int* num_new_facts
);
```

**Use Cases**:
- Discover implications of known facts
- Build knowledge base through inference
- Generate training examples automatically
- Model human pattern recognition

**Example**:
```
Facts: Human(socrates), Human(plato)
Rule:  Human(x) -> Mortal(x)
Derived: Mortal(socrates), Mortal(plato)
```

### 2.2 Backward Chaining

**What**: Prove goals from existing facts and rules
**How**: Goal-driven reasoning (top-down)

```c
bool symbolic_logic_backward_chain(
    symbolic_logic_t* logic,
    logic_clause_t* goal,
    inference_rule_t*** proof_trace,
    int* num_steps
);
```

**Use Cases**:
- Answer specific questions
- Verify hypotheses
- Explain reasoning steps
- Generate proof traces for training

### 2.3 Resolution-Based Theorem Proving

**What**: Automated theorem proving using resolution inference rule
**How**: Proof by contradiction with CNF conversion

```c
bool symbolic_logic_resolve(
    symbolic_logic_t* logic,
    logic_clause_t* negated_goal,
    bool* derived_empty
);
```

**Use Cases**:
- Formal verification of logical claims
- Contradiction detection
- Generate challenging reasoning tasks
- Model rigorous logical argumentation

---

## 3. UNIFICATION & SUBSTITUTION

### 3.1 Unification Algorithm

**What**: Find substitution that makes two terms identical
**How**: Classical Robinson unification algorithm

```c
unification_t* symbolic_logic_unify(
    logical_term_t* term1,
    logical_term_t* term2
);
```

**Capabilities**:
- Unify constants, variables, and functions
- Track variable bindings
- Support recursive term structures
- Provide statistics on success rates

**Example**:
```
Unify(Human(x), Human(socrates))
Result: x = socrates
```

### 3.2 Substitution

```c
logical_term_t* symbolic_logic_substitute(
    logical_term_t* term,
    const substitution_t* subst
);
```

**Use Cases**:
- Apply variable bindings to formulas
- Generate specific instances from templates
- Create training examples from rules

---

## 4. KNOWLEDGE BASE MANAGEMENT

### 4.1 Knowledge Entry Structure
```c
typedef struct {
    logic_clause_t* clause;
    float salience;            // Importance [0,1]
    uint64_t timestamp;        // When added
    char context[64];          // Context label
} kb_entry_t;
```

**Features**:
- Store facts with confidence scores
- Track temporal information
- Label facts with context
- Support salience-based retrieval

### 4.2 Inference Rules
```c
typedef struct {
    char name[LOGIC_MAX_NAME_LENGTH];
    logic_clause_t** premises;
    uint32_t num_premises;
    logic_clause_t* conclusion;
    float priority;            // Rule priority
} inference_rule_t;
```

**Limits**:
- Maximum 500 rules per system
- Maximum 200 facts per knowledge base
- Support rule prioritization

### 4.3 Query Operations

```c
bool symbolic_logic_query(
    symbolic_logic_t* logic,
    logic_clause_t* query,
    kb_entry_t*** results,
    int* num_results
);
```

**Capabilities**:
- Pattern matching against knowledge base
- Return matching facts with metadata
- Support complex queries

---

## 5. BRAIN INTEGRATION FEATURES

### 5.1 Novelty Detection

```c
float symbolic_logic_compute_novelty(
    symbolic_logic_t* logic,
    logic_clause_t* clause
);
```

**Returns**: 0.0 = familiar, 1.0 = completely novel
**Use Cases**:
- Identify surprising inferences
- Guide curiosity-driven learning
- Rank facts by interestingness

### 5.2 Salience Management

```c
bool symbolic_logic_get_salient_facts(
    symbolic_logic_t* logic,
    int top_k,
    kb_entry_t*** salient_facts,
    int* num_facts
);
```

**Use Cases**:
- Retrieve most important facts
- Implement attention mechanisms
- Focus reasoning on relevant knowledge

### 5.3 Memory Consolidation

```c
bool symbolic_logic_consolidate_memory(
    symbolic_logic_t* logic,
    logic_clause_t* clause,
    float salience,
    const char* context
);
```

**Mimics**: Hippocampal consolidation of memories
**Features**:
- Store facts with context labels
- Integrate with brain learning systems
- Support consolidation pipelines

### 5.4 Curiosity-Driven Exploration

```c
bool symbolic_logic_explore(
    symbolic_logic_t* logic,
    uint32_t exploration_depth,
    logic_clause_t*** interesting_facts,
    int* num_facts
);
```

**Use Cases**:
- Autonomous discovery of implications
- Generate training examples from inference
- Model intrinsic motivation

---

## 6. MULTI-DOMAIN KNOWLEDGE SYSTEM

### Location
- **Header**: `/home/bbrelin/nimcp/src/cognitive/knowledge/nimcp_knowledge.h`
- **Implementation**: `/home/bbrelin/nimcp/src/cognitive/knowledge/nimcp_knowledge.c`

### Supported Knowledge Domains

```c
typedef enum {
    KNOWLEDGE_DOMAIN_LANGUAGE,    // Words, grammar, communication
    KNOWLEDGE_DOMAIN_LITERATURE,  // Stories, poetry, books
    KNOWLEDGE_DOMAIN_ART,         // Visual art, music, creativity
    KNOWLEDGE_DOMAIN_ETHICS,      // Right/wrong, values, morality
    KNOWLEDGE_DOMAIN_HISTORY,     // Past events, people, civilizations
    KNOWLEDGE_DOMAIN_SCIENCE,     // Natural world, physics, biology
    KNOWLEDGE_DOMAIN_MATHEMATICS, // Numbers, patterns, logic
    KNOWLEDGE_DOMAIN_SOCIAL,      // Relationships, society, culture
    KNOWLEDGE_DOMAIN_TECHNICAL,   // How things work, skills
    KNOWLEDGE_DOMAIN_PHILOSOPHY,  // Meaning, existence, thinking
    KNOWLEDGE_DOMAIN_GENERAL      // General world knowledge
} knowledge_domain_t;
```

### 6.1 Knowledge Item Representation
```c
typedef struct {
    char concept[256];              // Main concept
    knowledge_domain_t domain;      // Which domain
    char definition[1024];          // What it means
    char context[512];              // When/where/why relevant
    char** examples;                // Example instances
    uint32_t num_examples;
    char** related_concepts;        // Related ideas
    uint32_t num_related;
    float confidence;               // Understanding level [0-1]
    uint64_t learned_timestamp;     // When learned
    uint32_t reinforcement_count;   // Repetition count
} knowledge_item_t;
```

### 6.2 Narrative Knowledge
```c
typedef struct {
    char title[256];
    char author[128];
    char summary[2048];
    char** characters;
    uint32_t num_characters;
    char** themes;                  // love, courage, betrayal, etc.
    uint32_t num_themes;
    char** moral_lessons;           // What we learn
    uint32_t num_lessons;
    knowledge_domain_t primary_domain;
    char cultural_context[256];
} narrative_knowledge_t;
```

**Use Cases**:
- Learn ethical reasoning from stories
- Understand human behavior patterns
- Extract symbolic lessons and morals

### 6.3 Aesthetic Knowledge
```c
typedef struct {
    char work_title[256];
    char creator[128];
    char medium[64];                // painting, sculpture, music
    char description[1024];
    char** aesthetic_qualities;     // beautiful, haunting, joyful
    uint32_t num_qualities;
    char emotional_impact[256];
    char historical_significance[512];
} aesthetic_knowledge_t;
```

### 6.4 Historical Knowledge
```c
typedef struct {
    char event_name[256];
    uint64_t timestamp_year;
    char** key_people;
    uint32_t num_people;
    char causes[1024];              // Why it happened
    char effects[1024];             // What resulted
    char significance[512];         // Why it matters
    char** related_events;
    uint32_t num_related_events;
} historical_knowledge_t;
```

### 6.5 Learning Methods

| Method | Purpose | Use Case |
|--------|---------|----------|
| `knowledge_learn_from_text()` | Extract concepts from text | Process articles, definitions |
| `knowledge_learn_from_story()` | Learn from narratives | Ethical/social reasoning |
| `knowledge_learn_from_art()` | Learn aesthetics | Creativity, appreciation |
| `knowledge_learn_from_history()` | Learn cause/effect patterns | Understanding consequences |
| `knowledge_learn_from_conversation()` | Social learning | Dialogue understanding |
| `knowledge_learn_from_demonstration()` | Learn by observation | Procedural knowledge |

### 6.6 Cross-Domain Learning

```c
uint32_t knowledge_find_connections(
    knowledge_system_t system,
    const char* concept,
    knowledge_item_t* connections,
    uint32_t max_connections
);

bool knowledge_transfer_learning(
    knowledge_system_t system,
    knowledge_domain_t source_domain,
    knowledge_domain_t target_domain,
    const char* situation,
    char* application,
    uint32_t max_length
);
```

**Use Cases**:
- Find metaphorical connections
- Apply lessons from one domain to another
- Generate analogical reasoning tasks

### 6.7 Indexed Queries (B-Tree)

```c
uint32_t knowledge_get_by_confidence_range(
    knowledge_system_t system,
    float min_confidence,
    float max_confidence,
    knowledge_item_t** results_out
);
```

**Features**:
- O(log n + k) range queries by confidence
- Find well-understood concepts: [0.8, 1.0]
- Find weak knowledge needing reinforcement: [0.0, 0.4]
- Efficient batch queries

---

## 7. STATISTICS & MONITORING

### Logic Engine Statistics
```c
typedef struct {
    uint64_t inferences_performed;
    uint32_t facts_stored;
    uint32_t rules_applied;
    float avg_inference_time;          // ms
    uint32_t unification_attempts;
    uint32_t unification_successes;
} logic_stats_t;
```

### Domain Knowledge Assessment
```c
typedef struct {
    knowledge_domain_t domain;
    uint32_t concepts_known;
    uint32_t estimated_total;
    float coverage_percentage;
    float avg_confidence;
    char gaps[5][256];                 // Top knowledge gaps
    uint32_t num_gaps;
} domain_knowledge_t;
```

---

## 8. TRAINING DATASET INCORPORATION OPPORTUNITIES

### 8.1 Symbolic Logic Examples

Generate training data using:

1. **Unification Puzzles**
   ```
   Input: Unify(P(x, y), P(john, mary))
   Output: x=john, y=mary
   ```

2. **Inference Tasks**
   ```
   Facts: Human(socrates), ∀x Human(x) → Mortal(x)
   Query: Mortal(socrates)?
   Output: Yes (with proof)
   ```

3. **Resolution Proofs**
   ```
   Goal: Prove ¬(Human(socrates) ∧ ¬Mortal(socrates))
   Process: Convert to CNF, apply resolution, derive empty clause
   Output: Proof steps
   ```

4. **Novelty Detection**
   ```
   KB: [Human(socrates), Human(plato)]
   New Fact: Mortal(aristotle)
   Output: Novelty score = 0.8
   ```

### 8.2 Knowledge System Examples

Generate training data using:

1. **Multi-Domain Questions**
   ```
   Domain: Literature + Ethics
   Question: "What does Romeo and Juliet teach us about consequences?"
   Answer: [Cross-domain connections and moral lessons]
   ```

2. **Confidence-Based Learning**
   ```
   Stage 1: Low confidence (0.2) - Initial exposure
   Stage 2: Medium confidence (0.5) - Understanding
   Stage 3: High confidence (0.9) - Mastery
   ```

3. **Cross-Domain Transfer**
   ```
   Source: Art (composition principles)
   Target: Photography (framing)
   Lesson: Apply grid techniques across domains
   ```

4. **Knowledge Gap Identification**
   ```
   Query: knowledge_get_by_confidence_range(0.0, 0.4)
   Output: Concepts needing reinforcement
   ```

### 8.3 Dataset Format Examples

**For Symbolic Logic**:
```json
{
  "type": "symbolic_logic_inference",
  "knowledge_base": [
    {"predicate": "Human", "args": ["socrates"]},
    {"predicate": "Human", "args": ["plato"]}
  ],
  "rule": "∀x Human(x) → Mortal(x)",
  "query": "Mortal(socrates)",
  "answer": true,
  "explanation": "Forward chaining from Human(socrates) and rule"
}
```

**For Knowledge Systems**:
```json
{
  "type": "multi_domain_learning",
  "domain_source": "Literature",
  "domain_target": "Ethics",
  "story": "Romeo and Juliet",
  "lesson": "Hasty decisions lead to tragedy",
  "application": "In real life, consider consequences before acting"
}
```

---

## 9. CONFIGURATION & LIMITS

### Logic Engine Configuration
```c
typedef struct {
    uint32_t max_predicates;              // Max 1000
    uint32_t max_rules;                   // Max 500
    uint32_t max_kb_size;                 // Max 200
    uint32_t max_inference_depth;         // Recursion limit
    bool enable_forward_chaining;         // Toggle feature
    bool enable_backward_chaining;        // Toggle feature
    bool enable_resolution;               // Toggle feature
    bool enable_memory_consolidation;     // Toggle feature
} logic_config_t;
```

### Size Constraints
| Component | Maximum |
|-----------|---------|
| Predicates | 1000 |
| Rules | 500 |
| Knowledge Base Facts | 200 |
| Variables | 26 (a-z) |
| Predicate Arity | 4 |
| Clause Literals | 16 |
| Name Length | 64 characters |

---

## 10. TESTING INFRASTRUCTURE

### Test File
- `/home/bbrelin/nimcp/src/tests/test_symbolic_logic.cpp`

### Test Coverage
- Term creation and unification
- Atomic formula creation and matching
- Knowledge base operations
- Query matching
- Forward chaining inference
- Unification with different term types
- Novelty detection
- Salience-based retrieval
- Memory consolidation
- Edge cases and overflow conditions

### Running Tests
```bash
cd /home/bbrelin/nimcp/build
make cognitive_tests
./src/tests/cognitive_tests
```

---

## 11. INTEGRATION WITH BRAIN SYSTEM

The symbolic logic engine integrates with:

1. **Attention System**: Uses salience to focus on important facts
2. **Memory System**: Consolidates knowledge with temporal information
3. **Curiosity System**: Drives exploration based on novelty
4. **Introspection**: Enables self-reasoning about knowledge
5. **Ethics System**: Supports moral reasoning with symbolic rules
6. **Well-being System**: Can encode goals and values

---

## 12. EXAMPLE USAGE PATTERNS

### Pattern 1: Learn & Infer
```c
// Create logic engine
logic_config_t config = {...};
symbolic_logic_t* logic = symbolic_logic_create(&config);

// Add facts
symbolic_logic_add_fact(logic, human_fact, 0.9f);

// Add rule
symbolic_logic_add_rule(logic, mortal_rule);

// Perform inference
symbolic_logic_forward_chain(logic, 5, &new_facts, &num_new_facts);

// Query novelty
float novelty = symbolic_logic_compute_novelty(logic, new_fact);
```

### Pattern 2: Knowledge Acquisition
```c
// Create knowledge system
knowledge_system_t ks = knowledge_system_create("learner");

// Learn from multiple sources
knowledge_learn_from_text(ks, article_text, KNOWLEDGE_DOMAIN_SCIENCE);
knowledge_learn_from_story(ks, story_struct, KNOWLEDGE_DOMAIN_ETHICS);

// Find connections
knowledge_find_connections(ks, "concept", connections, max);

// Query by confidence
knowledge_get_by_confidence_range(ks, 0.8f, 1.0f, &well_understood);
```

---

## 13. RECOMMENDATIONS FOR TRAINING DATA

### High-Priority Symbolic Tasks
1. Logic puzzles with varying difficulty
2. Unification examples (simple to complex)
3. Inference chains (short to long)
4. Novelty estimation tasks
5. Contradiction detection

### High-Priority Knowledge Tasks
1. Multi-domain concept linking
2. Cross-domain analogies
3. Story-based moral reasoning
4. Historical cause-effect reasoning
5. Confidence progression (learning curves)

### Balanced Dataset Structure
- 40% pure symbolic logic tasks
- 30% knowledge system tasks
- 20% cross-domain integration tasks
- 10% edge cases and error handling

---

## Summary

NIMCP provides:
- **First-order logic** with variables, constants, and functions
- **Three inference methods**: forward chaining, backward chaining, resolution
- **Full unification support** for pattern matching
- **11 knowledge domains** with specialized learning
- **Brain-integrated features**: novelty, salience, consolidation
- **Scalable architecture**: configurable limits and performance tuning

These capabilities enable rich symbolic reasoning, knowledge integration, and learning that can be directly incorporated into training datasets for improved logical reasoning and knowledge management in neural systems.
