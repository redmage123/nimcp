"""Real sensory stimuli for the developmental curriculum.

Replaces the keyword-seeded synthetic visual frames (immerse_athena's
generate_visual_frame) with real photographs when the description names an
object we have images for, and the synthesized speech channel with real
spoken-word recordings when the description contains a word we have clips
for. Everything else falls back to the synthetic generators, so coverage can
grow without changing the trainer.

Data (built by scripts/build_sensory_caches.py from the original archives):
    <data_dir>/cifar_by_label.pkl          {label: [32x32x3 uint8 bytes, ...]}  train split
    <data_dir>/cifar_heldout_by_label.pkl  same layout, test split (evaluation only)
    <data_dir>/speech_cmds_by_label.pkl    {word: [float32 16 kHz samples, ...]}

Sources and licenses: CIFAR-10/100 (Krizhevsky, Toronto; freely redistributed
for research and widely used commercially), Google Speech Commands v0.02
(CC BY 4.0). No non-commercial-only data is used here.

Biological rationale: a child never sees the same canonical rendering twice.
Every presentation is re-augmented (flip, shift, brightness/contrast, colour
jitter) so the cortex has viewpoint/lighting variation to become invariant
to, and views_per_object lets the trainer present successive augmented views
of one object on consecutive steps (temporal continuity).
"""
from __future__ import annotations

import os
import pickle
import random
import re
from typing import Optional

import numpy as np

# ---------------------------------------------------------------------------
# Vocabulary → image label mapping.
# CIFAR-100 fine labels + CIFAR-10 labels (airplane, automobile, bird, cat,
# deer, dog, frog, horse, ship, truck). Values are candidate labels; a random
# candidate is chosen per presentation so "tree" shows all five tree classes.
# ---------------------------------------------------------------------------
SYNONYMS: dict[str, list[str]] = {
    # people
    "mother": ["woman"], "mom": ["woman"], "mommy": ["woman"], "mum": ["woman"],
    "woman": ["woman"], "lady": ["woman"], "girl": ["girl"], "daughter": ["girl"],
    "father": ["man"], "dad": ["man"], "daddy": ["man"], "man": ["man"],
    "boy": ["boy"], "son": ["boy"], "child": ["boy", "girl"], "kid": ["boy", "girl"],
    "children": ["boy", "girl"], "baby": ["baby"], "infant": ["baby"], "toddler": ["baby"],
    "people": ["man", "woman", "boy", "girl"], "person": ["man", "woman"],
    # animals
    "dog": ["dog"], "puppy": ["dog"], "pup": ["dog"], "hound": ["dog"],
    "cat": ["cat"], "kitten": ["cat"], "kitty": ["cat"],
    "bird": ["bird"], "sparrow": ["bird"], "robin": ["bird"],
    "fish": ["aquarium_fish", "trout", "flatfish"], "trout": ["trout"],
    "horse": ["horse"], "pony": ["horse"], "foal": ["horse"],
    "butterfly": ["butterfly"], "rabbit": ["rabbit"], "bunny": ["rabbit"],
    "bear": ["bear"], "dolphin": ["dolphin"], "elephant": ["elephant"],
    "bee": ["bee"], "beetle": ["beetle"], "bug": ["beetle", "cockroach"],
    "insect": ["bee", "beetle", "butterfly"], "spider": ["spider"], "snail": ["snail"],
    "worm": ["worm"], "caterpillar": ["caterpillar"], "cockroach": ["cockroach"],
    "deer": ["deer"], "frog": ["frog"], "lizard": ["lizard"], "snake": ["snake"],
    "turtle": ["turtle"], "crocodile": ["crocodile"], "dinosaur": ["dinosaur"],
    "whale": ["whale"], "shark": ["shark"], "seal": ["seal"], "otter": ["otter"],
    "crab": ["crab"], "lobster": ["lobster"], "ray": ["ray"],
    "lion": ["lion"], "tiger": ["tiger"], "leopard": ["leopard"], "wolf": ["wolf"],
    "fox": ["fox"], "raccoon": ["raccoon"], "skunk": ["skunk"], "squirrel": ["squirrel"],
    "mouse": ["mouse"], "hamster": ["hamster"], "shrew": ["shrew"], "possum": ["possum"],
    "porcupine": ["porcupine"], "beaver": ["beaver"], "kangaroo": ["kangaroo"],
    "camel": ["camel"], "cow": ["cattle"], "cattle": ["cattle"], "bull": ["cattle"],
    "monkey": ["chimpanzee"], "ape": ["chimpanzee"], "chimpanzee": ["chimpanzee"],
    "owl": ["bird"], "duck": ["bird"], "chicken": ["bird"], "eagle": ["bird"],
    # plants / nature
    "tree": ["oak_tree", "maple_tree", "pine_tree", "palm_tree", "willow_tree"],
    "oak": ["oak_tree"], "maple": ["maple_tree"], "pine": ["pine_tree"], "palm": ["palm_tree"],
    "willow": ["willow_tree"], "forest": ["forest"], "woods": ["forest"], "jungle": ["forest"],
    "flower": ["rose", "tulip", "orchid", "poppy", "sunflower"], "rose": ["rose"],
    "tulip": ["tulip"], "orchid": ["orchid"], "poppy": ["poppy"], "sunflower": ["sunflower"],
    "mushroom": ["mushroom"], "grass": ["plain"], "field": ["plain"], "meadow": ["plain"],
    "plain": ["plain"], "mountain": ["mountain"], "hill": ["mountain"],
    "cloud": ["cloud"], "sky": ["cloud"], "sea": ["sea"], "ocean": ["sea"],
    "wave": ["sea"], "beach": ["sea"], "water": ["sea"], "lake": ["sea"],
    "apple": ["apple"], "orange": ["orange"], "pear": ["pear"], "pepper": ["sweet_pepper"],
    # objects / places
    "house": ["house"], "home": ["house"], "cottage": ["house"], "castle": ["castle"],
    "building": ["skyscraper"], "skyscraper": ["skyscraper"], "tower": ["skyscraper"],
    "bridge": ["bridge"], "road": ["road"], "street": ["road"], "path": ["road"],
    "car": ["automobile"], "automobile": ["automobile"], "truck": ["truck", "pickup_truck"],
    "bus": ["bus"], "train": ["train"], "tram": ["streetcar"], "streetcar": ["streetcar"],
    "tractor": ["tractor"], "motorcycle": ["motorcycle"], "bike": ["bicycle"],
    "bicycle": ["bicycle"], "boat": ["ship"], "ship": ["ship"], "rocket": ["rocket"],
    "plane": ["airplane"], "airplane": ["airplane"], "aeroplane": ["airplane"], "tank": ["tank"],
    "mower": ["lawn_mower"], "bed": ["bed"], "chair": ["chair"], "couch": ["couch"],
    "sofa": ["couch"], "table": ["table"], "wardrobe": ["wardrobe"], "closet": ["wardrobe"],
    "lamp": ["lamp"], "light": ["lamp"], "clock": ["clock"], "telephone": ["telephone"],
    "phone": ["telephone"], "television": ["television"], "tv": ["television"],
    "keyboard": ["keyboard"], "bottle": ["bottle"], "bowl": ["bowl"], "cup": ["cup"],
    "mug": ["cup"], "plate": ["plate"], "can": ["can"],
}

_WORD_RE = re.compile(r"[a-z]+")


def _candidate_words(description: str) -> list[str]:
    """Lowercase tokens plus naive singular forms, in order of appearance."""
    out: list[str] = []
    for w in _WORD_RE.findall(description.lower()):
        out.append(w)
        if len(w) > 3 and w.endswith("ies"):
            out.append(w[:-3] + "y")
        elif len(w) > 3 and w.endswith("es"):
            out.append(w[:-2])
        if len(w) > 3 and w.endswith("s"):
            out.append(w[:-1])
    return out


class RealSensoryBank:
    """Lazy-loading bank of real images and speech clips keyed by label."""

    def __init__(self, data_dir: str, seed: Optional[int] = None,
                 augment: bool = True):
        self.data_dir = data_dir
        self.augment = augment
        self.rng = random.Random(seed)
        self.nrng = np.random.RandomState(seed)
        self._images: Optional[dict[str, list[bytes]]] = None
        self._heldout: Optional[dict[str, list[bytes]]] = None
        self._speech: Optional[dict[str, list[np.ndarray]]] = None
        self.hits = 0
        self.misses = 0
        self.speech_hits = 0
        self.last_label: Optional[str] = None
        self.last_hit = False

    # ---- loading -----------------------------------------------------------
    def _load_pkl(self, name: str):
        path = os.path.join(self.data_dir, name)
        if not os.path.exists(path):
            return None
        with open(path, "rb") as f:
            return pickle.load(f)

    @property
    def images(self) -> dict[str, list[bytes]]:
        if self._images is None:
            self._images = self._load_pkl("cifar_by_label.pkl") or {}
        return self._images

    @property
    def heldout(self) -> dict[str, list[bytes]]:
        if self._heldout is None:
            self._heldout = self._load_pkl("cifar_heldout_by_label.pkl") or {}
        return self._heldout

    @property
    def speech(self) -> dict[str, list[np.ndarray]]:
        if self._speech is None:
            self._speech = self._load_pkl("speech_cmds_by_label.pkl") or {}
        return self._speech

    def available(self) -> bool:
        return bool(self.images)

    # ---- matching ----------------------------------------------------------
    def match_label(self, description: str) -> Optional[str]:
        """First vocabulary word in the description with images; random
        candidate label among its synonyms."""
        imgs = self.images
        if not imgs:
            return None
        for w in _candidate_words(description):
            cands = SYNONYMS.get(w)
            if cands is None and w in imgs:
                cands = [w]
            if not cands:
                continue
            cands = [c for c in cands if c in imgs]
            if cands:
                return self.rng.choice(cands)
        return None

    # ---- augmentation ------------------------------------------------------
    def augment_image(self, raw: bytes, w: int = 32, h: int = 32, ch: int = 3) -> bytes:
        img = np.frombuffer(raw, dtype=np.uint8).reshape(h, w, ch).astype(np.float32)
        r = self.nrng
        if r.rand() < 0.5:
            img = img[:, ::-1, :]
        # random shift up to 4 px with reflect padding (random crop)
        pad = 4
        dy, dx = r.randint(-pad, pad + 1), r.randint(-pad, pad + 1)
        padded = np.pad(img, ((pad, pad), (pad, pad), (0, 0)), mode="reflect")
        img = padded[pad + dy:pad + dy + h, pad + dx:pad + dx + w, :]
        # brightness / contrast
        img = img * r.uniform(0.8, 1.2)
        mean = img.mean()
        img = (img - mean) * r.uniform(0.8, 1.2) + mean
        # per-channel colour jitter
        img = img * r.uniform(0.9, 1.1, size=(1, 1, ch))
        return np.clip(img, 0, 255).astype(np.uint8).tobytes()

    # ---- public API --------------------------------------------------------
    def get_frame(self, description: str):
        """(bytes, w, h, ch, label) for a real, freshly augmented image matching
        the description, or None (caller falls back to the synthetic frame)."""
        label = self.match_label(description)
        self.last_hit = label is not None
        self.last_label = label
        if label is None:
            self.misses += 1
            return None
        raw = self.rng.choice(self.images[label])
        if self.augment:
            raw = self.augment_image(raw)
        self.hits += 1
        return raw, 32, 32, 3, label

    def get_heldout_frame(self, label_or_word: str, index: Optional[int] = None):
        """Un-augmented held-out (test split) image for evaluation, or None."""
        ho = self.heldout
        if not ho:
            return None
        label = label_or_word if label_or_word in ho else None
        if label is None:
            for c in SYNONYMS.get(label_or_word, []):
                if c in ho:
                    label = c
                    break
        if label is None:
            return None
        clips = ho[label]
        raw = clips[index % len(clips)] if index is not None else self.rng.choice(clips)
        return raw, 32, 32, 3, label

    def heldout_labels(self) -> list[str]:
        return sorted(self.heldout.keys())

    def get_speech(self, description: str, n_samples: int = 512):
        """(samples list, word) for a real spoken-word clip whose word appears
        in the description, or None.

        The speech cortex consumes a fixed 512-sample (32 ms at 16 kHz)
        waveform — the same shape the synthetic generator produces — so the
        clip's highest-energy 512-sample window is returned (the loudest
        part of the word, with its real spectral content). Clips are stored
        as float32 bytes so the cache is independent of the numpy version
        that built it."""
        sp = self.speech
        if not sp:
            return None
        for w in _candidate_words(description):
            if w in sp and sp[w]:
                self.speech_hits += 1
                raw = self.rng.choice(sp[w])
                clip = np.frombuffer(raw, dtype=np.float32) if isinstance(raw, (bytes, bytearray)) \
                    else np.asarray(raw, dtype=np.float32)
                if len(clip) <= n_samples:
                    out = np.zeros(n_samples, dtype=np.float32)
                    out[:len(clip)] = clip
                    return out.tolist(), w
                # highest-energy window (hop 64)
                energy = np.convolve(clip * clip, np.ones(n_samples, dtype=np.float32), mode="valid")
                start = int(np.argmax(energy[::64]) * 64)
                return clip[start:start + n_samples].tolist(), w
        return None

    def stats_str(self) -> str:
        tot = self.hits + self.misses
        rate = (self.hits / tot * 100.0) if tot else 0.0
        return (f"real-images: {self.hits}/{tot} matched ({rate:.0f}%), "
                f"{len(self.images)} labels; real-speech clips: {self.speech_hits}")
