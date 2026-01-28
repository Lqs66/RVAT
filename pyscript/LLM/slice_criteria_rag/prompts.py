SC_GEN="""
You are an expert in UPPAAL model checking, multi-copter flight control and program slicing techniques.
Your task is to analyze a UPPAAL property formula and multiple source code snippet groups (each with code, start/end line numbers, and source file path) to identify slicing criteria corresponding to the formula variables in the formula. The slicing criteria should include the variable or statement, its line number, and the source file path. For each code snippet group, only process it if it is not explicitly unrelated to multi-copter systems.

Definition:
- A slicing criterion is defined as a pair (s, V), where:
  - s is a specific program statement (line number in source code).
  - V is a set of variables of interest at that program statement (no duplicates).

Input:
- UPPAAL Property Formula: {formula}
- Source Code Snippet Groups: {code_snippet_list}

Steps to follow:
1. Parse the UPPAAL formula: Identify all key terms, variables, and expressions.
2. For each code snippet group: Analyze the code between the start and end lines in the given file path. Identify variables, statements, or expressions that semantically match or relate to the formula terms, leveraging your expertise in multi-copter flight control. Consider:
   - Variable name similarity (e.g., if formula has 'x', look for 'x' or similar in code).
   - Contextual relevance (e.g., assignments, conditions, or function calls affecting or representing formula terms).
   - Ignore irrelevant code; focus on potential slicing points for static or dynamic slicing.
   - Use file path context if it aids semantic matching (e.g., module-specific logic).
3. Map correspondences: For each formula variable, identify the closest matching slicing criterion (s, V), where:
   - s = the line number of the related code statement.
   - V = the variable(s) from the code that correspond to the formula variable.
4. If no direct match, infer based on code semantics, considering the context of the formula variable.

Output:
Return a JSON object with the following structure:
{{
  "slicing_criteria": [
    {{
      "formula_term": "[string: term from formula]",
      "slicing_criterion": {{
        "program_statement": [integer: line number in code],
        "variables": ["[string: variable name(s)]"],
        "source_file": "[string: file path]"
      }},
      "explanation": "[string: brief reason for the match]"
    }}
  ],
  "notes": "[string: explanation if no matches are found, otherwise empty]"
}}
"""