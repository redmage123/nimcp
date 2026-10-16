#!/usr/bin/env python3
"""
multimodal_data.py — Multimodal dataset loader for Athena's developmental training.

Downloads and manages real-world image, audio, and speech datasets so Claude
can _show_ (visual), _tell_ (audio), and _say_ (text) to Athena through her
native SNN sensory bridges.

Phase 1 datasets (~950 MB total):
  - CIFAR-100: 60K 32x32 color images, 100 object classes
  - ESC-50:    2,000 environmental audio clips, 50 sound classes
  - Speech Commands mini: 8K one-second spoken word clips, 8 commands

All datasets are lazily downloaded on first use via HuggingFace `datasets`
or direct URL. No API keys required.
"""

import logging
import os
import random
import struct
import tarfile
import pickle
import numpy as np

logger = logging.getLogger("multimodal_data")

DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "..", "data", "multimodal")


# ============================================================================
# CIFAR-100 loader (32x32 images, 100 fine-grained classes)
# ============================================================================

# CIFAR-100 fine labels → human-readable descriptions for teaching
CIFAR100_LABELS = [
    "apple", "aquarium fish", "baby", "bear", "beaver",
    "bed", "bee", "beetle", "bicycle", "bottle",
    "bowl", "boy", "bridge", "bus", "butterfly",
    "camel", "can", "castle", "caterpillar", "cattle",
    "chair", "chimpanzee", "clock", "cloud", "cockroach",
    "couch", "crab", "crocodile", "cup", "dinosaur",
    "dolphin", "elephant", "flatfish", "forest", "fox",
    "girl", "hamster", "house", "kangaroo", "keyboard",
    "lamp", "lawn mower", "leopard", "lion", "lizard",
    "lobster", "man", "maple tree", "motorcycle", "mountain",
    "mouse", "mushroom", "oak tree", "orange", "orchid",
    "otter", "palm tree", "pear", "pickup truck", "pine tree",
    "plain", "plate", "poppy", "porcupine", "possum",
    "rabbit", "raccoon", "ray", "road", "rocket",
    "rose", "sea", "seal", "shark", "shrew",
    "skunk", "skyscraper", "snail", "snake", "spider",
    "squirrel", "streetcar", "sunflower", "sweet pepper", "table",
    "tank", "telephone", "television", "tiger", "tractor",
    "train", "trout", "tulip", "turtle", "wardrobe",
    "whale", "willow tree", "wolf", "woman", "worm",
]

# CIFAR-100 coarse labels (20 superclasses)
CIFAR100_COARSE = [
    "aquatic mammals", "fish", "flowers", "food containers",
    "fruit and vegetables", "household electrical devices",
    "household furniture", "insects", "large carnivores",
    "large man-made outdoor things", "large natural outdoor scenes",
    "large omnivores and herbivores", "medium-sized mammals",
    "non-insect invertebrates", "people", "reptiles",
    "small mammals", "trees", "vehicles 1", "vehicles 2",
]

# Teaching descriptions for each label (Claude uses these as context)
CIFAR100_DESCRIPTIONS = {
    "apple": "A round fruit, often red or green, that grows on trees",
    "bear": "A large furry animal that lives in forests and mountains",
    "bee": "A small flying insect that makes honey and pollinates flowers",
    "beetle": "A small insect with hard wing covers",
    "bicycle": "A two-wheeled vehicle you pedal with your legs",
    "bottle": "A container for holding liquids like water or milk",
    "bridge": "A structure built over water or a road to cross it",
    "bus": "A large vehicle that carries many people on roads",
    "butterfly": "A beautiful insect with colorful wings that flutters",
    "camel": "A large animal with humps that lives in deserts",
    "castle": "A large stone building where kings and queens once lived",
    "caterpillar": "A fuzzy worm-like creature that becomes a butterfly",
    "chair": "A piece of furniture you sit on",
    "clock": "A device that shows what time it is",
    "cloud": "A white fluffy shape in the sky made of tiny water drops",
    "couch": "A long soft seat where you can sit or lie down",
    "crab": "A sea creature with a hard shell and pinching claws",
    "crocodile": "A large reptile with a long snout that lives in rivers",
    "cup": "A small container you drink from",
    "dinosaur": "A very large ancient reptile that lived long ago",
    "dolphin": "A playful sea mammal that leaps through waves",
    "elephant": "The largest land animal with a long trunk and big ears",
    "forest": "A large area covered with many tall trees",
    "fox": "A clever wild animal with red fur and a bushy tail",
    "hamster": "A small furry pet that runs on a wheel",
    "house": "A building where people live with their families",
    "kangaroo": "An Australian animal that hops and carries babies in a pouch",
    "lamp": "A device that makes light so you can see in the dark",
    "leopard": "A wild cat with beautiful spotted fur",
    "lion": "A large wild cat known as the king of the jungle",
    "lizard": "A small reptile with four legs and a long tail",
    "lobster": "A large sea creature with claws, often red when cooked",
    "mountain": "A very tall hill reaching up into the clouds",
    "mouse": "A tiny furry animal with a long thin tail",
    "mushroom": "A fungus that grows from the ground, some you can eat",
    "orange": "A round citrus fruit that is orange in color",
    "orchid": "A beautiful exotic flower with delicate petals",
    "rabbit": "A soft furry animal with long ears that hops",
    "raccoon": "A clever animal with a striped tail and a mask-like face",
    "rocket": "A vehicle that flies into space with powerful engines",
    "rose": "A beautiful flower with soft petals and thorns",
    "sea": "A vast body of salt water stretching to the horizon",
    "seal": "A sea mammal with flippers that swims in cold waters",
    "shark": "A large fish with sharp teeth that swims in the ocean",
    "snail": "A small creature that carries its shell home on its back",
    "snake": "A long reptile with no legs that slithers on the ground",
    "spider": "A small creature with eight legs that spins webs",
    "squirrel": "A small animal with a bushy tail that climbs trees",
    "sunflower": "A tall flower with a big yellow face that follows the sun",
    "table": "A flat piece of furniture where you eat or work",
    "tank": "A heavy armored military vehicle with a big gun",
    "telephone": "A device used to talk to people far away",
    "television": "A screen that shows moving pictures and sounds",
    "tiger": "A large wild cat with orange fur and black stripes",
    "tractor": "A powerful farm vehicle used to pull heavy things",
    "train": "A long vehicle that runs on rails and carries people or cargo",
    "turtle": "A reptile with a hard shell that moves slowly",
    "whale": "The largest animal in the ocean that sings deep songs",
    "wolf": "A wild dog-like animal that howls at the moon and lives in packs",
    "worm": "A small soft creature with no legs that lives in soil",
}

# Fill in any missing descriptions with a generic one
for _label in CIFAR100_LABELS:
    if _label not in CIFAR100_DESCRIPTIONS:
        CIFAR100_DESCRIPTIONS[_label] = f"A {_label}"


# ESC-50 class labels → descriptions
ESC50_LABELS = [
    "dog", "rooster", "pig", "cow", "frog",
    "cat", "hen", "insects", "sheep", "crow",
    "rain", "sea waves", "crackling fire", "crickets", "chirping birds",
    "water drops", "wind", "pouring water", "toilet flush", "thunderstorm",
    "crying baby", "sneezing", "clapping", "breathing", "coughing",
    "footsteps", "laughing", "brushing teeth", "snoring", "drinking sipping",
    "door knock", "mouse click", "keyboard typing", "door creaking",
    "can opening",
    "washing machine", "vacuum cleaner", "clock alarm", "clock tick",
    "glass breaking",
    "helicopter", "chainsaw", "siren", "car horn", "engine",
    "train", "church bells", "airplane", "fireworks", "hand saw",
]

ESC50_DESCRIPTIONS = {
    "dog": "A dog barking — the excited, rhythmic sound dogs make",
    "rooster": "A rooster crowing — the loud wake-up call at dawn",
    "pig": "A pig oinking — the snorting sounds pigs make",
    "cow": "A cow mooing — the deep, long call of cattle",
    "frog": "A frog croaking — the rhythmic sound frogs make near water",
    "cat": "A cat meowing — the vocal call cats use to communicate",
    "hen": "A hen clucking — the soft sounds chickens make",
    "insects": "Insects buzzing — the constant hum of flying bugs",
    "sheep": "A sheep bleating — the 'baa' sound sheep make",
    "crow": "A crow cawing — the harsh call of black crows",
    "rain": "Rain falling — drops of water hitting surfaces rhythmically",
    "sea waves": "Ocean waves — the rhythmic crash and retreat of water",
    "crackling fire": "A fire crackling — the snapping and popping of burning wood",
    "crickets": "Crickets chirping — the steady pulse of cricket song at night",
    "chirping birds": "Birds chirping — the cheerful singing of small birds",
    "water drops": "Water dripping — individual drops falling and splashing",
    "wind": "Wind blowing — the rushing sound of air moving",
    "pouring water": "Water pouring — the continuous splash of liquid flowing",
    "thunderstorm": "Thunder rumbling — the deep rolling boom after lightning",
    "crying baby": "A baby crying — the urgent wail of an infant",
    "sneezing": "A sneeze — the sudden explosive burst of air",
    "clapping": "Hands clapping — the sharp rhythmic sound of applause",
    "laughing": "Laughter — the joyful sound of someone finding something funny",
    "footsteps": "Footsteps — the rhythmic sound of walking",
    "door knock": "Knocking on a door — the rap of knuckles on wood",
    "clock tick": "A clock ticking — the steady mechanical pulse of time",
    "glass breaking": "Glass shattering — the sharp crash of breaking glass",
    "church bells": "Church bells ringing — the deep melodic tone of large bells",
    "fireworks": "Fireworks exploding — the boom and crackle of celebrations",
    "chainsaw": "A chainsaw running — the loud buzzing roar of a power saw",
    "siren": "A siren wailing — the rising and falling alarm of emergency vehicles",
    "car horn": "A car horn honking — the sharp warning blast from a vehicle",
    "helicopter": "A helicopter — the rhythmic chopping of spinning rotors",
    "airplane": "An airplane — the roaring whoosh of jet engines overhead",
    "train": "A train passing — the rumble and clatter of wheels on rails",
}

# Fill missing descriptions
for _label in ESC50_LABELS:
    if _label not in ESC50_DESCRIPTIONS:
        ESC50_DESCRIPTIONS[_label] = f"The sound of {_label}"


# Speech Commands labels
SPEECH_COMMANDS_LABELS = ["yes", "no", "up", "down", "left", "right", "go", "stop"]

SPEECH_DESCRIPTIONS = {
    "yes": "Someone saying 'yes' — an affirmative response",
    "no": "Someone saying 'no' — a negative response",
    "up": "Someone saying 'up' — the direction above",
    "down": "Someone saying 'down' — the direction below",
    "left": "Someone saying 'left' — the direction to one side",
    "right": "Someone saying 'right' — the direction to the other side",
    "go": "Someone saying 'go' — a command to begin moving",
    "stop": "Someone saying 'stop' — a command to halt",
}


# ============================================================================
# ImageNet-1K class labels (representative subset — 250 diverse classes)
# ============================================================================

IMAGENET_CLASSES = [
    # Fish & aquatic
    "tench", "goldfish", "great white shark", "tiger shark", "hammerhead shark",
    "electric ray", "stingray", "sturgeon", "pufferfish", "lionfish",
    # Birds
    "rooster", "hen", "ostrich", "flamingo", "pelican",
    "albatross", "king penguin", "hummingbird", "toucan", "eagle",
    "owl", "parrot", "peacock", "magpie", "jay",
    "robin", "goldfinch", "vulture", "crane", "swan",
    # Reptiles & amphibians
    "green sea turtle", "box turtle", "iguana", "chameleon", "komodo dragon",
    "alligator", "king cobra", "boa constrictor", "tree frog", "salamander",
    # Mammals — diverse (skip near-duplicate dog breeds)
    "timber wolf", "red fox", "arctic fox", "grizzly bear", "polar bear",
    "giant panda", "cheetah", "snow leopard", "lion", "tiger",
    "gorilla", "chimpanzee", "orangutan", "baboon", "gibbon",
    "African elephant", "Indian rhinoceros", "hippopotamus", "zebra", "giraffe",
    "bison", "ram", "ibex", "gazelle", "impala",
    "camel", "llama", "otter", "sea lion", "walrus",
    "humpback whale", "dolphin", "beaver", "porcupine", "hedgehog",
    "armadillo", "sloth", "koala", "wombat", "kangaroo",
    "platypus", "bat", "meerkat", "red panda", "raccoon",
    "skunk", "badger", "squirrel", "chipmunk", "hamster",
    # Insects & arachnids
    "monarch butterfly", "dragonfly", "ladybug", "grasshopper", "praying mantis",
    "cockroach", "honeybee", "ant", "scorpion", "tarantula",
    "garden spider", "tick", "centipede", "beetle", "firefly",
    # Marine invertebrates
    "jellyfish", "starfish", "sea urchin", "sea cucumber", "coral",
    "octopus", "squid", "lobster", "hermit crab", "snail",
    # Plants & fungi
    "mushroom", "agaric", "ear of corn", "acorn", "hip rose",
    "daisy", "sunflower", "lotus", "orchid", "dandelion",
    # Food
    "pizza", "cheeseburger", "pretzel", "bagel", "ice cream",
    "chocolate cake", "guacamole", "sushi", "espresso", "pomegranate",
    "banana", "pineapple", "strawberry", "lemon", "fig",
    "broccoli", "bell pepper", "artichoke", "cucumber", "cauliflower",
    # Vehicles & transport
    "sports car", "school bus", "fire truck", "ambulance", "pickup truck",
    "tank", "freight train", "steam locomotive", "speedboat", "canoe",
    "sailboat", "aircraft carrier", "space shuttle", "hot air balloon", "hang glider",
    "bicycle", "motorcycle", "scooter", "tractor", "forklift",
    # Buildings & structures
    "castle", "church", "mosque", "pagoda", "lighthouse",
    "barn", "greenhouse", "dam", "suspension bridge", "triumphal arch",
    "palace", "monastery", "skyscraper", "log cabin", "yurt",
    # Tools & instruments
    "hammer", "screwdriver", "chainsaw", "power drill", "hatchet",
    "acoustic guitar", "electric guitar", "grand piano", "violin", "cello",
    "drums", "harmonica", "flute", "saxophone", "French horn",
    # Household & everyday
    "desk", "rocking chair", "bookcase", "bathtub", "toilet",
    "television", "laptop", "desktop computer", "cellular phone", "typewriter",
    "refrigerator", "washing machine", "iron", "toaster", "microwave",
    "hourglass", "sundial", "combination lock", "padlock", "safe",
    # Clothing & accessories
    "sombrero", "cowboy hat", "crown", "graduation cap", "ski mask",
    "sunglasses", "running shoe", "sandal", "bow tie", "kimono",
    # Sports & recreation
    "basketball", "soccer ball", "tennis ball", "golf ball", "volleyball",
    "baseball bat", "tennis racket", "surfboard", "snowboard", "parachute",
    # Science & medical
    "microscope", "telescope", "stethoscope", "syringe", "magnetic compass",
    "barometer", "abacus", "hourglass", "globe", "solar panel",
]

# Auto-generate descriptions for ImageNet classes
IMAGENET_DESCRIPTIONS = {}
for _cls in IMAGENET_CLASSES:
    IMAGENET_DESCRIPTIONS[_cls] = f"A {_cls} — a real-world object or creature"


# ============================================================================
# Places365 scene categories (~200 diverse scenes)
# ============================================================================

PLACES_SCENES = [
    "airport terminal", "art gallery", "bakery", "bamboo forest", "barn",
    "baseball field", "bathroom", "beach", "bedroom", "bookstore",
    "bowling alley", "bridge", "bus interior", "campsite", "canyon",
    "castle", "cathedral", "cemetery", "classroom", "cliff",
    "closet", "coast", "coffee shop", "construction site", "coral reef",
    "corridor", "courtyard", "creek", "dam", "desert",
    "dining room", "dock", "elevator", "farm", "fire station",
    "forest", "fountain", "garage", "garden", "glacier",
    "golf course", "greenhouse", "gymnasium", "harbor", "highway",
    "hospital", "hotel room", "ice rink", "igloo", "island",
    "jail cell", "jungle", "kitchen", "lake", "laundromat",
    "library", "lighthouse", "living room", "marsh", "monastery",
    "mountain", "museum", "nursery", "ocean", "office",
    "opera house", "orchard", "palace", "parking lot", "patio",
    "pier", "playground", "plaza", "pond", "prison",
    "pub", "quarry", "racecourse", "railroad track", "rainforest",
    "restaurant", "river", "roof garden", "ruin", "runway",
    "sauna", "schoolhouse", "ship deck", "ski slope", "skyscraper",
    "slum", "snowfield", "stadium", "stage", "staircase",
    "stream", "subway station", "supermarket", "swamp", "swimming pool",
    "temple", "theater", "tower", "train station", "tree house",
    "trench", "tundra", "underwater", "valley", "volcano",
    "waterfall", "wheat field", "wind farm", "yard", "zen garden",
    "amusement park", "aquarium", "arcade", "attic", "balcony",
    "bazaar", "boardwalk", "botanical garden", "bowling green", "brewery",
    "bus stop", "cabin", "cafeteria", "casino", "cave",
    "chapel", "chicken coop", "church interior", "cityscape", "clearing",
    "coal mine", "cockpit", "conference room", "conservatory", "corn field",
    "covered bridge", "crosswalk", "dentist office", "diner", "dorm room",
    "driveway", "drugstore", "embassy", "engine room", "entrance hall",
    "escalator", "factory", "fairground", "flea market", "flower shop",
    "food court", "gas station", "gift shop", "golf driving range", "gorge",
    "grotto", "guest room", "gymnasium", "hangar", "helipad",
    "herb garden", "hot spring", "hunting lodge", "industrial area", "inn",
    "junkyard", "kasbah", "kennel", "lagoon", "landing deck",
    "laundry room", "lawn", "loading dock", "loft", "lookout point",
    "lumber yard", "mausoleum", "meadow", "mine shaft", "moat",
    "movie theater", "oasis", "observatory", "oil rig", "open market",
    "outhouse", "overlook", "pantry", "parkland", "pasture",
    "pavilion", "pharmacy", "phone booth", "picnic area", "planetarium",
    "porch", "pottery studio", "power plant", "pulpit", "putting green",
    "raft", "reception area", "recreation room", "riding arena", "rock arch",
    "rope bridge", "sand dune", "sawmill", "sculpture garden", "server room",
    "shed", "shelter", "shoe shop", "shopfront", "ski resort",
    "smoke stack", "solarium", "stable", "storage room", "tea room",
    "toll plaza", "topiary garden", "track field", "tree farm", "treetop",
    "utility room", "veranda", "vineyard", "waiting room", "warehouse",
    "water tower", "wetland", "wharf", "windmill", "wine cellar",
    "woodshop", "wrestling ring",
]


# ============================================================================
# Kinetics action classes (~200 human actions)
# ============================================================================

KINETICS_ACTIONS = [
    "climbing a tree", "playing guitar", "swimming", "riding a bicycle",
    "cooking", "painting", "dancing", "reading a book", "running",
    "jumping rope", "throwing a ball", "catching a fish", "building a sandcastle",
    "flying a kite", "rowing a boat", "skiing", "skateboarding",
    "playing drums", "singing", "writing", "typing on keyboard",
    "washing hands", "brushing teeth", "eating", "drinking water",
    "hugging", "shaking hands", "waving", "pointing", "laughing",
    "crying", "yawning", "sneezing", "stretching", "bending",
    "lifting weights", "doing yoga", "meditating", "gardening",
    "feeding animals", "petting a dog", "walking a dog",
    "riding a horse", "milking a cow", "shearing a sheep",
    "planting a seed", "watering plants", "pruning branches",
    "chopping wood", "building a fire", "roasting marshmallows",
    "blowing out candles", "wrapping a gift", "opening a present",
    "sewing", "knitting", "weaving", "spinning clay on a potter's wheel",
    "carving wood", "folding origami", "assembling a puzzle",
    "playing chess", "juggling", "doing a magic trick",
    "arm wrestling", "fencing", "boxing", "wrestling",
    "playing basketball", "playing soccer", "playing tennis",
    "playing baseball", "playing volleyball", "playing table tennis",
    "doing gymnastics", "doing a backflip", "doing a cartwheel",
    "hula hooping", "skipping", "crawling", "somersaulting",
    "rock climbing", "rappelling", "bungee jumping", "parachuting",
    "surfing", "windsurfing", "kayaking", "canoeing",
    "scuba diving", "snorkeling", "water skiing", "cliff diving",
    "ice skating", "figure skating", "playing hockey", "curling",
    "snowboarding", "sledding", "making a snowman", "throwing snowballs",
    "archery", "shooting a bow", "throwing a javelin", "throwing a discus",
    "pole vaulting", "high jumping", "long jumping", "triple jumping",
    "hurdling", "sprinting", "jogging", "walking slowly",
    "marching", "tiptoeing", "stomping", "shuffling cards",
    "dealing cards", "rolling dice", "spinning a top", "blowing bubbles",
    "flying a drone", "sailing a boat", "steering a ship",
    "driving a car", "parallel parking", "changing a tire",
    "pumping gas", "washing a car", "polishing shoes",
    "ironing clothes", "folding laundry", "hanging clothes on a line",
    "vacuuming", "sweeping the floor", "mopping", "scrubbing a pan",
    "dicing vegetables", "kneading dough", "flipping a pancake",
    "grilling meat", "stirring a pot", "frosting a cake",
    "pouring a drink", "setting the table", "clearing the table",
    "making a bed", "arranging flowers", "lighting a candle",
    "feeding a baby", "rocking a baby", "pushing a stroller",
    "tying shoelaces", "buttoning a shirt", "zipping a jacket",
    "putting on a hat", "applying sunscreen", "brushing hair",
    "braiding hair", "shaving", "putting on makeup",
    "taking a photograph", "filming a video", "editing on a computer",
    "drawing a sketch", "sculpting with clay", "spray painting",
    "playing the piano", "playing the violin", "playing the flute",
    "playing the trumpet", "conducting an orchestra",
    "performing on stage", "telling a joke", "giving a speech",
    "leading a meeting", "teaching a class", "taking notes",
    "raising a hand", "applauding", "bowing",
    "saluting", "praying", "crossing arms",
    "scratching head", "rubbing eyes", "cracking knuckles",
    "whistling", "humming a tune", "snapping fingers",
    "clapping rhythmically", "tapping foot", "nodding head",
    "shaking head", "shrugging shoulders", "blinking rapidly",
    "winking", "sticking out tongue", "making a face",
    "smiling broadly", "frowning deeply", "gasping in surprise",
]


# ============================================================================
# AudioSet sound classes (expanded beyond ESC-50 — ~200 unique sounds)
# ============================================================================

AUDIOSET_SOUNDS = [
    # Weather & nature
    "thunderstorm", "rain on leaves", "hailstorm", "wind howling",
    "ocean waves crashing", "stream bubbling", "waterfall roaring",
    "fire crackling", "avalanche rumbling", "earthquake shaking",
    "tornado siren", "ice cracking on a lake",
    # Explosions & impacts
    "explosion", "gunshot", "cannon firing", "fireworks bursting",
    "car crash", "tree falling", "rock slide", "balloon popping",
    "whip crack", "slap",
    # Vehicles & machines
    "car engine starting", "motorcycle revving", "airplane overhead",
    "helicopter", "train horn", "ship horn", "bicycle bell",
    "subway arriving", "jet engine roaring", "propeller airplane",
    "race car speeding", "truck reversing beep", "boat motor",
    "snowmobile", "lawn mower running", "leaf blower",
    # Household
    "door creaking", "door slamming", "glass breaking",
    "typing on keyboard", "phone ringing", "alarm clock",
    "microwave beeping", "blender running", "vacuum cleaner",
    "washing machine spinning", "dishwasher running", "toilet flushing",
    "shower running", "faucet dripping", "kettle whistling",
    "toaster popping", "oven timer dinging", "garbage disposal",
    "air conditioner humming", "ceiling fan whirring",
    # Human sounds
    "baby crying", "baby laughing", "child playing",
    "crowd cheering", "applause", "booing",
    "coughing", "snoring", "heartbeat",
    "stomach growling", "hiccup", "burp",
    "gargling", "whistling a tune", "humming",
    "whispering", "shouting", "screaming",
    "sighing deeply", "gasping for breath", "yawning loudly",
    # Footsteps
    "footsteps on gravel", "footsteps on wood", "footsteps in snow",
    "footsteps on marble", "footsteps in mud", "footsteps on metal",
    "high heels clicking", "boots stomping", "bare feet on tile",
    # Musical instruments
    "piano playing", "violin playing", "trumpet playing",
    "drums playing", "guitar strumming", "flute playing",
    "cello bowing", "harp plucking", "accordion playing",
    "bagpipes droning", "banjo picking", "didgeridoo humming",
    "xylophone tinkling", "tambourine shaking", "triangle ringing",
    "organ playing in a cathedral", "synthesizer buzzing",
    "steel drum in the tropics", "sitar twanging", "ukulele strumming",
    # Animals
    "bird singing", "bird chirping", "owl hooting",
    "dog barking", "cat meowing", "horse neighing",
    "cow mooing", "sheep bleating", "rooster crowing",
    "frog croaking", "cricket chirping", "bee buzzing",
    "mosquito buzzing", "fly buzzing", "cicada droning",
    "whale singing underwater", "dolphin clicking",
    "wolf howling", "coyote yipping", "monkey chattering",
    "elephant trumpeting", "lion roaring", "parrot squawking",
    "snake hissing", "rattlesnake rattling", "crow cawing",
    "seagull calling", "duck quacking", "goose honking",
    # Tools & construction
    "chainsaw", "hammer hitting nail", "saw cutting wood",
    "drill", "sanding", "welding arc",
    "rivet gun", "jackhammer", "bulldozer engine",
    "cement mixer turning", "nail gun firing", "wrench tightening",
    "sandblaster", "grinder spinning", "chisel on stone",
    # Urban & environment
    "traffic noise", "car alarm", "police siren",
    "ambulance siren", "fire truck siren", "church bell tolling",
    "clock tower chiming", "factory machinery", "construction site",
    "market crowd", "playground children", "fountain splashing",
    "flag flapping in wind", "skateboard on pavement", "shopping cart rolling",
    # Electronic & digital
    "computer startup sound", "notification ping", "dial-up modem",
    "printer printing", "scanner scanning", "hard drive clicking",
    "static white noise", "radio tuning between stations",
    "video game sound effects", "cash register beeping",
]


# ============================================================================
# Emotional/social scenarios (100 items)
# ============================================================================

EMOTIONAL_SCENARIOS = [
    # Joy & happiness
    "A child's face lighting up when they see a birthday cake with candles",
    "Two old friends recognizing each other after years apart",
    "A dog waiting patiently by the door, then leaping with joy when its owner arrives",
    "The quiet satisfaction of finishing a difficult puzzle",
    "The nervous excitement before opening a wrapped gift",
    "A parent holding their newborn baby for the first time",
    "A crowd erupting in cheers after a game-winning goal",
    "A student opening an acceptance letter to their dream school",
    "Children laughing together while splashing in puddles after rain",
    "A grandmother smiling as she teaches her grandchild to bake cookies",
    "The delight of discovering the first flower blooming in spring",
    "A musician's joy when the audience gives a standing ovation",
    "The thrill of riding a bicycle without training wheels for the first time",
    # Sadness & loss
    "The loneliness of an empty playground on a rainy day",
    "An elderly person sitting alone on a park bench watching families play",
    "A child waving goodbye as their best friend moves to another city",
    "The last leaf falling from a tree at the end of autumn",
    "A dog lying by the door of an owner who will not return",
    "Rain streaking down a window while someone stares out in silence",
    "An abandoned teddy bear left on a park bench",
    "The quiet emptiness of a house after everyone has moved away",
    "A wilted flower in a forgotten vase on a dusty windowsill",
    "The fading echo of laughter in an empty school hallway",
    # Fear & anxiety
    "A child hearing a strange noise in a dark room at night",
    "The tension of walking alone down an unfamiliar alley at dusk",
    "A cat arching its back and hissing at something unseen",
    "The moment of vertigo when looking down from a great height",
    "A deer frozen in headlights, eyes wide with alarm",
    "Thunder crashing close by while lights flicker during a storm",
    "The nervousness of standing backstage before a big performance",
    "A person's hands trembling before giving their first speech",
    "The unsettling feeling of being watched in an empty room",
    "A ship rocking violently in a sudden ocean storm",
    # Surprise & wonder
    "A person discovering a hidden waterfall deep in the forest",
    "A child seeing the ocean for the very first time, mouth agape",
    "Opening a door to find a room full of friends shouting surprise",
    "Watching a shooting star streak across a clear night sky",
    "A butterfly landing gently on someone's outstretched hand",
    "The awe of standing inside a massive cathedral with light streaming through stained glass",
    "Discovering a fossil embedded in a rock while hiking",
    "A baby seeing snow falling for the first time, reaching out to catch flakes",
    "Finding a four-leaf clover in a field of ordinary ones",
    "An astronaut seeing Earth from space through a tiny window",
    # Anger & frustration
    "A child stomping their feet when told they cannot have a toy",
    "A driver gripping the steering wheel in bumper-to-bumper traffic",
    "Two siblings arguing over who gets the last slice of cake",
    "A person crumpling up a failed drawing and starting over",
    "The frustration of a key that will not fit into a stubborn lock",
    "A chess player realizing they are about to lose the match",
    "Someone discovering their carefully built sandcastle washed away by a wave",
    "A hiker finding the trail blocked by a fallen tree",
    "The exasperation of untangling a knotted pair of earphones",
    "A painter accidentally knocking over an open jar of paint",
    # Disgust & discomfort
    "Stepping barefoot on something cold and slimy in the dark",
    "The grimace of tasting extremely sour or bitter food",
    "Finding a spider web stretched across a doorway at face height",
    "The recoil from the smell of spoiled milk",
    "A child's disgusted face when asked to eat vegetables they dislike",
    # Trust & safety
    "A small child reaching up to hold a parent's hand while crossing the street",
    "A dog sleeping peacefully curled up against its owner's feet",
    "A firefighter carrying a child to safety through smoke",
    "A teacher patiently explaining a concept until a student understands",
    "Two strangers helping each other carry a heavy box up stairs",
    "A lifeguard watching over swimmers from a tall chair",
    "A night light glowing softly in a child's bedroom",
    "A mother bird shielding her chicks under her wings during rain",
    # Anticipation & hope
    "Seeds planted in spring soil, waiting for the first green sprout",
    "A family setting up chairs and watching for fireworks to begin",
    "A pregnant woman feeling the baby kick for the first time",
    "Students counting down the final seconds before summer vacation",
    "A traveler checking the departure board at an airport",
    "An artist stepping back to look at a half-finished painting with fresh eyes",
    "A fisherman watching the bobber, waiting for a bite",
    "The smell of dinner cooking as children set the table",
    # Empathy & compassion
    "A child sharing their umbrella with a classmate in the rain",
    "A stranger stopping to help someone who dropped their groceries",
    "A nurse holding the hand of a nervous patient before surgery",
    "Neighbors bringing food to a family going through a hard time",
    "A person gently returning a baby bird to its nest",
    # Pride & accomplishment
    "The pride in a child's eyes when they ride a bicycle without training wheels",
    "An athlete standing on the podium receiving a gold medal",
    "A gardener admiring a garden they built from bare soil",
    "A student presenting a science project they worked on for months",
    "An elderly person finishing a marathon, crossing the line with arms raised",
    # Nostalgia & bittersweet
    "Finding a childhood photo tucked inside an old book",
    "The scent of a grandparent's perfume bringing back memories",
    "Walking through the halls of the school you graduated from years ago",
    "Hearing a song on the radio that was playing during an important moment in your life",
    "Looking at crayon drawings your children made when they were small",
    # Gratitude & contentment
    "Sitting by a campfire under the stars after a long day of hiking",
    "The warmth of a cup of tea on a cold morning",
    "A cat purring loudly in a sunbeam on a lazy afternoon",
    "The feeling of clean sheets and a soft pillow at the end of a tiring day",
    "Watching the sunset from a quiet hilltop with no one else around",
]


# ============================================================================
# Abstract concepts (80 items — physics, math, logic, relationships)
# ============================================================================

ABSTRACT_CONCEPTS = [
    # Physics & forces
    "The passage of time shown by a melting candle",
    "Gravity pulling a leaf from a tree to the ground",
    "A pendulum swinging back and forth with decreasing amplitude",
    "Light bending through a glass prism into a rainbow of colors",
    "Sound waves rippling outward from a ringing bell",
    "Heat rising from a hot road creating shimmering mirages",
    "Magnetism pulling iron filings into curved patterns",
    "Static electricity making hair stand on end",
    "Friction stopping a sliding hockey puck on rough ice",
    "Momentum carrying a rolling ball up a small hill",
    "Centrifugal force pushing water to the edges of a spinning bucket",
    "Buoyancy keeping a heavy ship floating on water",
    "Pressure squeezing a balloon until it pops",
    "Inertia keeping a passenger moving forward when a car stops suddenly",
    "Resonance causing a bridge to vibrate when soldiers march in step",
    # Symmetry & patterns
    "Symmetry in a butterfly's wings — each side a mirror of the other",
    "The spiral pattern of seeds in a sunflower following the Fibonacci sequence",
    "Concentric ripples spreading outward from a stone dropped in still water",
    "The fractal pattern of a fern where each frond copies the whole shape",
    "Tessellation — hexagonal tiles fitting together with no gaps like a honeycomb",
    "The branching pattern of lightning mirroring the branching of tree roots",
    "Rotational symmetry in a snowflake with six identical arms",
    "The golden ratio in the proportions of a nautilus shell",
    # Cause & effect
    "Cause and effect: a ball hits dominoes and they fall in sequence",
    "A spark igniting a chain of firecrackers one after another",
    "Rain filling a stream that feeds a river that reaches the ocean",
    "A seed absorbing water and cracking open its shell to sprout",
    "Wind pushing clouds together until they release rain",
    "Erosion slowly carving a canyon over millions of years",
    "A small crack in a dam growing until water breaks through",
    "One person's smile making another person smile in return",
    # Scale & proportion
    "Scale: an ant next to an elephant, showing vast size difference",
    "A grain of sand compared to the entire beach it sits on",
    "The Earth as a tiny blue dot seen from the edge of the solar system",
    "A single cell under a microscope compared to the whole organism",
    "A second compared to a century — both are time, vastly different",
    "A whisper compared to a thunderclap — both are sound, vastly different",
    # Transparency & visibility
    "Transparency: seeing fish through clear water",
    "A glass window letting light through while blocking wind",
    "Fog making distant objects fade into white nothingness",
    "An X-ray revealing bones hidden beneath skin and muscle",
    "Infrared vision showing heat signatures invisible to the naked eye",
    # Reflection & light
    "Reflection: a mountain mirrored in a still lake",
    "A candle flame reflected in a dark window at night",
    "A funhouse mirror distorting a person's reflection into strange shapes",
    "The moon reflecting sunlight to illuminate the night",
    "A kaleidoscope creating infinite reflections from a few colored pieces",
    # Shadow & darkness
    "Shadow: a tree's dark shape stretching across grass in evening light",
    "A sundial using shadow to tell time as the sun moves",
    "Shadows growing longer as the sun sets toward the horizon",
    "Eclipse: the moon's shadow falling across the face of the Earth",
    # Transformation & change
    "A caterpillar transforming into a butterfly inside a chrysalis",
    "Water changing from ice to liquid to steam with increasing heat",
    "A piece of iron slowly turning to rust when exposed to rain",
    "Wood becoming charcoal after burning in a low-oxygen fire",
    "Dough rising as yeast produces gas inside it",
    "A tadpole growing legs and losing its tail to become a frog",
    "Day slowly transitioning to night through the colors of sunset",
    # Logic & relationships
    "Part and whole: a single brick within a massive wall",
    "Sequence: the numbers 1, 1, 2, 3, 5, 8 — each is the sum of the previous two",
    "Negation: an empty glass versus a full glass",
    "Containment: a box inside a box inside a bigger box",
    "Hierarchy: a single soldier, a squad leader, a general, a commander",
    "Network: roads connecting villages that connect to cities that connect to countries",
    "Balance: a seesaw with equal weights on both sides staying level",
    "Entropy: a neat stack of blocks compared to the same blocks scattered randomly",
    "Feedback loop: a microphone picking up its own speaker output, creating a screech",
    # Abstract math
    "Infinity: a hallway of mirrors reflecting into mirrors forever",
    "Zero: an empty bowl that once held fruit",
    "Probability: a coin spinning in the air before it lands heads or tails",
    "Exponential growth: one grain of rice, then two, four, eight, filling a chessboard",
    "Convergence: many streams merging into a single river",
    "Divergence: a single path splitting into many trails in a forest",
    "Periodicity: the regular rise and fall of ocean tides",
    "Randomness: leaves falling from a tree, each taking a different unpredictable path",
]


# ============================================================================
# Compositional scene generator
# ============================================================================

def generate_compositional_scenes(n=2000):
    """Generate diverse scenes by combining elements. Deterministic (seed=42)."""
    adjectives = [
        "bright", "dark", "warm", "cold", "wet", "dry", "soft", "hard",
        "rough", "smooth", "heavy", "light", "old", "new", "broken", "whole",
        "quiet", "loud", "fast", "slow", "tall", "short", "wide", "narrow",
        "shiny", "dusty", "frozen", "burning", "faded", "vivid", "tiny", "enormous",
    ]

    subjects = [
        "cat", "dog", "bird", "child", "tree", "flower", "rock", "cloud",
        "river", "mountain", "house", "car", "boat", "fish", "butterfly",
        "spider", "ant", "leaf", "feather", "shell", "crystal", "flame",
        "snail", "turtle", "rabbit", "fox", "owl", "frog", "mushroom", "lantern",
        "kite", "balloon", "shadow", "raindrop", "snowflake", "candle", "key",
    ]

    actions = [
        "resting on", "moving across", "floating above", "hiding behind",
        "sitting next to", "falling from", "growing through", "reflecting in",
        "leaning against", "hanging from", "swimming in", "flying over",
        "crawling under", "climbing up", "sliding down", "rolling along",
        "balanced on", "trapped inside", "emerging from", "circling around",
        "drifting past", "perched atop", "buried beneath", "tangled with",
    ]

    locations = [
        "a sunlit meadow", "a dark forest", "a sandy beach", "a snowy field",
        "a dusty road", "a calm pond", "a rocky cliff", "a quiet room",
        "a busy street", "a misty morning", "a starlit night", "a rainy afternoon",
        "an open desert", "a mossy cave", "a frozen lake", "a blooming garden",
        "a crumbling ruin", "a moonlit path", "a windswept hill", "a shady orchard",
        "a steaming jungle", "a crystal-clear stream", "a cobblestone alley",
        "a vast wheat field",
    ]

    rng = random.Random(42)
    scenes = set()
    attempts = 0
    max_attempts = n * 10
    while len(scenes) < n and attempts < max_attempts:
        adj = rng.choice(adjectives)
        subj = rng.choice(subjects)
        act = rng.choice(actions)
        loc = rng.choice(locations)
        scene = f"A {adj} {subj} {act} {loc}"
        scenes.add(scene)
        attempts += 1
    return list(sorted(scenes))


# Pre-generate compositional scenes at module load time (deterministic)
COMPOSITIONAL_SCENES = generate_compositional_scenes(2000)


class MultimodalDataLoader:
    """Lazy-loading multimodal dataset manager.

    Downloads datasets on first access and caches them locally.
    Provides methods to sample visual, audio, and speech data by concept.
    """

    def __init__(self, data_dir=None):
        self.data_dir = data_dir or DATA_DIR
        os.makedirs(self.data_dir, exist_ok=True)

        # Lazy-loaded dataset storage
        self._cifar100 = None       # {label: [image_bytes, ...]}
        self._esc50 = None          # {label: [audio_float_array, ...]}
        self._speech_cmds = None    # {label: [audio_float_array, ...]}
        self._expanded_pool = None  # [(description, modality), ...]

        # Track what's been loaded
        self._cifar100_ready = False
        self._esc50_ready = False
        self._speech_ready = False

    # ------------------------------------------------------------------
    # CIFAR-100 (images)
    # ------------------------------------------------------------------

    def _load_cifar100(self):
        """Load CIFAR-100 from HuggingFace datasets or local cache."""
        if self._cifar100_ready:
            return

        cache_path = os.path.join(self.data_dir, "cifar100_by_label.pkl")
        if os.path.exists(cache_path):
            logger.info("Loading CIFAR-100 from local cache...")
            with open(cache_path, "rb") as f:
                self._cifar100 = pickle.load(f)
            self._cifar100_ready = True
            total = sum(len(v) for v in self._cifar100.values())
            logger.info("CIFAR-100 loaded: %d images across %d classes",
                        total, len(self._cifar100))
            return

        logger.info("Downloading CIFAR-100 via HuggingFace datasets...")
        try:
            from datasets import load_dataset
            ds = load_dataset("cifar100", split="train", trust_remote_code=True)
        except Exception as e:
            logger.warning("CIFAR-100 download failed: %s", e)
            self._cifar100 = {}
            self._cifar100_ready = True
            return

        # Organize by fine_label
        by_label = {}
        for item in ds:
            label_idx = item["fine_label"]
            label_name = CIFAR100_LABELS[label_idx]
            img = item["img"]  # PIL Image

            # Convert to 32x32x3 raw bytes
            img = img.convert("RGB").resize((32, 32))
            raw = np.array(img, dtype=np.uint8).tobytes()

            if label_name not in by_label:
                by_label[label_name] = []
            by_label[label_name].append(raw)

        self._cifar100 = by_label
        self._cifar100_ready = True

        # Cache locally
        try:
            with open(cache_path, "wb") as f:
                pickle.dump(by_label, f, protocol=4)
            logger.info("CIFAR-100 cached to %s", cache_path)
        except Exception as e:
            logger.warning("Failed to cache CIFAR-100: %s", e)

        total = sum(len(v) for v in by_label.values())
        logger.info("CIFAR-100 loaded: %d images across %d classes",
                    total, len(by_label))

    def get_visual(self, concept=None):
        """Get a visual sample (image bytes + label + description).

        Args:
            concept: Optional label to match (e.g. "dog", "butterfly").
                     If None, picks a random class.

        Returns:
            (image_bytes, width, height, channels, label, description) or None
        """
        self._load_cifar100()
        if not self._cifar100:
            return None

        if concept:
            # Try exact match first, then substring
            concept_lower = concept.lower()
            matching = [k for k in self._cifar100
                        if concept_lower in k or k in concept_lower]
            if matching:
                label = random.choice(matching)
            else:
                label = random.choice(list(self._cifar100.keys()))
        else:
            label = random.choice(list(self._cifar100.keys()))

        images = self._cifar100[label]
        raw = random.choice(images)
        desc = CIFAR100_DESCRIPTIONS.get(label, f"A {label}")
        return raw, 32, 32, 3, label, desc

    def get_visual_labels(self):
        """Get list of available visual labels."""
        self._load_cifar100()
        return list(self._cifar100.keys()) if self._cifar100 else []

    # ------------------------------------------------------------------
    # ESC-50 (environmental audio)
    # ------------------------------------------------------------------

    def _load_esc50(self):
        """Load ESC-50 from GitHub or local cache."""
        if self._esc50_ready:
            return

        cache_path = os.path.join(self.data_dir, "esc50_by_label.pkl")
        if os.path.exists(cache_path):
            logger.info("Loading ESC-50 from local cache...")
            with open(cache_path, "rb") as f:
                self._esc50 = pickle.load(f)
            self._esc50_ready = True
            total = sum(len(v) for v in self._esc50.values())
            logger.info("ESC-50 loaded: %d clips across %d classes",
                        total, len(self._esc50))
            return

        logger.info("Downloading ESC-50 via HuggingFace datasets...")
        try:
            # Audio dataset decode causes std::bad_alloc on memory-constrained containers.
            # Use synthetic audio instead — audio cortex CNN still trains on formant features.
            if os.environ.get('NIMCP_SKIP_AUDIO_DOWNLOAD') or not os.path.exists(cache_path):
                raise RuntimeError("Skipping audio download (NIMCP_SKIP_AUDIO_DOWNLOAD or no cache)")
            from datasets import load_dataset
            ds = load_dataset("ashraq/esc50", split="train", streaming=True)
        except Exception as e:
            logger.warning("ESC-50 download failed: %s", e)
            self._esc50 = {}
            self._esc50_ready = True
            return

        by_label = {}
        clip_count = 0
        max_clips = 500  # Limit to prevent OOM on memory-constrained containers
        for item in ds:
            if clip_count >= max_clips:
                break
            clip_count += 1
            label_idx = item["target"]
            if label_idx < len(ESC50_LABELS):
                label_name = ESC50_LABELS[label_idx]
            else:
                label_name = f"sound_{label_idx}"

            # Get audio array (resample to 16kHz, take first 2 seconds)
            audio = item.get("audio")
            if audio is None:
                continue

            samples = np.array(audio["array"], dtype=np.float32)
            sr = audio["sampling_rate"]

            # Resample to 16kHz if needed
            if sr != 16000:
                from scipy.signal import resample
                target_len = int(len(samples) * 16000 / sr)
                samples = resample(samples, target_len).astype(np.float32)

            # Take first 2 seconds (32000 samples) for consistency
            max_samples = 32000
            if len(samples) > max_samples:
                samples = samples[:max_samples]

            # Normalize to [-1, 1]
            peak = np.max(np.abs(samples))
            if peak > 1e-6:
                samples = samples / peak * 0.9

            if label_name not in by_label:
                by_label[label_name] = []
            by_label[label_name].append(samples.tolist())

        self._esc50 = by_label
        self._esc50_ready = True

        # Cache locally
        try:
            with open(cache_path, "wb") as f:
                pickle.dump(by_label, f, protocol=4)
            logger.info("ESC-50 cached to %s", cache_path)
        except Exception as e:
            logger.warning("Failed to cache ESC-50: %s", e)

        total = sum(len(v) for v in by_label.values())
        logger.info("ESC-50 loaded: %d clips across %d classes",
                    total, len(by_label))

    def get_audio(self, concept=None):
        """Get an audio sample (float array + label + description).

        Args:
            concept: Optional label to match (e.g. "dog", "rain").
                     If None, picks a random class.

        Returns:
            (audio_samples_list, label, description) or None
        """
        self._load_esc50()
        if not self._esc50:
            return None

        if concept:
            concept_lower = concept.lower()
            matching = [k for k in self._esc50
                        if concept_lower in k or k in concept_lower]
            if matching:
                label = random.choice(matching)
            else:
                label = random.choice(list(self._esc50.keys()))
        else:
            label = random.choice(list(self._esc50.keys()))

        clips = self._esc50[label]
        audio = random.choice(clips)
        desc = ESC50_DESCRIPTIONS.get(label, f"The sound of {label}")
        return audio, label, desc

    def get_audio_labels(self):
        """Get list of available audio labels."""
        self._load_esc50()
        return list(self._esc50.keys()) if self._esc50 else []

    # ------------------------------------------------------------------
    # Speech Commands (spoken words)
    # ------------------------------------------------------------------

    def _load_speech_commands(self):
        """Load Google Speech Commands mini from HuggingFace."""
        if self._speech_ready:
            return

        cache_path = os.path.join(self.data_dir, "speech_cmds_by_label.pkl")
        if os.path.exists(cache_path):
            logger.info("Loading Speech Commands from local cache...")
            with open(cache_path, "rb") as f:
                self._speech_cmds = pickle.load(f)
            self._speech_ready = True
            total = sum(len(v) for v in self._speech_cmds.values())
            logger.info("Speech Commands loaded: %d clips across %d words",
                        total, len(self._speech_cmds))
            return

        logger.info("Downloading Speech Commands via HuggingFace datasets...")
        try:
            if os.environ.get('NIMCP_SKIP_AUDIO_DOWNLOAD') or not os.path.exists(cache_path):
                raise RuntimeError("Skipping audio download (NIMCP_SKIP_AUDIO_DOWNLOAD or no cache)")
            from datasets import load_dataset
            ds = load_dataset("google/speech_commands", "v0.02",
                              split="train", streaming=True)
        except Exception as e:
            logger.warning("Speech Commands download failed: %s", e)
            self._speech_cmds = {}
            self._speech_ready = True
            return

        # Filter to 8 core commands and limit samples per class
        target_labels = set(SPEECH_COMMANDS_LABELS)
        by_label = {label: [] for label in target_labels}
        max_per_label = 200  # Keep dataset manageable

        for item in ds:
            label = item.get("label")
            # Streaming mode: label may be int or string depending on dataset version
            if isinstance(label, int):
                # Map integer to string using our known label list
                label_str = SPEECH_COMMANDS_LABELS[label] if label < len(SPEECH_COMMANDS_LABELS) else f"word_{label}"
            else:
                label_str = str(label)
            if label_str not in target_labels:
                continue
            if len(by_label[label_str]) >= max_per_label:
                continue

            audio = item.get("audio")
            if audio is None:
                continue

            samples = np.array(audio["array"], dtype=np.float32)
            sr = audio["sampling_rate"]

            # Resample to 16kHz if needed
            if sr != 16000:
                from scipy.signal import resample
                target_len = int(len(samples) * 16000 / sr)
                samples = resample(samples, target_len).astype(np.float32)

            # Normalize
            peak = np.max(np.abs(samples))
            if peak > 1e-6:
                samples = samples / peak * 0.9

            by_label[label_str].append(samples.tolist())

        # Remove empty labels
        by_label = {k: v for k, v in by_label.items() if v}
        self._speech_cmds = by_label
        self._speech_ready = True

        # Cache locally
        try:
            with open(cache_path, "wb") as f:
                pickle.dump(by_label, f, protocol=4)
            logger.info("Speech Commands cached to %s", cache_path)
        except Exception as e:
            logger.warning("Failed to cache Speech Commands: %s", e)

        total = sum(len(v) for v in by_label.values())
        logger.info("Speech Commands loaded: %d clips across %d words",
                    total, len(by_label))

    def get_speech(self, word=None):
        """Get a spoken word sample (float array + word + description).

        Args:
            word: Optional word to match (e.g. "yes", "stop").
                  If None, picks a random word.

        Returns:
            (audio_samples_list, word, description) or None
        """
        self._load_speech_commands()
        if not self._speech_cmds:
            return None

        if word and word.lower() in self._speech_cmds:
            label = word.lower()
        else:
            label = random.choice(list(self._speech_cmds.keys()))

        clips = self._speech_cmds[label]
        audio = random.choice(clips)
        desc = SPEECH_DESCRIPTIONS.get(label, f"Someone saying '{label}'")
        return audio, label, desc

    def get_speech_labels(self):
        """Get list of available spoken words."""
        self._load_speech_commands()
        return list(self._speech_cmds.keys()) if self._speech_cmds else []

    # ------------------------------------------------------------------
    # Cross-modal pairing
    # ------------------------------------------------------------------

    # Concepts that exist in both visual and audio datasets
    CROSS_MODAL_MAP = {
        # CIFAR-100 label → ESC-50 label(s) that correspond
        "bear": ["dog"],  # no bear sound, closest
        "butterfly": ["insects"],
        "clock": ["clock tick"],
        "dolphin": ["sea waves"],
        "elephant": ["dog"],  # no elephant sound
        "fox": ["dog"],
        "leopard": ["cat"],
        "lion": ["cat"],
        "sea": ["sea waves"],
        "shark": ["sea waves"],
        "snake": ["insects"],
        "spider": ["insects"],
        "squirrel": ["chirping birds"],
        "tiger": ["cat"],
        "train": ["train"],
        "turtle": ["sea waves"],
        "whale": ["sea waves"],
        "wolf": ["dog"],
        "forest": ["chirping birds", "wind", "crickets"],
        "mountain": ["wind", "thunderstorm"],
        "plain": ["wind", "crickets"],
        "bridge": ["train", "car horn"],
        "castle": ["church bells"],
        "house": ["door knock", "clock tick"],
        "road": ["car horn", "engine"],
        "skyscraper": ["car horn", "siren"],
        "rocket": ["airplane"],
        "tank": ["engine"],
        "tractor": ["engine"],
        "motorcycle": ["engine"],
        "bus": ["engine", "car horn"],
        "pickup truck": ["engine", "car horn"],
        "streetcar": ["train"],
        "telephone": ["clock alarm"],
        "television": ["laughing", "clapping"],
        "rose": ["chirping birds"],
        "sunflower": ["chirping birds", "insects"],
        "orchid": ["chirping birds"],
        "tulip": ["chirping birds"],
        "poppy": ["wind"],
        "mushroom": ["rain", "crickets"],
        "apple": ["rain"],
        "orange": ["rain"],
        "pear": ["rain"],
        "sweet pepper": ["rain"],
    }

    def get_multimodal_pair(self):
        """Get a paired visual+audio sample for cross-modal learning.

        Returns:
            {
                "visual": (image_bytes, w, h, ch, label, desc),
                "audio": (audio_samples, label, desc),
                "concept": str,
                "teaching_text": str,
            } or None
        """
        self._load_cifar100()
        self._load_esc50()

        if not self._cifar100 or not self._esc50:
            return None

        # Pick a concept that has cross-modal mapping
        available = [k for k in self.CROSS_MODAL_MAP
                     if k in self._cifar100]
        if not available:
            return None

        concept = random.choice(available)
        audio_labels = self.CROSS_MODAL_MAP[concept]
        audio_label = None
        for al in audio_labels:
            if al in self._esc50:
                audio_label = al
                break
        if not audio_label:
            return None

        visual = self.get_visual(concept)
        audio_clips = self._esc50[audio_label]
        audio_samples = random.choice(audio_clips)
        audio_desc = ESC50_DESCRIPTIONS.get(audio_label,
                                             f"The sound of {audio_label}")

        visual_desc = CIFAR100_DESCRIPTIONS.get(concept, f"A {concept}")
        teaching = (f"Look at this {concept}! {visual_desc}. "
                    f"Listen — {audio_desc.lower()}.")

        return {
            "visual": visual,
            "audio": (audio_samples, audio_label, audio_desc),
            "concept": concept,
            "teaching_text": teaching,
        }

    # ------------------------------------------------------------------
    # Status / summary
    # ------------------------------------------------------------------

    def summary(self):
        """Print summary of loaded datasets."""
        lines = ["Multimodal Dataset Status:"]
        if self._cifar100_ready and self._cifar100:
            total = sum(len(v) for v in self._cifar100.values())
            lines.append(f"  CIFAR-100:  {total:,} images, "
                         f"{len(self._cifar100)} classes")
        else:
            lines.append("  CIFAR-100:  not loaded")

        if self._esc50_ready and self._esc50:
            total = sum(len(v) for v in self._esc50.values())
            lines.append(f"  ESC-50:     {total:,} audio clips, "
                         f"{len(self._esc50)} classes")
        else:
            lines.append("  ESC-50:     not loaded")

        if self._speech_ready and self._speech_cmds:
            total = sum(len(v) for v in self._speech_cmds.values())
            lines.append(f"  Speech:     {total:,} word clips, "
                         f"{len(self._speech_cmds)} words")
        else:
            lines.append("  Speech:     not loaded")

        # Expanded text-description sources (always available, no download)
        lines.append(f"  ImageNet:   {len(IMAGENET_CLASSES)} class labels (text only)")
        lines.append(f"  Places365:  {len(PLACES_SCENES)} scene categories (text only)")
        lines.append(f"  Kinetics:   {len(KINETICS_ACTIONS)} action classes (text only)")
        lines.append(f"  AudioSet:   {len(AUDIOSET_SOUNDS)} sound classes (text only)")
        lines.append(f"  Emotional:  {len(EMOTIONAL_SCENARIOS)} scenarios (text only)")
        lines.append(f"  Abstract:   {len(ABSTRACT_CONCEPTS)} concepts (text only)")
        lines.append(f"  Composite:  {len(COMPOSITIONAL_SCENES)} generated scenes (text)")
        if self._expanded_pool is not None:
            lines.append(f"  TOTAL expanded pool: {len(self._expanded_pool)} descriptions")

        return "\n".join(lines)

    def ensure_downloaded(self):
        """Pre-download all datasets (call before brain init eats RAM)."""
        print("  [Multimodal] Downloading datasets...")
        self._load_cifar100()
        self._load_esc50()
        self._load_speech_commands()
        print(f"  [Multimodal] {self.summary()}")

    # ------------------------------------------------------------------
    # Expanded text-description datasets (no downloads needed)
    # ------------------------------------------------------------------

    def _build_expanded_pool(self):
        """Build the expanded stimulus pool from all text-description sources."""
        if self._expanded_pool is not None:
            return
        pool = []

        # ImageNet-1K labels
        for cls, desc in IMAGENET_DESCRIPTIONS.items():
            pool.append((desc, "visual"))

        # Places365 scenes
        for scene in PLACES_SCENES:
            pool.append((f"The scene of {scene}", "spatial"))

        # Kinetics actions
        for action in KINETICS_ACTIONS:
            pool.append((f"Someone {action}", "action"))

        # AudioSet expanded sounds
        for sound in AUDIOSET_SOUNDS:
            pool.append((f"The sound of {sound}", "auditory"))

        # Emotional/social scenarios
        for scenario in EMOTIONAL_SCENARIOS:
            pool.append((scenario, "emotional"))

        # Abstract concepts
        for concept in ABSTRACT_CONCEPTS:
            pool.append((concept, "abstract"))

        # Compositional scenes
        for scene in COMPOSITIONAL_SCENES:
            pool.append((scene, "compositional"))

        # Also include original CIFAR-100 and ESC-50 descriptions
        for label, desc in CIFAR100_DESCRIPTIONS.items():
            pool.append((desc, "visual"))

        for label, desc in ESC50_DESCRIPTIONS.items():
            pool.append((desc, "auditory"))

        for label, desc in SPEECH_DESCRIPTIONS.items():
            pool.append((desc, "speech"))

        self._expanded_pool = pool

    def get_expanded_stimulus(self):
        """Get a stimulus from the expanded dataset (10x larger than original).

        Returns:
            (description_text, modality_str) — e.g. ("A tench — ...", "visual")
        """
        self._build_expanded_pool()
        return random.choice(self._expanded_pool)

    def get_all_descriptions(self):
        """Get all text descriptions from every source (for counting/embedding).

        Returns:
            list of (description, modality) tuples
        """
        self._build_expanded_pool()
        return list(self._expanded_pool)
