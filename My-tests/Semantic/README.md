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
