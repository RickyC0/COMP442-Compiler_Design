# Code Generation Requirement Coverage Tests

Each `.src` file includes:
- full Assignment 5 rule legend (`1.1` ... `5.3`)
- per-test YES/NO matrix matching the polynomial example style

## Expected to Pass End-to-End (current backend)

- `cg_alloc_basic_types.src`
  - allocates basic scalar locals (`1.1`)
- `cg_alloc_basic_arrays.src`
  - allocates and indexes arrays of basic types (`1.2`, `4.1`)
- `cg_alloc_object.src`
  - allocates object locals (`1.3`)
- `cg_alloc_object_array.src`
  - allocates arrays of objects (`1.4`)
- `cg_ctrl_if_while_expr.src`
  - covers assignment/if/while/io/complex expression (`3.1`, `3.2`, `3.3`, `3.4`, `5.1`)
- `cg_io_terminal_read.src`
  - covers terminal input/output lowering for float values (`read`/`write`) without blocking CI (`3.4`)
- `cg_array_expr_index.src`
  - covers expression-based array indices (`4.1`, `5.2`)
- `cg_object_inheritance_alloc.src`
  - validates inherited object footprint allocation (`1.3`, `1.4`)
- `cg_object_array_members.src`
  - now passes with object-array member lowering (`4.2`, `4.4`, `5.2`, `5.3`)
- `cg_functions_return.src`
  - now passes with function call + parameter + return lowering (`2.1`, `2.2`, `2.3`)
- `cg_member_calls_and_fields.src`
  - now passes with member-call and receiver-field lowering (`2.4`, `4.3`, `5.3`)

## Current Gap Status

- No remaining codegen gaps in this test folder; all listed tests are passing semantic + codegen checks.

## Allocation Expectations

- `cg_alloc_basic_types.src`
  - `i` size = 4
  - `f` size = 4
- `cg_alloc_basic_arrays.src`
  - `nums[5]` size = 20
  - `weights[3]` size = 12
- `cg_alloc_object.src`
  - `Point` has `integer x` + `float y` => object size = 8
- `cg_alloc_object_array.src`
  - `Pixel` has 3 integer fields => object size = 12
  - `strip[4]` size = 48
- `cg_object_inheritance_alloc.src`
  - `Child` has inherited `Base.x` (4) + `y` (4) + `extra[2]` (8) => object size = 16
  - `items[3]` size = 48
