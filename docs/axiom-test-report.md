# ILP_FOR Axiom Test Report

Sample tests demonstrating how [Axiom](https://github.com/mattyv/axiom) validates claims about ILP_FOR.

## Overview

ILP_FOR has **935+ axioms** extracted from the codebase, covering:
- Macro preconditions and postconditions
- Control flow semantics (ILP_BREAK, ILP_CONTINUE, ILP_RETURN)
- Type constraints (SBO buffer, trivially destructible)
- Pairing requirements (ILP_FOR/ILP_END, ILP_RETURN/ILP_END_RETURN)

---

## Claim Validation

### Pairing Requirements

| Claim | Valid | Confidence | Grounding |
|-------|-------|------------|-----------|
| "ILP_FOR must be closed with ILP_END" | True | 0.50 | `ilp_end_macro_precondition_context_matching` |
| "ILP_RETURN requires ILP_END_RETURN not ILP_END" | True | 0.72 | `ILP_END_RETURN_macro_statement_context` |

**Proof Chain Example:**
```
Claim: ILP_FOR must be closed with ILP_END

1. ilp_end_macro_precondition_context_matching_fc8e2a91
   "ILP_END must be used in a context where it syntactically matches
    a preceding ILP_BEGIN or similar construct"

2. ilp_end_macro_anti_pattern_unmatched_context_b7e9f1c4
   "Using ILP_END without a corresponding ILP_BEGIN will result in
    compilation errors due to unbalanced braces"
```

### Type Constraints

| Claim | Valid | Confidence | Grounding |
|-------|-------|------------|-----------|
| "ILP_FOR_T should be used when return type is larger than 8 bytes" | True | 0.50 | `ilp_ctrl_set_constraint_size_sizeof` |

**Proof Chain:**
```
1. ilp_ctrl_set_constraint_size_sizeof_a1b2c3d4
   "The size of the decayed type U must not exceed arch::sbo_size bytes"
```

---

## Axiom Search Examples

### Macro Expansion

**Query:** `ILP_FOR macro precondition`

```
### ilp_for_macro_postcond_range_for_expansion_u1v2w3x4
**Function**: `ILP_FOR`
**Signature**: `ILP_FOR(loop_var_decl, start, end, N)`
**Content**: Macro expands to a range-based for loop:
            for (loop_var_decl : ::ilp::iota((start), (end)))
**Depends on**: 4 axiom(s)
  - for_loop_range_typed_impl_effect_forwards_range_p5q6r7s8
  - ilp_for_range_t_range_single_eval
  - for_loop_range_impl_precond_range_valid_b3c4d5e6
```

### Control Flow

**Query:** `ILP_BREAK ILP_CONTINUE control flow`

```
### ilp_continue_postcondition_no_subsequent_execution
**Function**: `ILP_CONTINUE`
**Content**: After ILP_CONTINUE is invoked, no code following it
             in the same function scope will execute
**Formal**: after(ILP_CONTINUE) → ¬executes(subsequent_statements_in_scope)

### ilp_break.anti_pattern.nested_context
**Function**: `ILP_BREAK`
**Content**: ILP_BREAK only exits the innermost loop/switch;
             to exit outer loops, different mechanisms are needed
**Formal**: nested_depth(loop) > 1 ⟹ ILP_BREAK exits only innermost
```

### Performance

**Query:** `ILP_CONTINUE runtime overhead`

```
### ilp_continue_complexity_zero_runtime_cost
**Function**: `ILP_CONTINUE`
**Content**: ILP_CONTINUE has zero runtime overhead; the do-while(0)
             wrapper is eliminated by any optimizer
**Formal**: runtime_cost(ILP_CONTINUE) = O(1) ∧ optimized_to(return)
```

---

## Direct Axiom Lookup

**Query:** `get_axiom ilp_for_macro_postcond_range_for_expansion_u1v2w3x4`

```
## Axiom: ilp_for_macro_postcond_range_for_expansion_u1v2w3x4

**Layer**: library
**Function**: `ILP_FOR`
**Header**: ilp_for/detail/macros_simple.hpp

### Content
Macro expands to a range-based for loop:
for (loop_var_decl : ::ilp::iota((start), (end)))

### Formal Specification
expansion(ILP_FOR(loop_var_decl, start, end, N)) ==
  'for (loop_var_decl : ::ilp::iota((start), (end)))'

### Dependencies
This axiom depends on 4 other axiom(s):
- ilp_for_range_t_range_single_eval (ILP_FOR_RANGE_T)
- for_loop_range_typed_auto_precond_range_valid_a1b2c3d4
- for_loop_range_impl_precond_range_valid_b3c4d5e6
- for_loop_range_typed_impl_effect_forwards_range_p5q6r7s8
```

---

## Function Pairings

ILP_FOR macros have explicit pairings loaded into the knowledge graph:

| Opener | Closer | Required |
|--------|--------|----------|
| `ILP_FOR` | `ILP_END` | Yes |
| `ILP_FOR` | `ILP_END_RETURN` | Yes |
| `ILP_FOR_AUTO` | `ILP_END` | Yes |
| `ILP_FOR_AUTO` | `ILP_END_RETURN` | Yes |
| `ILP_FOR_RANGE` | `ILP_END` | Yes |
| `ILP_FOR_RANGE` | `ILP_END_RETURN` | Yes |
| `ILP_FOR_T` | `ILP_END` | Yes |
| `ILP_FOR_T` | `ILP_END_RETURN` | Yes |
| `ILP_FOR_RANGE_T` | `ILP_END` | Yes |
| `ILP_FOR_RANGE_T` | `ILP_END_RETURN` | Yes |
| `ILP_FOR_T_AUTO` | `ILP_END` | Yes |
| `ILP_FOR_T_AUTO` | `ILP_END_RETURN` | Yes |
| `ILP_FOR_RANGE_T_AUTO` | `ILP_END` | Yes |
| `ILP_FOR_RANGE_T_AUTO` | `ILP_END_RETURN` | Yes |

When searching for any ILP_FOR variant, paired closers are automatically included in results.

---

## Grounding to C++20 Foundations

ILP_FOR axioms link to C++20 language foundations via `depends_on`:

```
ILP_FOR (library)
  └── for_loop_range_typed_impl (library)
        └── cpp20_dcl_init_ref_reference_related_def (cpp20_language)
              └── cpp20_dcl_init_ref_not_direct_bind_definition (cpp20_language)
```

This means claims about ILP_FOR can be validated against formal C++ semantics.

---

## Using with Claude Code

With the Axiom MCP server connected, Claude Code can:

1. **Validate usage patterns:**
   ```
   User: "Can I use ILP_FOR without ILP_END?"
   Claude: No - ILP_FOR must be closed with ILP_END or ILP_END_RETURN.
           [Proof: ilp_end_macro_precondition_context_matching]
   ```

2. **Catch incorrect closers:**
   ```
   User: "I used ILP_RETURN but closed with ILP_END"
   Claude: When using ILP_RETURN, you must close with ILP_END_RETURN.
           [Proof: ILP_END_RETURN_macro_statement_context]
   ```

3. **Suggest correct patterns:**
   ```
   User: "My return type is 24 bytes, ILP_FOR won't compile"
   Claude: Use ILP_FOR_T with explicit type parameter for types > 8 bytes.
           [Proof: ilp_ctrl_set_constraint_size_sizeof]
   ```

---

## Summary

| Capability | Status |
|------------|--------|
| Macro pairing validation | ✅ Working |
| Control flow semantics | ✅ Working |
| Type constraint checking | ✅ Working |
| Grounding to C++20 | ✅ Working |
| Proof chain generation | ✅ Working |
