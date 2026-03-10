# Update Rules of TCFA Data Valuation on LLVM-IR

In our LLVM IR-based implementation, we model data valuation $\nu$ as a mapping from LLVM virtual registers ($\text{Reg}$) and memory ($\text{Addr}$) to the value space $\mathcal{D}$, i.e., $\nu: (\text{Reg} \cup \text{Addr}) \to \mathcal{D}$. Based on this, we view the location switch $(l, \nu, \mu) \xrightarrow{\sigma} (l', \nu', \mu')$ as an update on $\nu$'s value space induced by executing the LLVM instruction sequence $\sigma$.

Table 1 defines the update rules of TCFA data valuation $\nu$. Here, $\nu[k \mapsto val]$ represents an update operation that assigns the value $val$ to the key $k$ (either a register or a memory address).

## Semantic Mappings

To further formalize these rules, we define the semantic mapping $\llbracket \cdot \rrbracket$ for binary operators ($op$) and comparison predicates ($cond$) as follows:

**Binary Operators** $\llbracket op \rrbracket$:

$$
\llbracket op \rrbracket \in \{\ \texttt{add}\to+,\ \texttt{fadd}\to+_f,\ \texttt{sub}\to-,\ \texttt{fsub}\to-_f,\ \texttt{mul}\to\times,\ \texttt{fmul}\to\times_f,
$$
$$
\texttt{udiv}\to\div_u,\ \texttt{sdiv}\to\div_s,\ \texttt{fdiv}\to\div_f,\ \texttt{urem}\to\%_u,\ \texttt{srem}\to\%_s,\ \texttt{frem}\to\%_f,
$$
$$
\texttt{and}\to\land,\ \texttt{or}\to\lor,\ \texttt{xor}\to\oplus,\ \texttt{shl}\to\ll,\ \texttt{lshr}\to\gg_l,\ \texttt{ashr}\to\gg_a\ \}
$$

**Comparison Predicates** $\llbracket cond \rrbracket$:

$$
\llbracket cond \rrbracket \in \{\ \texttt{eq}\to=,\ \texttt{ne}\to\neq,\ \texttt{ugt}\to>_u,\ \texttt{uge}\to\ge_u,\ \texttt{ult}\to<_u,\ \texttt{ule}\to\le_u,
$$
$$
\texttt{sgt}\to>_s,\ \texttt{sge}\to\ge_s,\ \texttt{slt}\to<_s,\ \texttt{sle}\to\le_s,\ \texttt{oeq}\to=_o,\ \texttt{one}\to\neq_o,
$$
$$
\texttt{ogt}\to>_o,\ \texttt{oge}\to\ge_o,\ \texttt{olt}\to<_o,\ \texttt{ole}\to\le_o,\ \texttt{ueq}\to=_u,\ \texttt{une}\to\neq_u,
$$
$$
\texttt{true}\to\top,\ \texttt{false}\to\bot,\ \texttt{ord}\to \lambda xy.\neg(\text{NaN}_x \lor \text{NaN}_y),\ \texttt{uno}\to \lambda xy.(\text{NaN}_x \lor \text{NaN}_y)\ \}
$$

## Type Casting Instructions

For the following LLVM IR type casting instructions, we use $\text{convert}(v, \tau)$ to denote the casting of value $val$ to type $\tau$:

$$
conv \in \{\ \texttt{trunc, zext, sext, fptosi, fpext, uitofp, sitofp, fptoui, fptrunc, ptrtoint, inttoptr, bitcast}\ \}
$$

## Instruction Semantics

We formalize the semantics for the remaining LLVM IR instruction categories as follows:

- **Select:** The $\text{ite}(c, t, f)$ evaluates to $t$ if the condition $c$ is true (non-zero), and $f$ otherwise.

- **Memory Access:** For `load`, the value is retrieved by dereferencing the pointer $p$ (i.e., $\nu(\nu(p))$). For `store`, the valuation is updated at the address key $\nu(p)$ with value $v$.

- **Getelementptr:** $\text{gep}(base, idx)$ computes and returns the memory address offset based on the base pointer and indices.

- **Alloc:** $\text{alloc}(\text{sizeof}(\tau))$ reserves a contiguous memory block of the size determined by type $\tau$ and returns its unique base address.

- **Extract and Insert:** $\nu(a).i$ represents extracting the $i$-th member of aggregate $a$, and $a[i \leftarrow v]$ represents creating a new aggregate with the $i$-th member updated.

> **Note:** Function calls (`call`) are handled as external invocations and are discussed separately in the Implementation Section.
