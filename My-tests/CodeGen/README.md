# Code Generation Allocation Tests

Each test validates stack-slot allocation comments in the generated `.moon` file.

Expected `alloc` sizes:
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
