/**
 * @file semantic.h
 * @brief Semantic analysis types, symbol tables, and analyzer visitor interface.
 *
 * @details
 * This header defines the semantic layer for the compiler pipeline:
 * - symbol categories and symbol metadata,
 * - hierarchical lexical scopes via SymbolTable,
 * - two-pass semantic analysis over the AST using a visitor.
 *
 * @par Why this structure?
 * Semantic checks require both declaration-time registration (pass 1) and
 * usage-time validation (pass 2). These APIs keep that separation explicit while
 * preserving traceable diagnostics with source line numbers.
 *
 * @par What comes next?
 * New language features should extend symbol metadata and analyzer visitors here,
 * then mirror those changes in parser AST construction and code generation.
 */
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "AST.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @enum SymbolKind
 * @brief Kinds of semantic symbols tracked in scope tables.
 */
enum class SymbolKind {
    Class,
    Function,
    Variable,
    Parameter,
    Field
};

/**
 * @struct SymbolEntry
 * @brief Canonical symbol metadata record stored in symbol tables.
 *
 * @details
 * A single structure is used for classes, functions, variables, parameters, and
 * fields. Unused members remain empty depending on symbol kind.
 */
struct SymbolEntry {
    /** @brief Symbol identifier. */
    std::string name;
    /** @brief Primary type text or signature root. */
    std::string type;
    /** @brief Array dimensions for arrays/array parameters (if applicable). */
    std::vector<int> dimensions;
    /** @brief Function return type for function symbols. */
    std::string returnType;
    /** @brief Function parameter base types. */
    std::vector<std::string> paramTypes;
    /** @brief Function parameter dimension specs for array parameters. */
    std::vector<std::vector<int>> paramDimensions;
    /** @brief Symbol category. */
    SymbolKind kind;
    /** @brief Visibility/role marker (for example local, param, public). */
    std::string visibility;
    /** @brief Human-readable semantic details. */
    std::string details;
    /** @brief 1-based source declaration line. */
    int line;
};

/**
 * @class SymbolTable
 * @brief Hierarchical lexical scope with symbol-definition and resolution services.
 *
 * @details
 * Each scope stores local declarations and links to parent/children scopes.
 * Resolution first checks current scope then walks upward to parent scopes.
 */
class SymbolTable : public std::enable_shared_from_this<SymbolTable> {
    public:
        /**
         * @brief Construct a symbol table scope.
         * @param scopeName Display name for diagnostics and dumps.
         * @param parent Parent scope (null for global).
         */
        SymbolTable(const std::string& scopeName, std::shared_ptr<SymbolTable> parent = nullptr);

        /** @brief Create and register a child scope below this scope. */
        std::shared_ptr<SymbolTable> createChild(const std::string& scopeName);
        /** @brief Define a symbol in current scope; fails on same-scope redeclaration. */
        bool define(const SymbolEntry& entry);
        /** @brief Find symbol only in current scope (const). */
        const SymbolEntry* lookupInCurrent(const std::string& name) const;
        /** @brief Find symbol only in current scope (mutable). */
        SymbolEntry* lookupMutableInCurrent(const std::string& name);
        /** @brief Resolve symbol through current scope then ancestors. */
        const SymbolEntry* resolve(const std::string& name) const;

        /** @brief Get scope display name. */
        const std::string& getScopeName() const;
        /** @brief Get parent scope (null for root). */
        std::shared_ptr<SymbolTable> getParent() const;
        /** @brief Get child scopes. */
        const std::vector<std::shared_ptr<SymbolTable>>& getChildren() const;
        /** @brief Get all entries defined in this scope. */
        const std::vector<SymbolEntry>& getEntries() const;

    private:
        std::string _scopeName;
        std::weak_ptr<SymbolTable> _parent;
        std::vector<std::shared_ptr<SymbolTable>> _children;
        std::vector<SymbolEntry> _entries;
        std::unordered_map<std::string, size_t> _entryIndex;
};

/**
 * @class SemanticAnalyzer
 * @brief Two-pass semantic analyzer implemented as an AST visitor.
 *
 * @details
 * Pass 1 builds declarations/scopes and signature metadata.
 * Pass 2 validates identifier use, type compatibility, member access, function
 * calls, control-flow constraints, and language semantic rules.
 */
class SemanticAnalyzer : public ASTVisitor {
    public:
        /** @brief True when analyzer is running declaration-building pass. */
        bool isPassOne() const { return _isPassOne; }
        /** @brief Set active semantic pass mode. */
        void setPassOne(bool passOne) { _isPassOne = passOne; }

        /**
         * @brief Run semantic analysis on program AST root.
         * @param root Program root node.
         * @return True when no semantic errors were produced.
         */
        bool analyze(const std::shared_ptr<ProgNode>& root);
        /** @brief Get accumulated semantic error diagnostics. */
        const std::vector<std::string>& getErrors() const;
        /** @brief Get accumulated semantic warning diagnostics. */
        const std::vector<std::string>& getWarnings() const;
        /** @brief Dump formatted symbol-table hierarchy. */
        std::string dumpSymbolTables() const;

        /** @name AST Visitor Overrides */
        /** @{ */
        void visit(IdNode& node) override;
        void visit(IntLitNode& node) override;
        void visit(FloatLitNode& node) override;
        void visit(TypeNode& node) override;
        void visit(BinaryOpNode& node) override;
        void visit(UnaryOpNode& node) override;
        void visit(FuncCallNode& node) override;
        void visit(DataMemberNode& node) override;
        void visit(AssignStmtNode& node) override;
        void visit(IfStmtNode& node) override;
        void visit(WhileStmtNode& node) override;
        void visit(IOStmtNode& node) override;
        void visit(ReturnStmtNode& node) override;
        void visit(BlockNode& node) override;
        void visit(VarDeclNode& node) override;
        void visit(FuncDefNode& node) override;
        void visit(ClassDeclNode& node) override;
        void visit(ProgNode& node) override;
        /** @} */

    private:
        /** @brief Current pass mode flag. */
        bool _isPassOne = false;
        /** @brief Global/root scope table. */
        std::shared_ptr<SymbolTable> _globalScope;
        /** @brief Scope currently being visited. */
        std::shared_ptr<SymbolTable> _currentScope;
        /** @brief Class name -> class scope map. */
        std::unordered_map<std::string, std::shared_ptr<SymbolTable>> _classScopes;
        /** @brief Declared member function identities: Class::name(paramProfile). */
        std::unordered_set<std::string> _declaredMemberFunctionKeys;
        /** @brief Collected semantic errors. */
        std::vector<std::string> _errors;
        /** @brief Collected semantic warnings. */
        std::vector<std::string> _warnings;
        /** @brief Return-type stack for nested function-body visits. */
        std::vector<std::string> _functionReturnTypeStack;
        /** @brief Counter for synthesized block scope naming. */
        int _blockCounter = 0;

        /** @brief Strip signature/array suffixes from a type name. */
        static std::string baseTypeName(const std::string& typeName);
        /** @brief Resolve member inside a class type scope. */
        const SymbolEntry* resolveClassMember(const std::string& classTypeName, const std::string& memberName) const;
        /** @brief Infer expression result type for semantic checks. */
        std::string inferExprType(const std::shared_ptr<ASTNode>& node) const;

        /** @brief Record semantic error with line context. */
        void reportError(int line, const std::string& message);
        /** @brief Record semantic warning with line context. */
        void reportWarning(int line, const std::string& message);
        /** @brief Define symbol in current scope with redefinition policy checks. */
        bool defineSymbol(const SymbolEntry& entry);
        /** @brief Null-safe helper for visiting an optional node. */
        void visitNode(const std::shared_ptr<ASTNode>& node);
        /** @brief Reuse existing child scope in pass 2 or create one in pass 1. */
        std::shared_ptr<SymbolTable> getOrCreateChildScope(const std::shared_ptr<SymbolTable>& parent, const std::string& scopeName);

        /** @brief Convert SymbolKind enum to printable label. */
        static std::string kindToString(SymbolKind kind);
        /** @brief Build canonical function signature string from AST function node. */
        static std::string functionSignature(const FuncDefNode& node);
        /** @brief Build class descriptor string including inheritance summary. */
        static std::string classTypeName(const ClassDeclNode& node);
        /** @brief Recursively dump scope tree in table format. */
        void dumpScope(const std::shared_ptr<SymbolTable>& scope, int depth, std::string& out) const;
};

#endif