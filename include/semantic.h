#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "AST.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

enum class SymbolKind {
    Class,
    Function,
    Variable,
    Parameter,
    Field
};

struct SymbolEntry {
    std::string name;
    std::string type;
    std::vector<int> dimensions;
    std::string returnType;
    std::vector<std::string> paramTypes;
    std::vector<std::vector<int>> paramDimensions;
    SymbolKind kind;
    std::string visibility;
    std::string details;
    int line;
};

class SymbolTable : public std::enable_shared_from_this<SymbolTable> {
    public:
        SymbolTable(const std::string& scopeName, std::shared_ptr<SymbolTable> parent = nullptr);

        std::shared_ptr<SymbolTable> createChild(const std::string& scopeName);
        bool define(const SymbolEntry& entry);
        const SymbolEntry* lookupInCurrent(const std::string& name) const;
        SymbolEntry* lookupMutableInCurrent(const std::string& name);
        const SymbolEntry* resolve(const std::string& name) const;

        const std::string& getScopeName() const;
        std::shared_ptr<SymbolTable> getParent() const;
        const std::vector<std::shared_ptr<SymbolTable>>& getChildren() const;
        const std::vector<SymbolEntry>& getEntries() const;

    private:
        std::string _scopeName;
        std::weak_ptr<SymbolTable> _parent;
        std::vector<std::shared_ptr<SymbolTable>> _children;
        std::vector<SymbolEntry> _entries;
        std::unordered_map<std::string, size_t> _entryIndex;
};

class SemanticAnalyzer : public ASTVisitor {
    public:
        bool isPassOne() const { return _isPassOne; }
        void setPassOne(bool passOne) { _isPassOne = passOne; }
        bool analyze(const std::shared_ptr<ProgNode>& root);
        const std::vector<std::string>& getErrors() const;
        const std::vector<std::string>& getWarnings() const;
        std::string dumpSymbolTables() const;

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

    private:
        bool _isPassOne = false;
        std::shared_ptr<SymbolTable> _globalScope;
        std::shared_ptr<SymbolTable> _currentScope;
        std::unordered_map<std::string, std::shared_ptr<SymbolTable>> _classScopes;
        std::vector<std::string> _errors;
        std::vector<std::string> _warnings;
        std::vector<std::string> _functionReturnTypeStack;
        int _blockCounter = 0;

        static std::string baseTypeName(const std::string& typeName);
        const SymbolEntry* resolveClassMember(const std::string& classTypeName, const std::string& memberName) const;
        std::string inferExprType(const std::shared_ptr<ASTNode>& node) const;

        void reportError(int line, const std::string& message);
        void reportWarning(int line, const std::string& message);
        bool defineSymbol(const SymbolEntry& entry);
        void visitNode(const std::shared_ptr<ASTNode>& node);
        std::shared_ptr<SymbolTable> getOrCreateChildScope(const std::shared_ptr<SymbolTable>& parent, const std::string& scopeName);

        static std::string kindToString(SymbolKind kind);
        static std::string functionSignature(const FuncDefNode& node);
        static std::string classTypeName(const ClassDeclNode& node);
        void dumpScope(const std::shared_ptr<SymbolTable>& scope, int depth, std::string& out) const;
};

#endif