# Compiler Design Document

## 1. Introduction and Architectural Overview

This project implements a custom object-oriented compiler in C++. The compilation flow is organized as:

$Lexical\ Analysis \rightarrow Syntax\ Analysis \rightarrow AST\ Construction \rightarrow Semantic\ Analysis \rightarrow Code\ Generation$

At a high level:

- Lexical analysis converts source text into tokens and lexical diagnostics.
- Syntax analysis validates token sequences with a predictive recursive-descent parser.
- AST construction produces a structured intermediate representation during parsing.
- Semantic analysis performs scoped name resolution, type checking, and language-rule enforcement.

The source language is **object-oriented** and **statically typed**, and requires an explicit entry point named **main**.

## 2. Syntactic Analysis (Parsing)

### Methodology

The parser is a **predictive recursive-descent LL(1)** parser using one-token lookahead. The implementation selects productions by inspecting the current lookahead token and advancing with strict terminal matching. Syntax errors are handled with panic-mode recovery using synchronization sets.

Key implementation traits:

- Single lookahead token (`_lookaheadToken`) drives production choice.
- Production application is recorded in derivation output (`_derivationSteps`).
- Errors trigger synchronization through `_skipUntil(...)` at higher-level non-terminals.
- Comment tokens are skipped during token consumption.

### Grammar Handling

To support LL(1) predictive parsing, the grammar was normalized using left-recursion removal and left-factoring style decomposition into **Tail** and **Rest** non-terminals.

Examples from the parser design:

- Expression decomposition: `ExprTail`, `AddOpTail`, `MultOpTail`.
- Identifier disambiguation: `FactorIdTail`, `FactorRest`, `FactorCallTail`.
- Statement disambiguation after identifiers: `StatementIdTail`, `StatementRest`, `StatementCallTail`.
- Member declaration disambiguation: `MemberDeclIdTail`, `MemberDeclTypeTail`.
- Nullable and list constructs use explicit `EPSILON` branches.

## 3. Abstract Syntax Tree (AST) and Visualization

### Node Architecture

The AST is built from a base `ASTNode` class and concrete node subclasses. The base class tracks source line numbers and supports binary links (`left`, `right`) for shared structural conventions.

Representative concrete node types include:

- Expressions: `BinaryOpNode`, `UnaryOpNode`, `FuncCallNode`, `DataMemberNode`, literals, identifiers.
- Statements: `AssignStmtNode`, `IfStmtNode`, `WhileStmtNode`, `IOStmtNode`, `ReturnStmtNode`, `BlockNode`.
- Declarations: `VarDeclNode`, `FuncDefNode`, `ClassDeclNode`, `ProgNode`.

### The Visitor Pattern (Double-Dispatch)

The AST uses the **Visitor Design Pattern** with **Double-Dispatch**:

- Each concrete node implements `accept(ASTVisitor&)`.
- `accept` dispatches to the visitor overload `visit(ConcreteNode&)`.

This design decouples AST data structures from traversal operations, allowing independent implementations for semantic checking and visualization without modifying node classes.

### AST Printer and UML DOT Visualization

AST export is implemented by dedicated visitors:

- `TextASTVisitor` produces readable hierarchical text.
- `DotASTVisitor` produces Graphviz `.dot` output for UML-style rendering.

The DOT output applies explicit visual semantics:

- **Functions**: `Mrecord` nodes.
- **Control flow** (`if`, `while`): diamond nodes.
- **Variables/members**: note nodes.
- **Program root**: folder style.
- **Classes**: record nodes with inheritance/member edges.

Edge styles and arrowheads encode relation semantics such as ownership (`owner`), composition/aggregation (`member` with `odiamond`), inheritance (`inherits` with empty arrowhead), and argument/index links.

## 4. Semantic Analysis and Symbol Tables

### Symbol Table Architecture

Semantic analysis uses hierarchical symbol tables with lexical nesting:

- Each `SymbolTable` stores local entries plus child scopes.
- Parent linkage (`_parent`) enables upward resolution.
- New scopes are created via `createChild(...)`.
- Name lookup is done by current scope lookup followed by parent-chain resolution.

Supported symbol kinds:

- `Class`
- `Function`
- `Variable`
- `Parameter`
- `Field`

Each `SymbolEntry` stores semantic metadata such as type, parameter types, declaration line, visibility/details, and array dimensions.

### The Two-Pass System (Crucial)

#### Pass 1: Symbol Table Construction

Pass 1 builds declarations first to support **forward referencing** and declaration-order independence where language rules allow it.

Mapping to AST traversal behavior:

- `ProgNode`: visits all classes then all functions.
- `ClassDeclNode`: registers class symbol and creates class scope.
- `ClassDeclNode` members: registers fields first, then function declarations/signatures.
- `FuncDefNode`: records function/method signatures; body analysis is deferred.
- `VarDeclNode`: records variables/fields, including declared array dimensions.

Additional checks between passes include:

- Declared-but-never-implemented member functions.
- Circular class dependency detection in inheritance graphs (DFS color marking).

#### Pass 2: Type Checking and Resolution

Pass 2 performs semantic validation with full symbol tables available.

Mapping to AST node responsibilities:

- `FuncDefNode`: creates function scope, installs parameters, checks body, enforces return requirements for non-void functions.
- `AssignStmtNode`: validates assignment compatibility (including integer-to-float promotion rule).
- `BinaryOpNode`: checks operand numeric compatibility.
- `IfStmtNode` and `WhileStmtNode`: enforce boolean-compatible conditions.
- `FuncCallNode`: resolves callee and validates argument count/types.
- `DataMemberNode`: checks member existence, dot-operator legality, array index arity, and index type constraints.

Type synthesis is centralized in `inferExprType(...)`, which synthesizes expression types for literals, identifiers, member access, function calls, and selected operators.

### Name Resolution and Object Orientation

Dot-notation resolution (for example `p.print()`) is handled by:

- Inferring the owner expression type.
- Resolving the corresponding class scope.
- Looking up the target member/function inside that class context.

The semantic system also enforces inheritance-related rules, including cycle detection and diagnostics for inherited-member shadowing and method overriding patterns.

### Error Handling

The semantic analyzer reports structured errors and warnings with line numbers. Covered categories include:

- Undeclared identifiers and undeclared classes.
- Multiply declared symbols in invalid scopes.
- Function call arity/type mismatches.
- Assignment and return type mismatches.
- Invalid operand types in expressions.
- Invalid use of `.` on non-class types.
- Missing/undefined member declarations and implementations.
- Array index type errors and dimension-access mismatch.
- Inheritance-cycle detection.
- Shadowing/overloading/overriding warnings.

## 5. Code Generation (Moon Backend)

The backend lowers the typed AST to Moon assembly using `CodeGenVisitor`.

### Main Design Choices (with Rationale and Consequences)

| Choice | Why I chose it | Consequence |
|---|---|---|
| Stack-frame based allocation for locals/params/temps | Keeps function calls uniform and naturally supports recursion and nested calls | Most program data is addressed as offsets from `r14`; assembly uses `lw/sw` with computed offsets rather than many static `db/dw/res` declarations |
| Static, layout-driven function frames | Offsets are computed once from declarations (`frameSize`, param offsets, local offsets) before emission | Stable addressing and easier diagnostics/tracing; requires strict layout bookkeeping in codegen |
| Class/object layout with inherited field flattening | Enables direct field offset addressing without runtime metadata | Fast object field access; no dynamic object layout changes at runtime |
| Static call convention (`jl` + labels) | Simple and deterministic for assignment scope | No virtual dispatch/runtime polymorphic call table; calls are compile-time resolved |
| Method receiver (`this`) passed in frame slot | Uniform treatment of free functions and methods | Method calls explicitly materialize/store receiver address; invalid receiver expressions fail codegen checks |
| Aggregate object copy as word-by-word memory copy | Correctly handles object assignments and object arguments/returns without per-field special cases | Object operations are heavier than scalar ones; copy size must be word-aligned |
| Scalar return in `r1`; object return as address handle in `r1` | Keeps one return channel while supporting object semantics | Caller must interpret `r1` based on return type (value vs address handle) |
| Fixed-point float lowering (`kFloatScale = 100`) | Moon integer ISA is used as execution base; fixed-point keeps arithmetic implementable | Predictable decimal behavior with 2 fractional digits; precision/range are bounded by scaling strategy |
| Runtime helper routines for I/O | Centralizes parsing/printing logic and keeps expression lowering simpler | Generated programs depend on helper labels (`rt_readInt`, `rt_readFloat`, `rt_writeInt`, `rt_writeFloat`) |
| Dense assembly trace comments with source-line context | Improves debugging, grading visibility, and backend validation | Larger `.moon` files; better execution traceability (`[Lx]`, register/memory comments, instruction tags) |

### Register Usage (Moon)

This backend treats registers as mostly caller-clobbered, except for the explicit stack/link conventions below.

| Register | Primary use in this backend | Consequence |
|---|---|---|
| `r0` | Constant zero base | Used for immediate loads and zero-comparisons; never holds program state |
| `r1` | Value channel: function return, I/O argument/result, and temporary expression value | Must be considered volatile across helper/function calls |
| `r2` | General temporary (allocator + runtime helper scratch) | No persistence guarantees across calls |
| `r3` | General temporary (allocator + runtime helper scratch) | No persistence guarantees across calls |
| `r4` | General temporary (allocator + runtime helper scratch) | No persistence guarantees across calls |
| `r5` | General temporary (allocator + runtime helper scratch) | No persistence guarantees across calls |
| `r6` | General temporary (allocator + runtime helper scratch) | No persistence guarantees across calls |
| `r7` | General temporary (allocator + runtime helper scratch) | No persistence guarantees across calls |
| `r8` | General temporary (allocator + runtime helper scratch) | No persistence guarantees across calls |
| `r9` | General temporary (allocator + runtime helper scratch) | No persistence guarantees across calls |
| `r10` | General temporary; often used by float/runtime helper paths | No persistence guarantees across calls |
| `r11` | General temporary; often used by float/runtime helper paths | No persistence guarantees across calls |
| `r12` | General temporary (highest-priority free register in allocator) | First-to-be-acquired temp in many expression paths |
| `r13` | Currently unused/reserved by this backend | Available for future conventions (for example global base/extra scratch) |
| `r14` | Stack/frame pointer | All local/param/object slot addressing is relative to `r14` |
| `r15` | Link register (return address for `jl`) | Saved/restored by callee prologue/epilogue in non-main functions |

### How Memory is Reserved and Used

- **Compile-time reservation plan**:
	- Types and object sizes are computed first.
	- Function frame layout computes offsets for return link, receiver, parameters, and locals.
- **Runtime reservation**:
	- `r14` is initialized to `topaddr`.
	- Calls reserve/restore frame space by adjusting `r14`.
- **Static storage directives (`res`)**:
	- Used only where persistent helper storage is needed (for example, integer print buffer in runtime helper).
	- Not heavily used for normal variables because program variables are stack-frame based by design.

### Arrays, Objects, and Addressing

- Array indexing is lowered by computing a linearized offset from indices and dimension strides.
- Element-size scaling is applied so address arithmetic lands on correct word boundaries.
- Object fields use precomputed field offsets.
- Member access with explicit owner and implicit `this` are both supported.

### Control Flow and Calls

- `if` and `while` lower to generated labels and conditional branches (`bz`, `j`).
- Function and method calls lower to:
	- argument placement in callee-expected frame slots,
	- receiver placement for methods,
	- `jl r15, <function_label>`,
	- return value pickup from `r1`.

### Numeric and I/O Behavior

- Integer operations use native Moon integer instructions.
- Float expressions use fixed-point arithmetic (including scaling on mixed int/float boundaries).
- Read/write are lowered to runtime helpers:
	- `read` -> integer or float parser helper,
	- `write` -> integer or float printer helper.

### Error Strategy in Codegen

Code generation reports structured diagnostics (with source lines) for unresolved symbols, unsupported l-values, invalid aggregate copy sizes, and register exhaustion, while still attempting to emit as much traceable assembly context as possible.

## 6. Build

```bash
cd ./src/
g++ -std=c++17 -static -I../include -o ../exe/driver *.cpp 
```

## 7. Generated Outputs

Current outputs per source file:

- `output/<name>/Lexer/<name>.outlextokens`
- `output/<name>/Lexer/<name>.outlexerrors`
- `output/<name>/Parser/<name>.outderivation`
- `output/<name>/Parser/<name>.outsyntaxerrors`
- `output/<name>/AST/<name>.outast`
- `output/<name>/AST/<name>.outast.dot`
- `output/<name>/AST/<name>.outast.png`
- `output/<name>/Semantics/<name>.outsymboltables`
- `output/<name>/Semantics/<name>.outsemanticerrors`
- `output/<name>/CodeGen/<name>.moon`
- `output/<name>/CodeGen/<name>.outcodegenerrors`
- `output/<name>/CodeGen/<name>.outmoonrun`