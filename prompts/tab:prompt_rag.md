# Prompt Template for RAG

## Overview
You are an expert in model checking and program slicing.

Your task is to analyze a UPPAAL-TCTL formula and multiple source code snippet groups (each with code, start/end line numbers, and source file path) to identify slicing criteria corresponding to the formula.

## Input
- **UPPAAL-TCTL formula**: `{formula}`
- **Source Code Snippet Groups**: `{code_snippet_list}`

## Analysis Steps

1. Parse the TCTL formula and extract all key variables/expressions.

2. For each snippet group, scan the given lines in the specified file, focusing on multi-copter flight control semantics. Match formula terms by variable name similarity and semantic relevance.

3. Map correspondences: For each formula, identify the closest matching slicing criterion $(stmt, Vars)$, where:
   - $stmt$ is the line number of the related code statement.
   - $Vars$ is the variables from the code that correspond to the formula.

4. If no exact match, infer the closest semantic equivalent.

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
    "slicing_criteria": [
        {
            "formula_term": "[string: term from formula]",
            "slicing_criterion": {
                "program_statement": [integer: line number in code],
                "variables": ["[string: variable name(s)]"],
                "source_file": "[string: file path]"
            },
            "explanation": "[string: brief reason for the match]"
        }
    ]
}
```