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

class ASTVisitor {
    public:
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

class ASTNode {
    public:
        ASTNode(int line = 0) : lineNumber(line) {}
        virtual ~ASTNode() = default;

        int getLineNumber() const { return lineNumber; }
        std::shared_ptr<ASTNode> getLeft() const { return left; }
        std::shared_ptr<ASTNode> getRight() const { return right; }

        void setLeft(std::shared_ptr<ASTNode> leftNode) { left = leftNode; }
        void setRight(std::shared_ptr<ASTNode> rightNode) { right = rightNode; }

        // Every node must be able to return a string representation of itself
        virtual std::string getValue() const = 0; 
        virtual void accept(ASTVisitor& visitor) = 0;

    private:
        int lineNumber; 
        std::shared_ptr<ASTNode> left; 
        std::shared_ptr<ASTNode> right;
};

// =============================================================================
// LEAF NODES (No children)
// =============================================================================

class IdNode : public ASTNode {
    private:
        std::string name;
    public:
        IdNode(int line, const std::string& idName) : ASTNode(line), name(idName) {}
        const std::string& getName() const { return name; }
        std::string getValue() const override { return name; }
        void accept(ASTVisitor& visitor) override;
};

class IntLitNode : public ASTNode {
    private:
        int value;
    public:
        IntLitNode(int line, int val) : ASTNode(line), value(val) {}
        int getIntValue() const { return value; }
        std::string getValue() const override { return std::to_string(value); }
        void accept(ASTVisitor& visitor) override;
};

class FloatLitNode : public ASTNode {
    private:
        float value;
    public:
        FloatLitNode(int line, float val) : ASTNode(line), value(val) {}
        float getFloatValue() const { return value; }
        std::string getValue() const override { return std::to_string(value); }
        void accept(ASTVisitor& visitor) override;
};

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

class ReturnStmtNode : public ASTNode {
    public:
        ReturnStmtNode(int line, std::shared_ptr<ASTNode> returnExpr) : ASTNode(line) {
            setLeft(returnExpr);
        }
        std::string getValue() const override { return "Return"; }
        void accept(ASTVisitor& visitor) override;
};

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

    namespace ASTPrinter {
        std::string toString(const std::shared_ptr<ASTNode>& root);
        bool writeToFile(const std::shared_ptr<ASTNode>& root, const std::string& filePath);
        std::string toDot(const std::shared_ptr<ASTNode>& root);
        bool writeDotToFile(const std::shared_ptr<ASTNode>& root, const std::string& filePath);
    }

#endif // AST_H