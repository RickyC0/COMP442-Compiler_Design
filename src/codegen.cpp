#include "../include/codegen.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <cctype>

namespace {
    std::string regName(int reg) {
        return "r" + std::to_string(reg);
    }

    std::string trimCopy(const std::string& raw) {
        size_t start = 0;
        while (start < raw.size() && std::isspace(static_cast<unsigned char>(raw[start]))) {
            ++start;
        }

        size_t end = raw.size();
        while (end > start && std::isspace(static_cast<unsigned char>(raw[end - 1]))) {
            --end;
        }

        return raw.substr(start, end - start);
    }

    bool isBasicScalarType(const std::string& typeName) {
        return typeName == "integer" || typeName == "float" || typeName == "bool";
    }

    bool hasUnspecifiedDimension(const std::vector<int>& dimensions) {
        for (int dim : dimensions) {
            if (dim <= 0) {
                return true;
            }
        }
        return false;
    }

    bool startsWith(const std::string& text, const std::string& prefix) {
        return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
    }

    std::string dimensionsToString(const std::vector<int>& dimensions) {
        if (dimensions.empty()) {
            return "scalar";
        }

        std::ostringstream oss;
        for (int dim : dimensions) {
            if (dim > 0) {
                oss << "[" << dim << "]";
            } else {
                oss << "[]";
            }
        }
        return oss.str();
    }

    std::string summarizeNode(const std::shared_ptr<ASTNode>& node) {
        if (node == nullptr) {
            return "<null>";
        }

        if (auto id = std::dynamic_pointer_cast<IdNode>(node)) {
            return "id:" + id->getName();
        }

        if (auto lit = std::dynamic_pointer_cast<IntLitNode>(node)) {
            return "int:" + std::to_string(lit->getIntValue());
        }

        if (auto lit = std::dynamic_pointer_cast<FloatLitNode>(node)) {
            return "float:" + std::to_string(lit->getFloatValue());
        }

        if (auto op = std::dynamic_pointer_cast<BinaryOpNode>(node)) {
            return "binary:" + op->getOperator();
        }

        if (auto op = std::dynamic_pointer_cast<UnaryOpNode>(node)) {
            return "unary:" + op->getOperator();
        }

        if (auto member = std::dynamic_pointer_cast<DataMemberNode>(node)) {
            std::ostringstream oss;
            oss << "member:" << member->getName();
            if (!member->getIndices().empty()) {
                oss << " idx=" << member->getIndices().size();
            }
            if (member->getLeft() != nullptr) {
                oss << " owner(" << summarizeNode(member->getLeft()) << ")";
            }
            return oss.str();
        }

        if (auto call = std::dynamic_pointer_cast<FuncCallNode>(node)) {
            std::ostringstream oss;
            oss << "call:" << call->getFunctionName() << " args=" << call->getArgs().size();
            return oss.str();
        }

        if (std::dynamic_pointer_cast<AssignStmtNode>(node) != nullptr) {
            return "stmt:assign";
        }
        if (std::dynamic_pointer_cast<IfStmtNode>(node) != nullptr) {
            return "stmt:if";
        }
        if (std::dynamic_pointer_cast<WhileStmtNode>(node) != nullptr) {
            return "stmt:while";
        }
        if (auto io = std::dynamic_pointer_cast<IOStmtNode>(node)) {
            return "stmt:" + io->getValue();
        }
        if (std::dynamic_pointer_cast<ReturnStmtNode>(node) != nullptr) {
            return "stmt:return";
        }
        if (std::dynamic_pointer_cast<BlockNode>(node) != nullptr) {
            return "stmt:block";
        }

        return node->getValue();
    }

    constexpr bool isPowerOfTen(int value) { // USED LATER IN CODEGEN FOR VALIDATING FLOAT SCALE CONSTANT
        if (value < 1) {
            return false;
        }

        while ((value % 10) == 0) {
            value /= 10;
        }

        return value == 1;
    }

    constexpr int kFloatScale = 100; //must be a power of 10 to avoid precision issues in scaled integer representation. USE THIS CONSTANT ACCROSS ALL CODEGEN LOGIC FOR CONSISTENCY
    constexpr int kFloatFirstFractionPlace = kFloatScale / 10;
    static_assert(kFloatScale >= 10 && isPowerOfTen(kFloatScale), "kFloatScale must be a power of ten >= 10");
}

CodeGenVisitor::RegisterAllocator::RegisterAllocator() {
    reset();
}

int CodeGenVisitor::RegisterAllocator::acquire() {
    if (_freeRegs.empty()) {
        return -1;
    }

    const int reg = _freeRegs.back();
    _freeRegs.pop_back();
    return reg;
}

void CodeGenVisitor::RegisterAllocator::release(int reg) {
    if (reg <= 0 || reg > 12) {
        return;
    }

    if (std::find(_freeRegs.begin(), _freeRegs.end(), reg) == _freeRegs.end()) {
        _freeRegs.push_back(reg);
    }
}

void CodeGenVisitor::RegisterAllocator::reset() {
    _freeRegs.clear();
    for (int reg = 12; reg >= 1; --reg) {
        _freeRegs.push_back(reg);
    }
}

CodeGenVisitor::CodeGenVisitor(std::ostream& out)
    : _out(out) {}

bool CodeGenVisitor::generate(const std::shared_ptr<ProgNode>& root) {
    _errors.clear();
    _labelCounter = 0;
    _traceEmitCounter = 0;
    _traceSourceLine = 0;
    _traceContextTag.clear();

    if (root == nullptr) {
        reportError(0, "cannot generate code from an empty AST");
        return false;
    }

    emitComment("Moon assembly generated by CodeGenVisitor");
    setTraceContext(0, "BOOT");
    emitComment("[BOOT] entry + stack base initialization");
    emit("entry");
    emit("addi r14, r0, topaddr");

    root->accept(*this);

    emitComment("[BOOT] halt after program end label");
    emit("hlt");
    emitRuntimeIntegerIO();

    return _errors.empty();
}

const std::vector<std::string>& CodeGenVisitor::getErrors() const {
    return _errors;
}

void CodeGenVisitor::emit(const std::string& line) {
    const bool isComment = !line.empty() && line[0] == '%';
    if (!isComment && !line.empty()) {
        ++_traceEmitCounter;

        std::ostringstream trace;
        trace << "[E" << _traceEmitCounter << "]";
        if (_traceSourceLine > 0) {
            trace << "[L" << _traceSourceLine << "]";
        } else {
            trace << "[RT]";
        }

        if (!_traceContextTag.empty()) {
            trace << "[" << _traceContextTag << "]";
        }

        trace << " " << line;
        _out << "% " << trace.str() << "\n";
    }

    _out << line << "\n";
}

void CodeGenVisitor::emitComment(const std::string& message) {
    // Keep Moon output readable by suppressing repetitive low-value traces.
    if (startsWith(message, "semantic step:")) {
        return;
    }
    if (startsWith(message, "  block stmt line=")) {
        return;
    }
    if (startsWith(message, "  assignment mode:")) {
        return;
    }
    if (startsWith(message, "  fixed-point promotion active")) {
        return;
    }

    emit("% " + message);
}

void CodeGenVisitor::emitSourceLineContext(int line, const std::string& message) {
    std::string tag = message;
    if (!message.empty() && message[0] == '[') {
        const size_t close = message.find(']');
        if (close != std::string::npos && close > 1) {
            tag = message.substr(1, close - 1);
        }
    }

    setTraceContext(line, tag);

    if (line > 0) {
        emitComment("[L" + std::to_string(line) + "] " + message);
    } else {
        emitComment("[RT] " + message);
    }
}

void CodeGenVisitor::setTraceContext(int line, const std::string& contextTag) {
    _traceSourceLine = line;
    _traceContextTag = contextTag;
}

void CodeGenVisitor::reportError(int line, const std::string& message) {
    _errors.push_back("[ERROR][CODEGEN] line " + std::to_string(line) + ": " + message);
}

std::string CodeGenVisitor::sanitizeName(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());

    for (char c : raw) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            result.push_back(c);
        } else {
            result.push_back('_');
        }
    }

    if (result.empty()) {
        return "anon";
    }

    return result;
}

std::string CodeGenVisitor::makeLabel(const std::string& prefix) {
    return sanitizeName(prefix) + "_" + std::to_string(++_labelCounter);
}

std::string CodeGenVisitor::functionKey(const std::string& className, const std::string& functionName) {
    if (className.empty()) {
        return functionName;
    }
    return className + "::" + functionName;
}

void CodeGenVisitor::resetFunctionState() {
    _stackOffsets.clear();
    _stackVarInfo.clear();
    _nextOffset = 0;
    _regs.reset();
    _lastExprReg = -1;
}

bool CodeGenVisitor::buildFunctionLayout(const std::shared_ptr<FuncDefNode>& functionNode) {
    if (functionNode == nullptr || functionNode->getRight() == nullptr) {
        return false;
    }

    FunctionLayoutInfo layout;
    layout.className = trimCopy(functionNode->getClassName());
    layout.name = functionNode->getName();
    layout.returnType = trimCopy(functionNode->getReturnType());
    layout.isMethod = !layout.className.empty();
    layout.key = functionKey(layout.className, layout.name);
    layout.label = "fn_" + sanitizeName(layout.key);

    long cursor = -4; // saved return link
    layout.returnLinkOffset = -4;

    if (layout.isMethod) {
        cursor -= 4;
        layout.thisOffset = cursor;
    }

    for (const auto& param : functionNode->getParams()) {
        const long paramSize = sizeOfVar(param);
        if (paramSize <= 0) {
            return false;
        }

        cursor -= paramSize;
        layout.paramNames.push_back(param->getName());
        layout.paramOffsets.push_back(cursor);
        layout.varOffsets[param->getName()] = cursor;

        const bool isArrayParam = !param->getDimensions().empty();
        long paramElementSize = sizeOfType(param->getTypeName(), param->getLineNumber());
        if (paramElementSize <= 0) {
            return false;
        }

        layout.varInfo[param->getName()] = {
            trimCopy(param->getTypeName()),
            param->getDimensions(),
            paramElementSize,
            isArrayParam
        };
    }

    for (const auto& local : functionNode->getLocalVars()) {
        const long localSize = sizeOfVar(local);
        if (localSize <= 0) {
            return false;
        }

        cursor -= localSize;
        layout.varOffsets[local->getName()] = cursor;

        const bool localHasUnspecifiedDim = hasUnspecifiedDimension(local->getDimensions());

        long localElementSize = 4;
        if (localHasUnspecifiedDim) {
            localElementSize = sizeOfType(local->getTypeName(), local->getLineNumber());
        } else {
            long localElements = 1;
            for (int dim : local->getDimensions()) {
                if (dim > 0) {
                    localElements *= dim;
                }
            }
            localElementSize = (localElements > 0) ? (localSize / localElements) : 4;
        }

        if (localElementSize <= 0) {
            localElementSize = 4;
        }

        layout.varInfo[local->getName()] = {
            trimCopy(local->getTypeName()),
            local->getDimensions(),
            localElementSize,
            localHasUnspecifiedDim
        };
    }

    layout.frameSize = -cursor;
    if (layout.frameSize < 4) {
        layout.frameSize = 4;
    }

    _functionLayouts[layout.key] = layout;
    return true;
}

const CodeGenVisitor::FunctionLayoutInfo* CodeGenVisitor::findFunctionLayout(const std::string& className, const std::string& functionName) const {
    const std::string key = functionKey(trimCopy(className), functionName);
    auto it = _functionLayouts.find(key);
    if (it == _functionLayouts.end()) {
        return nullptr;
    }
    return &it->second;
}

long CodeGenVisitor::sizeOfType(const std::string& typeName, int line) {
    const std::string cleanType = trimCopy(typeName);
    if (cleanType.empty()) {
        reportError(line, "cannot compute storage size for empty type name");
        return -1;
    }

    if (isBasicScalarType(cleanType)) {
        return 4;
    }

    std::unordered_set<std::string> visiting;
    return sizeOfClass(cleanType, visiting, line);
}

bool CodeGenVisitor::buildClassLayout(const std::string& className, std::unordered_set<std::string>& visiting, int line) {
    const std::string cleanClassName = trimCopy(className);

    if (_classLayouts.find(cleanClassName) != _classLayouts.end()) {
        return true;
    }

    if (visiting.find(cleanClassName) != visiting.end()) {
        reportError(line, "circular class storage dependency while building layout for '" + cleanClassName + "'");
        return false;
    }

    auto declIt = _classDecls.find(cleanClassName);
    if (declIt == _classDecls.end() || declIt->second == nullptr) {
        reportError(line, "unknown class type in storage allocation: '" + cleanClassName + "'");
        return false;
    }

    visiting.insert(cleanClassName);

    ClassLayoutInfo layout;
    long runningOffset = 0;

    for (const auto& parentName : declIt->second->getParents()) {
        if (!buildClassLayout(parentName, visiting, line)) {
            visiting.erase(cleanClassName);
            return false;
        }

        const ClassLayoutInfo& parentLayout = _classLayouts[parentName];
        for (const auto& kv : parentLayout.fields) {
            FieldLayoutInfo inherited = kv.second;
            inherited.offset += runningOffset;
            if (layout.fields.find(kv.first) == layout.fields.end()) {
                layout.fields[kv.first] = inherited;
            }
        }

        runningOffset += parentLayout.size;
    }

    for (const auto& member : declIt->second->getMembers()) {
        auto fieldDecl = std::dynamic_pointer_cast<VarDeclNode>(member);
        if (fieldDecl == nullptr) {
            continue;
        }

        const long fieldSize = sizeOfVar(fieldDecl);
        if (fieldSize <= 0) {
            visiting.erase(cleanClassName);
            return false;
        }

        FieldLayoutInfo field;
        field.typeName = trimCopy(fieldDecl->getTypeName());
        field.dimensions = fieldDecl->getDimensions();
        field.offset = runningOffset;
        field.size = fieldSize;

        layout.fields[fieldDecl->getName()] = field;
        runningOffset += fieldSize;
    }

    if (runningOffset <= 0) {
        runningOffset = 4;
    }

    layout.size = runningOffset;
    _classLayouts[cleanClassName] = layout;
    _classSizes[cleanClassName] = layout.size;

    visiting.erase(cleanClassName);
    return true;
}

long CodeGenVisitor::sizeOfClass(const std::string& className, std::unordered_set<std::string>& visiting, int line) {
    const std::string cleanClassName = trimCopy(className);

    auto knownSize = _classSizes.find(cleanClassName);
    if (knownSize != _classSizes.end()) {
        return knownSize->second;
    }

    if (!buildClassLayout(cleanClassName, visiting, line)) {
        return -1;
    }

    knownSize = _classSizes.find(cleanClassName);
    if (knownSize != _classSizes.end()) {
        return knownSize->second;
    }

    reportError(line, "failed to compute class size for '" + cleanClassName + "'");
    return -1;
}

bool CodeGenVisitor::lookupFieldLayout(const std::string& className, const std::string& fieldName, FieldLayoutInfo& out, int line) {
    const std::string cleanClassName = trimCopy(className);
    std::unordered_set<std::string> visiting;
    if (!buildClassLayout(cleanClassName, visiting, line)) {
        return false;
    }

    auto classIt = _classLayouts.find(cleanClassName);
    if (classIt == _classLayouts.end()) {
        reportError(line, "class layout missing for '" + cleanClassName + "'");
        return false;
    }

    auto fieldIt = classIt->second.fields.find(fieldName);
    if (fieldIt == classIt->second.fields.end()) {
        reportError(line, "class '" + cleanClassName + "' has no field '" + fieldName + "' in code generation");
        return false;
    }

    out = fieldIt->second;
    return true;
}

long CodeGenVisitor::sizeOfVar(const std::shared_ptr<VarDeclNode>& decl) {
    if (decl == nullptr) {
        return 0;
    }

    if (decl->getVisibility() == "param" && !decl->getDimensions().empty()) {
        return 4;
    }

    if (hasUnspecifiedDimension(decl->getDimensions())) {
        return 4;
    }

    long elements = 1;
    for (int dim : decl->getDimensions()) {
        elements *= dim;
    }

    const long elementSize = sizeOfType(decl->getTypeName(), decl->getLineNumber());
    if (elementSize <= 0) {
        return -1;
    }

    return elements * elementSize;
}

void CodeGenVisitor::assignOffsets(const std::vector<std::shared_ptr<VarDeclNode>>& vars) {
    for (const auto& var : vars) {
        if (var == nullptr) {
            continue;
        }

        const std::string name = var->getName();
        if (name.empty()) {
            continue;
        }

        const long varSize = sizeOfVar(var);
        if (varSize <= 0) {
            reportError(var->getLineNumber(), "invalid storage size for variable '" + name + "'");
            continue;
        }

        if (_stackOffsets.find(name) != _stackOffsets.end()) {
            reportError(var->getLineNumber(), "duplicate variable in codegen frame: '" + name + "'");
            continue;
        }

        _nextOffset -= varSize;
        _stackOffsets[name] = _nextOffset;

        long elementSize = 4;
        if (hasUnspecifiedDimension(var->getDimensions())) {
            elementSize = sizeOfType(var->getTypeName(), var->getLineNumber());
        } else {
            long elements = 1;
            for (int dim : var->getDimensions()) {
                if (dim > 0) {
                    elements *= dim;
                }
            }
            elementSize = (elements > 0) ? (varSize / elements) : 4;
        }

        if (elementSize <= 0) {
            elementSize = 4;
        }

        _stackVarInfo[name] = {
            trimCopy(var->getTypeName()),
            var->getDimensions(),
            elementSize,
            (var->getVisibility() == "param" && !var->getDimensions().empty()) || hasUnspecifiedDimension(var->getDimensions())
        };

        emitComment(
            "alloc " + name + " @ " + std::to_string(_nextOffset) + "(r14), size=" + std::to_string(varSize) +
            ", type=" + trimCopy(var->getTypeName()) + ", dims=" + dimensionsToString(var->getDimensions()) +
            ", elem=" + std::to_string(elementSize) + ", ref=" + (_stackVarInfo[name].isReferenceParam ? "yes" : "no")
        );
    }
}

bool CodeGenVisitor::hasOffset(const std::string& name) const {
    return _stackOffsets.find(name) != _stackOffsets.end();
}

long CodeGenVisitor::lookupOffset(const std::string& name) const {
    auto it = _stackOffsets.find(name);
    if (it == _stackOffsets.end()) {
        return 0;
    }

    return it->second;
}

int CodeGenVisitor::evalExpr(const std::shared_ptr<ASTNode>& node) {
    _lastExprReg = -1;

    if (node == nullptr) {
        reportError(0, "null expression in code generation");
        return -1;
    }

    node->accept(*this);
    return _lastExprReg;
}

bool CodeGenVisitor::loadThisPointerInto(int targetReg, int line) {
    if (targetReg < 0) {
        return false;
    }

    if (_currentClassName.empty() || _currentThisOffset == 0) {
        reportError(line, "implicit receiver is unavailable outside member function context");
        return false;
    }

    emitSourceLineContext(line, "[MEM] " + regName(targetReg) + " <- mem[" + std::to_string(_currentThisOffset) + "(r14)]  ; this");
    emit("lw " + regName(targetReg) + ", " + std::to_string(_currentThisOffset) + "(r14)");
    return true;
}

bool CodeGenVisitor::resolveDataMemberType(const std::shared_ptr<DataMemberNode>& node, std::string& typeName, std::vector<int>& dimensions) {
    if (node == nullptr) {
        return false;
    }

    if (node->getLeft() == nullptr) {
        auto infoIt = _stackVarInfo.find(node->getName());
        if (infoIt != _stackVarInfo.end()) {
            typeName = infoIt->second.typeName;
            dimensions = infoIt->second.dimensions;
        } else if (!_currentClassName.empty()) {
            FieldLayoutInfo fieldLayout;
            if (!lookupFieldLayout(_currentClassName, node->getName(), fieldLayout, node->getLineNumber())) {
                return false;
            }

            typeName = fieldLayout.typeName;
            dimensions = fieldLayout.dimensions;
        } else {
            reportError(node->getLineNumber(), "unknown variable '" + node->getName() + "' in type resolution");
            return false;
        }
    } else {
        std::string ownerType;
        std::vector<int> ownerDimensions;
        if (!resolveNodeType(node->getLeft(), ownerType, ownerDimensions)) {
            return false;
        }

        if (!ownerDimensions.empty()) {
            reportError(node->getLineNumber(), "member access requires scalar object owner for '" + node->getName() + "'");
            return false;
        }

        FieldLayoutInfo fieldLayout;
        if (!lookupFieldLayout(ownerType, node->getName(), fieldLayout, node->getLineNumber())) {
            return false;
        }

        typeName = fieldLayout.typeName;
        dimensions = fieldLayout.dimensions;
    }

    if (node->getIndices().size() > dimensions.size()) {
        reportError(node->getLineNumber(), "too many indices for member '" + node->getName() + "' in type resolution");
        return false;
    }

    dimensions.erase(dimensions.begin(), dimensions.begin() + static_cast<long long>(node->getIndices().size()));
    return true;
}

bool CodeGenVisitor::resolveNodeType(const std::shared_ptr<ASTNode>& node, std::string& typeName, std::vector<int>& dimensions) {
    if (node == nullptr) {
        return false;
    }

    if (auto idNode = std::dynamic_pointer_cast<IdNode>(node)) {
        auto infoIt = _stackVarInfo.find(idNode->getName());
        if (infoIt != _stackVarInfo.end()) {
            typeName = infoIt->second.typeName;
            dimensions = infoIt->second.dimensions;
            return true;
        }

        if (!_currentClassName.empty()) {
            FieldLayoutInfo fieldLayout;
            if (lookupFieldLayout(_currentClassName, idNode->getName(), fieldLayout, idNode->getLineNumber())) {
                typeName = fieldLayout.typeName;
                dimensions = fieldLayout.dimensions;
                return true;
            }
        }

        reportError(idNode->getLineNumber(), "unknown identifier '" + idNode->getName() + "' in type resolution");
        return false;
    }

    if (auto dataMember = std::dynamic_pointer_cast<DataMemberNode>(node)) {
        return resolveDataMemberType(dataMember, typeName, dimensions);
    }

    if (std::dynamic_pointer_cast<IntLitNode>(node) != nullptr) {
        typeName = "integer";
        dimensions.clear();
        return true;
    }

    if (std::dynamic_pointer_cast<FloatLitNode>(node) != nullptr) {
        typeName = "float";
        dimensions.clear();
        return true;
    }

    if (auto unaryNode = std::dynamic_pointer_cast<UnaryOpNode>(node)) {
        return resolveNodeType(unaryNode->getLeft(), typeName, dimensions);
    }

    if (auto binaryNode = std::dynamic_pointer_cast<BinaryOpNode>(node)) {
        std::string leftType;
        std::vector<int> leftDims;
        std::string rightType;
        std::vector<int> rightDims;
        if (!resolveNodeType(binaryNode->getLeft(), leftType, leftDims) || !resolveNodeType(binaryNode->getRight(), rightType, rightDims)) {
            return false;
        }

        const std::string op = binaryNode->getOperator();
        if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=" || op == "and" || op == "or" || op == "&&" || op == "||") {
            typeName = "bool";
        } else if (leftType == "float" || rightType == "float") {
            typeName = "float";
        } else {
            typeName = "integer";
        }

        dimensions.clear();
        return true;
    }

    if (auto callNode = std::dynamic_pointer_cast<FuncCallNode>(node)) {
        const FunctionLayoutInfo* targetLayout = nullptr;

        auto calleeMember = std::dynamic_pointer_cast<DataMemberNode>(callNode->getLeft());
        if (calleeMember != nullptr && calleeMember->getLeft() != nullptr) {
            std::string ownerType;
            std::vector<int> ownerDimensions;
            if (!resolveNodeType(calleeMember->getLeft(), ownerType, ownerDimensions)) {
                return false;
            }

            if (!ownerDimensions.empty()) {
                reportError(callNode->getLineNumber(), "method receiver must be a scalar object");
                return false;
            }

            targetLayout = findFunctionLayout(ownerType, callNode->getFunctionName());
        } else {
            targetLayout = findFunctionLayout("", callNode->getFunctionName());
            if (targetLayout == nullptr && !_currentClassName.empty()) {
                targetLayout = findFunctionLayout(_currentClassName, callNode->getFunctionName());
            }
        }

        if (targetLayout == nullptr) {
            reportError(callNode->getLineNumber(), "function call code generation target not found in type resolution: '" + callNode->getFunctionName() + "'");
            return false;
        }

        typeName = trimCopy(targetLayout->returnType);
        dimensions.clear();
        return true;
    }

    return false;
}

bool CodeGenVisitor::emitIndexOffsetIntoAddress(int addrReg,
                                                const std::vector<std::shared_ptr<ASTNode>>& indices,
                                                const std::vector<int>& declaredDimensions,
                                                long elementSize,
                                                int line) {
    if (indices.empty()) {
        return true;
    }

    emitSourceLineContext(
        line,
        "[ADDR] " + regName(addrReg) + " += indexExpr(" + std::to_string(indices.size()) +
        ") * " + std::to_string(elementSize) +
        "  dims=" + dimensionsToString(declaredDimensions)
    );

    if (indices.size() > declaredDimensions.size()) {
        reportError(line, "too many array indices in code generation");
        return false;
    }

    const int linearReg = _regs.acquire();
    if (linearReg < 0) {
        reportError(line, "register exhaustion while generating indexed address");
        return false;
    }

    emit("addi " + regName(linearReg) + ", r0, 0");

    for (size_t i = 0; i < indices.size(); ++i) {
        const int idxReg = evalExpr(indices[i]);
        if (idxReg < 0) {
            _regs.release(linearReg);
            return false;
        }

        long stride = 1;
        for (size_t j = i + 1; j < declaredDimensions.size(); ++j) {
            const int dim = declaredDimensions[j];
            if (dim <= 0) {
                continue;
            }
            stride *= dim;
        }

        if (stride != 1) {
            emit("muli " + regName(idxReg) + ", " + regName(idxReg) + ", " + std::to_string(stride));
        }

        emit("add " + regName(linearReg) + ", " + regName(linearReg) + ", " + regName(idxReg));
        _regs.release(idxReg);
    }

    if (elementSize != 1) {
        emit("muli " + regName(linearReg) + ", " + regName(linearReg) + ", " + std::to_string(elementSize));
    }

    emit("add " + regName(addrReg) + ", " + regName(addrReg) + ", " + regName(linearReg));
    _regs.release(linearReg);
    return true;
}

int CodeGenVisitor::emitAddressForLValue(const std::shared_ptr<ASTNode>& node, int line) {
    if (node == nullptr) {
        reportError(line, "null l-value in address generation");
        return -1;
    }

    if (auto idNode = std::dynamic_pointer_cast<IdNode>(node)) {
        if (hasOffset(idNode->getName())) {
            const int addrReg = _regs.acquire();
            if (addrReg < 0) {
                reportError(idNode->getLineNumber(), "register exhaustion while generating l-value address");
                return -1;
            }

            const auto infoIt = _stackVarInfo.find(idNode->getName());
            const bool isReferenceParam =
                infoIt != _stackVarInfo.end() && infoIt->second.isReferenceParam;

            if (isReferenceParam) {
                emitSourceLineContext(idNode->getLineNumber(), "[ADDR] " + regName(addrReg) + " <- mem[" + std::to_string(lookupOffset(idNode->getName())) + "(r14)]  ; ref param '" + idNode->getName() + "'");
                emit("lw " + regName(addrReg) + ", " + std::to_string(lookupOffset(idNode->getName())) + "(r14)");
            } else {
                emitSourceLineContext(idNode->getLineNumber(), "[ADDR] " + regName(addrReg) + " <- &" + idNode->getName() + " @ " + std::to_string(lookupOffset(idNode->getName())) + "(r14)");
                emit("addi " + regName(addrReg) + ", r14, " + std::to_string(lookupOffset(idNode->getName())));
            }
            return addrReg;
        }

        if (!_currentClassName.empty()) {
            FieldLayoutInfo fieldLayout;
            if (lookupFieldLayout(_currentClassName, idNode->getName(), fieldLayout, idNode->getLineNumber())) {
                const int addrReg = _regs.acquire();
                if (addrReg < 0) {
                    reportError(idNode->getLineNumber(), "register exhaustion while generating field address");
                    return -1;
                }

                if (!loadThisPointerInto(addrReg, idNode->getLineNumber())) {
                    _regs.release(addrReg);
                    return -1;
                }

                if (fieldLayout.offset != 0) {
                    emit("addi " + regName(addrReg) + ", " + regName(addrReg) + ", " + std::to_string(fieldLayout.offset));
                }

                emitSourceLineContext(idNode->getLineNumber(), "[ADDR] " + regName(addrReg) + " -> field '" + idNode->getName() + "' offset=" + std::to_string(fieldLayout.offset));
                return addrReg;
            }
        }

        reportError(idNode->getLineNumber(), "unknown variable '" + idNode->getName() + "' in code generation");
        return -1;
    }

    if (auto memberNode = std::dynamic_pointer_cast<DataMemberNode>(node)) {
        return emitAddressForDataMember(*memberNode);
    }

    reportError(line, "unsupported l-value expression in address generation");
    return -1;
}

int CodeGenVisitor::emitAddressForDataMember(DataMemberNode& node) {
    if (node.getLeft() == nullptr) {
        const bool isLocalOrParam = hasOffset(node.getName());
        if (isLocalOrParam) {
            const int addrReg = _regs.acquire();
            if (addrReg < 0) {
                reportError(node.getLineNumber(), "register exhaustion while generating l-value address");
                return -1;
            }

            std::vector<int> declaredDimensions;
            long elementSize = 4;
            bool isReferenceParam = false;
            auto infoIt = _stackVarInfo.find(node.getName());
            if (infoIt != _stackVarInfo.end()) {
                declaredDimensions = infoIt->second.dimensions;
                elementSize = infoIt->second.elementSize;
                isReferenceParam = infoIt->second.isReferenceParam;
            }

            if (isReferenceParam) {
                emitSourceLineContext(node.getLineNumber(), "[ADDR] " + regName(addrReg) + " <- mem[" + std::to_string(lookupOffset(node.getName())) + "(r14)]  ; ref '" + node.getName() + "'");
                emit("lw " + regName(addrReg) + ", " + std::to_string(lookupOffset(node.getName())) + "(r14)");
            } else {
                emitSourceLineContext(node.getLineNumber(), "[ADDR] " + regName(addrReg) + " <- &" + node.getName() + " @ " + std::to_string(lookupOffset(node.getName())) + "(r14)");
                emit("addi " + regName(addrReg) + ", r14, " + std::to_string(lookupOffset(node.getName())));
            }

            if (!emitIndexOffsetIntoAddress(addrReg, node.getIndices(), declaredDimensions, elementSize, node.getLineNumber())) {
                _regs.release(addrReg);
                return -1;
            }

            return addrReg;
        }

        if (!_currentClassName.empty()) {
            FieldLayoutInfo fieldLayout;
            if (!lookupFieldLayout(_currentClassName, node.getName(), fieldLayout, node.getLineNumber())) {
                return -1;
            }

            const int addrReg = _regs.acquire();
            if (addrReg < 0) {
                reportError(node.getLineNumber(), "register exhaustion while generating field address");
                return -1;
            }

            if (!loadThisPointerInto(addrReg, node.getLineNumber())) {
                _regs.release(addrReg);
                return -1;
            }

            if (fieldLayout.offset != 0) {
                emit("addi " + regName(addrReg) + ", " + regName(addrReg) + ", " + std::to_string(fieldLayout.offset));
            }

            emitSourceLineContext(node.getLineNumber(), "[ADDR] " + regName(addrReg) + " -> implicit field '" + node.getName() + "' offset=" + std::to_string(fieldLayout.offset));

            long fieldElementSize = sizeOfType(fieldLayout.typeName, node.getLineNumber());
            if (fieldElementSize <= 0) {
                fieldElementSize = 4;
            }

            if (!hasUnspecifiedDimension(fieldLayout.dimensions)) {
                long elementCount = 1;
                for (int dim : fieldLayout.dimensions) {
                    elementCount *= dim;
                }
                if (elementCount > 0) {
                    long computed = fieldLayout.size / elementCount;
                    if (computed > 0) {
                        fieldElementSize = computed;
                    }
                }
            }

            if (!emitIndexOffsetIntoAddress(addrReg, node.getIndices(), fieldLayout.dimensions, fieldElementSize, node.getLineNumber())) {
                _regs.release(addrReg);
                return -1;
            }

            return addrReg;
        }

        reportError(node.getLineNumber(), "unknown variable '" + node.getName() + "' in code generation");
        return -1;
    }

    const int ownerAddrReg = emitAddressForLValue(node.getLeft(), node.getLineNumber());
    if (ownerAddrReg < 0) {
        return -1;
    }
    emitSourceLineContext(node.getLineNumber(), "[ADDR] explicit owner for member '" + node.getName() + "' in " + regName(ownerAddrReg));

    std::string ownerType;
    std::vector<int> ownerDimensions;
    if (!resolveNodeType(node.getLeft(), ownerType, ownerDimensions)) {
        _regs.release(ownerAddrReg);
        return -1;
    }

    if (!ownerDimensions.empty()) {
        reportError(node.getLineNumber(), "member access requires scalar object owner for '" + node.getName() + "'");
        _regs.release(ownerAddrReg);
        return -1;
    }

    FieldLayoutInfo fieldLayout;
    if (!lookupFieldLayout(ownerType, node.getName(), fieldLayout, node.getLineNumber())) {
        _regs.release(ownerAddrReg);
        return -1;
    }

    if (fieldLayout.offset != 0) {
        emit("addi " + regName(ownerAddrReg) + ", " + regName(ownerAddrReg) + ", " + std::to_string(fieldLayout.offset));
    }

    long fieldElementSize = sizeOfType(fieldLayout.typeName, node.getLineNumber());
    if (fieldElementSize <= 0) {
        fieldElementSize = 4;
    }

    if (!hasUnspecifiedDimension(fieldLayout.dimensions)) {
        long elementCount = 1;
        for (int dim : fieldLayout.dimensions) {
            elementCount *= dim;
        }
        if (elementCount > 0) {
            long computed = fieldLayout.size / elementCount;
            if (computed > 0) {
                fieldElementSize = computed;
            }
        }
    }

    if (!emitIndexOffsetIntoAddress(ownerAddrReg, node.getIndices(), fieldLayout.dimensions, fieldElementSize, node.getLineNumber())) {
        _regs.release(ownerAddrReg);
        return -1;
    }

    return ownerAddrReg;
}

int CodeGenVisitor::emitAddressForObjectExpression(const std::shared_ptr<ASTNode>& node, int line) {
    if (node == nullptr) {
        reportError(line, "null object expression in code generation");
        return -1;
    }

    if (std::dynamic_pointer_cast<IdNode>(node) != nullptr ||
        std::dynamic_pointer_cast<DataMemberNode>(node) != nullptr) {
        return emitAddressForLValue(node, line);
    }

    if (std::dynamic_pointer_cast<FuncCallNode>(node) != nullptr) {
        return evalExpr(node);
    }

    reportError(line, "unsupported object expression in code generation; expected object variable/member or object-returning call");
    return -1;
}

bool CodeGenVisitor::emitCopyWords(int dstAddrReg, int srcAddrReg, long byteCount, int line) {
    if (dstAddrReg < 0 || srcAddrReg < 0) {
        return false;
    }

    if (byteCount <= 0) {
        reportError(line, "invalid aggregate copy size in code generation");
        return false;
    }

    if ((byteCount % 4) != 0) {
        reportError(line, "aggregate copy size is not word-aligned in code generation");
        return false;
    }

    const int tmpReg = _regs.acquire();
    if (tmpReg < 0) {
        reportError(line, "register exhaustion while copying aggregate value");
        return false;
    }

    emitSourceLineContext(
        line,
        "[MEM] memcpy " + std::to_string(byteCount) + "B : " + regName(dstAddrReg) + " <- " + regName(srcAddrReg)
    );

    for (long offset = 0; offset < byteCount; offset += 4) {
        emit("lw " + regName(tmpReg) + ", " + std::to_string(offset) + "(" + regName(srcAddrReg) + ")");
        emit("sw " + std::to_string(offset) + "(" + regName(dstAddrReg) + "), " + regName(tmpReg));
    }

    _regs.release(tmpReg);
    return true;
}

bool CodeGenVisitor::emitStoreTarget(const std::shared_ptr<ASTNode>& target, int valueReg) {
    if (target == nullptr || valueReg < 0) {
        return false;
    }

    if (std::dynamic_pointer_cast<IdNode>(target) == nullptr &&
        std::dynamic_pointer_cast<DataMemberNode>(target) == nullptr) {
        reportError(target->getLineNumber(), "unsupported assignment target in code generation");
        return false;
    }

    const int addrReg = emitAddressForLValue(target, target->getLineNumber());
    if (addrReg < 0) {
        return false;
    }

    emitSourceLineContext(
        target->getLineNumber(),
        "[MEM] mem[0(" + regName(addrReg) + ")] <- " + regName(valueReg) +
        "  target=" + summarizeNode(target)
    );
    emit("sw 0(" + regName(addrReg) + "), " + regName(valueReg));
    _regs.release(addrReg);
    return true;
}

void CodeGenVisitor::emitFunctionBody(const std::shared_ptr<FuncDefNode>& functionNode,
                                      const FunctionLayoutInfo& layout,
                                      bool isMainBody) {
    if (functionNode == nullptr) {
        return;
    }

    resetFunctionState();

    _currentFunction = functionNode->getName();
    _currentClassName = layout.className;
    _currentFrameSize = layout.frameSize;
    _currentThisOffset = layout.thisOffset;
    _currentReturnLabel = layout.label + "_ret";
    _currentReturnType = trimCopy(functionNode->getReturnType());

    _stackOffsets = layout.varOffsets;
    _stackVarInfo = layout.varInfo;

    emitSourceLineContext(functionNode->getLineNumber(), "[FUNC] begin body emission");
    emitComment("============================================================");
    emitComment("BEGIN FUNCTION: " + layout.key + " label=" + layout.label);
    emitComment("[FRAME] size=" + std::to_string(layout.frameSize) +
                "B, retLink=" + std::to_string(layout.returnLinkOffset) +
                ", method=" + std::string(layout.isMethod ? "yes" : "no"));

    if (!isMainBody) {
        emitComment("[FRAME] save caller return address to mem[" + std::to_string(layout.returnLinkOffset) + "(r14)]");
        emit(layout.label);
        emit("sw " + std::to_string(layout.returnLinkOffset) + "(r14), r15");
    }

    if (layout.isMethod) {
        emitComment("[FRAME] method receiver slot: mem[" + std::to_string(layout.thisOffset) + "(r14)]");
    }

    for (size_t i = 0; i < layout.paramNames.size(); ++i) {
        const std::string& paramName = layout.paramNames[i];
        const long paramOffset = layout.paramOffsets[i];
        auto infoIt = layout.varInfo.find(paramName);
        if (infoIt != layout.varInfo.end()) {
            emitComment("param " + paramName + " @ " + std::to_string(paramOffset) +
                        "(r14), type=" + infoIt->second.typeName +
                        ", dims=" + dimensionsToString(infoIt->second.dimensions) +
                        ", elem=" + std::to_string(infoIt->second.elementSize) +
                        ", ref=" + std::string(infoIt->second.isReferenceParam ? "yes" : "no"));
        } else {
            emitComment("param " + paramName + " @ " + std::to_string(paramOffset) + "(r14)");
        }
    }

    for (const auto& kv : layout.varOffsets) {
        if (std::find(layout.paramNames.begin(), layout.paramNames.end(), kv.first) != layout.paramNames.end()) {
            continue;
        }

        auto infoIt = layout.varInfo.find(kv.first);
        if (infoIt != layout.varInfo.end()) {
            emitComment("local " + kv.first + " @ " + std::to_string(kv.second) +
                        "(r14), type=" + infoIt->second.typeName +
                        ", dims=" + dimensionsToString(infoIt->second.dimensions) +
                        ", elem=" + std::to_string(infoIt->second.elementSize));
        } else {
            emitComment("local " + kv.first + " @ " + std::to_string(kv.second) + "(r14)");
        }
    }

    if (functionNode->getRight() != nullptr) {
        functionNode->getRight()->accept(*this);
    }

    if (!isMainBody) {
        emitComment("[FRAME] epilogue: restore r15 and return");
        emit(_currentReturnLabel);
        emit("lw r15, " + std::to_string(layout.returnLinkOffset) + "(r14)");
        emit("jr r15");
    }

    emitComment("END FUNCTION: " + layout.key);
    emitComment("============================================================");
}

void CodeGenVisitor::emitRuntimeIntegerIO() {
    emitSourceLineContext(0, "[RT_READINT] helper begin");
    emitComment("runtime helper: rt_readInt (returns parsed integer in r1)");
    emitComment("upgrade from PROVIDED_DOCS/A5/moon/samples/newlib.m: alias + align conventions");
    emit("align");
    emit("getint");
    emit("j rt_readInt");
    emit("align");
    emit("rt_readInt");
    emit("addi r1, r0, 0");
    emit("addi r2, r0, 1");
    emit("rt_readInt_skip_ws");
    emit("getc r3");
    emit("andi r3, r3, 255");
    emit("ceqi r4, r3, 32");
    emit("bz r4, rt_readInt_check_tab");
    emit("j rt_readInt_skip_ws");
    emit("rt_readInt_check_tab");
    emit("ceqi r4, r3, 9");
    emit("bz r4, rt_readInt_check_lf");
    emit("j rt_readInt_skip_ws");
    emit("rt_readInt_check_lf");
    emit("ceqi r4, r3, 10");
    emit("bz r4, rt_readInt_check_cr");
    emit("j rt_readInt_skip_ws");
    emit("rt_readInt_check_cr");
    emit("ceqi r4, r3, 13");
    emit("bz r4, rt_readInt_check_sign");
    emit("j rt_readInt_skip_ws");
    emit("rt_readInt_check_sign");
    emit("ceqi r4, r3, 45");
    emit("bz r4, rt_readInt_check_plus");
    emit("addi r2, r0, -1");
    emit("getc r3");
    emit("andi r3, r3, 255");
    emit("j rt_readInt_digit_loop");
    emit("rt_readInt_check_plus");
    emit("ceqi r4, r3, 43");
    emit("bz r4, rt_readInt_digit_loop");
    emit("getc r3");
    emit("andi r3, r3, 255");
    emit("rt_readInt_digit_loop");
    emit("addi r6, r0, 48");
    emit("addi r7, r0, 57");
    emit("clt r4, r3, r6");
    emit("bz r4, rt_readInt_check_upper");
    emit("j rt_readInt_apply_sign");
    emit("rt_readInt_check_upper");
    emit("cgt r4, r3, r7");
    emit("bz r4, rt_readInt_consume_digit");
    emit("j rt_readInt_apply_sign");
    emit("rt_readInt_consume_digit");
    emit("muli r1, r1, 10");
    emit("sub r8, r3, r6");
    emit("add r1, r1, r8");
    emit("getc r3");
    emit("andi r3, r3, 255");
    emit("j rt_readInt_digit_loop");
    emit("rt_readInt_apply_sign");
    emit("clt r4, r2, r0");
    emit("bz r4, rt_readInt_ret");
    emit("sub r1, r0, r1");
    emit("rt_readInt_ret");
    emit("jr r15");

    emitSourceLineContext(0, "[RT_READFLOAT] helper begin");
    emitComment("runtime helper: rt_readFloat (returns fixed-point float in r1, scale=" + std::to_string(kFloatScale) + ")");
    emit("rt_readFloat");
    emit("addi r1, r0, 0");
    emit("addi r2, r0, 1");
    emit("rt_readFloat_skip_ws");
    emit("getc r3");
    emit("andi r3, r3, 255");
    emit("ceqi r4, r3, 32");
    emit("bz r4, rt_readFloat_check_tab");
    emit("j rt_readFloat_skip_ws");
    emit("rt_readFloat_check_tab");
    emit("ceqi r4, r3, 9");
    emit("bz r4, rt_readFloat_check_lf");
    emit("j rt_readFloat_skip_ws");
    emit("rt_readFloat_check_lf");
    emit("ceqi r4, r3, 10");
    emit("bz r4, rt_readFloat_check_cr");
    emit("j rt_readFloat_skip_ws");
    emit("rt_readFloat_check_cr");
    emit("ceqi r4, r3, 13");
    emit("bz r4, rt_readFloat_check_sign");
    emit("j rt_readFloat_skip_ws");
    emit("rt_readFloat_check_sign");
    emit("ceqi r4, r3, 45");
    emit("bz r4, rt_readFloat_check_plus");
    emit("addi r2, r0, -1");
    emit("getc r3");
    emit("andi r3, r3, 255");
    emit("j rt_readFloat_int_loop");
    emit("rt_readFloat_check_plus");
    emit("ceqi r4, r3, 43");
    emit("bz r4, rt_readFloat_int_loop");
    emit("getc r3");
    emit("andi r3, r3, 255");
    emit("rt_readFloat_int_loop");
    emit("addi r6, r0, 48");
    emit("addi r7, r0, 57");
    emit("clt r4, r3, r6");
    emit("bz r4, rt_readFloat_int_check_upper");
    emit("j rt_readFloat_maybe_frac");
    emit("rt_readFloat_int_check_upper");
    emit("cgt r4, r3, r7");
    emit("bz r4, rt_readFloat_int_consume");
    emit("j rt_readFloat_maybe_frac");
    emit("rt_readFloat_int_consume");
    emit("muli r1, r1, 10");
    emit("sub r8, r3, r6");
    emit("add r1, r1, r8");
    emit("getc r3");
    emit("andi r3, r3, 255");
    emit("j rt_readFloat_int_loop");
    emit("rt_readFloat_maybe_frac");
    emit("addi r11, r0, 0");
    emit("addi r10, r0, 0");
    emit("muli r1, r1, " + std::to_string(kFloatScale));
    emit("ceqi r4, r3, 46");
    emit("bz r4, rt_readFloat_apply_sign");
    emit("getc r3");
    emit("andi r3, r3, 255");
    emit("addi r11, r0, " + std::to_string(kFloatFirstFractionPlace));
    emit("addi r10, r0, 0");
    emit("rt_readFloat_frac_loop");
    emit("clt r4, r3, r6");
    emit("bz r4, rt_readFloat_frac_check_upper");
    emit("j rt_readFloat_apply_sign");
    emit("rt_readFloat_frac_check_upper");
    emit("cgt r4, r3, r7");
    emit("bz r4, rt_readFloat_frac_consume");
    emit("j rt_readFloat_apply_sign");
    emit("rt_readFloat_frac_consume");
    emit("ceqi r4, r11, 0");
    emit("bz r4, rt_readFloat_frac_store");
    emit("getc r3");
    emit("andi r3, r3, 255");
    emit("j rt_readFloat_frac_loop");
    emit("rt_readFloat_frac_store");
    emit("sub r8, r3, r6");
    emit("mul r8, r8, r11");
    emit("add r10, r10, r8");
    emit("ceqi r4, r11, 1");
    emit("bz r4, rt_readFloat_frac_scale_down");
    emit("addi r11, r0, 0");
    emit("j rt_readFloat_frac_next");
    emit("rt_readFloat_frac_scale_down");
    emit("divi r11, r11, 10");
    emit("rt_readFloat_frac_next");
    emit("getc r3");
    emit("andi r3, r3, 255");
    emit("j rt_readFloat_frac_loop");
    emit("rt_readFloat_apply_sign");
    emit("add r1, r1, r10");
    emit("clt r4, r2, r0");
    emit("bz r4, rt_readFloat_ret");
    emit("sub r1, r0, r1");
    emit("rt_readFloat_ret");
    emit("jr r15");

    emitSourceLineContext(0, "[RT_WRITEINT] helper begin");
    emitComment("runtime helper: rt_writeInt (prints integer from r1)");
    emitComment("upgrade from PROVIDED_DOCS/A5/moon/samples/newlib.m: buffer-based decimal emission");
    emit("align");
    emit("putint");
    emit("j rt_writeInt");
    emit("align");
    emit("rt_writeInt");
    emit("add r2, r0, r0");
    emit("add r3, r0, r0");
    emit("addi r4, r0, rt_writeInt_endbuf");
    emit("cge r5, r1, r0");
    emit("bnz r5, rt_writeInt_digits");
    emit("addi r3, r0, 1");
    emit("sub r1, r0, r1");
    emit("rt_writeInt_digits");
    emit("modi r2, r1, 10");
    emit("addi r2, r2, 48");
    emit("subi r4, r4, 1");
    emit("sb 0(r4), r2");
    emit("divi r1, r1, 10");
    emit("bnz r1, rt_writeInt_digits");
    emit("bz r3, rt_writeInt_emit");
    emit("addi r2, r0, 45");
    emit("subi r4, r4, 1");
    emit("sb 0(r4), r2");
    emit("rt_writeInt_emit");
    emit("lb r2, 0(r4)");
    emit("putc r2");
    emit("addi r4, r4, 1");
    emit("cgei r5, r4, rt_writeInt_endbuf");
    emit("bz r5, rt_writeInt_emit");
    emit("jr r15");
    emit("rt_writeInt_buf res 20");
    emit("rt_writeInt_endbuf");

    emitSourceLineContext(0, "[RT_WRITEFLOAT] helper begin");
    emitComment("runtime helper: rt_writeFloat (prints fixed-point float from r1, scale=" + std::to_string(kFloatScale) + ")");
    emit("rt_writeFloat");
    emit("add r2, r1, r0");
    emit("clt r3, r2, r0");
    emit("bz r3, rt_writeFloat_abs_ready");
    emit("addi r4, r0, 45");
    emit("putc r4");
    emit("sub r2, r0, r2");
    emit("rt_writeFloat_abs_ready");
    emit("divi r5, r2, " + std::to_string(kFloatScale));
    emit("modi r6, r2, " + std::to_string(kFloatScale));
    emit("add r11, r6, r0");
    emit("add r10, r15, r0");
    emit("add r1, r5, r0");
    emit("jl r15, rt_writeInt");
    emit("add r15, r10, r0");
    emit("add r6, r11, r0");
    emit("addi r4, r0, 46");
    emit("putc r4");
    for (int divisor = kFloatFirstFractionPlace; divisor > 0; divisor /= 10) {
        emit("divi r7, r6, " + std::to_string(divisor));
        emit("addi r7, r7, 48");
        emit("putc r7");
        if (divisor > 1) {
            emit("modi r6, r6, " + std::to_string(divisor));
        }
    }
    emit("jr r15");
}

void CodeGenVisitor::visit(IdNode& node) {
    if (hasOffset(node.getName())) {
        int reg = _regs.acquire();
        if (reg < 0) {
            reportError(node.getLineNumber(), "register exhaustion while loading identifier");
            _lastExprReg = -1;
            return;
        }

        emitSourceLineContext(
            node.getLineNumber(),
            "[REG] " + regName(reg) + " <- mem[" + std::to_string(lookupOffset(node.getName())) + "(r14)]  ; id=" + node.getName()
        );
        emit("lw " + regName(reg) + ", " + std::to_string(lookupOffset(node.getName())) + "(r14)");
        _lastExprReg = reg;
        return;
    }

    if (!_currentClassName.empty()) {
        FieldLayoutInfo fieldLayout;
        if (lookupFieldLayout(_currentClassName, node.getName(), fieldLayout, node.getLineNumber())) {
            const int addrReg = _regs.acquire();
            const int valueReg = _regs.acquire();
            if (addrReg < 0 || valueReg < 0) {
                if (addrReg > 0) {
                    _regs.release(addrReg);
                }
                if (valueReg > 0) {
                    _regs.release(valueReg);
                }
                reportError(node.getLineNumber(), "register exhaustion while loading member field");
                _lastExprReg = -1;
                return;
            }

            if (!loadThisPointerInto(addrReg, node.getLineNumber())) {
                _regs.release(addrReg);
                _regs.release(valueReg);
                _lastExprReg = -1;
                return;
            }

            if (fieldLayout.offset != 0) {
                emit("addi " + regName(addrReg) + ", " + regName(addrReg) + ", " + std::to_string(fieldLayout.offset));
            }

            emitSourceLineContext(
                node.getLineNumber(),
                "[REG] " + regName(valueReg) + " <- mem[0(" + regName(addrReg) + ")]  ; field=" + node.getName()
            );
            emit("lw " + regName(valueReg) + ", 0(" + regName(addrReg) + ")");
            _regs.release(addrReg);
            _lastExprReg = valueReg;
            return;
        }
    }

    reportError(node.getLineNumber(), "unknown identifier in code generation: '" + node.getName() + "'");
    _lastExprReg = -1;
}

void CodeGenVisitor::visit(IntLitNode& node) {
    int reg = _regs.acquire();
    if (reg < 0) {
        reportError(node.getLineNumber(), "register exhaustion while loading integer literal");
        _lastExprReg = -1;
        return;
    }

    emitSourceLineContext(node.getLineNumber(), "[REG] " + regName(reg) + " <- imm " + std::to_string(node.getIntValue()));
    emit("addi " + regName(reg) + ", r0, " + std::to_string(node.getIntValue()));
    _lastExprReg = reg;
}

void CodeGenVisitor::visit(FloatLitNode& node) {
    int reg = _regs.acquire();
    if (reg < 0) {
        reportError(node.getLineNumber(), "register exhaustion while loading float literal");
        _lastExprReg = -1;
        return;
    }

    const long lowered = static_cast<long>(std::llround(node.getFloatValue() * static_cast<float>(kFloatScale)));
    emitSourceLineContext(
        node.getLineNumber(),
        "[REG] " + regName(reg) + " <- float(" + std::to_string(node.getFloatValue()) + ") * " + std::to_string(kFloatScale) +
        " = " + std::to_string(lowered)
    );
    emit("addi " + regName(reg) + ", r0, " + std::to_string(lowered));
    _lastExprReg = reg;
}

void CodeGenVisitor::visit(TypeNode&) {
    _lastExprReg = -1;
}

void CodeGenVisitor::visit(BinaryOpNode& node) {
    emitSourceLineContext(node.getLineNumber(), "[EXPR] binary op='" + node.getOperator() + "'");
    int leftReg = evalExpr(node.getLeft());
    int rightReg = evalExpr(node.getRight());

    if (leftReg < 0 || rightReg < 0) {
        if (leftReg > 0) {
            _regs.release(leftReg);
        }
        if (rightReg > 0) {
            _regs.release(rightReg);
        }
        _lastExprReg = -1;
        return;
    }

    const std::string op = node.getOperator();

    std::string leftType;
    std::vector<int> leftDims;
    std::string rightType;
    std::vector<int> rightDims;
    const bool leftResolved = resolveNodeType(node.getLeft(), leftType, leftDims);
    const bool rightResolved = resolveNodeType(node.getRight(), rightType, rightDims);
    const bool leftIsFloat = leftResolved && leftDims.empty() && trimCopy(leftType) == "float";
    const bool rightIsFloat = rightResolved && rightDims.empty() && trimCopy(rightType) == "float";

    const bool isArithmetic = op == "+" || op == "-" || op == "*" || op == "/" || op == "mod";
    const bool isComparison = op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=";
    const bool useFloatFixedPoint = (isArithmetic || isComparison) && (leftIsFloat || rightIsFloat);

    if (useFloatFixedPoint) {
        emitComment("  fixed-point promotion active for binary op (scale=" + std::to_string(kFloatScale) + ")");
        if (!leftIsFloat) {
            emit("muli " + regName(leftReg) + ", " + regName(leftReg) + ", " + std::to_string(kFloatScale));
        }
        if (!rightIsFloat) {
            emit("muli " + regName(rightReg) + ", " + regName(rightReg) + ", " + std::to_string(kFloatScale));
        }
    }

    if (op == "+") {
        emit("add " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else if (op == "-") {
        emit("sub " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else if (op == "*") {
        emit("mul " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
        if (useFloatFixedPoint) {
            emit("divi " + regName(leftReg) + ", " + regName(leftReg) + ", " + std::to_string(kFloatScale));
        }
    } else if (op == "/") {
        if (useFloatFixedPoint) {
            emit("muli " + regName(leftReg) + ", " + regName(leftReg) + ", " + std::to_string(kFloatScale));
        }
        emit("div " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else if (op == "mod") {
        emit("mod " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else if (op == "and" || op == "&&") {
        emit("and " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else if (op == "or" || op == "||") {
        emit("or " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else if (op == "==") {
        emit("ceq " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else if (op == "!=") {
        emit("cne " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else if (op == "<") {
        emit("clt " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else if (op == "<=") {
        emit("cle " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else if (op == ">") {
        emit("cgt " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else if (op == ">=") {
        emit("cge " + regName(leftReg) + ", " + regName(leftReg) + ", " + regName(rightReg));
    } else {
        reportError(node.getLineNumber(), "unsupported binary operator in code generation: '" + op + "'");
    }

    _regs.release(rightReg);
    emitSourceLineContext(node.getLineNumber(), "[REG] result " + regName(leftReg) + " after op '" + op + "'");
    _lastExprReg = leftReg;
}

void CodeGenVisitor::visit(UnaryOpNode& node) {
    emitSourceLineContext(node.getLineNumber(), "[EXPR] unary op='" + node.getOperator() + "'");
    int operandReg = evalExpr(node.getLeft());
    if (operandReg < 0) {
        _lastExprReg = -1;
        return;
    }

    const std::string op = node.getOperator();
    if (op == "-") {
        emit("sub " + regName(operandReg) + ", r0, " + regName(operandReg));
    } else if (op == "not") {
        emit("ceqi " + regName(operandReg) + ", " + regName(operandReg) + ", 0");
    } else if (op != "+") {
        reportError(node.getLineNumber(), "unsupported unary operator in code generation: '" + op + "'");
    }

    _lastExprReg = operandReg;
}

void CodeGenVisitor::visit(FuncCallNode& node) {
    emitSourceLineContext(node.getLineNumber(), "[CALL] begin '" + node.getFunctionName() + "'");
    const FunctionLayoutInfo* targetLayout = nullptr;
    std::shared_ptr<ASTNode> ownerExpr = nullptr;
    bool explicitMethodCall = false;
    bool implicitMethodCall = false;

    auto calleeMember = std::dynamic_pointer_cast<DataMemberNode>(node.getLeft());
    if (calleeMember != nullptr && calleeMember->getLeft() != nullptr) {
        explicitMethodCall = true;
        ownerExpr = calleeMember->getLeft();

        std::string ownerType;
        std::vector<int> ownerDimensions;
        if (!resolveNodeType(ownerExpr, ownerType, ownerDimensions)) {
            _lastExprReg = -1;
            return;
        }
        if (!ownerDimensions.empty()) {
            reportError(node.getLineNumber(), "method receiver must be a scalar object");
            _lastExprReg = -1;
            return;
        }

        targetLayout = findFunctionLayout(ownerType, node.getFunctionName());
    } else {
        targetLayout = findFunctionLayout("", node.getFunctionName());
        if (targetLayout == nullptr && !_currentClassName.empty()) {
            targetLayout = findFunctionLayout(_currentClassName, node.getFunctionName());
            if (targetLayout != nullptr && targetLayout->isMethod) {
                implicitMethodCall = true;
            }
        }
    }

    if (targetLayout == nullptr) {
        reportError(node.getLineNumber(), "function call code generation target not found: '" + node.getFunctionName() + "'");
        _lastExprReg = -1;
        return;
    }

    emitComment("[CALL] target=" + targetLayout->key + " label=" + targetLayout->label +
                " return=" + trimCopy(targetLayout->returnType));

    const std::string callReturnType = trimCopy(targetLayout->returnType);
    const bool callReturnsObject = !callReturnType.empty() && !isBasicScalarType(callReturnType);

    if (targetLayout->paramOffsets.size() != node.getArgs().size()) {
        reportError(node.getLineNumber(), "argument count mismatch in call to '" + node.getFunctionName() + "'");
        _lastExprReg = -1;
        return;
    }

    const long callerFrameSize = _currentFrameSize;

    for (size_t i = 0; i < node.getArgs().size(); ++i) {
        std::string expectedType;
        std::vector<int> expectedDims;
        if (i < targetLayout->paramNames.size()) {
            auto paramInfoIt = targetLayout->varInfo.find(targetLayout->paramNames[i]);
            if (paramInfoIt != targetLayout->varInfo.end()) {
                expectedType = trimCopy(paramInfoIt->second.typeName);
                expectedDims = paramInfoIt->second.dimensions;
            }
        }

        const bool expectedIsArray = !expectedDims.empty();
        const bool expectedIsObject = !expectedIsArray && !expectedType.empty() && !isBasicScalarType(expectedType);
        const long storeOffset = targetLayout->paramOffsets[i] - callerFrameSize;

        emitComment("[CALL] arg[" + std::to_string(i) + "] -> mem[" + std::to_string(storeOffset) + "(r14)], expectedType=" +
            (expectedType.empty() ? std::string("<unknown>") : expectedType) +
                ", expectedDims=" + dimensionsToString(expectedDims));

        if (expectedIsObject) {
            const int srcAddrReg = emitAddressForObjectExpression(node.getArgs()[i], node.getLineNumber());
            if (srcAddrReg < 0) {
                _lastExprReg = -1;
                return;
            }

            const int dstAddrReg = _regs.acquire();
            if (dstAddrReg < 0) {
                _regs.release(srcAddrReg);
                reportError(node.getLineNumber(), "register exhaustion while preparing aggregate argument slot");
                _lastExprReg = -1;
                return;
            }

            const long objectSize = sizeOfType(expectedType, node.getLineNumber());
            if (objectSize <= 0) {
                _regs.release(dstAddrReg);
                _regs.release(srcAddrReg);
                _lastExprReg = -1;
                return;
            }

            emit("addi " + regName(dstAddrReg) + ", r14, " + std::to_string(storeOffset));
            if (!emitCopyWords(dstAddrReg, srcAddrReg, objectSize, node.getLineNumber())) {
                _regs.release(dstAddrReg);
                _regs.release(srcAddrReg);
                _lastExprReg = -1;
                return;
            }

            _regs.release(dstAddrReg);
            _regs.release(srcAddrReg);
            continue;
        }

        int argReg = -1;
        if (expectedIsArray) {
            argReg = emitAddressForLValue(node.getArgs()[i], node.getLineNumber());
        } else {
            argReg = evalExpr(node.getArgs()[i]);
        }

        if (argReg < 0) {
            _lastExprReg = -1;
            return;
        }

        std::string actualType;
        std::vector<int> actualDims;
        const bool actualResolved = resolveNodeType(node.getArgs()[i], actualType, actualDims);
        const bool actualIsFloat = actualResolved && actualDims.empty() && trimCopy(actualType) == "float";
        const bool expectedIsFloat = !expectedIsArray && expectedType == "float";

        if (expectedIsFloat && !actualIsFloat) {
            emit("muli " + regName(argReg) + ", " + regName(argReg) + ", " + std::to_string(kFloatScale));
        }

        emit("sw " + std::to_string(storeOffset) + "(r14), " + regName(argReg));
        emitComment("[CALL] store arg from " + regName(argReg));
        _regs.release(argReg);
    }

    if (targetLayout->isMethod) {
        emitComment("[CALL] prepare receiver (this)");
        const int thisReg = _regs.acquire();
        if (thisReg < 0) {
            reportError(node.getLineNumber(), "register exhaustion while preparing method receiver");
            _lastExprReg = -1;
            return;
        }

        if (explicitMethodCall) {
            const int ownerAddrReg = emitAddressForLValue(ownerExpr, node.getLineNumber());
            if (ownerAddrReg < 0) {
                _regs.release(thisReg);
                _lastExprReg = -1;
                return;
            }

            emit("add " + regName(thisReg) + ", " + regName(ownerAddrReg) + ", r0");
            _regs.release(ownerAddrReg);
        } else if (implicitMethodCall) {
            if (!loadThisPointerInto(thisReg, node.getLineNumber())) {
                _regs.release(thisReg);
                _lastExprReg = -1;
                return;
            }
        } else {
            reportError(node.getLineNumber(), "method call requires an object receiver");
            _regs.release(thisReg);
            _lastExprReg = -1;
            return;
        }

        const long thisStoreOffset = targetLayout->thisOffset - callerFrameSize;
        emit("sw " + std::to_string(thisStoreOffset) + "(r14), " + regName(thisReg));
        emitComment("[CALL] receiver -> mem[" + std::to_string(thisStoreOffset) + "(r14)]");
        _regs.release(thisReg);
    }

    if (callerFrameSize > 0) {
        emitComment("[FRAME] reserve " + std::to_string(callerFrameSize) + "B before call");
        emit("subi r14, r14, " + std::to_string(callerFrameSize));
    }
    emitComment("[CALL] jl r15 -> " + targetLayout->label);
    emit("jl r15, " + targetLayout->label);
    if (callerFrameSize > 0) {
        emitComment("[FRAME] restore r14 after call");
        emit("addi r14, r14, " + std::to_string(callerFrameSize));
    }

    const int resultReg = _regs.acquire();
    if (resultReg < 0) {
        reportError(node.getLineNumber(), "register exhaustion while retrieving function result");
        _lastExprReg = -1;
        return;
    }

    emit("add " + regName(resultReg) + ", r1, r0");
    if (callReturnsObject) {
        emitComment("object-returning call result propagated as object-address handle in register");
    }
    emitSourceLineContext(node.getLineNumber(), "[REG] " + regName(resultReg) + " <- r1  ; call result");
    _lastExprReg = resultReg;
}

void CodeGenVisitor::visit(DataMemberNode& node) {
    emitSourceLineContext(node.getLineNumber(), "[EXPR] data member '" + node.getName() + "' idx=" + std::to_string(node.getIndices().size()));
    const int addrReg = emitAddressForDataMember(node);
    if (addrReg < 0) {
        _lastExprReg = -1;
        return;
    }

    const int valueReg = _regs.acquire();
    if (valueReg < 0) {
        _regs.release(addrReg);
        reportError(node.getLineNumber(), "register exhaustion while loading data member");
        _lastExprReg = -1;
        return;
    }

    emitSourceLineContext(
        node.getLineNumber(),
        "[REG] " + regName(valueReg) + " <- mem[0(" + regName(addrReg) + ")]  ; member=" + node.getName()
    );
    emit("lw " + regName(valueReg) + ", 0(" + regName(addrReg) + ")");
    _regs.release(addrReg);
    _lastExprReg = valueReg;
}

void CodeGenVisitor::visit(AssignStmtNode& node) {
    emitSourceLineContext(node.getLineNumber(), "[ASSIGN] begin");
    std::string leftType;
    std::vector<int> leftDims;
    std::string rightType;
    std::vector<int> rightDims;
    const bool leftResolved = resolveNodeType(node.getLeft(), leftType, leftDims);
    const bool rightResolved = resolveNodeType(node.getRight(), rightType, rightDims);

    const std::string cleanLeftType = trimCopy(leftType);
    const std::string cleanRightType = trimCopy(rightType);
    const bool isObjectCopy =
        leftResolved && rightResolved &&
        leftDims.empty() && rightDims.empty() &&
        !cleanLeftType.empty() &&
        !isBasicScalarType(cleanLeftType) &&
        cleanLeftType == cleanRightType;

    if (isObjectCopy) {
        emitSourceLineContext(node.getLineNumber(), "[ASSIGN] object copy type=" + cleanLeftType);
        const long objectSize = sizeOfType(cleanLeftType, node.getLineNumber());
        if (objectSize <= 0) {
            return;
        }

        // Evaluate RHS first because it may be an object-returning call that clobbers call/return registers.
        const int srcAddrReg = emitAddressForObjectExpression(node.getRight(), node.getLineNumber());
        if (srcAddrReg < 0) {
            return;
        }

        const int dstAddrReg = emitAddressForLValue(node.getLeft(), node.getLineNumber());
        if (dstAddrReg < 0) {
            _regs.release(srcAddrReg);
            return;
        }

        emitSourceLineContext(
            node.getLineNumber(),
            "[MEM] object copy " + std::to_string(objectSize) + "B via dst=" + regName(dstAddrReg) + " src=" + regName(srcAddrReg)
        );

        if (!emitCopyWords(dstAddrReg, srcAddrReg, objectSize, node.getLineNumber())) {
            _regs.release(srcAddrReg);
            _regs.release(dstAddrReg);
            return;
        }

        _regs.release(srcAddrReg);
        _regs.release(dstAddrReg);
        return;
    }

    int valueReg = evalExpr(node.getRight());
    if (valueReg < 0) {
        return;
    }

    emitSourceLineContext(
        node.getLineNumber(),
        "[ASSIGN] scalar leftType=" + cleanLeftType + ", rightType=" + cleanRightType + ", valueReg=" + regName(valueReg)
    );

    const bool leftIsFloat = leftResolved && leftDims.empty() && trimCopy(leftType) == "float";
    const bool rightIsFloat = rightResolved && rightDims.empty() && trimCopy(rightType) == "float";

    if (leftIsFloat && !rightIsFloat) {
        emit("muli " + regName(valueReg) + ", " + regName(valueReg) + ", " + std::to_string(kFloatScale));
    }

    if (!emitStoreTarget(node.getLeft(), valueReg)) {
        _regs.release(valueReg);
        return;
    }

    _regs.release(valueReg);
}

void CodeGenVisitor::visit(IfStmtNode& node) {
    emitSourceLineContext(node.getLineNumber(), "[CTRL] if begin");
    int condReg = evalExpr(node.getLeft());
    if (condReg < 0) {
        return;
    }

    const std::string elseLabel = makeLabel("else");
    const std::string endLabel = makeLabel("endif");
    emitSourceLineContext(
        node.getLineNumber(),
        "[CTRL] if condReg=" + regName(condReg) + " false->" + elseLabel + " end->" + endLabel
    );

    emit("bz " + regName(condReg) + ", " + elseLabel);
    _regs.release(condReg);

    if (node.getRight() != nullptr) {
        node.getRight()->accept(*this);
    }

    emit("j " + endLabel);
    emit(elseLabel);

    if (node.getElseBlock() != nullptr) {
        node.getElseBlock()->accept(*this);
    }

    emit(endLabel);
}

void CodeGenVisitor::visit(WhileStmtNode& node) {
    emitSourceLineContext(node.getLineNumber(), "[CTRL] while begin");
    const std::string startLabel = makeLabel("while_start");
    const std::string endLabel = makeLabel("while_end");
    emitComment("[CTRL] while labels start=" + startLabel + " end=" + endLabel);

    emit(startLabel);

    int condReg = evalExpr(node.getLeft());
    if (condReg < 0) {
        return;
    }

    emitSourceLineContext(node.getLineNumber(), "[CTRL] while condReg=" + regName(condReg) + " false->" + endLabel);

    emit("bz " + regName(condReg) + ", " + endLabel);
    _regs.release(condReg);

    if (node.getRight() != nullptr) {
        node.getRight()->accept(*this);
    }

    emit("j " + startLabel);
    emit(endLabel);
}

void CodeGenVisitor::visit(IOStmtNode& node) {
    const std::string ioType = node.getValue();
    emitSourceLineContext(node.getLineNumber(), "[IO] begin kind='" + ioType + "'");

    if (ioType == "read") {
        std::string targetType;
        std::vector<int> targetDims;
        if (!resolveNodeType(node.getLeft(), targetType, targetDims)) {
            return;
        }

        if (!targetDims.empty()) {
            reportError(node.getLineNumber(), "read statement requires scalar target");
            return;
        }

        if (!targetType.empty() && targetType != "integer" && targetType != "float" && targetType != "bool") {
            reportError(node.getLineNumber(), "read statement is only supported for scalar integer/float/bool targets");
            return;
        }

        if (targetType == "float") {
            emitComment("read lowered to decimal float parser (rt_readFloat)");
            emit("jl r15, rt_readFloat");
        } else {
            emitComment("read lowered to decimal integer parser (rt_readInt)");
            emit("jl r15, rt_readInt");
        }

        int inputReg = _regs.acquire();
        if (inputReg < 0) {
            reportError(node.getLineNumber(), "register exhaustion while storing readInt result");
            return;
        }

        emit("add " + regName(inputReg) + ", r1, r0");
        emitSourceLineContext(node.getLineNumber(), "[REG] " + regName(inputReg) + " <- r1  ; read result");
        emitStoreTarget(node.getLeft(), inputReg);
        _regs.release(inputReg);
        return;
    }

    if (ioType == "write") {
        int valueReg = evalExpr(node.getLeft());
        if (valueReg < 0) {
            return;
        }

        std::string valueType;
        std::vector<int> valueDims;
        const bool valueResolved = resolveNodeType(node.getLeft(), valueType, valueDims);
        const bool valueIsFloat = valueResolved && valueDims.empty() && trimCopy(valueType) == "float";

        emit("add r1, " + regName(valueReg) + ", r0");
        emitSourceLineContext(node.getLineNumber(), "[REG] r1 <- " + regName(valueReg) + "  ; write argument");
        if (valueIsFloat) {
            emitComment("write lowered to fixed-point float printer (rt_writeFloat)");
            emit("jl r15, rt_writeFloat");
        } else {
            emitComment("write lowered to decimal integer printer (rt_writeInt)");
            emit("jl r15, rt_writeInt");
        }
        _regs.release(valueReg);
        return;
    }

    reportError(node.getLineNumber(), "unknown I/O statement kind in code generation: '" + ioType + "'");
}

void CodeGenVisitor::visit(ReturnStmtNode& node) {
    emitSourceLineContext(node.getLineNumber(), "[RET] begin");
    if (node.getLeft() != nullptr) {
        const std::string cleanReturnType = trimCopy(_currentReturnType);
        const bool returnIsObject = !cleanReturnType.empty() && !isBasicScalarType(cleanReturnType);

        if (returnIsObject) {
            int retAddrReg = emitAddressForObjectExpression(node.getLeft(), node.getLineNumber());
            if (retAddrReg >= 0) {
                emitComment("object return lowered as object-address handle in r1");
                emit("add r1, " + regName(retAddrReg) + ", r0");
                emitSourceLineContext(node.getLineNumber(), "[REG] r1 <- " + regName(retAddrReg) + "  ; object return handle");
                _regs.release(retAddrReg);
            }
        } else {
            int retReg = evalExpr(node.getLeft());
            if (retReg >= 0) {
                std::string actualType;
                std::vector<int> actualDims;
                const bool actualResolved = resolveNodeType(node.getLeft(), actualType, actualDims);
                const bool actualIsFloat = actualResolved && actualDims.empty() && trimCopy(actualType) == "float";
                if (cleanReturnType == "float" && !actualIsFloat) {
                    emit("muli " + regName(retReg) + ", " + regName(retReg) + ", " + std::to_string(kFloatScale));
                }

                emitComment("return value currently lowered into r1");
                emit("add r1, " + regName(retReg) + ", r0");
                emitSourceLineContext(node.getLineNumber(), "[REG] r1 <- " + regName(retReg) + "  ; scalar return");
                _regs.release(retReg);
            }
        }
    }

    if (_currentFunction != "main") {
        emit("j " + _currentReturnLabel);
    }
}

void CodeGenVisitor::visit(BlockNode& node) {
    emitSourceLineContext(node.getLineNumber(), "[BLOCK] statements=" + std::to_string(node.getStatements().size()));
    for (const auto& stmt : node.getStatements()) {
        if (stmt != nullptr) {
            emitSourceLineContext(stmt->getLineNumber(), "[STMT] " + summarizeNode(stmt));
            stmt->accept(*this);
            if (std::dynamic_pointer_cast<FuncCallNode>(stmt) != nullptr && _lastExprReg > 0) {
                _regs.release(_lastExprReg);
                _lastExprReg = -1;
            }
        }
    }
}

void CodeGenVisitor::visit(VarDeclNode&) {
    // Storage is assigned when entering a function frame.
}

void CodeGenVisitor::visit(FuncDefNode& node) {
    (void)node;
    // Function bodies are emitted in visit(ProgNode) with layout-aware call metadata.
}

void CodeGenVisitor::visit(ClassDeclNode& node) {
    emitComment("class code generation not implemented for: " + node.getName());
}

void CodeGenVisitor::visit(ProgNode& node) {
    _classDecls.clear();
    _classLayouts.clear();
    _classSizes.clear();
    _functionLayouts.clear();

    for (const auto& cls : node.getClasses()) {
        if (cls != nullptr) {
            _classDecls[cls->getName()] = cls;
        }
    }

    emitComment("semantic step: collect class declarations count=" + std::to_string(_classDecls.size()));

    for (const auto& cls : node.getClasses()) {
        if (cls == nullptr) {
            continue;
        }

        std::unordered_set<std::string> visiting;
        if (!buildClassLayout(cls->getName(), visiting, cls->getLineNumber())) {
            continue;
        }

        auto layoutIt = _classLayouts.find(cls->getName());
        if (layoutIt == _classLayouts.end()) {
            continue;
        }

        emitComment("class layout '" + cls->getName() + "' totalSize=" + std::to_string(layoutIt->second.size));
        for (const auto& field : layoutIt->second.fields) {
            emitComment("  field " + field.first +
                        " offset=" + std::to_string(field.second.offset) +
                        " size=" + std::to_string(field.second.size) +
                        " type=" + field.second.typeName +
                        " dims=" + dimensionsToString(field.second.dimensions));
        }
    }

    std::shared_ptr<FuncDefNode> mainFunction = nullptr;
    std::vector<std::shared_ptr<FuncDefNode>> nonMainFunctions;

    for (const auto& fn : node.getFunctions()) {
        if (fn == nullptr || fn->getRight() == nullptr) {
            continue;
        }

        if (!buildFunctionLayout(fn)) {
            continue;
        }

        const FunctionLayoutInfo* built = findFunctionLayout(fn->getClassName(), fn->getName());
        if (built != nullptr) {
            emitComment("function layout built: " + built->key +
                        " frame=" + std::to_string(built->frameSize) +
                        " params=" + std::to_string(built->paramNames.size()) +
                        " locals+temps=" + std::to_string(built->varOffsets.size()));
        }

        if (fn->getClassName().empty() && fn->getName() == "main") {
            mainFunction = fn;
        } else {
            nonMainFunctions.push_back(fn);
        }
    }

    if (mainFunction == nullptr) {
        reportError(0, "program has no main function for code generation");
        return;
    }

    const FunctionLayoutInfo* mainLayout = findFunctionLayout("", "main");
    if (mainLayout == nullptr) {
        reportError(mainFunction->getLineNumber(), "failed to build frame layout for main");
        return;
    }

    const std::string programEndLabel = makeLabel("program_end");

    emitFunctionBody(mainFunction, *mainLayout, true);
    emit("j " + programEndLabel);

    for (const auto& fn : nonMainFunctions) {
        const FunctionLayoutInfo* layout = findFunctionLayout(fn->getClassName(), fn->getName());
        if (layout == nullptr) {
            reportError(fn->getLineNumber(), "missing function layout for '" + functionKey(fn->getClassName(), fn->getName()) + "'");
            continue;
        }

        emitFunctionBody(fn, *layout, false);
    }

    emit(programEndLabel);
}

bool generateMoonAssembly(const std::shared_ptr<ProgNode>& root, const std::string& outputPath, std::vector<std::string>* errors) {
    std::ofstream out(outputPath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open()) {
        if (errors != nullptr) {
            errors->push_back("[ERROR][CODEGEN] Failed to open output file: " + outputPath);
        }
        return false;
    }

    CodeGenVisitor generator(out);
    const bool success = generator.generate(root);

    if (errors != nullptr) {
        *errors = generator.getErrors();
    }

    return success;
}
