#include "../include/semantic.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iomanip>
#include <sstream>
#include <unordered_set>

/**
 * @file semantic.cpp
 * @brief Semantic analyzer implementation: two-pass checks and scope management.
 *
 * @details
 * Implements:
 * - symbol table construction and resolution,
 * - semantic rule enforcement over AST traversal,
 * - expression type inference for compatibility checks,
 * - symbol-table dump utilities for diagnostics/reporting.
 *
 * @par Why two-pass analysis?
 * Pass 1 collects declarations/signatures to support forward references and
 * class/member visibility relationships. Pass 2 then validates usage against a
 * complete symbol context.
 *
 * @par What comes next?
 * New semantic rules should be added in visit methods with consistent diagnostic
 * formatting and accompanied by inferExprType updates when rule evaluation depends
 * on expression category synthesis.
 */

namespace {
/** @brief Check if a type name is a language builtin scalar/void type. */
bool isBuiltinType(const std::string& typeName) {
    return typeName == "integer" || typeName == "float" || typeName == "void" || typeName == "bool";
}

/** @brief Return trimmed copy of input string. */
std::string trimCopy(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

/**
 * @brief Parse inherited parent class names from encoded class type descriptor.
 * @param classType Class descriptor text (for example "class : A, B").
 * @return Parent class names in declaration order.
 */
std::vector<std::string> parseParentsFromClassType(const std::string& classType) {
    std::vector<std::string> parents;
    size_t colon = classType.find(':');
    if (colon == std::string::npos) {
        return parents;
    }

    std::string suffix = classType.substr(colon + 1);
    size_t start = 0;
    while (start <= suffix.size()) {
        size_t comma = suffix.find(',', start);
        if (comma == std::string::npos) {
            std::string token = trimCopy(suffix.substr(start));
            if (!token.empty()) {
                parents.push_back(token);
            }
            break;
        }

        std::string token = trimCopy(suffix.substr(start, comma - start));
        if (!token.empty()) {
            parents.push_back(token);
        }
        start = comma + 1;
    }

    return parents;
}

/** @brief Extract class name from scope label "class X". */
std::string classNameFromScope(const std::string& scopeName) {
    if (scopeName.rfind("class ", 0) == 0) {
        return scopeName.substr(6);
    }
    return "";
}

/**
 * @brief Find enclosing class name from function scope ancestry.
 * @param scope Current scope.
 * @return Enclosing class name or empty string.
 */
std::string enclosingClassFromFunctionScope(const std::shared_ptr<SymbolTable>& scope) {
    std::shared_ptr<SymbolTable> currentScope = scope;
    while (currentScope != nullptr) {
        const std::string name = currentScope->getScopeName();
        if (name.rfind("function ", 0) == 0) {
            const size_t functionPrefixLen = std::string("function ").size();
            size_t sep = name.find("::", functionPrefixLen);
            if (sep != std::string::npos) {
                return name.substr(functionPrefixLen, sep - functionPrefixLen);
            }
            return "";
        }
        currentScope = currentScope->getParent();
    }
    return "";
}

/**
 * @brief Convert internal scope name into user-facing diagnostic scope label.
 * @param scope Scope pointer.
 * @return Friendly scope name.
 */
std::string getUserFriendlyScopeName(const std::shared_ptr<SymbolTable>& scope) {
    if (scope == nullptr) {
        return "";
    }

    const std::string scopeName = scope->getScopeName();

    // If it's a block, find the enclosing function or class
    if (scopeName.rfind("block#", 0) == 0) {
        std::shared_ptr<SymbolTable> currentScope = scope->getParent();
        while (currentScope != nullptr) {
            const std::string name = currentScope->getScopeName();
            if (name.rfind("function ", 0) == 0) {
                return name;
            }
            if (name.rfind("class ", 0) == 0) {
                return name;
            }
            currentScope = currentScope->getParent();
        }
    }

    return scopeName;
}

/**
 * @brief Detect whether subtree contains any return statement.
 * @param node Subtree root.
 * @return True if at least one ReturnStmtNode exists in subtree.
 */
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

/**
 * @brief Try compile-time evaluation of integer constant expressions.
 * @param node Expression node.
 * @param outValue Result when evaluation succeeds.
 * @return True if expression is an evaluable integer constant.
 *
 * @details
 * Used for semantic bounds checks where literal or foldable integer values are
 * required to issue precise diagnostics.
 */
bool tryEvalIntConst(const std::shared_ptr<ASTNode>& node, int& outValue) {
    if (node == nullptr) {
        return false;
    }

    if (auto intNode = std::dynamic_pointer_cast<IntLitNode>(node)) {
        outValue = intNode->getIntValue();
        return true;
    }

    if (auto unaryNode = std::dynamic_pointer_cast<UnaryOpNode>(node)) {
        int operand = 0;
        if (!tryEvalIntConst(unaryNode->getLeft(), operand)) {
            return false;
        }

        const std::string op = unaryNode->getOperator();
        if (op == "-") {
            outValue = -operand;
            return true;
        }
        if (op == "+") {
            outValue = operand;
            return true;
        }

        return false;
    }

    if (auto binaryNode = std::dynamic_pointer_cast<BinaryOpNode>(node)) {
        int leftValue = 0;
        int rightValue = 0;
        if (!tryEvalIntConst(binaryNode->getLeft(), leftValue) || !tryEvalIntConst(binaryNode->getRight(), rightValue)) {
            return false;
        }

        const std::string op = binaryNode->getOperator();
        if (op == "+") {
            outValue = leftValue + rightValue;
            return true;
        }
        if (op == "-") {
            outValue = leftValue - rightValue;
            return true;
        }
        if (op == "*") {
            outValue = leftValue * rightValue;
            return true;
        }
        if (op == "/") {
            if (rightValue == 0) {
                return false;
            }
            outValue = leftValue / rightValue;
            return true;
        }
        if (op == "mod") {
            if (rightValue == 0) {
                return false;
            }
            outValue = leftValue % rightValue;
            return true;
        }
    }

    return false;
}

std::string functionParamProfile(const FuncDefNode& node) {
    std::ostringstream profile;
    profile << "(";
    const auto params = node.getParams();
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            profile << ",";
        }

        profile << params[i]->getTypeName();
        for (int dim : params[i]->getDimensions()) {
            if (dim < 0) {
                profile << "[]";
            } else {
                profile << "[" << dim << "]";
            }
        }
    }
    profile << ")";
    return profile.str();
}
}

/**
 * @brief Construct scope table with optional parent link.
 * @param scopeName Scope display name.
 * @param parent Parent scope or null for global scope.
 */
SymbolTable::SymbolTable(const std::string& scopeName, std::shared_ptr<SymbolTable> parent)
    : _scopeName(scopeName), _parent(parent) {}

/** @brief Create child scope and register in current scope children list. */
std::shared_ptr<SymbolTable> SymbolTable::createChild(const std::string& scopeName) {
    std::shared_ptr<SymbolTable> child = std::make_shared<SymbolTable>(scopeName, shared_from_this());
    _children.push_back(child);
    return child;
}

/** @brief Define symbol in current scope if name is not already present. */
bool SymbolTable::define(const SymbolEntry& entry) {
    if (_entryIndex.find(entry.name) != _entryIndex.end()) {
        return false;
    }

    _entryIndex[entry.name] = _entries.size();
    _entries.push_back(entry);
    return true;
}

/** @brief Lookup symbol only in current scope (const). */
const SymbolEntry* SymbolTable::lookupInCurrent(const std::string& name) const {
    auto it = _entryIndex.find(name);
    if (it == _entryIndex.end()) {
        return nullptr;
    }
    return &_entries[it->second];
}

/** @brief Lookup symbol only in current scope (mutable). */
SymbolEntry* SymbolTable::lookupMutableInCurrent(const std::string& name) {
    auto it = _entryIndex.find(name);
    if (it == _entryIndex.end()) {
        return nullptr;
    }
    return &_entries[it->second];
}

/** @brief Resolve symbol recursively through parent scopes. */
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

/** @brief Get scope name. */
const std::string& SymbolTable::getScopeName() const {
    return _scopeName;
}

/** @brief Get parent scope. */
std::shared_ptr<SymbolTable> SymbolTable::getParent() const {
    return _parent.lock();
}

/** @brief Get child scopes. */
const std::vector<std::shared_ptr<SymbolTable>>& SymbolTable::getChildren() const {
    return _children;
}

/** @brief Get entries defined directly in this scope. */
const std::vector<SymbolEntry>& SymbolTable::getEntries() const {
    return _entries;
}

/**
 * @brief Run semantic analyzer passes over program root.
 * @param root Program AST root.
 * @return True if no semantic errors were emitted.
 *
 * @details
 * Execution order:
 * 1) pass 1 declaration/scope construction,
 * 2) cross-pass global checks (undefined member declarations, inheritance cycles),
 * 3) pass 2 usage/type checks.
 */
bool SemanticAnalyzer::analyze(const std::shared_ptr<ProgNode>& root) {
    _errors.clear();
    _warnings.clear();
    _blockCounter = 0;
    _classScopes.clear();
    _declaredClassNames.clear();
    _declaredMemberFunctionKeys.clear();
    _functionReturnTypeStack.clear();
    _globalScope = std::make_shared<SymbolTable>("global");

    if (root != nullptr) {
        for (const auto& cls : root->getClasses()) {
            if (cls != nullptr) {
                _declaredClassNames.insert(cls->getName());
            }
        }

        setPassOne(true);
        _blockCounter = 0;
        _currentScope = _globalScope;
        root->accept(*this);

        setPassOne(false);
        _blockCounter = 0;
        _functionReturnTypeStack.clear();

        // 6.2: member function declared but never implemented.
        for (const auto& classPair : _classScopes) {
            const auto& classScope = classPair.second;
            if (classScope == nullptr) {
                continue;
            }

            for (const auto& entry : classScope->getEntries()) {
                if (entry.kind != SymbolKind::Function) {
                    continue;
                }

                const bool hasDecl = entry.details.find("declaration") != std::string::npos;
                const bool hasImpl = entry.details.find("implementation") != std::string::npos;
                if (hasDecl && !hasImpl) {
                    reportError(entry.line, "6.2 undefined member function declaration: '" + classPair.first + "::" + entry.name + "'");
                }
            }
        }

        // 14.1: circular class dependency in inheritance graph.
        std::unordered_map<std::string, std::vector<std::string>> graph;
        std::unordered_map<std::string, int> classLine;
        for (const auto& entry : _globalScope->getEntries()) {
            if (entry.kind != SymbolKind::Class) {
                continue;
            }
            graph[entry.name] = parseParentsFromClassType(entry.type);
            classLine[entry.name] = entry.line;
        }

        enum class Color { White, Gray, Black };
        std::unordered_map<std::string, Color> color;
        for (const auto& kv : graph) {
            color[kv.first] = Color::White;
        }

        std::function<void(const std::string&)> dfs = [&](const std::string& nodeName) {
            color[nodeName] = Color::Gray;
            for (const auto& parent : graph[nodeName]) {
                if (graph.find(parent) == graph.end()) {
                    continue;
                }
                if (color[parent] == Color::Gray) {
                    reportError(classLine[nodeName], "14.1 circular class dependency involving '" + nodeName + "' and '" + parent + "'");
                    continue;
                }
                if (color[parent] == Color::White) {
                    dfs(parent);
                }
            }
            color[nodeName] = Color::Black;
        };

        for (const auto& kv : graph) {
            if (color[kv.first] == Color::White) {
                dfs(kv.first);
            }
        }

        _currentScope = _globalScope;
        root->accept(*this);
    }

    return _errors.empty();
}

/** @brief Return semantic error list. */
const std::vector<std::string>& SemanticAnalyzer::getErrors() const {
    return _errors;
}

/** @brief Return semantic warning list. */
const std::vector<std::string>& SemanticAnalyzer::getWarnings() const {
    return _warnings;
}

/** @brief Produce formatted symbol-table dump for all scopes. */
std::string SemanticAnalyzer::dumpSymbolTables() const {
    std::string out;
    dumpScope(_globalScope, 0, out);
    return out;
}

/** @brief Validate identifier use in pass 2 by lexical resolution. */
void SemanticAnalyzer::visit(IdNode& node) {
    if (isPassOne()) {
        return;
    }

    if (_currentScope->resolve(node.getName()) == nullptr) {
        reportError(node.getLineNumber(), "11.1 undeclared local variable: '" + node.getName() + "'");
    }
}

/** @brief Integer literal has no standalone semantic action. */
void SemanticAnalyzer::visit(IntLitNode&) {}
/** @brief Float literal has no standalone semantic action. */
void SemanticAnalyzer::visit(FloatLitNode&) {}
/** @brief Type node has no standalone semantic action. */
void SemanticAnalyzer::visit(TypeNode&) {}

/**
 * @brief Validate binary-expression operand compatibility.
 * @details
 * Current rule set enforces numeric operands for arithmetic-style expressions.
 */
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
        reportError(node.getLineNumber(), "10.1 type error in expression: left operand must be numeric");
    }
    if (rightType != "integer" && rightType != "float") {
        reportError(node.getLineNumber(), "10.1 type error in expression: right operand must be numeric");
    }
}

/** @brief Traverse unary operand. */
void SemanticAnalyzer::visit(UnaryOpNode& node) {
    visitNode(node.getLeft());
}

/**
 * @brief Validate function/method call declaration and parameter compatibility.
 * @details
 * Handles free-function and owner-qualified method-call forms, then enforces
 * argument count/type compatibility and selected array-size semantic checks.
 */
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
                reportError(node.getLineNumber(), "11.3 undeclared member function: '" + baseTypeName(ownerType) + "::" + methodName + "'");
            }
        }
    } else {
        // Free function call. Some AST shapes keep the callee identifier in node.getLeft().
        symbol = _currentScope->resolve(node.getFunctionName());
        if (symbol == nullptr || symbol->kind != SymbolKind::Function) {
            reportError(node.getLineNumber(), "11.4 undeclared/undefined free function: '" + node.getFunctionName() + "'");
        }
    }

    // Avoid duplicate 11.4 diagnostics for free-function call callee identifiers.
    if (isOwnerQualifiedMethodCall && calleeMember != nullptr) {
        visitNode(calleeMember->getLeft());
    }
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
                "12.1 function call with wrong number of parameters: '" + node.getFunctionName() +
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
                    "12.2 function call with wrong type of parameters in call to '" + node.getFunctionName() +
                    "': expected '" + expectedType + "', got '" + actualType + "'"
                );
            }
        }

        auto resolveArrayFirstDimension = [&](const std::shared_ptr<ASTNode>& arg, int& firstDimension, std::string& displayName) -> bool {
            firstDimension = -1;
            displayName.clear();

            if (auto idNode = std::dynamic_pointer_cast<IdNode>(arg)) {
                const SymbolEntry* argSymbol = _currentScope->resolve(idNode->getName());
                if (argSymbol != nullptr && !argSymbol->dimensions.empty() && argSymbol->dimensions[0] > 0) {
                    firstDimension = argSymbol->dimensions[0];
                    displayName = idNode->getName();
                    return true;
                }
                return false;
            }

            auto memberNode = std::dynamic_pointer_cast<DataMemberNode>(arg);
            if (memberNode == nullptr || !memberNode->getIndices().empty()) {
                return false;
            }

            if (memberNode->getLeft() == nullptr) {
                const SymbolEntry* argSymbol = _currentScope->resolve(memberNode->getName());
                if (argSymbol != nullptr && !argSymbol->dimensions.empty() && argSymbol->dimensions[0] > 0) {
                    firstDimension = argSymbol->dimensions[0];
                    displayName = memberNode->getName();
                    return true;
                }
                return false;
            }

            const std::string ownerType = inferExprType(memberNode->getLeft());
            const SymbolEntry* memberSymbol = resolveClassMember(ownerType, memberNode->getName());
            if (memberSymbol != nullptr && !memberSymbol->dimensions.empty() && memberSymbol->dimensions[0] > 0) {
                firstDimension = memberSymbol->dimensions[0];
                displayName = memberNode->getName();
                return true;
            }

            return false;
        };

        const std::vector<std::vector<int>>& expectedParamDimensions = symbol->paramDimensions;
        if (expectedParamDimensions.size() == args.size()) {
            for (size_t i = 0; i + 1 < args.size(); ++i) {
                const std::vector<int>& dims = expectedParamDimensions[i];
                if (dims.empty() || dims[0] >= 0) {
                    continue;
                }

                if (expectedParamTypes[i + 1] != "integer") {
                    continue;
                }

                int requestedExtent = 0;
                if (!tryEvalIntConst(args[i + 1], requestedExtent)) {
                    continue;
                }

                int actualExtent = -1;
                std::string arrayName;
                if (!resolveArrayFirstDimension(args[i], actualExtent, arrayName)) {
                    continue;
                }

                if (requestedExtent < 0) {
                    reportError(
                        node.getLineNumber(),
                        "13.3 potential out-of-bounds access in call to '" + node.getFunctionName() +
                        "': size argument " + std::to_string(requestedExtent) +
                        " is negative for array '" + arrayName + "'"
                    );
                } else if (requestedExtent > actualExtent) {
                    reportError(
                        node.getLineNumber(),
                        "13.3 potential out-of-bounds access in call to '" + node.getFunctionName() +
                        "': size argument " + std::to_string(requestedExtent) +
                        " exceeds declared size " + std::to_string(actualExtent) +
                        " for array '" + arrayName + "'"
                    );
                }
            }
        }
    }
}

/**
 * @brief Validate member access and array indexing semantics.
 * @details
 * Checks declaration visibility, owner-type/member existence, dimensional arity,
 * integer index typing, and constant index bounds when evaluable.
 */
void SemanticAnalyzer::visit(DataMemberNode& node) {
    if (isPassOne()) {
        return;
    }

    std::vector<int> declaredDimensions;

    if (node.getLeft() == nullptr) {
        const SymbolEntry* symbol = _currentScope->resolve(node.getName());
        if (symbol == nullptr) {
            reportError(node.getLineNumber(), "11.2 undeclared member variable or unresolved identifier: '" + node.getName() + "'");
        } else {
            declaredDimensions = symbol->dimensions;
            // Check array dimensions match
            const size_t declaredDimensions = symbol->dimensions.size();
            const size_t accessedDimensions = node.getIndices().size();
            if (declaredDimensions != accessedDimensions) {
                reportError(
                    node.getLineNumber(),
                    "13.1 array '" + node.getName() + "' has " + std::to_string(declaredDimensions) +
                    " dimension(s) but is accessed with " + std::to_string(accessedDimensions) + " index/indices"
                );
            }
        }
    } else {
        const std::string ownerType = inferExprType(node.getLeft());
        if (ownerType.empty()) {
            reportError(node.getLineNumber(), "cannot resolve owner type for member access '" + node.getName() + "'");
        } else {
            const SymbolEntry* member = resolveClassMember(ownerType, node.getName());
            const std::string ownerBase = baseTypeName(ownerType);
            if (ownerBase == "integer" || ownerBase == "float" || ownerBase == "void" || ownerBase == "bool") {
                reportError(node.getLineNumber(), "15.1 '.' operator used on non-class type '" + ownerBase + "'");
            } else if (member == nullptr) {
                reportError(node.getLineNumber(), "11.2 undeclared member variable: type '" + ownerBase + "' has no member named '" + node.getName() + "'");
            } else {
                declaredDimensions = member->dimensions;
            }
        }
    }

    size_t dimIdx = 0;
    visitNode(node.getLeft());
    for (const auto& idx : node.getIndices()) {
        const std::string indexType = inferExprType(idx);
        if (indexType != "null" && indexType != "integer") {
            reportError(node.getLineNumber(), "13.2 array index is not an integer");
        }

        int constantIndex = 0;
        if (tryEvalIntConst(idx, constantIndex)) {
            if (constantIndex < 0) {
                reportError(
                    node.getLineNumber(),
                    "13.3 array index out of bounds: index " + std::to_string(constantIndex) +
                    " for array '" + node.getName() + "'"
                );
            } else if (dimIdx < declaredDimensions.size() && declaredDimensions[dimIdx] > 0 &&
                       constantIndex >= declaredDimensions[dimIdx]) {
                reportError(
                    node.getLineNumber(),
                    "13.3 array index out of bounds: index " + std::to_string(constantIndex) +
                    " exceeds upper bound " + std::to_string(declaredDimensions[dimIdx] - 1) +
                    " for array '" + node.getName() + "'"
                );
            }
        }

        ++dimIdx;
        visitNode(idx);
    }
}

/** @brief Validate assignment type compatibility rules. */
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
             reportError(node.getLineNumber(), "10.2 type error in assignment statement: cannot assign '" + rightType + "' to '" + leftType + "'");
        }
    }
}

/** @brief Validate if-condition type and traverse branches. */
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

/** @brief Validate while-condition type and traverse loop body. */
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

/** @brief Validate read/write statement operand constraints. */
void SemanticAnalyzer::visit(IOStmtNode& node) {
    if (isPassOne()) {
        visitNode(node.getLeft());
        return;
    }

    visitNode(node.getLeft());

    const std::string ioType = node.getValue();
    const std::string exprType = inferExprType(node.getLeft());
    const std::string baseType = baseTypeName(exprType);

    if (exprType == "null") {
        return;
    }

    if (ioType == "read") {
        if (std::dynamic_pointer_cast<IdNode>(node.getLeft()) == nullptr &&
            std::dynamic_pointer_cast<DataMemberNode>(node.getLeft()) == nullptr) {
            reportError(node.getLineNumber(), "read statement requires an assignable variable/member target");
            return;
        }

        if (baseType != "integer" && baseType != "float" && baseType != "bool") {
            reportError(
                node.getLineNumber(),
                "read statement target must be scalar integer/float/bool, found '" + baseType + "'"
            );
        }

        return;
    }

    if (ioType == "write") {
        if (baseType == "void") {
            reportError(node.getLineNumber(), "write statement cannot output expression of type 'void'");
        }
    }
}

/** @brief Validate return statement placement and type compatibility. */
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
                "10.3 type error in return statement: function expects 'void' but return has expression of type '" + actualType + "'"
            );
        }
        return;
    }

    if (node.getLeft() == nullptr) {
        reportError(
            node.getLineNumber(),
            "10.3 type error in return statement: function expects '" + expectedType + "' but return has no expression"
        );
        return;
    }

    if (actualType != "null" && !isAssignableTo(expectedType, actualType)) {
        reportError(
            node.getLineNumber(),
            "10.3 type error in return statement: expected '" + expectedType + "', got '" + actualType + "'"
        );
    }
}

/** @brief Enter block scope, visit statements, then restore previous scope. */
void SemanticAnalyzer::visit(BlockNode& node) {
    std::shared_ptr<SymbolTable> prev = _currentScope;
    _currentScope = getOrCreateChildScope(_currentScope, "block#" + std::to_string(++_blockCounter));

    for (const auto& stmt : node.getStatements()) {
        visitNode(stmt);
    }

    _currentScope = prev;
}

/**
 * @brief Register and validate variable/field declarations.
 * @details
 * Enforces class-type existence and selected shadowing diagnostics.
 */
void SemanticAnalyzer::visit(VarDeclNode& node) {
    const std::string declaredTypeBase = baseTypeName(node.getTypeName());
    if (!isBuiltinType(declaredTypeBase) && _declaredClassNames.find(declaredTypeBase) == _declaredClassNames.end()) {
        reportError(node.getLineNumber(), "11.5 undeclared class: '" + declaredTypeBase + "'");
    }

    if (node.getVisibility() == "local") {
        const std::string enclosingClass = enclosingClassFromFunctionScope(_currentScope);
        if (!enclosingClass.empty()) {
            auto classIt = _classScopes.find(enclosingClass);
            if (classIt != _classScopes.end() && classIt->second != nullptr) {
                const SymbolEntry* classMember = classIt->second->lookupInCurrent(node.getName());
                if (classMember != nullptr && classMember->kind == SymbolKind::Field) {
                    reportWarning(node.getLineNumber(), "8.7 local variable in member function shadows data member: '" + node.getName() + "'");
                }
            }
        }
    }

    if (isPassOne() && node.getVisibility() != "local") {
        // 8.5 shadowed inherited data member.
        std::string currentClass = classNameFromScope(_currentScope != nullptr ? _currentScope->getScopeName() : "");
        if (!currentClass.empty()) {
            const SymbolEntry* classEntry = _globalScope->lookupInCurrent(currentClass);
            if (classEntry != nullptr) {
                for (const auto& parentName : parseParentsFromClassType(classEntry->type)) {
                    auto parentIt = _classScopes.find(parentName);
                    if (parentIt != _classScopes.end() && parentIt->second != nullptr) {
                        const SymbolEntry* inherited = parentIt->second->lookupInCurrent(node.getName());
                        if (inherited != nullptr && inherited->kind == SymbolKind::Field) {
                            reportWarning(node.getLineNumber(), "8.6 shadowed inherited data member: '" + node.getName() + "'");
                        }
                    }
                }
            }
        }
    }

    SymbolEntry entry;
    entry.name = node.getName();
    entry.type = node.getTypeName();
    entry.dimensions = node.getDimensions();
    entry.returnType = "null";
    entry.paramTypes.clear();
    entry.kind = node.getVisibility() == "local" ? SymbolKind::Variable : SymbolKind::Field;
    entry.visibility = node.getVisibility();
    entry.details = "null";
    entry.line = node.getLineNumber();
    defineSymbol(entry);
}

/**
 * @brief Handle function/method declaration, implementation, and body checks.
 * @details
 * Pass 1: signature registration and declaration/implementation bookkeeping.
 * Pass 2: scope setup, parameter definitions, body traversal, and return checks.
 */
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
    const std::string paramProfile = functionParamProfile(node);
    const std::string memberFunctionKey = ownerClass.empty()
        ? std::string()
        : (ownerClass + "::" + node.getName() + paramProfile);

    if (isPassOne() && !ownerClass.empty() && !isImplementation) {
        _declaredMemberFunctionKeys.insert(memberFunctionKey);
    }

    if (isPassOne()) {
        SymbolEntry entry;
        entry.name = node.getName();
        entry.type = functionSignature(node);
        entry.returnType = node.getReturnType();
        entry.paramTypes.clear();
        entry.paramDimensions.clear();
        for (const auto& param : node.getParams()) {
            entry.paramTypes.push_back(param->getTypeName());
            entry.paramDimensions.push_back(param->getDimensions());
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
            const bool sameParameterProfile = (existing->paramTypes == entry.paramTypes) &&
                                              (existing->paramDimensions == entry.paramDimensions);

            if (!sameParameterProfile) {
                if (ownerClass.empty()) {
                    reportWarning(entry.line, "9.1 overloaded free function: '" + entry.name + "'");
                } else {
                    reportWarning(entry.line, "9.2 overloaded member function: '" + ownerClass + "::" + entry.name + "'");
                }
            } else if (!isImplementation && ownerClass.empty()) {
                reportError(entry.line, "8.2 multiply declared free function: '" + entry.name + "'");
            }

            const bool existingHasDecl = existing->details.find("declaration") != std::string::npos;
            const bool existingHasImpl = existing->details.find("implementation") != std::string::npos;
            if (isImplementation && sameParameterProfile && existingHasDecl && !existingHasImpl) {
                existing->details = ownerDescriptor + " (declaration + implementation)";
            } else if (isImplementation && sameParameterProfile && existingHasImpl) {
                if (ownerClass.empty()) {
                    reportError(entry.line, "8.2 multiply declared free function: '" + entry.name + "'");
                } else {
                    reportError(entry.line, "multiple implementations for function '" + entry.name + "' in scope '" + _currentScope->getScopeName() + "'");
                }
            }
        }
    }

    if (!isImplementation) {
        _currentScope = prevScope;
        return;
    }

    if (!isPassOne() && !ownerClass.empty()) {
        const bool hasDeclaration = _declaredMemberFunctionKeys.find(memberFunctionKey) != _declaredMemberFunctionKeys.end();
        if (!hasDeclaration) {
            reportError(
                node.getLineNumber(),
                "6.1 undeclared member function definition: '" + ownerClass + "::" + node.getName() + "'"
            );
        }
    }

    if (isPassOne()) {
        // Pass 1 only declares signatures; body analysis is deferred to pass 2.
        _currentScope = prevScope;
        return;
    }

    std::string funcScopeName = ownerClass.empty()
        ? ("function " + node.getName() + paramProfile)
        : ("function " + ownerClass + "::" + node.getName() + paramProfile);
    std::shared_ptr<SymbolTable> prev = _currentScope;
    _currentScope = getOrCreateChildScope(_currentScope, funcScopeName);

    for (const auto& param : node.getParams()) {
        SymbolEntry paramEntry;
        paramEntry.name = param->getName();
        paramEntry.type = param->getTypeName();
        paramEntry.dimensions = param->getDimensions();
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

/**
 * @brief Process class declaration and class-member semantic rules.
 * @details
 * Pass 1 registers class scope and fields before methods.
 * Pass 2 validates member bodies/usages.
 */
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

        if (isPassOne()) {
            auto method = std::dynamic_pointer_cast<FuncDefNode>(member);
            if (method != nullptr) {
                const SymbolEntry* classEntry = _globalScope->lookupInCurrent(node.getName());
                if (classEntry != nullptr) {
                    for (const auto& parentName : parseParentsFromClassType(classEntry->type)) {
                        auto parentIt = _classScopes.find(parentName);
                        if (parentIt != _classScopes.end() && parentIt->second != nullptr) {
                            const SymbolEntry* inheritedFn = parentIt->second->lookupInCurrent(method->getName());
                            if (inheritedFn != nullptr && inheritedFn->kind == SymbolKind::Function && inheritedFn->type == functionSignature(*method)) {
                                reportWarning(method->getLineNumber(), "9.3 overridden member function: '" + node.getName() + "::" + method->getName() + "'");
                            }
                        }
                    }
                }
            }
        }

        visitNode(member);
    }

    _currentScope = prev;
}

/** @brief Program-level traversal entry: classes then functions. */
void SemanticAnalyzer::visit(ProgNode& node) {
    for (const auto& cls : node.getClasses()) {
        visitNode(cls);
    }

    for (const auto& function : node.getFunctions()) {
        visitNode(function);
    }
}

/** @brief Append semantic error diagnostic with standardized prefix. */
void SemanticAnalyzer::reportError(int line, const std::string& message) {
    _errors.push_back("[ERROR][SEMANTIC] line " + std::to_string(line) + ": " + message);
}

/** @brief Append semantic warning diagnostic with standardized prefix. */
void SemanticAnalyzer::reportWarning(int line, const std::string& message) {
    _warnings.push_back("[WARNING][SEMANTIC] line " + std::to_string(line) + ": " + message);
}

/**
 * @brief Define symbol in current scope with language-specific redeclaration policy.
 * @param entry Symbol to define.
 * @return True if accepted (defined or pass-2-safe reuse), false if rejected.
 */
bool SemanticAnalyzer::defineSymbol(const SymbolEntry& entry) {
    if (_currentScope == nullptr) {
        return false;
    }

    if (_currentScope->define(entry)) {
        return true;
    }

    // Pass 2 reuses pass-1 symbol tables; repeated declarations in the same
    // scope should not emit duplicate redefinition diagnostics for symbols
    // already registered in pass 1 (e.g., class fields or function declarations).
    // However, local variables and parameters are always errors in pass 2.
    if (!isPassOne() && entry.kind != SymbolKind::Variable && entry.kind != SymbolKind::Parameter) {
        const SymbolEntry* existing = _currentScope->lookupInCurrent(entry.name);
        if (existing != nullptr) {
            return true;
        }
    }

    const std::string scopeName = _currentScope->getScopeName();
    const std::string userFriendlyScope = getUserFriendlyScopeName(_currentScope);
    if (entry.kind == SymbolKind::Class) {
        reportError(entry.line, "8.1 multiply declared class: '" + entry.name + "'");
    } else if (entry.kind == SymbolKind::Function && scopeName == "global") {
        reportError(entry.line, "8.2 multiply declared free function: '" + entry.name + "'");
    } else if (entry.kind == SymbolKind::Field && scopeName.rfind("class ", 0) == 0) {
        reportError(entry.line, "8.3 multiply declared data member in class: '" + entry.name + "'");
    } else if (entry.kind == SymbolKind::Variable || entry.kind == SymbolKind::Parameter) {
        reportError(entry.line, "8.4 multiply declared variable '" + entry.name + "' in scope '" + userFriendlyScope + "'");
    } else {
        reportError(entry.line, "redefinition of symbol '" + entry.name + "' in scope '" + userFriendlyScope + "'");
    }
    return false;
}

/** @brief Null-safe node visitation helper. */
void SemanticAnalyzer::visitNode(const std::shared_ptr<ASTNode>& node) {
    if (node != nullptr) {
        node->accept(*this);
    }
}

/**
 * @brief Reuse matching child scope (pass 2) or create fresh child scope (pass 1).
 * @param parent Parent scope.
 * @param scopeName Target child scope label.
 * @return Matching or newly created child scope.
 */
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

/** @brief Convert SymbolKind to printable lowercase text label. */
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

/** @brief Build normalized signature text for function declaration comparison. */
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

/** @brief Build class descriptor including optional inheritance list. */
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

/**
 * @brief Extract base type token from signature/array decorated type string.
 * @param typeName Raw type string.
 * @return Base type.
 */
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

/**
 * @brief Resolve member symbol in class scope by class type name.
 * @param classTypeName Class type string.
 * @param memberName Member identifier.
 * @return Matching member symbol or null.
 */
const SymbolEntry* SemanticAnalyzer::resolveClassMember(const std::string& classTypeName, const std::string& memberName) const {
    const std::string base = baseTypeName(classTypeName);
    auto it = _classScopes.find(base);
    if (it == _classScopes.end() || it->second == nullptr) {
        return nullptr;
    }
    return it->second->lookupInCurrent(memberName);
}

/**
 * @brief Infer expression result type for semantic compatibility checks.
 * @param node Expression node.
 * @return Inferred type string, or "null" when unresolved.
 */
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

/**
 * @brief Recursively emit formatted symbol-table scopes as aligned text tables.
 * @param scope Scope root for current dump recursion.
 * @param depth Nesting depth used for indentation.
 * @param out Accumulated output buffer.
 */
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

    // Minimum column widths (ensures headers always fit)
    size_t kindWidth = 8;       // "Function"
    size_t nameWidth = 12;
    size_t typeWidth = 18;
    size_t visibilityWidth = 10;// "Visibility"
    size_t lineWidth = 4;       // "Line"
    size_t detailsWidth = 20;

    for (const auto* entry : entries) {
        kindWidth = std::max(kindWidth, kindToString(entry->kind).size());
        nameWidth = std::max(nameWidth, entry->name.size());
        typeWidth = std::max(typeWidth, entry->type.size());
        visibilityWidth = std::max(visibilityWidth, entry->visibility.size());
        lineWidth = std::max(lineWidth, std::to_string(entry->line).size());
        detailsWidth = std::max(detailsWidth, entry->details.size());
    }

    // Use a single string stream for the entire table to prevent memory thrashing
    std::ostringstream tableStream;
    tableStream << indent << "Scope: " << scope->getScopeName() << "\n";

    // Lambda: Draws a separator line using zero-allocation setfill
    auto makeSeparator = [&](char ch) {
        tableStream << indent << '+' << std::setfill(ch)
                    << std::setw(static_cast<int>(kindWidth + 2)) << "" << '+'
                    << std::setw(static_cast<int>(nameWidth + 2)) << "" << '+'
                    << std::setw(static_cast<int>(typeWidth + 2)) << "" << '+'
                    << std::setw(static_cast<int>(visibilityWidth + 2)) << "" << '+'
                    << std::setw(static_cast<int>(lineWidth + 2)) << "" << '+'
                    << std::setw(static_cast<int>(detailsWidth + 2)) << "" << "+\n"
                    << std::setfill(' '); // reset fill character to space
    };

    // Lambda: Formats and streams a row directly
    auto formatRow = [&](const std::string& kind, const std::string& name, const std::string& type, 
                         const std::string& visibility, const std::string& line, const std::string& details) {
        tableStream << indent << "| " 
                    << std::left << std::setw(static_cast<int>(kindWidth)) << kind << " | "
                    << std::left << std::setw(static_cast<int>(nameWidth)) << name << " | "
                    << std::left << std::setw(static_cast<int>(typeWidth)) << type << " | "
                    << std::left << std::setw(static_cast<int>(visibilityWidth)) << visibility << " | "
                    << std::right << std::setw(static_cast<int>(lineWidth)) << line << " | "
                    << std::left << std::setw(static_cast<int>(detailsWidth)) << details << " |\n";
    };

    makeSeparator('-');
    formatRow("Kind", "Name", "Type", "Visibility", "Line", "Details");
    makeSeparator('=');

    if (entries.empty()) {
        formatRow("<none>", "-", "-", "-", "-", "-");
    } else {
        for (const auto* entry : entries) {
            formatRow(
                kindToString(entry->kind),
                entry->name,
                entry->type,
                entry->visibility,
                std::to_string(entry->line),
                entry->details
            );
        }
    }

    makeSeparator('-');
    tableStream << "\n";
    
    // Append the fully constructed table to the output string once
    out += tableStream.str();

    std::unordered_set<const SymbolTable*> visited;

    // Cleaner standard library lookup
    auto findChildByName = [&](const std::string& expectedName) -> std::shared_ptr<SymbolTable> {
        auto it = std::find_if(children.begin(), children.end(), [&](const auto& child) {
            return child != nullptr && child->getScopeName() == expectedName;
        });
        return it != children.end() ? *it : nullptr;
    };

    // Organized Child Scope Traversal
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
                std::shared_ptr<SymbolTable> fnChild = findChildByName("function " + entry->name);
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
            std::shared_ptr<SymbolTable> fnChild = findChildByName("function " + className + "::" + entry->name);
            if (fnChild == nullptr) {
                fnChild = findChildByName("function " + entry->name);
            }
            if (fnChild != nullptr && !visited.count(fnChild.get())) {
                visited.insert(fnChild.get());
                dumpScope(fnChild, depth + 1, out);
            }
        }
    }

    // Traverse any remaining unvisited children (e.g., nested blocks)
    for (const auto& child : children) {
        if (child != nullptr && !visited.count(child.get())) {
            dumpScope(child, depth + 1, out);
        }
    }
}