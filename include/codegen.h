#ifndef CODEGEN_H
#define CODEGEN_H

#include "AST.h"

#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class CodeGenVisitor : public ASTVisitor {
    public:
        explicit CodeGenVisitor(std::ostream& out);

        bool generate(const std::shared_ptr<ProgNode>& root);
        const std::vector<std::string>& getErrors() const;

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
        class RegisterAllocator {
            public:
                RegisterAllocator();
                int acquire();
                void release(int reg);
                void reset();

            private:
                std::vector<int> _freeRegs;
        };

        struct StackVarInfo {
            std::string typeName;
            std::vector<int> dimensions;
            long elementSize = 4;
            bool isReferenceParam = false;
        };

        struct FieldLayoutInfo {
            std::string typeName;
            std::vector<int> dimensions;
            long offset = 0;
            long size = 0;
        };

        struct ClassLayoutInfo {
            long size = 0;
            std::unordered_map<std::string, FieldLayoutInfo> fields;
        };

        struct FunctionLayoutInfo {
            std::string key;
            std::string label;
            std::string name;
            std::string className;
            std::string returnType;
            bool isMethod = false;
            long frameSize = 0;
            long returnLinkOffset = -4;
            long thisOffset = 0;
            std::vector<std::string> paramNames;
            std::vector<long> paramOffsets;
            std::unordered_map<std::string, long> varOffsets;
            std::unordered_map<std::string, StackVarInfo> varInfo;
        };

        std::ostream& _out;
        RegisterAllocator _regs;
        std::vector<std::string> _errors;

        std::unordered_map<std::string, std::shared_ptr<ClassDeclNode>> _classDecls;
        std::unordered_map<std::string, ClassLayoutInfo> _classLayouts;
        std::unordered_map<std::string, long> _classSizes;
        std::unordered_map<std::string, FunctionLayoutInfo> _functionLayouts;
        std::unordered_map<std::string, StackVarInfo> _stackVarInfo;
        std::unordered_map<std::string, long> _stackOffsets;
        long _nextOffset = 0;
        int _labelCounter = 0;
        int _lastExprReg = -1;
        std::string _currentFunction;
        std::string _currentClassName;
        std::string _currentReturnLabel;
        std::string _currentReturnType;
        long _currentFrameSize = 0;
        long _currentThisOffset = 0;

        void emit(const std::string& line);
        void emitComment(const std::string& message);
        void reportError(int line, const std::string& message);

        static std::string sanitizeName(const std::string& raw);
        std::string makeLabel(const std::string& prefix);
        static std::string functionKey(const std::string& className, const std::string& functionName);

        void resetFunctionState();
        long sizeOfType(const std::string& typeName, int line);
        long sizeOfClass(const std::string& className, std::unordered_set<std::string>& visiting, int line);
        bool buildClassLayout(const std::string& className, std::unordered_set<std::string>& visiting, int line);
        bool lookupFieldLayout(const std::string& className, const std::string& fieldName, FieldLayoutInfo& out, int line);
        bool buildFunctionLayout(const std::shared_ptr<FuncDefNode>& functionNode);
        const FunctionLayoutInfo* findFunctionLayout(const std::string& className, const std::string& functionName) const;
        void assignOffsets(const std::vector<std::shared_ptr<VarDeclNode>>& vars);
        long sizeOfVar(const std::shared_ptr<VarDeclNode>& decl);
        bool resolveNodeType(const std::shared_ptr<ASTNode>& node, std::string& typeName, std::vector<int>& dimensions);
        bool resolveDataMemberType(const std::shared_ptr<DataMemberNode>& node, std::string& typeName, std::vector<int>& dimensions);
        bool emitIndexOffsetIntoAddress(int addrReg,
                        const std::vector<std::shared_ptr<ASTNode>>& indices,
                        const std::vector<int>& declaredDimensions,
                        long elementSize,
                        int line);

        bool hasOffset(const std::string& name) const;
        long lookupOffset(const std::string& name) const;

        int evalExpr(const std::shared_ptr<ASTNode>& node);
        bool loadThisPointerInto(int targetReg, int line);
        int emitAddressForLValue(const std::shared_ptr<ASTNode>& node, int line);
        int emitAddressForDataMember(DataMemberNode& node);
        int emitAddressForObjectExpression(const std::shared_ptr<ASTNode>& node, int line);
        bool emitCopyWords(int dstAddrReg, int srcAddrReg, long byteCount, int line);
        bool emitStoreTarget(const std::shared_ptr<ASTNode>& target, int valueReg);
        void emitFunctionBody(const std::shared_ptr<FuncDefNode>& functionNode, const FunctionLayoutInfo& layout, bool isMainBody);
        void emitRuntimeIntegerIO();
};

bool generateMoonAssembly(const std::shared_ptr<ProgNode>& root, const std::string& outputPath, std::vector<std::string>* errors = nullptr);

#endif
