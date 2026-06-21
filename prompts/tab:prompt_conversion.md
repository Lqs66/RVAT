# Prompt Template for Property Extraction

## Overview
You are an expert in Temporal Logic (TL) with years of experience in formal verification and testing. For each extracted piece of information, provide the corresponding UPPAAL-TCTL formula that represents it.

---

## Policy Constraint Preservation
LLMs may omit some policy constraints during extraction, which can lead to over-simplified policy descriptions and formulas. To avoid this issue, you must preserve all policy constraints expressed in the given text segment when deriving the UPPAAL-TCTL formula.

These policy constraints include:
1. **Configuration constraints**: constraints on system states, modes, parameters, thresholds, or enabling conditions.
2. **Behavioral constraints**: required or forbidden system behaviors.
3. **Timing constraints**: deadlines, durations, time bounds, or ordering requirements.

Do not abstract, weaken, or omit these constraints. If the text contains multiple independent policies, extract them separately and generate one formula for each policy.

---

## UPPAAL-TCTL Syntax

```
formula := A[] expr | E[] expr | A<> expr | E<> expr | expr → expr
```

## UPPAAL-TCTL Semantics

1. **UPPAAL strictly prohibits nesting of temporal operators** (A[], A<>, E[], E<>, →). As a hard rule, `expr` must be a basic expression and must not contain any temporal operators. When generating formulas, enforce this restriction without exception.

2. **Possibly** (E<> p): There exists a path where expr p eventually holds.

3. **Always** (A[] p): expr p holds in all states along all paths.

4. **Potentially Always** (E[] p): There exists a path where expr p holds in all states.

5. **Eventually** (A<> p): Along all paths, expr p eventually holds.

6. **Leads to** (p → q): Whenever expr p holds, expr q will eventually hold along all paths.

---

## Response Format

Use this JSON schema to reply:

```json
{'formula': str, 'explanation': str}
```

**Field Explanation:**
- **"formula"**: UPPAAL-TCTL formula.
- **"explanation"**: Brief reasoning for the derived formula, explaining the choice of temporal operators and time constraints based on the specification.

**Here is the text to analyze**: `{specification}`
