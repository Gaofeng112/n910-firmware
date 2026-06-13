# LayerNorm host simulator

This folder is for PC-side C simulation of the fused ViT LayerNorm CPU op.

Current mode: scheme A, firmware-like format1 data path.

Flow:

```text
input.bin normal int16 LE
weight.bin LN params
-> convert input normal to format1
-> run LayerNorm in C with format1 input/output
-> convert output format1 to normal
-> result_host.bin normal int16 LE
-> compare with golden.bin
```

Default case directory:

```text
cases/vit_ln_v3/case_ncf
```

Direct run with WSL conda `gf` from repo root:

```powershell
wsl -e bash host_sim/layernorm/run_ln_host_sim.sh
```

Run another case:

```powershell
wsl -e bash host_sim/layernorm/run_ln_host_sim.sh cases/vit_ln_v3/case_ncf
```

Build examples:

```powershell
gcc -O2 host_sim/layernorm/ln_host_sim.c -o host_sim/layernorm/ln_host_sim.exe
```

WSL conda `gf` example:

```powershell
wsl -e bash -lc "cd /mnt/d/0_Project/N910/firmware && conda run -n gf gcc -O2 host_sim/layernorm/ln_host_sim.c -o host_sim/layernorm/ln_host_sim"
```

Run from repo root:

```powershell
host_sim/layernorm/ln_host_sim.exe cases/vit_ln_v3/case_ncf
```

WSL conda `gf` run:

```powershell
wsl -e bash -lc "cd /mnt/d/0_Project/N910/firmware && conda run -n gf ./host_sim/layernorm/ln_host_sim cases/vit_ln_v3/case_ncf"
```

The simulator parses:

```text
opflow.bin: input/output node shape
weight.bin: axis, stash_type, epsilon, x_radix, y_radix, scale, bias
input.bin: normal contiguous int16 little-endian
golden.bin: normal contiguous int16 little-endian
```

Before running LN, it checks:

```text
input/output format must be 1
input/output h,w,c must match
axis must be 2
scale_len and bias_len must equal channel
radix arrays must cover channel or contain one shared radix
input.bin and golden.bin sizes must equal h*w*c*2
output raw address - input raw address must equal format1 byte size
```

It prints exact mismatch count, mismatch count with absolute difference greater than 1, and max absolute difference.
It also prints common error metrics:

```text
mae: mean absolute error
mse: mean squared error
rmse: root mean squared error
exact_rate: percentage of exactly equal elements
tol1_rate: percentage of elements with abs(diff) <= 1
```

It also writes all mismatched elements to:

```text
cases/vit_ln_v3/case_ncf/diff_report.csv
```

CSV columns:

```text
idx,h,w,c,out,gold,diff,abs_diff
```

Result meaning:

```text
PASS_EXACT: output equals golden exactly.
PASS_TOL1: output differs from golden only by +/-1.
FAIL: at least one output differs from golden by more than 1.
```
