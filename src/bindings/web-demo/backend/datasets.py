"""
NIMCP Web Demo - Multi-Dataset Support v3.0.0
==============================================

WHAT: Comprehensive dataset collection for demonstrating NIMCP across different problem domains
WHY:  Showcase NIMCP's versatility in handling various types of learning tasks
HOW:  Unified Dataset interface with different data types and configurations

Supported Datasets:
1. Iris - Classic flower classification (4 features, 3 classes)
2. MNIST - Handwritten digit recognition (784 features, 10 classes)
3. Titanic - Survival prediction (8 features, 2 classes)
4. Visual Patterns - Simple geometric pattern recognition (64 features, 4 patterns)
5. XOR/Logic Gates - Classic neural network test (2-4 features, 1 output)
6. Sine Wave - Time series prediction (20 time steps → 1 value)
"""

import numpy as np
from typing import Dict, List, Tuple, Optional
from benchmarks import MNISTLoader
import math

#=============================================================================
# Dataset Base Class
#=============================================================================

class Dataset:
    """Base class for all datasets with unified interface"""

    def __init__(self):
        self.name = "Base Dataset"
        self.description = "Base dataset class"
        self.num_inputs = 0
        self.num_outputs = 0
        self.classes = []
        self.feature_names = []
        self.input_type = "numerical"  # "numerical", "image", "time_series"

    def get_config(self) -> Dict:
        """Get brain configuration for this dataset"""
        return {
            'num_inputs': self.num_inputs,
            'num_outputs': self.num_outputs,
            'classes': self.classes,
            'feature_names': self.feature_names,
            'input_type': self.input_type
        }

    def get_examples(self, count: int = 10) -> List[Tuple[List[float], str]]:
        """Get sample training examples"""
        raise NotImplementedError

    def get_test_examples(self, count: int = 5) -> List[Tuple[List[float], str]]:
        """Get test examples for prediction"""
        raise NotImplementedError

#=============================================================================
# 1. Iris Dataset
#=============================================================================

class IrisDataset(Dataset):
    """
    WHAT: Iris flower classification
    WHY:  Classic ML dataset, simple and interpretable
    HOW:  4 measurements (sepal/petal length/width) → 3 species
    """

    def __init__(self):
        super().__init__()
        self.name = "Iris Flowers"
        self.description = "Classify iris flowers by sepal and petal measurements"
        self.num_inputs = 4
        self.num_outputs = 3
        self.classes = ['setosa', 'versicolor', 'virginica']
        self.feature_names = ['Sepal Length', 'Sepal Width', 'Petal Length', 'Petal Width']
        self.input_type = "numerical"

        # Sample data for each class
        self.data = {
            'setosa': [
                [5.1, 3.5, 1.4, 0.2], [4.9, 3.0, 1.4, 0.2], [4.7, 3.2, 1.3, 0.2],
                [4.6, 3.1, 1.5, 0.2], [5.0, 3.6, 1.4, 0.2], [5.4, 3.9, 1.7, 0.4],
                [4.6, 3.4, 1.4, 0.3], [5.0, 3.4, 1.5, 0.2], [4.4, 2.9, 1.4, 0.2],
                [4.9, 3.1, 1.5, 0.1], [5.4, 3.7, 1.5, 0.2], [4.8, 3.4, 1.6, 0.2],
            ],
            'versicolor': [
                [7.0, 3.2, 4.7, 1.4], [6.4, 3.2, 4.5, 1.5], [6.9, 3.1, 4.9, 1.5],
                [5.5, 2.3, 4.0, 1.3], [6.5, 2.8, 4.6, 1.5], [5.7, 2.8, 4.5, 1.3],
                [6.3, 3.3, 4.7, 1.6], [4.9, 2.4, 3.3, 1.0], [6.6, 2.9, 4.6, 1.3],
                [5.2, 2.7, 3.9, 1.4], [5.0, 2.0, 3.5, 1.0], [5.9, 3.0, 4.2, 1.5],
            ],
            'virginica': [
                [6.3, 3.3, 6.0, 2.5], [5.8, 2.7, 5.1, 1.9], [7.1, 3.0, 5.9, 2.1],
                [6.3, 2.9, 5.6, 1.8], [6.5, 3.0, 5.8, 2.2], [7.6, 3.0, 6.6, 2.1],
                [4.9, 2.5, 4.5, 1.7], [7.3, 2.9, 6.3, 1.8], [6.7, 2.5, 5.8, 1.8],
                [7.2, 3.6, 6.1, 2.5], [6.5, 3.2, 5.1, 2.0], [6.4, 2.7, 5.3, 1.9],
            ]
        }

    def normalize_features(self, features: List[float]) -> List[float]:
        """Normalize to 0-1 range based on typical Iris ranges"""
        ranges = [(4.0, 8.0), (2.0, 5.0), (1.0, 7.0), (0.0, 3.0)]
        return [(features[i] - ranges[i][0]) / (ranges[i][1] - ranges[i][0])
                for i in range(len(features))]

    def get_examples(self, count: int = 10) -> List[Tuple[List[float], str]]:
        examples = []
        per_class = count // 3 + 1
        for class_name, class_data in self.data.items():
            for features in class_data[:per_class]:
                examples.append((self.normalize_features(features), class_name))
            if len(examples) >= count:
                break
        return examples[:count]

    def get_test_examples(self, count: int = 5) -> List[Tuple[List[float], str]]:
        examples = []
        for class_name, class_data in self.data.items():
            if class_data:
                examples.append((self.normalize_features(class_data[0]), class_name))
        return examples[:count]

#=============================================================================
# 2. MNIST Dataset
#=============================================================================

class MNISTDataset(Dataset):
    """
    WHAT: Handwritten digit recognition
    WHY:  Standard computer vision benchmark
    HOW:  28×28 pixel images (784 features) → 10 digits (0-9)
    """

    def __init__(self, data_dir: str = "/tmp/mnist"):
        super().__init__()
        self.name = "MNIST Digits"
        self.description = "Recognize handwritten digits from 28×28 pixel images"
        self.num_inputs = 784  # 28×28 pixels
        self.num_outputs = 10  # Digits 0-9
        self.classes = [str(i) for i in range(10)]
        self.feature_names = [f"Pixel_{i}" for i in range(784)]
        self.input_type = "image"

        self.loader = MNISTLoader(data_dir)
        self.images = None
        self.labels = None
        self.loaded = False

    def load(self):
        """Load MNIST dataset (download if needed)"""
        if not self.loaded:
            try:
                self.loader.download()
                self.images, self.labels = self.loader.load_train()
                # Normalize to 0-1
                self.images = self.images / 255.0
                self.loaded = True
            except Exception as e:
                print(f"Warning: Could not load MNIST dataset: {e}")
                # Create simple synthetic digit-like patterns
                self._create_synthetic_digits()

    def _create_synthetic_digits(self):
        """Create simple synthetic patterns when MNIST unavailable"""
        self.images = []
        self.labels = []
        for digit in range(10):
            # Create 10 synthetic examples per digit
            for i in range(10):
                img = np.zeros((28, 28))
                # Simple pattern based on digit
                if digit == 0:  # Circle
                    for y in range(28):
                        for x in range(28):
                            if 8 < ((x-14)**2 + (y-14)**2)**0.5 < 12:
                                img[y, x] = 1.0
                elif digit == 1:  # Vertical line
                    img[:, 12:16] = 1.0
                elif digit == 2:  # Two horizontal lines
                    img[8:12, :] = 1.0
                    img[16:20, :] = 1.0
                # Add more patterns as needed...

                self.images.append(img.flatten())
                self.labels.append(digit)

        self.images = np.array(self.images)
        self.labels = np.array(self.labels)
        self.loaded = True

    def get_examples(self, count: int = 10) -> List[Tuple[List[float], str]]:
        if not self.loaded:
            self.load()
        examples = []
        for i in range(min(count, len(self.images))):
            examples.append((self.images[i].tolist(), str(self.labels[i])))
        return examples

    def get_test_examples(self, count: int = 5) -> List[Tuple[List[float], str]]:
        if not self.loaded:
            self.load()
        examples = []
        offset = len(self.images) // 2  # Use second half for testing
        for i in range(min(count, len(self.images) - offset)):
            idx = offset + i
            examples.append((self.images[idx].tolist(), str(self.labels[idx])))
        return examples

#=============================================================================
# 3. Titanic Dataset
#=============================================================================

class TitanicDataset(Dataset):
    """
    WHAT: Titanic survival prediction
    WHY:  Classic binary classification with mixed feature types
    HOW:  Passenger info (age, class, fare, etc.) → survived yes/no
    """

    def __init__(self):
        super().__init__()
        self.name = "Titanic Survival"
        self.description = "Predict passenger survival based on demographics and ticket info"
        self.num_inputs = 8
        self.num_outputs = 2
        self.classes = ['died', 'survived']
        self.feature_names = ['Pclass', 'Sex', 'Age', 'SibSp', 'Parch', 'Fare', 'Embarked_C', 'Embarked_Q']
        self.input_type = "numerical"

        # Sample Titanic data: [pclass, sex, age, sibsp, parch, fare, embarked_c, embarked_q]
        # sex: 0=male, 1=female; embarked: one-hot encoded
        self.data = {
            'died': [
                [3, 0, 22, 1, 0, 7.25, 0, 0],    # 3rd class male, died
                [3, 0, 25, 0, 0, 8.05, 0, 0],    # 3rd class male, died
                [3, 0, 28, 0, 0, 7.90, 0, 1],    # 3rd class male, died
                [2, 0, 54, 0, 0, 14.00, 0, 0],   # 2nd class male, died
                [3, 0, 21, 0, 0, 7.75, 0, 1],    # 3rd class male, died
                [2, 0, 30, 0, 0, 13.00, 0, 0],   # 2nd class male, died
                [1, 0, 45, 0, 0, 26.55, 0, 0],   # 1st class male, died
                [3, 0, 32, 0, 0, 8.05, 0, 0],    # 3rd class male, died
            ],
            'survived': [
                [1, 1, 38, 1, 0, 71.28, 1, 0],   # 1st class female, survived
                [1, 1, 35, 1, 0, 53.10, 0, 0],   # 1st class female, survived
                [2, 1, 27, 0, 0, 10.50, 0, 0],   # 2nd class female, survived
                [1, 1, 29, 0, 0, 30.00, 1, 0],   # 1st class female, survived
                [2, 1, 24, 1, 0, 24.00, 0, 0],   # 2nd class female, survived
                [1, 0, 40, 1, 1, 39.60, 1, 0],   # 1st class male child, survived
                [2, 1, 28, 0, 0, 13.00, 0, 0],   # 2nd class female, survived
                [1, 1, 31, 1, 0, 50.49, 0, 0],   # 1st class female, survived
            ]
        }

    def normalize_features(self, features: List[float]) -> List[float]:
        """Normalize Titanic features to 0-1 range"""
        # Ranges: pclass(1-3), sex(0-1), age(0-80), sibsp(0-8), parch(0-6), fare(0-512), embarked_c(0-1), embarked_q(0-1)
        ranges = [(1, 3), (0, 1), (0, 80), (0, 8), (0, 6), (0, 512), (0, 1), (0, 1)]
        normalized = []
        for i, val in enumerate(features):
            min_val, max_val = ranges[i]
            normalized.append((val - min_val) / (max_val - min_val) if max_val > min_val else val)
        return normalized

    def get_examples(self, count: int = 10) -> List[Tuple[List[float], str]]:
        examples = []
        per_class = count // 2 + 1
        for class_name, class_data in self.data.items():
            for features in class_data[:per_class]:
                examples.append((self.normalize_features(features), class_name))
            if len(examples) >= count:
                break
        return examples[:count]

    def get_test_examples(self, count: int = 5) -> List[Tuple[List[float], str]]:
        examples = []
        for class_name, class_data in self.data.items():
            if class_data:
                examples.append((self.normalize_features(class_data[0]), class_name))
        return examples[:min(count, 2)]

#=============================================================================
# 4. Visual Pattern Recognition
#=============================================================================

class VisualPatternDataset(Dataset):
    """
    WHAT: Simple geometric pattern recognition
    WHY:  Demonstrate visual pattern learning without full images
    HOW:  8×8 pixel grids → 4 patterns (horizontal, vertical, diagonal, circle)
    """

    def __init__(self):
        super().__init__()
        self.name = "Visual Patterns"
        self.description = "Recognize simple geometric patterns in 8×8 grids"
        self.num_inputs = 64  # 8×8 grid
        self.num_outputs = 4
        self.classes = ['horizontal', 'vertical', 'diagonal', 'circle']
        self.feature_names = [f"Pixel_{i}" for i in range(64)]
        self.input_type = "image"

    def _create_pattern(self, pattern_type: str, variation: int = 0) -> List[float]:
        """Generate pattern with slight variations"""
        grid = np.zeros((8, 8))
        noise = np.random.random((8, 8)) * 0.1 * variation

        if pattern_type == 'horizontal':
            grid[3:5, :] = 1.0
        elif pattern_type == 'vertical':
            grid[:, 3:5] = 1.0
        elif pattern_type == 'diagonal':
            for i in range(8):
                grid[i, i] = 1.0
                if i < 7:
                    grid[i+1, i] = 0.7
        elif pattern_type == 'circle':
            for y in range(8):
                for x in range(8):
                    if 2 < ((x-3.5)**2 + (y-3.5)**2)**0.5 < 3.5:
                        grid[y, x] = 1.0

        return (grid + noise).flatten().clip(0, 1).tolist()

    def get_examples(self, count: int = 10) -> List[Tuple[List[float], str]]:
        examples = []
        per_class = count // 4 + 1
        for pattern in self.classes:
            for i in range(per_class):
                examples.append((self._create_pattern(pattern, i), pattern))
            if len(examples) >= count:
                break
        return examples[:count]

    def get_test_examples(self, count: int = 5) -> List[Tuple[List[float], str]]:
        examples = []
        for pattern in self.classes:
            examples.append((self._create_pattern(pattern, 5), pattern))
        return examples[:count]

#=============================================================================
# 5. XOR and Logic Gates
#=============================================================================

class LogicGatesDataset(Dataset):
    """
    WHAT: Classic XOR and logic gate learning
    WHY:  Prove capability for non-linearly separable problems
    HOW:  2 binary inputs → 1 binary output for various logic operations
    """

    def __init__(self):
        super().__init__()
        self.name = "Logic Gates (XOR)"
        self.description = "Learn XOR and other logic gates (classic non-linear test)"
        self.num_inputs = 2
        self.num_outputs = 2
        self.classes = ['false', 'true']
        self.feature_names = ['Input A', 'Input B']
        self.input_type = "numerical"

        # XOR truth table
        self.xor_data = [
            ([0.0, 0.0], 'false'),  # 0 XOR 0 = 0
            ([0.0, 1.0], 'true'),   # 0 XOR 1 = 1
            ([1.0, 0.0], 'true'),   # 1 XOR 0 = 1
            ([1.0, 1.0], 'false'),  # 1 XOR 1 = 0
        ]

        # AND gate
        self.and_data = [
            ([0.0, 0.0], 'false'),
            ([0.0, 1.0], 'false'),
            ([1.0, 0.0], 'false'),
            ([1.0, 1.0], 'true'),
        ]

        # OR gate
        self.or_data = [
            ([0.0, 0.0], 'false'),
            ([0.0, 1.0], 'true'),
            ([1.0, 0.0], 'true'),
            ([1.0, 1.0], 'true'),
        ]

    def get_examples(self, count: int = 10) -> List[Tuple[List[float], str]]:
        # Repeat XOR examples to fill count
        examples = []
        while len(examples) < count:
            examples.extend(self.xor_data)
        return examples[:count]

    def get_test_examples(self, count: int = 4) -> List[Tuple[List[float], str]]:
        return self.xor_data[:count]

#=============================================================================
# 6. Sine Wave Prediction
#=============================================================================

class SineWaveDataset(Dataset):
    """
    WHAT: Time series sine wave prediction
    WHY:  Demonstrate temporal pattern learning
    HOW:  20 time steps of sine wave → predict next value
    """

    def __init__(self):
        super().__init__()
        self.name = "Sine Wave Prediction"
        self.description = "Predict next value in sine wave time series"
        self.num_inputs = 20
        self.num_outputs = 4  # Discretize into 4 ranges
        self.classes = ['low', 'mid-low', 'mid-high', 'high']
        self.feature_names = [f"t-{20-i}" for i in range(20)]
        self.input_type = "time_series"

    def _discretize_value(self, value: float) -> str:
        """Convert sine value (-1 to 1) to class"""
        if value < -0.5:
            return 'low'
        elif value < 0:
            return 'mid-low'
        elif value < 0.5:
            return 'mid-high'
        else:
            return 'high'

    def _generate_sequence(self, start_phase: float, frequency: float = 1.0) -> Tuple[List[float], str]:
        """Generate sine wave sequence"""
        sequence = []
        for i in range(21):
            t = start_phase + i * 0.1 * frequency
            sequence.append(math.sin(t))

        # Normalize to 0-1
        inputs = [(val + 1) / 2 for val in sequence[:20]]
        target = self._discretize_value(sequence[20])

        return (inputs, target)

    def get_examples(self, count: int = 10) -> List[Tuple[List[float], str]]:
        examples = []
        for i in range(count):
            phase = i * 0.5
            examples.append(self._generate_sequence(phase))
        return examples

    def get_test_examples(self, count: int = 5) -> List[Tuple[List[float], str]]:
        examples = []
        for i in range(count):
            phase = 100 + i * 0.3  # Different phase for testing
            examples.append(self._generate_sequence(phase))
        return examples

#=============================================================================
# Dataset Registry
#=============================================================================

DATASETS = {
    'iris': IrisDataset,
    'mnist': MNISTDataset,
    'titanic': TitanicDataset,
    'patterns': VisualPatternDataset,
    'xor': LogicGatesDataset,
    'sinewave': SineWaveDataset
}

def get_dataset(dataset_name: str) -> Dataset:
    """Get dataset instance by name"""
    if dataset_name not in DATASETS:
        raise ValueError(f"Unknown dataset: {dataset_name}. Available: {list(DATASETS.keys())}")
    return DATASETS[dataset_name]()

def list_datasets() -> List[Dict]:
    """List all available datasets with metadata"""
    datasets_info = []
    for name, dataset_class in DATASETS.items():
        ds = dataset_class()
        datasets_info.append({
            'id': name,
            'name': ds.name,
            'description': ds.description,
            'num_inputs': ds.num_inputs,
            'num_outputs': ds.num_outputs,
            'classes': ds.classes,
            'input_type': ds.input_type
        })
    return datasets_info
