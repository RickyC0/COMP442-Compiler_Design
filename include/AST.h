/**
 * @file AST.h
 * @brief Abstract Syntax Tree (AST) model and visitor contracts.
 *
 * @details
 * This header defines the complete AST node hierarchy used across parsing,
 * semantic analysis, code generation, and visualization. Nodes preserve source
 * line numbers and expose a visitor interface so each compiler phase can traverse
 * the same tree without embedding phase-specific logic into node classes.
 *
 * @par Why this design?
 * The compiler uses a strongly-typed AST with explicit node classes to keep
 * language structure clear and maintainable. The Visitor pattern allows adding
 * new passes (analysis, transformations, exports) without modifying the AST API.
 *
 * @par What comes next?
 * Any language feature extension should add or adapt node types here, then update
 * parser construction, semantic visitor checks, codegen visitor lowering, and
 * AST printer behavior in lockstep.
 */
#ifndef AST_H
#define AST_H
#include <string>
#include <vector>
#include <memory>

class ASTVisitor;
class IdNode;
class IntLitNode;
class FloatLitNode;
class TypeNode;
class BinaryOpNode;
class UnaryOpNode;
class FuncCallNode;
class DataMemberNode;
class AssignStmtNode;
class IfStmtNode;
class WhileStmtNode;
class IOStmtNode;
class ReturnStmtNode;
class BlockNode;
class VarDeclNode;
class FuncDefNode;
class ClassDeclNode;
class ProgNode;

/**
 * @class ASTVisitor
 * @brief Visitor interface for all concrete AST node kinds.
 *
 * @details
 * This interface is the single dispatch surface for compiler passes that operate
 * on AST nodes. Each pass implements these visit methods and receives a concrete
 * node type through double-dispatch via ASTNode::accept().
 *
 * @par Why explicit visit overloads?
 * Explicit overloads provide type-safe traversal and avoid runtime casts in each
 * compiler phase.
 */
class ASTVisitor {
    public:
        /** @brief Virtual destructor for interface-safe polymorphic deletion. */
        virtual ~ASTVisitor() = default;
        virtual void visit(IdNode& node) = 0;
        virtual void visit(IntLitNode& node) = 0;
        virtual void visit(FloatLitNode& node) = 0;
        virtual void visit(TypeNode& node) = 0;
        virtual void visit(BinaryOpNode& node) = 0;
        virtual void visit(UnaryOpNode& node) = 0;
        virtual void visit(FuncCallNode& node) = 0;
        virtual void visit(DataMemberNode& node) = 0;
        virtual void visit(AssignStmtNode& node) = 0;
        virtual void visit(IfStmtNode& node) = 0;
        virtual void visit(WhileStmtNode& node) = 0;
        virtual void visit(IOStmtNode& node) = 0;
        virtual void visit(ReturnStmtNode& node) = 0;
        virtual void visit(BlockNode& node) = 0;
        virtual void visit(VarDeclNode& node) = 0;
        virtual void visit(FuncDefNode& node) = 0;
        virtual void visit(ClassDeclNode& node) = 0;
        virtual void visit(ProgNode& node) = 0;
};

// =============================================================================
// BASE CLASS
// =============================================================================

/**
 * @class ASTNode
 * @brief Base class for all AST nodes.
 *
 * @details
 * Provides source line metadata and optional left/right child slots used by
 * many unary/binary/shared structural nodes.
 */
class ASTNode {
    public:
        /**
         * @brief Construct base AST node with source line metadata.
         * @param line 1-based source line number associated with this node.
         */
        ASTNode(int line = 0) : lineNumber(line) {}
        /** @brief Virtual destructor for polymorphic ownership safety. */
        virtual ~ASTNode() = default;

        /** @brief Get source line number for diagnostics and tracing. */
        int getLineNumber() const { return lineNumber; }
        /** @brief Get left child pointer (binary/unary shared convention). */
        std::shared_ptr<ASTNode> getLeft() const { return left; }
        /** @brief Get right child pointer (binary/shared convention). */
        std::shared_ptr<ASTNode> getRight() const { return right; }

        /** @brief Set left child pointer. */
        void setLeft(std::shared_ptr<ASTNode> leftNode) { left = leftNode; }
        /** @brief Set right child pointer. */
        void setRight(std::shared_ptr<ASTNode> rightNode) { right = rightNode; }

        /**
         * @brief Return compact textual label for diagnostics/printers.
         * @return Node-specific display string.
         */
        virtual std::string getValue() const = 0; 
        /**
         * @brief Accept a visitor (double-dispatch entry point).
         * @param visitor Visitor implementation for current compiler phase.
         */
        virtual void accept(ASTVisitor& visitor) = 0;

    private:
        int lineNumber; 
        std::shared_ptr<ASTNode> left; 
        std::shared_ptr<ASTNode> right;
};

// =============================================================================
// LEAF NODES (No children)
// =============================================================================

/**
 * @class IdNode
 * @brief Identifier leaf node.
 *
 * @details
 * Represents declared names used in expressions/statements. Resolution and type
 * meaning are deferred to semantic analysis.
 */
class IdNode : public ASTNode {
    private:
        std::string name;
    public:
        IdNode(int line, const std::string& idName) : ASTNode(line), name(idName) {}
        const std::string& getName() const { return name; }
        std::string getValue() const override { return name; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class IntLitNode
 * @brief Integer literal leaf node.
 */
class IntLitNode : public ASTNode {
    private:
        int value;
    public:
        IntLitNode(int line, int val) : ASTNode(line), value(val) {}
        int getIntValue() const { return value; }
        std::string getValue() const override { return std::to_string(value); }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class FloatLitNode
 * @brief Floating-point literal leaf node.
 *
 * @details
 * The literal value is preserved as parsed; backend lowering policy (for example
 * fixed-point emission) is applied in code generation, not in this node.
 */
class FloatLitNode : public ASTNode {
    private:
        float value;
    public:
        FloatLitNode(int line, float val) : ASTNode(line), value(val) {}
        float getFloatValue() const { return value; }
        std::string getValue() const override { return std::to_string(value); }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class TypeNode
 * @brief Type name leaf node used by grammar/type annotations.
 */
class TypeNode : public ASTNode {
    private:
        std::string typeName;
    public:
        TypeNode(int line, const std::string& type) : ASTNode(line), typeName(type) {}
        const std::string& getTypeName() const { return typeName; }
        std::string getValue() const override { return typeName; }
        void accept(ASTVisitor& visitor) override;
};

// =============================================================================
// EXPRESSION NODES (Math, Logic, Function Calls)
// =============================================================================

/**
 * @class BinaryOpNode
 * @brief Binary expression node with left/right operands.
 *
 * @details
 * Used for arithmetic, relational, and logical operators.
 */
class BinaryOpNode : public ASTNode {
    private:
        std::string op; 
    public:
        BinaryOpNode(int line, const std::string& oper, std::shared_ptr<ASTNode> l, std::shared_ptr<ASTNode> r) 
            : ASTNode(line), op(oper) {
            setLeft(l);
            setRight(r);
        }
        const std::string& getOperator() const { return op; }
        std::string getValue() const override { return op; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class UnaryOpNode
 * @brief Unary expression node with operand in left child.
 */
class UnaryOpNode : public ASTNode {
    private:
        std::string op; 
    public:
        UnaryOpNode(int line, const std::string& oper, std::shared_ptr<ASTNode> operand) 
            : ASTNode(line), op(oper) {
            setLeft(operand); 
        }
        const std::string& getOperator() const { return op; }
        std::string getValue() const override { return op; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class FuncCallNode
 * @brief Function or method call expression node.
 *
 * @details
 * Stores textual function name plus argument expressions. Optional left child can
 * hold receiver/callee-owner context for member-call forms.
 */
class FuncCallNode : public ASTNode {
    private:
        std::string funcName;
        std::vector<std::shared_ptr<ASTNode>> arguments; 
    public:
        FuncCallNode(int line, const std::string& name) : ASTNode(line), funcName(name) {}

        void addArgument(std::shared_ptr<ASTNode> arg) { arguments.push_back(arg); }
        const std::string& getFunctionName() const { return funcName; }
        std::vector<std::shared_ptr<ASTNode>> getArgs() const { return arguments; }
        
        std::string getValue() const override { return funcName + "()"; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class DataMemberNode
 * @brief Data-member access (optionally indexed) node.
 *
 * @details
 * Encodes both plain identifiers and chained member/index access forms depending
 * on owner in left child and entries in indices.
 */
class DataMemberNode : public ASTNode {
    private:
        std::string idName;
        std::vector<std::shared_ptr<ASTNode>> indices; 
    public:
        DataMemberNode(int line, const std::string& name) : ASTNode(line), idName(name) {}

        void addIndex(std::shared_ptr<ASTNode> indexExpr) { indices.push_back(indexExpr); }
        const std::string& getName() const { return idName; }
        std::vector<std::shared_ptr<ASTNode>> getIndices() const { return indices; }
        std::string getValue() const override { return idName; }
        void accept(ASTVisitor& visitor) override;
};

// =============================================================================
// STATEMENT NODES (Control Flow, IO, Assignments)
// =============================================================================

/**
 * @class AssignStmtNode
 * @brief Assignment statement node.
 *
 * @details
 * Left child is assignment target, right child is assigned expression.
 */
class AssignStmtNode : public ASTNode {
    public:
        AssignStmtNode(int line, std::shared_ptr<ASTNode> target, std::shared_ptr<ASTNode> value) 
            : ASTNode(line) {
            setLeft(target); 
            setRight(value); 
        }
        std::string getValue() const override { return "="; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class IfStmtNode
 * @brief Conditional statement node with optional else block.
 */
class IfStmtNode : public ASTNode {
    private:
        std::shared_ptr<ASTNode> elseBlock; 
    public:
        IfStmtNode(int line, std::shared_ptr<ASTNode> cond, std::shared_ptr<ASTNode> thenB, std::shared_ptr<ASTNode> elseB = nullptr) 
            : ASTNode(line), elseBlock(elseB) {
            setLeft(cond);
            setRight(thenB);
        }
        std::shared_ptr<ASTNode> getElseBlock() const { return elseBlock; }
        std::string getValue() const override { return "If"; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class WhileStmtNode
 * @brief While-loop statement node.
 */
class WhileStmtNode : public ASTNode {
    public:
        WhileStmtNode(int line, std::shared_ptr<ASTNode> cond, std::shared_ptr<ASTNode> body) 
            : ASTNode(line) {
            setLeft(cond); 
            setRight(body); 
        }
        std::string getValue() const override { return "While"; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class IOStmtNode
 * @brief I/O statement node (read/write forms).
 */
class IOStmtNode : public ASTNode {
    private:
        std::string ioType; 
    public:
        IOStmtNode(int line, const std::string& type, std::shared_ptr<ASTNode> target) 
            : ASTNode(line), ioType(type) {
            setLeft(target);
        }
        std::string getValue() const override { return ioType; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class ReturnStmtNode
 * @brief Return statement node.
 */
class ReturnStmtNode : public ASTNode {
    public:
        ReturnStmtNode(int line, std::shared_ptr<ASTNode> returnExpr) : ASTNode(line) {
            setLeft(returnExpr);
        }
        std::string getValue() const override { return "Return"; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class BlockNode
 * @brief Ordered statement sequence node.
 */
class BlockNode : public ASTNode {
    private:
        std::vector<std::shared_ptr<ASTNode>> statements;
    public:
        BlockNode(int line) : ASTNode(line) {}

        void addStatement(std::shared_ptr<ASTNode> stmt) { statements.push_back(stmt); }
        std::vector<std::shared_ptr<ASTNode>> getStatements() const { return statements; }
        
        std::string getValue() const override { return "Block"; }
        void accept(ASTVisitor& visitor) override;
};

// =============================================================================
// DECLARATION NODES (Classes, Functions, Variables)
// =============================================================================

/**
 * @class VarDeclNode
 * @brief Variable/field/parameter declaration node.
 *
 * @details
 * Carries type, name, visibility/scope role, and optional array dimensions.
 */
class VarDeclNode : public ASTNode {
    private:
        std::string type;
        std::string name;
        std::vector<int> arrayDimensions; 
        std::string visibility; 
    public:
        VarDeclNode(int line, const std::string& t, const std::string& n, const std::string& vis = "local") 
            : ASTNode(line), type(t), name(n), visibility(vis) {}

        void addDimension(int size) { arrayDimensions.push_back(size); }
        const std::string& getTypeName() const { return type; }
        const std::string& getName() const { return name; }
        const std::string& getVisibility() const { return visibility; }
        std::vector<int> getDimensions() const { return arrayDimensions; }
        std::string getValue() const override { return visibility + " " + type + " " + name; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class FuncDefNode
 * @brief Function or method definition node.
 *
 * @details
 * Holds signature metadata (return type/name/class owner), parameter list,
 * local declarations, and body block (in right child).
 */
class FuncDefNode : public ASTNode {
    private:
        std::string returnType;
        std::string name;
        std::string className; 
        std::vector<std::shared_ptr<VarDeclNode>> parameters;
        std::vector<std::shared_ptr<VarDeclNode>> localVariables;
    public:
        FuncDefNode(int line, const std::string& ret, const std::string& n, const std::string& cls = "") 
            : ASTNode(line), returnType(ret), name(n), className(cls) {}

        void addParam(std::shared_ptr<VarDeclNode> param) { parameters.push_back(param); }
        void addLocalVar(std::shared_ptr<VarDeclNode> var) { localVariables.push_back(var); }
        const std::string& getReturnType() const { return returnType; }
        const std::string& getName() const { return name; }
        const std::string& getClassName() const { return className; }
        std::vector<std::shared_ptr<VarDeclNode>> getParams() const { return parameters; }
        std::vector<std::shared_ptr<VarDeclNode>> getLocalVars() const { return localVariables; }
        
        std::string getValue() const override { 
            return (className.empty() ? "" : className + "::") + name + "() -> " + returnType; 
        }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class ClassDeclNode
 * @brief Class declaration node.
 *
 * @details
 * Stores class name, inherited parents, and declared members (fields/methods).
 */
class ClassDeclNode : public ASTNode {
    private:
        std::string name;
        std::vector<std::string> inheritedClasses;
        std::vector<std::shared_ptr<ASTNode>> members; 
    public:
        ClassDeclNode(int line, const std::string& n) : ASTNode(line), name(n) {}

        void addParentClass(const std::string& parentName) { inheritedClasses.push_back(parentName); }
        void addMember(std::shared_ptr<ASTNode> member) { members.push_back(member); }
        const std::string& getName() const { return name; }
        std::vector<std::string> getParents() const { return inheritedClasses; }
        std::vector<std::shared_ptr<ASTNode>> getMembers() const { return members; }

        std::string getValue() const override { return "Class " + name; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @class ProgNode
 * @brief Root program node containing classes and top-level functions.
 */
class ProgNode : public ASTNode {
    private:
        std::vector<std::shared_ptr<ClassDeclNode>> classes;
        std::vector<std::shared_ptr<FuncDefNode>> functions;
    public:
        ProgNode(int line = 0) : ASTNode(line) {}

        void addClass(std::shared_ptr<ClassDeclNode> cls) { classes.push_back(cls); }
        void addFunction(std::shared_ptr<FuncDefNode> func) { functions.push_back(func); }
        std::vector<std::shared_ptr<ClassDeclNode>> getClasses() const { return classes; }
        std::vector<std::shared_ptr<FuncDefNode>> getFunctions() const { return functions; }

        std::string getValue() const override { return "Program"; }
        void accept(ASTVisitor& visitor) override;
};

/**
 * @namespace ASTPrinter
 * @brief AST export utilities for text and DOT formats.
 *
 * @details
 * These functions are side-effect free on AST state and are used by driver/tooling
 * to produce human-readable and visual artifacts.
 */
    namespace ASTPrinter {
        /** @brief Convert AST to structured text tree. */
        std::string toString(const std::shared_ptr<ASTNode>& root);
        /** @brief Write structured text AST to file. */
        bool writeToFile(const std::shared_ptr<ASTNode>& root, const std::string& filePath);
        /** @brief Convert AST to Graphviz DOT graph content. */
        std::string toDot(const std::shared_ptr<ASTNode>& root);
        /** @brief Write Graphviz DOT AST to file. */
        bool writeDotToFile(const std::shared_ptr<ASTNode>& root, const std::string& filePath);
    }

#endif // AST_H