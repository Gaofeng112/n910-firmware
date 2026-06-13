#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CASE_DIR="${1:-cases/vit_ln_v3/case_ncf}"
CONDA_ENV="${CONDA_ENV:-gf}"
BIN="$SCRIPT_DIR/ln_host_sim"

cd "$REPO_ROOT"

echo "[BUILD] conda=$CONDA_ENV"
conda run -n "$CONDA_ENV" gcc -O2 host_sim/layernorm/ln_host_sim.c -o "$BIN"

echo "[RUN] case=$CASE_DIR"
conda run -n "$CONDA_ENV" "$BIN" "$CASE_DIR"
