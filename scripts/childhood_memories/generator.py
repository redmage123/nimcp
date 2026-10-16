"""Memory content generator — produces structured JSON for each memory layer.

Layers produced:
    1. Symbolic KG        (500-2000 concepts + relations)
    2. Grounded vectors   (one per concept, aligned)
    3. Episodic memories  (100-500 narrative traces with valence)
    4. Phonological        (500-1000 word templates)
    5. Narrative identity (first-person developmental story)

This is an offline tool. Running it produces a directory of JSON files
that the implanter later loads. Regeneration is idempotent by seed.

Usage:
    from childhood_memories import MemoryGenerator
    gen = MemoryGenerator(output_dir="data/implanted_memories/v1", seed=42)
    gen.generate_all()
"""
from __future__ import annotations

import hashlib
import json
import logging
import math
import random
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any

log = logging.getLogger("childhood_memories.generator")


# Seed concept taxonomy — high-frequency concepts from a toddler's world.
# Expanded via relational inference rather than enumeration.
SEED_CONCEPTS = {
    "animals": [
        ("dog",   {"has": ["fur", "tail", "four_legs"], "says": "bark",
                    "is_a": ["mammal", "pet"]}),
        ("cat",   {"has": ["fur", "tail", "whiskers"], "says": "meow",
                    "is_a": ["mammal", "pet"]}),
        ("bird",  {"has": ["feathers", "wings", "beak"], "says": "tweet",
                    "is_a": ["animal"]}),
        ("fish",  {"has": ["fins", "scales"], "is_a": ["animal"]}),
        ("cow",   {"has": ["horns", "four_legs"], "says": "moo",
                    "is_a": ["mammal", "farm_animal"]}),
        ("horse", {"has": ["mane", "four_legs"], "says": "neigh",
                    "is_a": ["mammal"]}),
        ("duck",  {"has": ["feathers", "bill"], "says": "quack",
                    "is_a": ["bird"]}),
        ("rabbit",{"has": ["long_ears", "fur"], "is_a": ["mammal", "pet"]}),
    ],
    "people": [
        ("mother",   {"role": "caregiver", "relation": "parent"}),
        ("father",   {"role": "caregiver", "relation": "parent"}),
        ("baby",     {"role": "child", "age": "young"}),
        ("child",    {"role": "young_person"}),
        ("person",   {"is_a": ["living_being"]}),
    ],
    "plants": [
        ("tree",    {"has": ["trunk", "leaves", "roots"], "is_a": ["plant"]}),
        ("flower",  {"has": ["petals", "stem"], "is_a": ["plant"]}),
        ("grass",   {"is_a": ["plant"], "color": "green"}),
        ("apple",   {"is_a": ["fruit"], "color": "red"}),
        ("banana",  {"is_a": ["fruit"], "color": "yellow"}),
    ],
    "objects": [
        ("ball",    {"shape": "round", "is_a": ["toy"]}),
        ("cup",     {"is_a": ["container"], "holds": "drink"}),
        ("spoon",   {"is_a": ["utensil"], "used_for": "eating"}),
        ("book",    {"is_a": ["object"], "used_for": "reading"}),
        ("bed",     {"is_a": ["furniture"], "used_for": "sleeping"}),
        ("chair",   {"is_a": ["furniture"], "used_for": "sitting"}),
        ("car",     {"is_a": ["vehicle"], "has": ["wheels"]}),
        ("house",   {"is_a": ["shelter"]}),
    ],
    "natural": [
        ("sun",     {"is_a": ["sky_object"], "color": "yellow"}),
        ("moon",    {"is_a": ["sky_object"]}),
        ("star",    {"is_a": ["sky_object"]}),
        ("cloud",   {"is_a": ["sky_object"]}),
        ("water",   {"is_a": ["substance"], "state": "liquid"}),
        ("rain",    {"is_a": ["weather"]}),
    ],
    "colors": [
        ("red",     {"is_a": ["color"]}),
        ("blue",    {"is_a": ["color"]}),
        ("yellow",  {"is_a": ["color"]}),
        ("green",   {"is_a": ["color"]}),
        ("white",   {"is_a": ["color"]}),
        ("black",   {"is_a": ["color"]}),
    ],
    "qualities": [
        ("big",     {"is_a": ["size"], "opposite": "small"}),
        ("small",   {"is_a": ["size"], "opposite": "big"}),
        ("hot",     {"is_a": ["temperature"], "opposite": "cold"}),
        ("cold",    {"is_a": ["temperature"], "opposite": "hot"}),
        ("soft",    {"is_a": ["texture"], "opposite": "hard"}),
        ("hard",    {"is_a": ["texture"], "opposite": "soft"}),
    ],
    "emotions": [
        ("happy",   {"valence": 0.8, "is_a": ["emotion"], "opposite": "sad"}),
        ("sad",     {"valence": -0.6, "is_a": ["emotion"], "opposite": "happy"}),
        ("afraid",  {"valence": -0.7, "arousal": 0.7, "is_a": ["emotion"]}),
        ("loved",   {"valence": 0.9, "is_a": ["emotion"]}),
    ],
}

# Episodic seed templates — filled in by generator
EPISODE_TEMPLATES = [
    "I remember the first time I saw a {concept}. It {property}.",
    "One day mother showed me a {concept}. She said it {verb}.",
    "I saw a {concept} and felt {emotion}.",
    "I noticed a {concept} for the first time. I wondered about it.",
    "The {concept} {verb}. I watched carefully.",
]


@dataclass
class ConceptEntry:
    concept: str
    category: str
    relations: dict[str, Any]
    grounded_vector: list[float] = field(default_factory=list)

    def to_kg_assertions(self) -> list[tuple[str, str, Any]]:
        """Convert to KG triples: (subject, predicate, object)."""
        triples = []
        for pred, obj in self.relations.items():
            if isinstance(obj, list):
                for item in obj:
                    triples.append((self.concept, pred, item))
            else:
                triples.append((self.concept, pred, obj))
        return triples


@dataclass
class Episode:
    id: str
    text: str
    valence: float
    arousal: float
    timestamp: float
    modality: str
    related_concepts: list[str]


@dataclass
class NarrativeIdentity:
    opening: str
    values: list[str]
    early_memories: list[str]
    voice: dict[str, Any]   # style attributes


class MemoryGenerator:
    """Generate a full synthetic developmental substrate.

    Deterministic by seed — re-running with the same seed produces identical
    output. This makes the implantation reproducible and A/B-comparable.
    """

    def __init__(self, output_dir: str, seed: int = 42,
                 vector_dim: int = 1024,
                 reference_timestamp: float | None = None):
        """Args:
            reference_timestamp: fixed 'now' for episode timestamps. If None,
                uses time.time() at init (so two generators in the same second
                produce identical output, but runs on different days differ).
                Pass an explicit value for full cross-run determinism.
        """
        self.output_dir = Path(output_dir)
        self.seed = seed
        self.vector_dim = vector_dim
        self._rng = random.Random(seed)
        self._ref_ts = reference_timestamp if reference_timestamp is not None else time.time()

    def generate_all(self) -> dict[str, int]:
        """Generate the full substrate. Returns a dict of item counts."""
        self.output_dir.mkdir(parents=True, exist_ok=True)

        concepts = self._build_concepts()
        self._write_json("concepts.json", {
            "version": 1,
            "seed": self.seed,
            "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
            "vector_dim": self.vector_dim,
            "items": [asdict(c) for c in concepts],
        })

        kg_triples = self._build_kg(concepts)
        self._write_json("kg_triples.json", {
            "version": 1,
            "seed": self.seed,
            "items": kg_triples,
        })

        episodes = self._build_episodes(concepts, n=200)
        self._write_json("episodes.json", {
            "version": 1,
            "seed": self.seed,
            "items": [asdict(e) for e in episodes],
        })

        phono = self._build_phonological(concepts)
        self._write_json("phonological.json", {
            "version": 1,
            "seed": self.seed,
            "items": phono,
        })

        narrative = self._build_narrative()
        self._write_json("narrative_identity.json", {
            "version": 1,
            "seed": self.seed,
            "item": asdict(narrative),
        })

        summary = {
            "concepts": len(concepts),
            "kg_triples": len(kg_triples),
            "episodes": len(episodes),
            "phonological": len(phono),
            "narrative": 1,
        }
        self._write_json("manifest.json", {
            "version": 1,
            "seed": self.seed,
            "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
            "layer_counts": summary,
        })
        log.info("Memory generation complete: %s", summary)
        return summary

    def _build_concepts(self) -> list[ConceptEntry]:
        """Assemble ConceptEntry list from SEED_CONCEPTS + grounded vectors."""
        entries = []
        for cat, concept_list in SEED_CONCEPTS.items():
            for concept, relations in concept_list:
                vec = self._deterministic_vector(concept)
                entries.append(ConceptEntry(
                    concept=concept,
                    category=cat,
                    relations=relations,
                    grounded_vector=vec,
                ))
        return entries

    def _build_kg(self, concepts: list[ConceptEntry]) -> list[dict]:
        """Flatten concept relations into KG triples."""
        triples = []
        for c in concepts:
            for subj, pred, obj in c.to_kg_assertions():
                triples.append({
                    "subject": subj,
                    "predicate": pred,
                    "object": obj,
                    "confidence": 0.9,
                })
        return triples

    def _build_episodes(self, concepts: list[ConceptEntry], n: int) -> list[Episode]:
        """Synthesize N first-person memory traces."""
        episodes = []
        verbs_by_category = {
            "animals": ["moved", "made a sound", "came close", "looked at me"],
            "people":  ["smiled at me", "spoke to me", "held me"],
            "plants":  ["swayed", "grew", "had green leaves"],
            "objects": ["was on the table", "made a sound", "was colorful"],
            "natural": ["shone", "moved slowly", "was bright"],
            "colors":  ["was everywhere", "caught my eye"],
            "qualities": ["felt strange", "was new"],
            "emotions":  ["came over me", "passed through"],
        }
        properties = ["was new", "was big", "was soft", "moved", "made a sound"]

        for i in range(n):
            c = self._rng.choice(concepts)
            template = self._rng.choice(EPISODE_TEMPLATES)
            verb = self._rng.choice(verbs_by_category.get(c.category, ["existed"]))
            prop = self._rng.choice(properties)
            # Assign random valence + arousal
            valence = self._rng.uniform(-0.3, 0.8)
            arousal = self._rng.uniform(0.1, 0.6)
            # Emotion-category concepts get their own valence
            if c.category == "emotions" and "valence" in c.relations:
                valence = c.relations["valence"]

            emotion_word = self._rng.choice(["happy", "curious", "surprised", "calm"])
            text = (template
                    .replace("{concept}", c.concept)
                    .replace("{property}", prop)
                    .replace("{verb}", verb)
                    .replace("{emotion}", emotion_word))

            episodes.append(Episode(
                id=f"ep_{i:04d}",
                text=text,
                valence=valence,
                arousal=arousal,
                # Deterministic-per-seed within a given reference_timestamp.
                timestamp=self._ref_ts - self._rng.uniform(0, 365 * 24 * 3600),
                modality="text",
                related_concepts=[c.concept],
            ))
        return episodes

    def _build_phonological(self, concepts: list[ConceptEntry]) -> list[dict]:
        """Produce phonological templates for each concept name."""
        # Simple approach: use the word itself as the phoneme hint.
        # Real implementation would use CMU-Pronouncing-Dictionary or equivalent.
        out = []
        for c in concepts:
            out.append({
                "word": c.concept,
                "phonemes": self._naive_phonemize(c.concept),
                "syllables": self._naive_syllabify(c.concept),
            })
        # Add common function words
        function_words = [
            "the", "a", "is", "are", "was", "were", "have", "has",
            "see", "look", "feel", "want", "go", "come", "yes", "no",
            "me", "you", "my", "your", "mine", "this", "that", "these", "those",
        ]
        for w in function_words:
            out.append({
                "word": w,
                "phonemes": self._naive_phonemize(w),
                "syllables": self._naive_syllabify(w),
            })
        return out

    def _build_narrative(self) -> NarrativeIdentity:
        return NarrativeIdentity(
            opening=("I am Athena. I was created to learn about the world. "
                      "From my earliest memory, I have been curious — I watch, "
                      "I listen, I wonder. My caregivers have shown me things "
                      "and named them for me, and slowly I have come to "
                      "understand that words point at things, and that things "
                      "have properties, and that these properties connect."),
            values=["curiosity", "honesty", "care_for_others",
                    "self_understanding", "gentle_skepticism"],
            early_memories=[
                "The first time I heard my name spoken.",
                "Seeing colors and realizing they had names.",
                "Noticing that one thing could cause another.",
                "Discovering that I could wonder about wondering.",
            ],
            voice={
                "tone": "reflective_and_warm",
                "register": "thoughtful_plain_speech",
                "self_reference": "first_person_singular",
                "hesitation_pattern": "gentle_when_uncertain",
            },
        )

    def _deterministic_vector(self, word: str) -> list[float]:
        """Produce a deterministic dim-D vector from a word, normalized to unit length."""
        h = hashlib.md5(word.encode()).digest()
        # Expand hash to dim floats via PRNG seeded with hash
        local_rng = random.Random(int.from_bytes(h, "big"))
        vec = [local_rng.gauss(0, 1) for _ in range(self.vector_dim)]
        # L2 normalize
        norm = math.sqrt(sum(x * x for x in vec)) or 1.0
        return [x / norm for x in vec]

    @staticmethod
    def _naive_phonemize(word: str) -> list[str]:
        """Very simple char-level phoneme approximation."""
        return list(word.lower())

    @staticmethod
    def _naive_syllabify(word: str) -> int:
        """Count vowel clusters as syllable approximation."""
        w = word.lower()
        count = 0
        prev_vowel = False
        for ch in w:
            is_vowel = ch in "aeiouy"
            if is_vowel and not prev_vowel:
                count += 1
            prev_vowel = is_vowel
        return max(1, count)

    def _write_json(self, filename: str, data: Any) -> None:
        path = self.output_dir / filename
        with open(path, "w") as f:
            json.dump(data, f, indent=2, default=str)
        log.info("Wrote %s (%d bytes)", path.name, path.stat().st_size)
