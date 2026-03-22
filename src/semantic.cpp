#include "../include/semantic.h"

#include <sstream>

SymbolTable::SymbolTable(const std::string& scopeName, std::shared_ptr<SymbolTable> parent)
    : _scopeName(scopeName), _parent(parent) {}

std::shared_ptr<SymbolTable> SymbolTable::createChild(const std::string& scopeName) {
    std::shared_ptr<SymbolTable> child = std::make_shared<SymbolTable>(scopeName, shared_from_this());
    _children.push_back(child);
    return child;
}

bool SymbolTable::define(const SymbolEntry& entry) {
    if (_entryIndex.find(entry.name) != _entryIndex.end()) {
        return false;
    }

    _entryIndex[entry.name] = _entries.size();
    _entries.push_back(entry);
    return true;
}

const SymbolEntry* SymbolTable::lookupInCurrent(const std::string& name) const {
    auto it = _entryIndex.find(name);
    if (it == _entryIndex.end()) {
        return nullptr;
    }
    return &_entries[it->second];
}

const SymbolEntry* SymbolTable::resolve(const std::string& name) const {
    const SymbolEntry* inCurrent = lookupInCurrent(name);
    if (inCurrent != nullptr) {
        return inCurrent;
    }

    std::shared_ptr<SymbolTable> parent = _parent.lock();
    if (parent == nullptr) {
        return nullptr;
    }
    return parent->resolve(name);
}

const std::string& SymbolTable::getScopeName() const {
    return _scopeName;
}

std::shared_ptr<SymbolTable> SymbolTable::getParent() const {
    return _parent.lock();
}

const std::vector<std::shared_ptr<SymbolTable>>& SymbolTable::getChildren() const {
    return _children;
}

const std::vector<SymbolEntry>& SymbolTable::getEntries() const {
    return _entries;
}

bool SemanticAnalyzer::analyze(const std::shared_ptr<ProgNode>& root) {
    _errors.clear();
    _blockCounter = 0;
    _globalScope = std::make_shared<SymbolTable>("global");
    _currentScope = _globalScope;

    if (root != nullptr) {
        root->accept(*this);
    }

    return _errors.empty();
}

const std::vector<std::string>& SemanticAnalyzer::getErrors() const {
    return _errors;
}

std::string SemanticAnalyzer::dumpSymbolTables() const {
    std::string out;
    dumpScope(_globalScope, 0, out);
    return out;
}

void SemanticAnalyzer::visit(IdNode& node) {
    if (_currentScope->resolve(node.getName()) == nullptr) {
        reportError(node.getLineNumber(), "use of undeclared identifier '" + node.getName() + "'");
    }
}

void SemanticAnalyzer::visit(IntLitNode&) {}
void SemanticAnalyzer::visit(FloatLitNode&) {}
void SemanticAnalyzer::visit(TypeNode&) {}

void SemanticAnalyzer::visit(BinaryOpNode& node) {
    visitNode(node.getLeft());
    visitNode(node.getRight());
}

void SemanticAnalyzer::visit(UnaryOpNode& node) {
    visitNode(node.getLeft());
}

void SemanticAnalyzer::visit(FuncCallNode& node) {
    const SymbolEntry* symbol = _currentScope->resolve(node.getFunctionName());
    if (symbol == nullptr || symbol->kind != SymbolKind::Function) {
        reportError(node.getLineNumber(), "call to undeclared function '" + node.getFunctionName() + "'");
    }

    visitNode(node.getLeft());
    for (const auto& arg : node.getArgs()) {
        visitNode(arg);
    }
}

void SemanticAnalyzer::visit(DataMemberNode& node) {
    // For owner.member cases, member lookup requires type-based resolution,
    // which will be added in a later pass. For now only check standalone IDs.
    if (node.getLeft() == nullptr && _currentScope->resolve(node.getName()) == nullptr) {
        reportError(node.getLineNumber(), "use of undeclared member/variable '" + node.getName() + "'");
    }

    visitNode(node.getLeft());
    for (const auto& idx : node.getIndices()) {
        visitNode(idx);
    }
}

void SemanticAnalyzer::visit(AssignStmtNode& node) {
    visitNode(node.getLeft());
    visitNode(node.getRight());
}

void SemanticAnalyzer::visit(IfStmtNode& node) {
    visitNode(node.getLeft());
    visitNode(node.getRight());
    visitNode(node.getElseBlock());
}

void SemanticAnalyzer::visit(WhileStmtNode& node) {
    visitNode(node.getLeft());
    visitNode(node.getRight());
}

void SemanticAnalyzer::visit(IOStmtNode& node) {
    visitNode(node.getLeft());
}

void SemanticAnalyzer::visit(ReturnStmtNode& node) {
    visitNode(node.getLeft());
}

void SemanticAnalyzer::visit(BlockNode& node) {
    std::shared_ptr<SymbolTable> prev = _currentScope;
    _currentScope = _currentScope->createChild("block#" + std::to_string(++_blockCounter));

    for (const auto& stmt : node.getStatements()) {
        visitNode(stmt);
    }

    _currentScope = prev;
}

void SemanticAnalyzer::visit(VarDeclNode& node) {
    SymbolEntry entry;
    entry.name = node.getName();
    entry.type = node.getTypeName();
    entry.kind = node.getVisibility() == "local" ? SymbolKind::Variable : SymbolKind::Field;
    entry.line = node.getLineNumber();
    defineSymbol(entry);
}

void SemanticAnalyzer::visit(FuncDefNode& node) {
    SymbolEntry entry;
    entry.name = node.getName();
    entry.type = functionSignature(node);
    entry.kind = SymbolKind::Function;
    entry.line = node.getLineNumber();
    defineSymbol(entry);

    std::shared_ptr<SymbolTable> prev = _currentScope;
    _currentScope = _currentScope->createChild("func " + node.getName());

    for (const auto& param : node.getParams()) {
        SymbolEntry paramEntry;
        paramEntry.name = param->getName();
        paramEntry.type = param->getTypeName();
        paramEntry.kind = SymbolKind::Parameter;
        paramEntry.line = param->getLineNumber();
        defineSymbol(paramEntry);
    }

    visitNode(node.getRight());
    _currentScope = prev;
}

void SemanticAnalyzer::visit(ClassDeclNode& node) {
    SymbolEntry entry;
    entry.name = node.getName();
    entry.type = classTypeName(node);
    entry.kind = SymbolKind::Class;
    entry.line = node.getLineNumber();
    defineSymbol(entry);

    std::shared_ptr<SymbolTable> prev = _currentScope;
    _currentScope = _currentScope->createChild("class " + node.getName());

    // Pass 1: register all fields first, so methods can reference them regardless of source order.
    for (const auto& member : node.getMembers()) {
        auto field = std::dynamic_pointer_cast<VarDeclNode>(member);
        if (field != nullptr) {
            field->accept(*this);
        }
    }

    // Pass 2: analyze all members.
    for (const auto& member : node.getMembers()) {
        if (std::dynamic_pointer_cast<VarDeclNode>(member) != nullptr) {
            continue;
        }
        visitNode(member);
    }

    _currentScope = prev;
}

void SemanticAnalyzer::visit(ProgNode& node) {
    for (const auto& cls : node.getClasses()) {
        visitNode(cls);
    }

    for (const auto& func : node.getFunctions()) {
        visitNode(func);
    }
}

void SemanticAnalyzer::reportError(int line, const std::string& message) {
    _errors.push_back("[ERROR][SEMANTIC] line " + std::to_string(line) + ": " + message);
}

bool SemanticAnalyzer::defineSymbol(const SymbolEntry& entry) {
    if (_currentScope->define(entry)) {
        return true;
    }

    reportError(entry.line, "redefinition of symbol '" + entry.name + "' in scope '" + _currentScope->getScopeName() + "'");
    return false;
}

void SemanticAnalyzer::visitNode(const std::shared_ptr<ASTNode>& node) {
    if (node != nullptr) {
        node->accept(*this);
    }
}

std::string SemanticAnalyzer::kindToString(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Class:
            return "class";
        case SymbolKind::Function:
            return "function";
        case SymbolKind::Variable:
            return "variable";
        case SymbolKind::Parameter:
            return "parameter";
        case SymbolKind::Field:
            return "field";
    }

    return "unknown";
}

std::string SemanticAnalyzer::functionSignature(const FuncDefNode& node) {
    std::ostringstream sig;
    sig << node.getReturnType() << "(";
    const auto params = node.getParams();
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            sig << ", ";
        }
        sig << params[i]->getTypeName();
    }
    sig << ")";
    return sig.str();
}

std::string SemanticAnalyzer::classTypeName(const ClassDeclNode& node) {
    std::ostringstream oss;
    oss << "class";
    const auto parents = node.getParents();
    if (!parents.empty()) {
        oss << " : ";
        for (size_t i = 0; i < parents.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << parents[i];
        }
    }
    return oss.str();
}

void SemanticAnalyzer::dumpScope(const std::shared_ptr<SymbolTable>& scope, int depth, std::string& out) const {
    if (scope == nullptr) {
        return;
    }

    std::string indent(depth * 2, ' ');
    out += indent + "Scope: " + scope->getScopeName() + "\n";

    for (const auto& entry : scope->getEntries()) {
        out += indent + "  - [" + kindToString(entry.kind) + "] " + entry.name + " : " + entry.type +
               " (line " + std::to_string(entry.line) + ")\n";
    }

    if (scope->getEntries().empty()) {
        out += indent + "  - <empty>\n";
    }

    out += "\n";

    for (const auto& child : scope->getChildren()) {
        dumpScope(child, depth + 1, out);
    }
}
