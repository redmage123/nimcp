#!/usr/bin/env python3
"""
JEPA Weight Converter - Convert V-JEPA PyTorch weights to NIMCP format

Usage:
    python jepa_weight_converter.py --input vjepa2_vitl.pth --output weights/vjepa2_vitl.nimcp

Supports:
    - V-JEPA 2 models (ViT-L, ViT-H, ViT-G)
    - I-JEPA models
    - Custom PyTorch state dictionaries

Output Format:
    Binary .nimcp format compatible with jepa_weights_load()
"""

import argparse
import struct
import sys
from pathlib import Path
from typing import Dict, Tuple, Optional, List
import numpy as np

try:
    import torch
    HAS_TORCH = True
except ImportError:
    HAS_TORCH = False
    print("Warning: PyTorch not available. Limited functionality.")

try:
    from safetensors import safe_open
    HAS_SAFETENSORS = True
except ImportError:
    HAS_SAFETENSORS = False

# Constants matching C header
JEPA_WEIGHTS_MAGIC = 0x54574A4E  # "NJWT"
JEPA_WEIGHTS_VERSION = 1
JEPA_WEIGHTS_HEADER_SIZE = 64
JEPA_WEIGHTS_MAX_NAME_LEN = 128
JEPA_WEIGHTS_MAX_DIMS = 8

# Model type enum (matching C)
class JEPAModelType:
    CUSTOM = 0
    VJEPA2_VITL = 1
    VJEPA2_VITH = 2
    VJEPA2_VITG = 3
    IJEPA_VITL = 4
    IJEPA_VITH = 5

# Weight dtype enum
class JEPAWeightDtype:
    F32 = 0
    F16 = 1
    BF16 = 2
    INT8 = 3


def compute_crc32(data: bytes) -> int:
    """Compute CRC32 checksum matching C implementation."""
    import zlib
    return zlib.crc32(data) & 0xFFFFFFFF


def detect_model_type(state_dict: Dict) -> Tuple[int, int, int, int]:
    """
    Detect model architecture from state dict.

    Returns:
        (model_type, latent_dim, hidden_dim, num_layers)
    """
    # Look for predictor-related keys
    predictor_keys = [k for k in state_dict.keys() if 'predictor' in k.lower()]

    # Default values
    model_type = JEPAModelType.CUSTOM
    latent_dim = 768  # ViT-L default
    hidden_dim = 3072
    num_layers = 2

    # Try to infer from weight shapes
    for key, tensor in state_dict.items():
        if 'predictor' in key.lower() and 'weight' in key.lower():
            shape = tuple(tensor.shape)
            if len(shape) == 2:
                # First layer: hidden_dim x input_dim
                # Last layer: output_dim x hidden_dim
                if shape[0] > shape[1]:
                    hidden_dim = shape[0]
                    latent_dim = shape[1]
                else:
                    latent_dim = shape[0]
                    hidden_dim = shape[1]

    # Detect known model architectures
    if latent_dim == 1024:
        model_type = JEPAModelType.VJEPA2_VITL
    elif latent_dim == 1280:
        model_type = JEPAModelType.VJEPA2_VITH
    elif latent_dim == 1408:
        model_type = JEPAModelType.VJEPA2_VITG

    return model_type, latent_dim, hidden_dim, num_layers


def load_pytorch_weights(path: str) -> Dict[str, np.ndarray]:
    """Load weights from PyTorch checkpoint."""
    if not HAS_TORCH:
        raise RuntimeError("PyTorch required for .pth files")

    checkpoint = torch.load(path, map_location='cpu')

    # Handle different checkpoint formats
    if isinstance(checkpoint, dict):
        if 'model' in checkpoint:
            state_dict = checkpoint['model']
        elif 'state_dict' in checkpoint:
            state_dict = checkpoint['state_dict']
        else:
            state_dict = checkpoint
    else:
        state_dict = checkpoint.state_dict()

    # Convert to numpy
    weights = {}
    for key, tensor in state_dict.items():
        if isinstance(tensor, torch.Tensor):
            weights[key] = tensor.detach().cpu().numpy().astype(np.float32)
        else:
            weights[key] = np.array(tensor, dtype=np.float32)

    return weights


def load_safetensors_weights(path: str) -> Dict[str, np.ndarray]:
    """Load weights from safetensors format."""
    if not HAS_SAFETENSORS:
        raise RuntimeError("safetensors library required")

    weights = {}
    with safe_open(path, framework="numpy") as f:
        for key in f.keys():
            weights[key] = f.get_tensor(key).astype(np.float32)

    return weights


def load_npz_weights(path: str) -> Dict[str, np.ndarray]:
    """Load weights from numpy npz format."""
    data = np.load(path)
    return {key: data[key].astype(np.float32) for key in data.files}


def filter_predictor_weights(weights: Dict[str, np.ndarray],
                             prefix_filter: Optional[str] = None) -> Dict[str, np.ndarray]:
    """Filter to only predictor-related weights."""
    filtered = {}

    for key, value in weights.items():
        # Skip non-predictor weights unless no filter specified
        if prefix_filter:
            if not key.startswith(prefix_filter):
                continue

        # Common predictor patterns
        keep = any(pattern in key.lower() for pattern in [
            'predictor', 'mlp', 'fc', 'linear', 'projection'
        ])

        if keep or not prefix_filter:
            # Normalize key names
            new_key = key
            if prefix_filter:
                new_key = key[len(prefix_filter):].lstrip('.')
            filtered[new_key] = value

    return filtered


def rename_weights_to_nimcp(weights: Dict[str, np.ndarray]) -> Dict[str, np.ndarray]:
    """Rename weight keys to match NIMCP naming convention."""
    renamed = {}

    # Build layer index mapping
    layer_idx = 0
    seen_layers = set()

    for key in sorted(weights.keys()):
        parts = key.split('.')

        # Try to extract layer number
        for i, part in enumerate(parts):
            if part.isdigit() and int(part) not in seen_layers:
                seen_layers.add(int(part))

        # Determine new name
        if 'weight' in key.lower():
            new_key = f"predictor.layer{layer_idx}.weight"
            if new_key in renamed:
                layer_idx += 1
                new_key = f"predictor.layer{layer_idx}.weight"
        elif 'bias' in key.lower():
            new_key = f"predictor.layer{layer_idx}.bias"
        else:
            new_key = key

        renamed[new_key] = weights[key]

    return renamed


def write_nimcp_header(f, num_tensors: int, total_params: int,
                       model_type: int, latent_dim: int,
                       hidden_dim: int, num_layers: int) -> None:
    """Write NIMCP weight file header."""
    # Pack header (64 bytes total)
    header = struct.pack(
        '<IIIQIIIII24s',  # Little-endian
        JEPA_WEIGHTS_MAGIC,      # magic (4 bytes)
        JEPA_WEIGHTS_VERSION,    # version (4 bytes)
        num_tensors,             # num_tensors (4 bytes)
        total_params,            # total_params (8 bytes)
        model_type,              # model_type (4 bytes)
        latent_dim,              # latent_dim (4 bytes)
        hidden_dim,              # hidden_dim (4 bytes)
        num_layers,              # num_layers (4 bytes)
        0,                       # checksum placeholder (4 bytes)
        b'\x00' * 24             # reserved (24 bytes)
    )

    assert len(header) == JEPA_WEIGHTS_HEADER_SIZE, f"Header size mismatch: {len(header)}"
    f.write(header)


def write_tensor(f, name: str, tensor: np.ndarray) -> int:
    """Write a single tensor to file. Returns bytes written."""
    bytes_written = 0

    # Ensure float32
    tensor = tensor.astype(np.float32)

    # Name length and name
    name_bytes = name.encode('utf-8')[:JEPA_WEIGHTS_MAX_NAME_LEN - 1]
    f.write(struct.pack('<H', len(name_bytes)))
    f.write(name_bytes)
    bytes_written += 2 + len(name_bytes)

    # Number of dimensions
    ndims = len(tensor.shape)
    assert ndims <= JEPA_WEIGHTS_MAX_DIMS, f"Too many dimensions: {ndims}"
    f.write(struct.pack('<B', ndims))
    bytes_written += 1

    # Dimensions
    for dim in tensor.shape:
        f.write(struct.pack('<I', dim))
        bytes_written += 4

    # Data type (F32)
    f.write(struct.pack('<B', JEPAWeightDtype.F32))
    bytes_written += 1

    # Data
    f.write(tensor.tobytes())
    bytes_written += tensor.nbytes

    return bytes_written


def convert_weights(input_path: str, output_path: str,
                   prefix_filter: Optional[str] = None,
                   verbose: bool = True) -> bool:
    """
    Convert weights from PyTorch/safetensors/npz to NIMCP format.

    Args:
        input_path: Path to input weight file
        output_path: Path to output .nimcp file
        prefix_filter: Optional prefix to filter weights
        verbose: Print progress information

    Returns:
        True on success
    """
    input_path = Path(input_path)
    output_path = Path(output_path)

    # Load weights based on format
    suffix = input_path.suffix.lower()

    if verbose:
        print(f"Loading weights from {input_path}...")

    if suffix in ['.pth', '.pt', '.bin']:
        weights = load_pytorch_weights(str(input_path))
    elif suffix == '.safetensors':
        weights = load_safetensors_weights(str(input_path))
    elif suffix == '.npz':
        weights = load_npz_weights(str(input_path))
    else:
        print(f"Error: Unsupported format {suffix}")
        return False

    if verbose:
        print(f"Loaded {len(weights)} tensors")

    # Filter and rename weights
    weights = filter_predictor_weights(weights, prefix_filter)
    weights = rename_weights_to_nimcp(weights)

    if verbose:
        print(f"Filtered to {len(weights)} predictor tensors")
        for name, tensor in weights.items():
            print(f"  {name}: {tensor.shape} ({tensor.dtype})")

    # Detect architecture
    model_type, latent_dim, hidden_dim, num_layers = detect_model_type(weights)

    if verbose:
        print(f"Detected architecture:")
        print(f"  Model type: {model_type}")
        print(f"  Latent dim: {latent_dim}")
        print(f"  Hidden dim: {hidden_dim}")
        print(f"  Num layers: {num_layers}")

    # Count total parameters
    total_params = sum(t.size for t in weights.values())

    # Write output file
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, 'wb') as f:
        # Write header
        write_nimcp_header(f, len(weights), total_params,
                          model_type, latent_dim, hidden_dim, num_layers)

        # Write tensors
        for name, tensor in weights.items():
            write_tensor(f, name, tensor)

    if verbose:
        file_size = output_path.stat().st_size
        print(f"\nWrote {output_path}")
        print(f"  Tensors: {len(weights)}")
        print(f"  Parameters: {total_params:,}")
        print(f"  File size: {file_size:,} bytes ({file_size / 1024 / 1024:.2f} MB)")

    return True


def inspect_nimcp_file(path: str) -> None:
    """Inspect a .nimcp weight file."""
    with open(path, 'rb') as f:
        # Read header
        header_data = f.read(JEPA_WEIGHTS_HEADER_SIZE)
        (magic, version, num_tensors, total_params,
         model_type, latent_dim, hidden_dim, num_layers,
         checksum) = struct.unpack('<IIIQIIIII', header_data[:40])

        print(f"NIMCP Weight File: {path}")
        print(f"  Magic: 0x{magic:08X} ({'OK' if magic == JEPA_WEIGHTS_MAGIC else 'INVALID'})")
        print(f"  Version: {version}")
        print(f"  Tensors: {num_tensors}")
        print(f"  Total params: {total_params:,}")
        print(f"  Model type: {model_type}")
        print(f"  Latent dim: {latent_dim}")
        print(f"  Hidden dim: {hidden_dim}")
        print(f"  Num layers: {num_layers}")
        print(f"\nTensors:")

        # Read tensors
        for i in range(num_tensors):
            name_len = struct.unpack('<H', f.read(2))[0]
            name = f.read(name_len).decode('utf-8')
            ndims = struct.unpack('<B', f.read(1))[0]
            dims = [struct.unpack('<I', f.read(4))[0] for _ in range(ndims)]
            dtype = struct.unpack('<B', f.read(1))[0]

            num_elements = 1
            for d in dims:
                num_elements *= d

            dtype_names = ['f32', 'f16', 'bf16', 'int8']
            dtype_sizes = [4, 2, 2, 1]
            data_size = num_elements * dtype_sizes[dtype]

            print(f"  [{i}] {name}: {dims} ({dtype_names[dtype]}, {num_elements:,} elements)")

            # Skip data
            f.seek(data_size, 1)


def main():
    parser = argparse.ArgumentParser(
        description='Convert V-JEPA weights to NIMCP format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert V-JEPA 2 checkpoint
  python jepa_weight_converter.py -i vjepa2_vitl.pth -o weights/vjepa2_vitl.nimcp

  # Convert with prefix filter
  python jepa_weight_converter.py -i model.pth -o out.nimcp --prefix predictor

  # Inspect existing file
  python jepa_weight_converter.py --inspect weights/vjepa2_vitl.nimcp
        """
    )

    parser.add_argument('-i', '--input', type=str,
                       help='Input weight file (.pth, .safetensors, .npz)')
    parser.add_argument('-o', '--output', type=str,
                       help='Output .nimcp file')
    parser.add_argument('--prefix', type=str, default=None,
                       help='Filter weights by prefix (e.g., "predictor")')
    parser.add_argument('--inspect', type=str,
                       help='Inspect existing .nimcp file')
    parser.add_argument('-v', '--verbose', action='store_true', default=True,
                       help='Verbose output')
    parser.add_argument('-q', '--quiet', action='store_true',
                       help='Quiet mode')

    args = parser.parse_args()

    if args.quiet:
        args.verbose = False

    if args.inspect:
        inspect_nimcp_file(args.inspect)
        return 0

    if not args.input or not args.output:
        parser.print_help()
        print("\nError: --input and --output required for conversion")
        return 1

    success = convert_weights(args.input, args.output,
                             args.prefix, args.verbose)

    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
