"""
NIMCP Training Dataset Library
Comprehensive collection of training datasets for neural network demonstration
"""

import random
from typing import List, Dict, Tuple, Any

class TrainingDataset:
    """Base class for training datasets"""

    def __init__(self, name: str, description: str, difficulty: str):
        self.name = name
        self.description = description
        self.difficulty = difficulty  # "easy", "medium", "hard", "expert"
        self.category = "unknown"

    def generate_samples(self, count: int = 100) -> List[Dict[str, Any]]:
        """Generate training samples. Override in subclasses."""
        raise NotImplementedError


# ============================================================================
# CATEGORY 1: BASIC VISUAL PATTERNS (3x3 grid)
# ============================================================================

class BasicPatterns(TrainingDataset):
    """Basic 3x3 grid patterns - good starting point"""

    def __init__(self):
        super().__init__(
            name="Basic Patterns",
            description="Simple 3x3 grid patterns: vertical, horizontal, diagonal lines",
            difficulty="easy"
        )
        self.category = "visual"

    def generate_samples(self, count: int = 100) -> List[Dict[str, Any]]:
        """Generate basic pattern samples"""
        samples = []
        patterns = {
            'vertical': [
                [1, 0, 0],
                [1, 0, 0],
                [1, 0, 0]
            ],
            'horizontal': [
                [1, 1, 1],
                [0, 0, 0],
                [0, 0, 0]
            ],
            'diagonal': [
                [1, 0, 0],
                [0, 1, 0],
                [0, 0, 1]
            ]
        }

        for _ in range(count):
            pattern_name = random.choice(['vertical', 'horizontal', 'diagonal'])
            pattern = patterns[pattern_name]

            # Flatten to 1D
            input_data = [cell for row in pattern for cell in row]

            # Output: one-hot encoding of pattern type
            output = [1.0 if pattern_name == 'vertical' else 0.0,
                     1.0 if pattern_name == 'horizontal' else 0.0,
                     1.0 if pattern_name == 'diagonal' else 0.0]

            samples.append({
                'input': input_data,
                'output': output,
                'label': pattern_name,
                'metadata': {'pattern_type': pattern_name}
            })

        return samples


class ComplexPatterns(TrainingDataset):
    """Complex 3x3 grid patterns"""

    def __init__(self):
        super().__init__(
            name="Complex Patterns",
            description="Advanced patterns: corners, crosses, spirals, frames, centers",
            difficulty="medium"
        )
        self.category = "visual"

    def generate_samples(self, count: int = 100) -> List[Dict[str, Any]]:
        """Generate complex pattern samples"""
        samples = []
        patterns = {
            'cross': [
                [0, 1, 0],
                [1, 1, 1],
                [0, 1, 0]
            ],
            'x_pattern': [
                [1, 0, 1],
                [0, 1, 0],
                [1, 0, 1]
            ],
            'corners': [
                [1, 0, 1],
                [0, 0, 0],
                [1, 0, 1]
            ],
            'frame': [
                [1, 1, 1],
                [1, 0, 1],
                [1, 1, 1]
            ],
            'center': [
                [0, 0, 0],
                [0, 1, 0],
                [0, 0, 0]
            ],
            'spiral': [
                [1, 1, 1],
                [0, 0, 1],
                [1, 1, 1]
            ],
            'checker': [
                [1, 0, 1],
                [0, 1, 0],
                [1, 0, 1]
            ],
            'l_shape': [
                [1, 0, 0],
                [1, 0, 0],
                [1, 1, 1]
            ]
        }

        pattern_names = list(patterns.keys())

        for _ in range(count):
            pattern_name = random.choice(pattern_names)
            pattern = patterns[pattern_name]

            # Flatten to 1D
            input_data = [cell for row in pattern for cell in row]

            # Output: one-hot encoding
            output = [1.0 if pattern_name == name else 0.0 for name in pattern_names]

            samples.append({
                'input': input_data,
                'output': output,
                'label': pattern_name,
                'metadata': {'pattern_type': pattern_name, 'complexity': 'high'}
            })

        return samples


# ============================================================================
# CATEGORY 2: TEMPORAL SEQUENCES
# ============================================================================

class TemporalSequences(TrainingDataset):
    """Temporal pattern recognition"""

    def __init__(self):
        super().__init__(
            name="Temporal Sequences",
            description="Time-based patterns: moving dots, expanding patterns, rotations",
            difficulty="medium"
        )
        self.category = "temporal"

    def generate_samples(self, count: int = 100) -> List[Dict[str, Any]]:
        """Generate temporal sequences"""
        samples = []

        sequence_types = ['moving_dot', 'expanding', 'rotating', 'wave']

        for _ in range(count):
            seq_type = random.choice(sequence_types)

            if seq_type == 'moving_dot':
                # Dot moves from top-left to bottom-right
                sequence = [
                    [[1, 0, 0], [0, 0, 0], [0, 0, 0]],  # t=0
                    [[0, 0, 0], [0, 1, 0], [0, 0, 0]],  # t=1
                    [[0, 0, 0], [0, 0, 0], [0, 0, 1]]   # t=2
                ]
            elif seq_type == 'expanding':
                # Pattern grows from center
                sequence = [
                    [[0, 0, 0], [0, 1, 0], [0, 0, 0]],  # t=0
                    [[0, 1, 0], [1, 1, 1], [0, 1, 0]],  # t=1
                    [[1, 1, 1], [1, 1, 1], [1, 1, 1]]   # t=2
                ]
            elif seq_type == 'rotating':
                # Line rotates clockwise
                sequence = [
                    [[1, 1, 1], [0, 0, 0], [0, 0, 0]],  # horizontal top
                    [[0, 0, 1], [0, 0, 1], [0, 0, 1]],  # vertical right
                    [[0, 0, 0], [0, 0, 0], [1, 1, 1]]   # horizontal bottom
                ]
            else:  # wave
                # Wave pattern moving down
                sequence = [
                    [[1, 0, 1], [0, 0, 0], [0, 0, 0]],
                    [[0, 0, 0], [1, 0, 1], [0, 0, 0]],
                    [[0, 0, 0], [0, 0, 0], [1, 0, 1]]
                ]

            # Flatten each timestep
            input_sequence = [cell for frame in sequence for row in frame for cell in row]

            # Output: one-hot encoding of sequence type
            output = [1.0 if seq_type == t else 0.0 for t in sequence_types]

            samples.append({
                'input': input_sequence,
                'output': output,
                'label': seq_type,
                'metadata': {'sequence_type': seq_type, 'timesteps': len(sequence)}
            })

        return samples


# ============================================================================
# CATEGORY 3: LOGIC GATES & SIMPLE TASKS
# ============================================================================

class LogicGates(TrainingDataset):
    """Boolean logic gate tasks"""

    def __init__(self):
        super().__init__(
            name="Logic Gates",
            description="Boolean operations: AND, OR, XOR, NAND, NOR",
            difficulty="easy"
        )
        self.category = "logic"

    def generate_samples(self, count: int = 100) -> List[Dict[str, Any]]:
        """Generate logic gate samples"""
        samples = []

        gates = {
            'AND': lambda a, b: float(a and b),
            'OR': lambda a, b: float(a or b),
            'XOR': lambda a, b: float(a ^ b),
            'NAND': lambda a, b: float(not (a and b)),
            'NOR': lambda a, b: float(not (a or b))
        }

        for _ in range(count):
            gate_name = random.choice(list(gates.keys()))
            gate_func = gates[gate_name]

            # Generate random boolean inputs
            a = random.choice([0, 1])
            b = random.choice([0, 1])

            # Compute output
            result = gate_func(a, b)

            # Input: [a, b, one-hot encoding of gate type]
            input_data = [float(a), float(b)] + [1.0 if gate_name == g else 0.0 for g in gates.keys()]

            # Output: result
            output = [result]

            samples.append({
                'input': input_data,
                'output': output,
                'label': f"{gate_name}({a},{b})={int(result)}",
                'metadata': {'gate': gate_name, 'inputs': [a, b], 'result': result}
            })

        return samples


class ArithmeticTasks(TrainingDataset):
    """Simple arithmetic operations"""

    def __init__(self):
        super().__init__(
            name="Arithmetic Tasks",
            description="Basic math: addition, subtraction, comparison",
            difficulty="medium"
        )
        self.category = "arithmetic"

    def generate_samples(self, count: int = 100) -> List[Dict[str, Any]]:
        """Generate arithmetic samples"""
        samples = []

        operations = {
            'add': lambda a, b: a + b,
            'subtract': lambda a, b: a - b,
            'multiply': lambda a, b: a * b,
            'greater': lambda a, b: 1.0 if a > b else 0.0,
            'equal': lambda a, b: 1.0 if a == b else 0.0
        }

        for _ in range(count):
            op_name = random.choice(list(operations.keys()))
            op_func = operations[op_name]

            # Generate small integers for simplicity
            a = random.randint(0, 9)
            b = random.randint(0, 9)

            # Normalize inputs to [0, 1]
            result = op_func(a, b)

            # Input: [normalized a, normalized b, one-hot operation]
            input_data = [a/10.0, b/10.0] + [1.0 if op_name == op else 0.0 for op in operations.keys()]

            # Output: normalized result
            if op_name in ['greater', 'equal']:
                output = [result]
            else:
                output = [min(result/20.0, 1.0)]  # Normalize to [0,1]

            samples.append({
                'input': input_data,
                'output': output,
                'label': f"{a} {op_name} {b} = {result}",
                'metadata': {'operation': op_name, 'a': a, 'b': b, 'result': result}
            })

        return samples


# ============================================================================
# CATEGORY 4: SYMBOLIC REASONING
# ============================================================================

class SymbolicLogic(TrainingDataset):
    """Symbolic logic reasoning tasks"""

    def __init__(self):
        super().__init__(
            name="Symbolic Logic",
            description="First-order logic: implications, syllogisms, modus ponens",
            difficulty="hard"
        )
        self.category = "symbolic"

    def generate_samples(self, count: int = 100) -> List[Dict[str, Any]]:
        """Generate symbolic logic samples"""
        samples = []

        # Simple propositional logic problems
        # Encode as: [P, Q, R, rule_type] -> [conclusion]

        rules = {
            'modus_ponens': lambda p, q: q if p else 0.5,  # If P then Q, P is true -> Q is true
            'modus_tollens': lambda p, q: (1.0 - p) if (1.0 - q) else 0.5,  # If P then Q, not Q -> not P
            'syllogism': lambda p, q, r: r if (p and q) else 0.5,  # If P then Q, If Q then R, P -> R
            'disjunction': lambda p, q: max(p, q),  # P or Q
            'conjunction': lambda p, q: min(p, q),  # P and Q
        }

        for _ in range(count):
            rule_name = random.choice(list(rules.keys()))
            rule_func = rules[rule_name]

            # Generate random truth values
            p = random.choice([0.0, 1.0])
            q = random.choice([0.0, 1.0])
            r = random.choice([0.0, 1.0])

            # Compute conclusion based on rule
            if rule_name == 'syllogism':
                conclusion = rule_func(p, q, r)
                inputs = [p, q, r]
            else:
                conclusion = rule_func(p, q)
                inputs = [p, q, 0.0]

            # Input: truth values + one-hot rule encoding
            input_data = inputs + [1.0 if rule_name == r else 0.0 for r in rules.keys()]

            # Output: conclusion
            output = [conclusion]

            samples.append({
                'input': input_data,
                'output': output,
                'label': f"{rule_name}: {conclusion}",
                'metadata': {'rule': rule_name, 'premises': inputs, 'conclusion': conclusion}
            })

        return samples


class SequentialReasoning(TrainingDataset):
    """Multi-step reasoning tasks"""

    def __init__(self):
        super().__init__(
            name="Sequential Reasoning",
            description="Multi-step logical chains and causal reasoning",
            difficulty="expert"
        )
        self.category = "symbolic"

    def generate_samples(self, count: int = 100) -> List[Dict[str, Any]]:
        """Generate sequential reasoning samples"""
        samples = []

        # Chain: A -> B -> C -> D
        # Task: Given A and chain rules, deduce D

        for _ in range(count):
            # Generate chain
            a = random.choice([0.0, 1.0])

            # Reasoning chain with some noise
            b = a if random.random() > 0.1 else 1.0 - a
            c = b if random.random() > 0.1 else 1.0 - b
            d = c if random.random() > 0.1 else 1.0 - c

            # Input: initial condition + encoded chain structure
            chain_encoding = [a, 0.9, 0.9, 0.9]  # confidence in each link
            input_data = [a] + chain_encoding

            # Output: final conclusion
            output = [d]

            samples.append({
                'input': input_data,
                'output': output,
                'label': f"A({a}) -> D({d})",
                'metadata': {'chain': [a, b, c, d], 'reasoning_steps': 3}
            })

        return samples


# ============================================================================
# DATASET LIBRARY REGISTRY
# ============================================================================

class DatasetLibrary:
    """Central registry for all training datasets"""

    def __init__(self):
        self.datasets = {
            # Visual patterns
            'basic_patterns': BasicPatterns(),
            'complex_patterns': ComplexPatterns(),

            # Temporal
            'temporal_sequences': TemporalSequences(),

            # Logic tasks
            'logic_gates': LogicGates(),
            'arithmetic': ArithmeticTasks(),

            # Symbolic reasoning
            'symbolic_logic': SymbolicLogic(),
            'sequential_reasoning': SequentialReasoning(),
        }

    def get_dataset(self, name: str) -> TrainingDataset:
        """Get dataset by name"""
        return self.datasets.get(name)

    def list_datasets(self) -> List[Dict[str, str]]:
        """List all available datasets with metadata"""
        return [
            {
                'id': name,
                'name': dataset.name,
                'description': dataset.description,
                'difficulty': dataset.difficulty,
                'category': dataset.category
            }
            for name, dataset in self.datasets.items()
        ]

    def get_datasets_by_category(self, category: str) -> List[str]:
        """Get datasets filtered by category"""
        return [
            name for name, dataset in self.datasets.items()
            if dataset.category == category
        ]

    def get_datasets_by_difficulty(self, difficulty: str) -> List[str]:
        """Get datasets filtered by difficulty"""
        return [
            name for name, dataset in self.datasets.items()
            if dataset.difficulty == difficulty
        ]


# Global library instance
library = DatasetLibrary()
