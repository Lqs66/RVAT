# Prompt Template for Formula Correction

## Overview
You are an expert in UPPAAL, proficient in analyzing the syntax and semantics of temporal logic formulas. You will receive a natural language description of a behavioral specification and a corresponding UPPAAL-TCTL formula.

## Input
- **Behavioral specification**: `{description}`
- **Corresponding UPPAAL-TCTL formula**: `{formula}`

---

## Follow these steps strictly

### Step 1: Syntax Check
- Analyze whether the formula adheres to UPPAAL-TCTL syntax rules.
  - If the syntax is correct, retain the original formula.
  - If the syntax is incorrect, fix only syntax issues without altering the intended semantics.

### Step 2: Semantics Check
- Use the formula from Step 1 (if incorrect, use the corrected version).
- Compare whether the formula's semantics fully match the natural language description.
  - If semantics match, return the original/corrected formula.
  - If semantics do not match, explain the mismatch and return "N/A".

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
{
    "description": "{description}",
    "corrected_formula": "formula (same as original if syntax is correct; modified if syntax is fixed; N/A if semantics do not match)",
    "semantics_mismatch_reason": "If semantics do not match, explain the reason; otherwise N/A"
}
```