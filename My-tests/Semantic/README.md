# Semantic Test Suite

This folder contains focused semantic-analysis tests.

## How to read expected outcomes

- `PASS`: should produce no semantic errors.
- `FAIL`: should fail with the current semantic checks.

## Test Cases

1. `sem_pass_basic_valid.src`
   - Purpose: baseline valid program with arithmetic, assignment, and control flow.
   - Expected: `PASS`.

2. `sem_pass_method_call_and_member.src`
   - Purpose: valid member assignment and valid class method call.
   - Expected: `PASS`.

3. `sem_pass_int_to_float_promotion.src`
   - Purpose: verifies allowed numeric promotion (`integer` -> `float`) in assignment and args.
   - Expected: `PASS`.

4. `sem_fail_undeclared_identifier.src`
   - Purpose: uses an undeclared variable.
   - Expected: `FAIL`.

5. `sem_fail_undeclared_function.src`
   - Purpose: calls a free function that does not exist.
   - Expected: `FAIL`.

6. `sem_fail_undeclared_method.src`
   - Purpose: calls an undefined method on an object.
   - Expected: `FAIL`.

7. `sem_fail_missing_member.src`
   - Purpose: accesses a non-existent data member.
   - Expected: `FAIL`.

8. `sem_fail_assignment_type_mismatch.src`
   - Purpose: assigns `float` to `integer` and object to scalar.
   - Expected: `FAIL`.

9. `sem_fail_binary_non_numeric.src`
   - Purpose: uses non-numeric operand in a binary arithmetic expression.
   - Expected: `FAIL`.

10. `sem_fail_arg_count_mismatch.src`
    - Purpose: calls function with wrong number of arguments.
    - Expected: `FAIL`.

11. `sem_fail_arg_type_mismatch.src`
    - Purpose: calls function with wrong argument type.
    - Expected: `FAIL`.

12. `sem_fail_if_condition_object.src`
    - Purpose: uses object type in `if` condition.
    - Expected: `FAIL`.

13. `sem_fail_while_condition_object.src`
    - Purpose: uses object type in `while` condition.
    - Expected: `FAIL`.

14. `sem_fail_redefinition_same_scope.src`
    - Purpose: redeclares same symbol in same scope.
    - Expected: `FAIL`.

15. `sem_fail_return_type_mismatch.src`
    - Purpose: return expression type mismatches declared return type.
    - Expected: `FAIL`.

16. `sem_fail_undeclared_member_function_definition.src`
   - Purpose: class method implemented without prior class declaration (`6.1`).
   - Expected: `FAIL`.

17. `sem_fail_undefined_member_function_declaration.src`
   - Purpose: class method declared but never implemented (`6.2`).
   - Expected: `FAIL`.

18. `sem_warn_shadowed_inherited_member.src`
   - Purpose: child class field shadows inherited parent field (`8.5`).
   - Expected: `WARNING`.

19. `sem_warn_local_shadows_member.src`
   - Purpose: local variable in member function shadows class field (`8.6`).
   - Expected: `WARNING`.

20. `sem_warn_overloaded_free_function.src`
   - Purpose: free function overload declarations/signatures (`9.1`).
   - Expected: `WARNING`.

21. `sem_warn_overloaded_member_function.src`
   - Purpose: member function overload declarations/signatures (`9.2`).
   - Expected: `WARNING`.

22. `sem_warn_overridden_member_function.src`
   - Purpose: derived class method overrides base method (`9.3`).
   - Expected: `WARNING`.

23. `sem_fail_array_index_not_integer.src`
   - Purpose: array index expression has non-integer type (`13.2`).
   - Expected: `FAIL`.

24. `sem_fail_dot_on_non_class.src`
   - Purpose: dot operator used on non-class value (`15.1`).
   - Expected: `FAIL`.

25. `sem_fail_undeclared_class_type.src`
   - Purpose: variable declared with undeclared class type (`11.5`).
   - Expected: `FAIL`.

26. `sem_fail_circular_class_dependency.src`
   - Purpose: circular class inheritance dependency (`14.1`).
   - Expected: `FAIL`.

27. `sem_fail_multiply_declared_free_function.src`
   - Purpose: same free function signature defined more than once (`8.2`).
   - Expected: `FAIL`.

28. `sem_pass_array_decl_access_update.src`
   - Purpose: valid array declaration with explicit size plus array access/update through an unsized array parameter.
   - Expected: `PASS`.

29. `sem_fail_array_decl_access_update_bounds.src`
   - Purpose: declared-size array triggers semantic bounds filters (negative constant index, oversize/negative size literals passed to unsized array parameter + size pair).
   - Expected: `FAIL`.

## Additional Semantic Filters (Declared-size Arrays)

- When an array has an explicit declared size (for example `integer data[4]`), semantic analysis applies additional constant checks:
- constant negative index usage is rejected.
- constant out-of-range index usage is rejected.
- for calls shaped like `fn(arr[], size)`, constant size arguments are validated against the declared first dimension of the actual array argument.

