## Build

```bash
cd ./src/
g++ -std=c++17 -static -I../include -o driver *.cpp
```

## Compiler Status

Completed so far:
- Lexical analysis (tokenization + lexical errors)
- Syntactic analysis (recursive-descent parser + panic-mode recovery)
- AST construction during parsing
- AST exports: text, DOT, and PNG

Current outputs per source file:
- `output/<name>/Lexer/<name>.outlextokens`
- `output/<name>/Lexer/<name>.outlexerrors`
- `output/<name>/Parser/<name>.outderivation`
- `output/<name>/Parser/<name>.outsyntaxerrors`
- `output/<name>/AST/<name>.outast`
- `output/<name>/AST/<name>.outast.dot`
- `output/<name>/AST/<name>.outast.png`

## Next Steps

- Symbol table construction
- Semantic analysis (type checking, scope checks)