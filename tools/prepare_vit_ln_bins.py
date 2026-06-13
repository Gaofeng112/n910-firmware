#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

import numpy as np


def radix_from_name(path: Path) -> str:
    match = re.search(r"_radix_(-?\d+)\.npy$", path.name)
    return match.group(1) if match else "unknown"


def npy_to_le_i16(path: Path) -> bytes:
    arr = np.load(path)
    rounded = np.rint(arr)
    if not np.allclose(arr, rounded):
        raise ValueError(f"{path} contains non-integer values")
    if rounded.min() < -32768 or rounded.max() > 32767:
        raise ValueError(f"{path} is out of int16 range")
    return rounded.astype("<i2", copy=False).tobytes()


def write_bin(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(npy_to_le_i16(src))
    arr = np.load(src)
    print(f"{src} -> {dst}")
    print(f"  shape={arr.shape} radix={radix_from_name(src)} bytes={dst.stat().st_size}")


def main() -> None:
    repo = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="Prepare first ViT LN input/golden raw bins.")
    parser.add_argument(
        "--input-npy",
        type=Path,
        default=repo / "cases/int_res/layer_node_add_img_ILSVRC2012_val_00000014_radix_9.npy",
    )
    parser.add_argument(
        "--golden-npy",
        type=Path,
        default=repo / "cases/int_res/layer_node_layer_norm_img_ILSVRC2012_val_00000014_radix_12.npy",
    )
    parser.add_argument(
        "--case-dir",
        type=Path,
        default=repo / "cases/vit_ln/case_ncf",
    )
    args = parser.parse_args()

    input_arr = np.load(args.input_npy)
    golden_arr = np.load(args.golden_npy)
    if input_arr.shape != golden_arr.shape:
        raise ValueError(f"shape mismatch: input={input_arr.shape}, golden={golden_arr.shape}")

    write_bin(args.input_npy, args.case_dir / "input.bin")
    write_bin(args.golden_npy, args.case_dir / "golden.bin")


if __name__ == "__main__":
    main()
