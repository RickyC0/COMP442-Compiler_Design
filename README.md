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

## Build

```bash
cd ./src/
g++ -std=c++17 -static -I../include -o driver *.cpp
```

## Generated Outputs

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