# Update Rules of TCFA Data Valuation on LLVM-IR

In our LLVM IR-based implementation, we model data valuation $\nu$ as a mapping from LLVM virtual registers ($\text{Reg}$) and memory ($\text{Addr}$) to the value space $\mathcal{D}$, i.e., $\nu: (\text{Reg} \cup \text{Addr}) \to \mathcal{D}$. Based on this, we view the location switch $(l, \nu, \mu) \xrightarrow{\sigma} (l', \nu', \mu')$ as an update on $\nu$'s value space induced by executing the LLVM instruction sequence $\sigma$.

Table 1 defines the update rules of TCFA data valuation $\nu$. Here, $\nu[k \mapsto val]$ represents an update operation that assigns the value $val$ to the key $k$ (either a register or a memory address).

---

## Semantic Mappings

To further formalize these rules, we define the semantic mapping $\llbracket \cdot \rrbracket$ for binary operators ($op$) and comparison predicates ($cond$) as follows.

### Binary Operators $\llbracket op \rrbracket$

| LLVM Instruction | Semantic Operator |
|:---:|:---:|
| `add` | $+$ |
| `fadd` | $+_f$ |
| `sub` | $-$ |
| `fsub` | $-_f$ |
| `mul` | $\times$ |
| `fmul` | $\times_f$ |
| `udiv` | $\div_u$ |
| `sdiv` | $\div_s$ |
| `fdiv` | $\div_f$ |
| `urem` | $\mathop{\%}_u$ |
| `srem` | $\mathop{\%}_s$ |
| `frem` | $\mathop{\%}_f$ |
| `and` | $\land$ |
| `or` | $\lor$ |
| `xor` | $\oplus$ |
| `shl` | $\ll$ |
| `lshr` | $\gg_l$ |
| `ashr` | $\gg_a$ |

### Comparison Predicates $\llbracket cond \rrbracket$

| LLVM Predicate | Semantic Operator |
|:---:|:---:|
| `eq` | $=$ |
| `ne` | $\neq$ |
| `ugt` | $>_u$ |
| `uge` | $\ge_u$ |
| `ult` | $<_u$ |
| `ule` | $\le_u$ |
| `sgt` | $>_s$ |
| `sge` | $\ge_s$ |
| `slt` | $<_s$ |
| `sle` | $\le_s$ |
| `oeq` | $=_o$ |
| `one` | $\neq_o$ |
| `ogt` | $>_o$ |
| `oge` | $\ge_o$ |
| `olt` | $<_o$ |
| `ole` | $\le_o$ |
| `ueq` | $=_u$ |
| `une` | $\neq_u$ |
| `true` | $\top$ |
| `false` | $\bot$ |
| `ord` | $\lambda xy.\ \neg(\text{NaN}_x \lor \text{NaN}_y)$ |
| `uno` | $\lambda xy.\ (\text{NaN}_x \lor \text{NaN}_y)$ |

---

## Type Casting Instructions

For the following LLVM IR type casting instructions, we use $\text{convert}(v, \tau)$ to denote the casting of value $v$ to type $\tau$:

```
conv ∈ { trunc, zext, sext, fptosi, fpext, uitofp, sitofp,
         fptoui, fptrunc, ptrtoint, inttoptr, bitcast }
```

---

## Instruction Semantics

We formalize the semantics for the remaining LLVM IR instruction categories as follows.

### Select

The $\text{ite}(c,\ t,\ f)$ expression evaluates to $t$ if the condition $c$ is true (non-zero), and $f$ otherwise.

### Memory Access

- **`load`** — The value is retrieved by dereferencing the pointer $p$, i.e., $\nu(\nu(p))$.
- **`store`** — The valuation is updated at the address key $\nu(p)$ with value $v$.

### Getelementptr

$\text{gep}(base,\ idx)$ computes and returns the memory address offset based on the base pointer and indices.

### Alloc

$\text{alloc}(\text{sizeof}(\tau))$ reserves a contiguous memory block of the size determined by type $\tau$ and returns its unique base address.

### Extract and Insert

- $\nu(a).i$ — Extracts the $i$-th member of aggregate $a$.
- $a[i \leftarrow v]$ — Creates a new aggregate with the $i$-th member updated to $v$.

---

> **Note:** Function calls (`call`) are handled as external invocations and are discussed separately in the [Implementation Section](#implementation).
