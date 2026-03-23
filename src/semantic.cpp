#include "../include/semantic.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace {
bool containsReturnStatement(const std::shared_ptr<ASTNode>& node) {
    if (node == nullptr) {
        return false;
    }

    if (std::dynamic_pointer_cast<ReturnStmtNode>(node) != nullptr) {
        return true;
    }

    if (auto block = std::dynamic_pointer_cast<BlockNode>(node)) {
        for (const auto& stmt : block->getStatements()) {
            if (containsReturnStatement(stmt)) {
                return true;
            }
        }
    }

    if (auto call = std::dynamic_pointer_cast<FuncCallNode>(node)) {
        for (const auto& arg : call->getArgs()) {
            if (containsReturnStatement(arg)) {
                return true;
            }
        }
    }

    if (auto member = std::dynamic_pointer_cast<DataMemberNode>(node)) {
        for (const auto& idx : member->getIndices()) {
            if (containsReturnStatement(idx)) {
                return true;
            }
        }
    }

    if (auto ifStmt = std::dynamic_pointer_cast<IfStmtNode>(node)) {
        if (containsReturnStatement(ifStmt->getElseBlock())) {
            return true;
        }
    }

    return containsReturnStatement(node->getLeft()) || containsReturnStatement(node->getRight());
}
}

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

SymbolEntry* SymbolTable::lookupMutableInCurrent(const std::string& name) {
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
    _classScopes.clear();
    _functionReturnTypeStack.clear();
    _globalScope = std::make_shared<SymbolTable>("global");

    if (root != nullptr) {
        setPassOne(true);
        _blockCounter = 0;
        _currentScope = _globalScope;
        root->accept(*this);

        setPassOne(false);
        _blockCounter = 0;
        _functionReturnTypeStack.clear();
        _currentScope = _globalScope;
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
    if (isPassOne()) {
        return;
    }

    if (_currentScope->resolve(node.getName()) == nullptr) {
        reportError(node.getLineNumber(), "use of undeclared identifier '" + node.getName() + "'");
    }
}

void SemanticAnalyzer::visit(IntLitNode&) {}
void SemanticAnalyzer::visit(FloatLitNode&) {}
void SemanticAnalyzer::visit(TypeNode&) {}

void SemanticAnalyzer::visit(BinaryOpNode& node) {
    if (isPassOne()) {
        return;
    }

    visitNode(node.getLeft());
    visitNode(node.getRight());
    
    std::string leftType = inferExprType(node.getLeft());
    std::string rightType = inferExprType(node.getRight());
    
    // Make sure we are adding numbers!
    if (leftType != "integer" && leftType != "float") {
        reportError(node.getLineNumber(), "Left side of math operation must be a number");
    }
    if (rightType != "integer" && rightType != "float") {
        reportError(node.getLineNumber(), "Right side of math operation must be a number");
    }
}

void SemanticAnalyzer::visit(UnaryOpNode& node) {
    visitNode(node.getLeft());
}

void SemanticAnalyzer::visit(FuncCallNode& node) {
    if (isPassOne()) {
        return;
    }

    const SymbolEntry* symbol = nullptr;

    auto calleeMember = std::dynamic_pointer_cast<DataMemberNode>(node.getLeft());
    const bool isOwnerQualifiedMethodCall = calleeMember != nullptr && calleeMember->getLeft() != nullptr;

    if (isOwnerQualifiedMethodCall) {
        // For calls like p.print(), the callee node is the member (print),
        // so receiver type comes from the owner expression (p).
        std::string ownerType = inferExprType(calleeMember->getLeft());
        std::string methodName = calleeMember->getName();

        if (ownerType.empty()) {
            reportError(node.getLineNumber(), "cannot resolve receiver type for method call '" + node.getFunctionName() + "'");
        } else {
            symbol = resolveClassMember(ownerType, methodName);
            if (symbol == nullptr || symbol->kind != SymbolKind::Function) {
                reportError(node.getLineNumber(), "call to undeclared method '" + baseTypeName(ownerType) + "::" + methodName + "'");
            }
        }
    } else {
        // Free function call. Some AST shapes keep the callee identifier in node.getLeft().
        symbol = _currentScope->resolve(node.getFunctionName());
        if (symbol == nullptr || symbol->kind != SymbolKind::Function) {
            reportError(node.getLineNumber(), "call to undeclared function '" + node.getFunctionName() + "'");
        }
    }

    visitNode(node.getLeft());
    for (const auto& arg : node.getArgs()) {
        visitNode(arg);
    }

    if (symbol != nullptr && symbol->kind == SymbolKind::Function) {
        auto isAssignableTo = [](const std::string& expectedType, const std::string& actualType) {
            if (expectedType == "null" || actualType == "null") {
                return true;
            }
            if (expectedType == actualType) {
                return true;
            }
            return expectedType == "float" && actualType == "integer";
        };

        const std::vector<std::string>& expectedParamTypes = symbol->paramTypes;
        const auto& args = node.getArgs();
        if (expectedParamTypes.size() != args.size()) {
            reportError(
                node.getLineNumber(),
                "argument count mismatch in call to '" + node.getFunctionName() +
                "': expected " + std::to_string(expectedParamTypes.size()) +
                ", got " + std::to_string(args.size())
            );
            return;
        }

        for (size_t i = 0; i < args.size(); ++i) {
            const std::string actualType = inferExprType(args[i]);
            const std::string& expectedType = expectedParamTypes[i];
            if (!isAssignableTo(expectedType, actualType)) {
                reportError(
                    node.getLineNumber(),
                    "argument " + std::to_string(i + 1) + " type mismatch in call to '" + node.getFunctionName() +
                    "': expected '" + expectedType + "', got '" + actualType + "'"
                );
            }
        }
    }
}

void SemanticAnalyzer::visit(DataMemberNode& node) {
    if (isPassOne()) {
        return;
    }

    if (node.getLeft() == nullptr) {
        if (_currentScope->resolve(node.getName()) == nullptr) {
            reportError(node.getLineNumber(), "use of undeclared member/variable '" + node.getName() + "'");
        }
    } else {
        const std::string ownerType = inferExprType(node.getLeft());
        if (ownerType.empty()) {
            reportError(node.getLineNumber(), "cannot resolve owner type for member access '" + node.getName() + "'");
        } else {
            const SymbolEntry* member = resolveClassMember(ownerType, node.getName());
            if (member == nullptr) {
                reportError(node.getLineNumber(), "type '" + baseTypeName(ownerType) + "' has no member named '" + node.getName() + "'");
            }
        }
    }

    visitNode(node.getLeft());
    for (const auto& idx : node.getIndices()) {
        visitNode(idx);
    }
}

void SemanticAnalyzer::visit(AssignStmtNode& node) {
    if (isPassOne()) {
        return;
    }

    visitNode(node.getLeft());
    visitNode(node.getRight());
    
    std::string leftType = inferExprType(node.getLeft());
    std::string rightType = inferExprType(node.getRight());
    
    if (leftType != "null" && rightType != "null" && leftType != rightType) {
        // Allow int to float promotion, but block float to int, or int to Class.
        if (!(leftType == "float" && rightType == "integer")) {
             reportError(node.getLineNumber(), "Type mismatch in assignment: cannot assign '" + rightType + "' to '" + leftType + "'");
        }
    }
}

void SemanticAnalyzer::visit(IfStmtNode& node) {
    if (isPassOne()) {
        visitNode(node.getRight());
        visitNode(node.getElseBlock());
        return;
    }

    visitNode(node.getLeft());

    const std::string condType = inferExprType(node.getLeft());
    const bool isConditionCompatible =
        condType == "bool";
    if (condType != "null" && !isConditionCompatible) {
        reportError(
            node.getLineNumber(),
            "condition in 'if' must evaluate to numeric/boolean-compatible type, found '" + condType + "'"
        );
    }

    visitNode(node.getRight());
    visitNode(node.getElseBlock());
}

void SemanticAnalyzer::visit(WhileStmtNode& node) {
    if (isPassOne()) {
        visitNode(node.getRight());
        return;
    }

    visitNode(node.getLeft());

    const std::string condType = inferExprType(node.getLeft());
    const bool isConditionCompatible =
        condType == "bool";
    if (condType != "null" && !isConditionCompatible) {
        reportError(
            node.getLineNumber(),
            "condition in 'while' must evaluate to numeric/boolean-compatible type, found '" + condType + "'"
        );
    }

    visitNode(node.getRight());
}

void SemanticAnalyzer::visit(IOStmtNode& node) {
    visitNode(node.getLeft());
}

void SemanticAnalyzer::visit(ReturnStmtNode& node) {
    if (isPassOne()) {
        return;
    }

    visitNode(node.getLeft());

    if (_functionReturnTypeStack.empty()) {
        reportError(node.getLineNumber(), "return statement used outside of function body");
        return;
    }

    const std::string expectedType = _functionReturnTypeStack.back();
    const std::string actualType = inferExprType(node.getLeft());

    auto isAssignableTo = [](const std::string& expected, const std::string& actual) {
        if (expected == actual) {
            return true;
        }
        else if(expected == "null" && actual != "null") {
            return false;
        }
        return expected == "float" && actual == "integer";
    };

    if (expectedType == "void") {
        if (node.getLeft() != nullptr) {
            reportError(
                node.getLineNumber(),
                "return type mismatch: function expects 'void' but return has expression of type '" + actualType + "'"
            );
        }
        return;
    }

    if (node.getLeft() == nullptr) {
        reportError(
            node.getLineNumber(),
            "return type mismatch: function expects '" + expectedType + "' but return has no expression"
        );
        return;
    }

    if (actualType != "null" && !isAssignableTo(expectedType, actualType)) {
        reportError(
            node.getLineNumber(),
            "return type mismatch: expected '" + expectedType + "', got '" + actualType + "'"
        );
    }
}

void SemanticAnalyzer::visit(BlockNode& node) {
    std::shared_ptr<SymbolTable> prev = _currentScope;
    _currentScope = getOrCreateChildScope(_currentScope, "block#" + std::to_string(++_blockCounter));

    for (const auto& stmt : node.getStatements()) {
        visitNode(stmt);
    }

    _currentScope = prev;
}

void SemanticAnalyzer::visit(VarDeclNode& node) {
    SymbolEntry entry;
    entry.name = node.getName();
    entry.type = node.getTypeName();
    entry.returnType = "null";
    entry.paramTypes.clear();
    entry.kind = node.getVisibility() == "local" ? SymbolKind::Variable : SymbolKind::Field;
    entry.visibility = node.getVisibility();
    entry.details = "null";
    entry.line = node.getLineNumber();
    defineSymbol(entry);
}

void SemanticAnalyzer::visit(FuncDefNode& node) {
    std::shared_ptr<SymbolTable> ownerScope = _currentScope;
    std::string ownerClass;

    if (!node.getClassName().empty()) {
        ownerClass = node.getClassName();
        auto it = _classScopes.find(ownerClass);
        if (it == _classScopes.end()) {
            reportError(node.getLineNumber(), "definition for method '" + ownerClass + "::" + node.getName() + "' has unknown class");
            ownerScope = _globalScope;
        } else {
            ownerScope = it->second;
        }
    } else if (_currentScope != nullptr) {
        const std::string scopeName = _currentScope->getScopeName();
        if (scopeName.rfind("class ", 0) == 0) {
            ownerClass = scopeName.substr(6);
            auto it = _classScopes.find(ownerClass);
            if (it != _classScopes.end()) {
                ownerScope = it->second;
            }
        }
    }

    std::shared_ptr<SymbolTable> prevScope = _currentScope;
    _currentScope = ownerScope;

    const bool isImplementation = node.getRight() != nullptr;
    if (isPassOne()) {
        SymbolEntry entry;
        entry.name = node.getName();
        entry.type = functionSignature(node);
        entry.returnType = node.getReturnType();
        entry.paramTypes.clear();
        for (const auto& param : node.getParams()) {
            entry.paramTypes.push_back(param->getTypeName());
        }
        entry.kind = SymbolKind::Function;
        entry.visibility = "n/a";
        const std::string ownerDescriptor = ownerClass.empty() ? "free function" : ("method of " + ownerClass);
        const std::string role = isImplementation ? "implementation" : "declaration";
        entry.details = ownerDescriptor + " (" + role + ")";
        entry.line = node.getLineNumber();

        SymbolEntry* existing = _currentScope->lookupMutableInCurrent(entry.name);
        if (existing == nullptr) {
            // Class-qualified implementations must match a declaration in class scope.
            // Free functions are allowed to be implemented without prior declaration.
            const bool isClassMethodImplementation = !ownerClass.empty() && isImplementation;
            if (!isClassMethodImplementation) {
                defineSymbol(entry);
            }
        } else if (existing->kind != SymbolKind::Function) {
            reportError(entry.line, "symbol '" + entry.name + "' already exists with non-function kind in scope '" + _currentScope->getScopeName() + "'");
        } else {
            if (existing->type != entry.type) {
                reportError(entry.line, "signature mismatch for function '" + entry.name + "' in scope '" + _currentScope->getScopeName() + "'");
            }

            const bool existingHasDecl = existing->details.find("declaration") != std::string::npos;
            const bool existingHasImpl = existing->details.find("implementation") != std::string::npos;
            if (isImplementation && existingHasDecl && !existingHasImpl) {
                existing->details = ownerDescriptor + " (declaration + implementation)";
            } else if (isImplementation && existingHasImpl) {
                reportError(entry.line, "multiple implementations for function '" + entry.name + "' in scope '" + _currentScope->getScopeName() + "'");
            }
        }
    }

    if (!isImplementation) {
        _currentScope = prevScope;
        return;
    }

    if (!isPassOne() && !ownerClass.empty()) {
        const SymbolEntry* declaredMethod = _currentScope->lookupInCurrent(node.getName());
        const bool hasDeclaration = declaredMethod != nullptr &&
            declaredMethod->kind == SymbolKind::Function &&
            declaredMethod->details.find("declaration") != std::string::npos;
        if (!hasDeclaration) {
            reportError(
                node.getLineNumber(),
                "implementation for method '" + ownerClass + "::" + node.getName() + "' has no prior declaration in class '" + ownerClass + "'"
            );
        }
    }

    if (isPassOne()) {
        // Pass 1 only declares signatures; body analysis is deferred to pass 2.
        _currentScope = prevScope;
        return;
    }

    std::string funcScopeName = ownerClass.empty() ? ("func " + node.getName()) : ("func " + ownerClass + "::" + node.getName());
    std::shared_ptr<SymbolTable> prev = _currentScope;
    _currentScope = getOrCreateChildScope(_currentScope, funcScopeName);

    for (const auto& param : node.getParams()) {
        SymbolEntry paramEntry;
        paramEntry.name = param->getName();
        paramEntry.type = param->getTypeName();
        paramEntry.kind = SymbolKind::Parameter;
        paramEntry.visibility = "param";
        paramEntry.details = "null";
        paramEntry.line = param->getLineNumber();
        defineSymbol(paramEntry);
    }

    _functionReturnTypeStack.push_back(node.getReturnType());
    visitNode(node.getRight());
    _functionReturnTypeStack.pop_back();

    if (node.getReturnType() != "void" && !containsReturnStatement(node.getRight())) {
        reportError(
            node.getLineNumber(),
            "non-void function '" + node.getName() + "' must contain a return statement"
        );
    }

    _currentScope = prev;
    _currentScope = prevScope;
}

void SemanticAnalyzer::visit(ClassDeclNode& node) {
    if (isPassOne()) {
        SymbolEntry entry;
        entry.name = node.getName();
        entry.type = classTypeName(node);
        entry.returnType = "null";
        entry.paramTypes.clear();
        entry.kind = SymbolKind::Class;
        entry.visibility = "n/a";
        entry.details = node.getParents().empty() ? "no inheritance" : "inherits " + std::to_string(node.getParents().size()) + " class(es)";
        entry.line = node.getLineNumber();
        defineSymbol(entry);
    }

    std::shared_ptr<SymbolTable> prev = _currentScope;
    _currentScope = getOrCreateChildScope(_currentScope, "class " + node.getName());
    if (isPassOne()) {
        _classScopes[node.getName()] = _currentScope;
    }

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
    if (isPassOne()) {
        return;
    }

    _errors.push_back("[ERROR][SEMANTIC] line " + std::to_string(line) + ": " + message);
}

bool SemanticAnalyzer::defineSymbol(const SymbolEntry& entry) {
    if (_currentScope == nullptr) {
        return false;
    }

    if (_currentScope->define(entry)) {
        return true;
    }

    // Pass 2 reuses pass-1 symbol tables; repeated declarations in the same
    // scope should not emit duplicate redefinition diagnostics.
    if (!isPassOne()) {
        const SymbolEntry* existing = _currentScope->lookupInCurrent(entry.name);
        if (existing != nullptr) {
            return true;
        }
    }

    reportError(entry.line, "redefinition of symbol '" + entry.name + "' in scope '" + _currentScope->getScopeName() + "'");
    return false;
}

void SemanticAnalyzer::visitNode(const std::shared_ptr<ASTNode>& node) {
    if (node != nullptr) {
        node->accept(*this);
    }
}

std::shared_ptr<SymbolTable> SemanticAnalyzer::getOrCreateChildScope(const std::shared_ptr<SymbolTable>& parent, const std::string& scopeName) {
    if (parent == nullptr) {
        return nullptr;
    }

    if (!isPassOne()) {
        for (const auto& child : parent->getChildren()) {
            if (child != nullptr && child->getScopeName() == scopeName) {
                return child;
            }
        }
    }

    return parent->createChild(scopeName);
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

std::string SemanticAnalyzer::baseTypeName(const std::string& typeName) {
    std::string base = typeName;
    size_t parenPos = base.find('(');
    if (parenPos != std::string::npos) {
        base = base.substr(0, parenPos);
    }

    size_t arrayPos = base.find('[');
    if (arrayPos != std::string::npos) {
        base = base.substr(0, arrayPos);
    }

    auto trim = [](std::string& s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
            s.erase(s.begin());
        }
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
            s.pop_back();
        }
    };
    trim(base);
    return base;
}

const SymbolEntry* SemanticAnalyzer::resolveClassMember(const std::string& classTypeName, const std::string& memberName) const {
    const std::string base = baseTypeName(classTypeName);
    auto it = _classScopes.find(base);
    if (it == _classScopes.end() || it->second == nullptr) {
        return nullptr;
    }
    return it->second->lookupInCurrent(memberName);
}

std::string SemanticAnalyzer::inferExprType(const std::shared_ptr<ASTNode>& node) const {
    if (node == nullptr) {
        return "null";
    }

    if (auto intNode = std::dynamic_pointer_cast<IntLitNode>(node)) {
        (void)intNode;
        return "integer";
    }
    if (auto floatNode = std::dynamic_pointer_cast<FloatLitNode>(node)) {
        (void)floatNode;
        return "float";
    }
    if (auto idNode = std::dynamic_pointer_cast<IdNode>(node)) {
        const SymbolEntry* symbol = _currentScope->resolve(idNode->getName());
        return symbol != nullptr ? symbol->type : "null";
    }
    if (auto memberNode = std::dynamic_pointer_cast<DataMemberNode>(node)) {
        if (memberNode->getLeft() == nullptr) {
            const SymbolEntry* symbol = _currentScope->resolve(memberNode->getName());
            return symbol != nullptr ? symbol->type : "null";
        }

        const std::string ownerType = inferExprType(memberNode->getLeft());
        const SymbolEntry* member = resolveClassMember(ownerType, memberNode->getName());
        return member != nullptr ? member->type : "null";
    }
    if (auto callNode = std::dynamic_pointer_cast<FuncCallNode>(node)) {
        const SymbolEntry* symbol = nullptr;
        auto calleeMember = std::dynamic_pointer_cast<DataMemberNode>(callNode->getLeft());
        const bool isOwnerQualifiedMethodCall = calleeMember != nullptr && calleeMember->getLeft() != nullptr;
        if (isOwnerQualifiedMethodCall) {
            const std::string ownerType = inferExprType(calleeMember->getLeft());
            symbol = resolveClassMember(ownerType, calleeMember->getName());
        } else {
            symbol = _currentScope->resolve(callNode->getFunctionName());
        }
        if (symbol == nullptr) {
            return "null";
        }

        const std::string signature = symbol->type;
        size_t paren = signature.find('(');
        if (paren == std::string::npos) {
            return signature;
        }
        return signature.substr(0, paren);
    }

    if (auto binaryNode = std::dynamic_pointer_cast<BinaryOpNode>(node)) {
        const std::string leftType = inferExprType(binaryNode->getLeft());
        const std::string rightType = inferExprType(binaryNode->getRight());
        const std::string op = binaryNode->getOperator();

        const bool isRelational =
            op == "<" || op == ">" || op == "<=" || op == ">=" ||
            op == "==" || op == "!=";
        const bool isLogical =
            op == "and" || op == "or" || op == "&&" || op == "||";

        if (isRelational || isLogical) {
            return "bool";
        }

        if (leftType == "float" || rightType == "float") {
            return "float";
        }
        if (leftType == "integer" && rightType == "integer") {
            return "integer";
        }
    }

    // Keep this conservative for now; richer synthesis will be added incrementally.
    return "null";
}

void SemanticAnalyzer::dumpScope(const std::shared_ptr<SymbolTable>& scope, int depth, std::string& out) const {
    if (scope == nullptr) {
        return;
    }

    std::string indent(depth * 2, ' ');
    const auto& allEntries = scope->getEntries();
    const auto& children = scope->getChildren();

    const bool isGlobal = scope->getScopeName() == "global";
    const bool isClassScope = scope->getScopeName().rfind("class ", 0) == 0;

    std::vector<const SymbolEntry*> entries;
    entries.reserve(allEntries.size());
    for (const auto& entry : allEntries) {
        if (isGlobal && entry.kind == SymbolKind::Function && entry.details.find("method of ") != std::string::npos) {
            continue;
        }
        entries.push_back(&entry);
    }

    out += indent + "Scope: " + scope->getScopeName() + "\n";

    for (const auto* entry : entries) {
        out += indent + "  - [" + kindToString(entry->kind) + "] " + entry->name + " : " + entry->type;
        if (!entry->visibility.empty()) {
            out += " [" + entry->visibility + "]";
        }
        if (!entry->details.empty()) {
            out += " {" + entry->details + "}";
        }
        out += " (line " + std::to_string(entry->line) + ")\n";
    }

    if (entries.empty()) {
        out += indent + "  - <empty>\n";
    }

    out += "\n";

    std::unordered_set<const SymbolTable*> visited;

    auto findChildByName = [&](const std::string& expectedName) -> std::shared_ptr<SymbolTable> {
        for (const auto& child : children) {
            if (child != nullptr && child->getScopeName() == expectedName) {
                return child;
            }
        }
        return nullptr;
    };

    if (isGlobal) {
        for (const auto* entry : entries) {
            if (entry->kind == SymbolKind::Class) {
                std::shared_ptr<SymbolTable> classChild = findChildByName("class " + entry->name);
                if (classChild != nullptr) {
                    visited.insert(classChild.get());
                    dumpScope(classChild, depth + 1, out);
                }
            }
        }

        for (const auto* entry : entries) {
            if (entry->kind == SymbolKind::Function && entry->details.find("free function") != std::string::npos) {
                std::shared_ptr<SymbolTable> fnChild = findChildByName("func " + entry->name);
                if (fnChild != nullptr && !visited.count(fnChild.get())) {
                    visited.insert(fnChild.get());
                    dumpScope(fnChild, depth + 1, out);
                }
            }
        }
    } else if (isClassScope) {
        const std::string className = scope->getScopeName().substr(6);
        for (const auto* entry : entries) {
            if (entry->kind != SymbolKind::Function) {
                continue;
            }
            std::shared_ptr<SymbolTable> fnChild = findChildByName("func " + className + "::" + entry->name);
            if (fnChild == nullptr) {
                fnChild = findChildByName("func " + entry->name);
            }
            if (fnChild != nullptr && !visited.count(fnChild.get())) {
                visited.insert(fnChild.get());
                dumpScope(fnChild, depth + 1, out);
            }
        }
    }

    for (const auto& child : children) {
        if (child != nullptr && !visited.count(child.get())) {
            dumpScope(child, depth + 1, out);
        }
    }
}
