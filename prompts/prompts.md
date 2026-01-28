Mapping Between TCTL and UPPAAL Formulas

We employ UPPAAL as our property verifier. Consequently, when describing the prompt template, we utilize the syntax and semantics of the TCTL logic supported by UPPAAL. For the correspondence between UPPAAL and the standard definition, refer to the table below.

| **TCTL Formula** | **UPPAAL Formula** | **Description** |
|------------------|-------------------|-----------------|
| $\forall \Box \varphi$ | A[] $\varphi$ | $\varphi$ should be true in all reachable states. |
| $\exists \Box \varphi$ | E[] $\varphi$ | There should exist a maximal path for which $\varphi$ is always true. |
| $\forall \Diamond \varphi$ | A<> $\varphi$ | For all paths, $\varphi$ should be eventually true. |
| $\exists \Diamond \varphi$ | E<> $\varphi$ | There should exist at least one path for which $\varphi$ is eventually true. |
| $\forall \Box (\varphi_1 \to \forall \Diamond \varphi_2)$ | $\varphi_1$ --> $\varphi_2$ | For all reachable states, whenever $\varphi_1$ is true, then eventually $\varphi_2$ will be true. |

## Prompt Templates

We use three prompt templates in our approach:

1. **[Prompt Template for Property Extraction](tab%3Aprompt_conversion.md)**: Converts natural language specifications into UPPAAL-TCTL formulas.

2. **[Prompt Template for RAG](tab%3Aprompt_rag.md)**: Analyzes UPPAAL-TCTL formulas and source code snippets to identify slicing criteria.

3. **[Prompt Template for Formula Correction](tab%3Aprompt_refine.md)**: Validates and corrects UPPAAL-TCTL formulas against behavioral specifications.
