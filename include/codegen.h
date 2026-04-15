/**
 * @file codegen.h
 * @brief Moon backend code generation visitor and emission interfaces.
 *
 * @details
 * This header defines the AST-driven backend that lowers typed AST nodes into
 * Moon assembly. The backend is layout-driven (class/object layouts, function
 * frames) and uses a register allocator plus stack-based addressing discipline.
 *
 * @par Why this shape?
 * Keeping layout, type-resolution, and emission helpers in one visitor provides
 * deterministic lowering and consistent diagnostics across all node categories.
 */
#ifndef CODEGEN_H
#define CODEGEN_H

#include "AST.h"

#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @class CodeGenVisitor
 * @brief AST visitor that emits Moon assembly for the full program.
 *
 * @details
 * Responsibilities include:
 * - frame and class layout construction,
 * - expression/type-driven instruction selection,
 * - object/member/array address lowering,
 * - runtime helper emission for integer/float I/O,
 * - trace-aware assembly comments and diagnostics.
 */
class CodeGenVisitor : public ASTVisitor {
    public:
        /**
         * @brief Construct code generator with destination output stream.
         * @param out Stream receiving emitted Moon assembly.
         */
        explicit CodeGenVisitor(std::ostream& out);

        /**
         * @brief Generate Moon assembly from program AST root.
         * @param root Program root node.
         * @return True when code generation completes without codegen errors.
         */
        bool generate(const std::shared_ptr<ProgNode>& root);

        /** @brief Get accumulated code generation diagnostics. */
        const std::vector<std::string>& getErrors() const;

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
        /**
         * @class RegisterAllocator
         * @brief Simple pool allocator for Moon general-purpose temporaries.
         *
         * @details
         * Current policy allocates from r1..r12 (excluding reserved frame/link usage
         * in backend conventions). Exhaustion returns -1 and triggers codegen errors.
         */
        class RegisterAllocator {
            public:
                /** @brief Initialize allocator with full temporary register pool. */
                RegisterAllocator();
                /** @brief Acquire a temporary register, or -1 if none available. */
                int acquire();
                /** @brief Return a previously acquired register to the pool. */
                void release(int reg);
                /** @brief Reset allocator to initial full-pool state. */
                void reset();

            private:
                /** @brief Free-register pool (LIFO policy). */
                std::vector<int> _freeRegs;
        };

        /**
         * @struct StackVarInfo
         * @brief Per-variable frame metadata for local/param lowering.
         */
        struct StackVarInfo {
            /** @brief Declared/base type name. */
            std::string typeName;
            /** @brief Declared dimensions for arrays. */
            std::vector<int> dimensions;
            /** @brief Size in bytes of one element. */
            long elementSize = 4;
            /** @brief True when parameter is lowered as address/reference slot. */
            bool isReferenceParam = false;
        };

        /**
         * @struct FieldLayoutInfo
         * @brief Memory layout metadata for class fields.
         */
        struct FieldLayoutInfo {
            /** @brief Field type name. */
            std::string typeName;
            /** @brief Field dimensions for array members. */
            std::vector<int> dimensions;
            /** @brief Byte offset within owning object. */
            long offset = 0;
            /** @brief Total field byte size. */
            long size = 0;
        };

        /**
         * @struct ClassLayoutInfo
         * @brief Fully flattened class layout including inherited fields.
         */
        struct ClassLayoutInfo {
            /** @brief Total object size in bytes. */
            long size = 0;
            /** @brief Field name -> layout metadata. */
            std::unordered_map<std::string, FieldLayoutInfo> fields;
        };

        /**
         * @struct FunctionLayoutInfo
         * @brief Stack-frame and call metadata for a lowered function/method.
         */
        struct FunctionLayoutInfo {
            /** @brief Canonical key (for example class::name or free function name). */
            std::string key;
            /** @brief Generated assembly label for function entry. */
            std::string label;
            /** @brief Function identifier. */
            std::string name;
            /** @brief Owning class name for methods, empty for free functions. */
            std::string className;
            /** @brief Declared return type. */
            std::string returnType;
            /** @brief True when function is class method. */
            bool isMethod = false;
            /** @brief Total frame size in bytes. */
            long frameSize = 0;
            /** @brief Stack offset for saved return link slot. */
            long returnLinkOffset = -4;
            /** @brief Stack offset for implicit receiver slot (methods). */
            long thisOffset = 0;
            /** @brief Parameter names in declaration order. */
            std::vector<std::string> paramNames;
            /** @brief Parameter stack offsets aligned with paramNames. */
            std::vector<long> paramOffsets;
            /** @brief Variable name -> stack offset map for this frame. */
            std::unordered_map<std::string, long> varOffsets;
            /** @brief Variable name -> stack/type metadata map for this frame. */
            std::unordered_map<std::string, StackVarInfo> varInfo;
        };

        /** @brief Assembly output stream target. */
        std::ostream& _out;
        /** @brief Temporary register allocator. */
        RegisterAllocator _regs;
        /** @brief Collected codegen errors. */
        std::vector<std::string> _errors;

        /** @brief Class declarations indexed by name from AST root. */
        std::unordered_map<std::string, std::shared_ptr<ClassDeclNode>> _classDecls;
        /** @brief Cached class layouts. */
        std::unordered_map<std::string, ClassLayoutInfo> _classLayouts;
        /** @brief Cached class sizes. */
        std::unordered_map<std::string, long> _classSizes;
        /** @brief Cached function frame layouts. */
        std::unordered_map<std::string, FunctionLayoutInfo> _functionLayouts;
        /** @brief Active frame variable metadata. */
        std::unordered_map<std::string, StackVarInfo> _stackVarInfo;
        /** @brief Active frame offsets. */
        std::unordered_map<std::string, long> _stackOffsets;
        /** @brief Active frame allocation cursor. */
        long _nextOffset = 0;
        /** @brief Unique label counter. */
        int _labelCounter = 0;
        /** @brief Last expression result register. */
        int _lastExprReg = -1;
        /** @brief Current function name during emission. */
        std::string _currentFunction;
        /** @brief Current class name during method emission. */
        std::string _currentClassName;
        /** @brief Current function return label. */
        std::string _currentReturnLabel;
        /** @brief Current function return type. */
        std::string _currentReturnType;
        /** @brief Current frame size. */
        long _currentFrameSize = 0;
        /** @brief Current implicit receiver slot offset. */
        long _currentThisOffset = 0;
        /** @brief Trace instruction counter. */
        long _traceEmitCounter = 0;
        /** @brief Trace source line context. */
        int _traceSourceLine = 0;
        /** @brief Trace context tag. */
        std::string _traceContextTag;

        /** @brief Emit raw assembly line plus optional trace line. */
        void emit(const std::string& line);
        /** @brief Emit comment line with readability filtering policy. */
        void emitComment(const std::string& message);
        /** @brief Emit comment with source line context tag. */
        void emitSourceLineContext(int line, const std::string& message);
        /** @brief Set active trace context fields. */
        void setTraceContext(int line, const std::string& contextTag);
        /** @brief Record formatted codegen diagnostic. */
        void reportError(int line, const std::string& message);

        /** @brief Sanitize string for assembly-safe label token. */
        static std::string sanitizeName(const std::string& raw);
        /** @brief Build unique label from prefix. */
        std::string makeLabel(const std::string& prefix);
        /** @brief Build canonical function key from class/function names. */
        static std::string functionKey(const std::string& className, const std::string& functionName);

        /** @brief Reset per-function emission state. */
        void resetFunctionState();
        /** @brief Compute byte size for scalar/class type. */
        long sizeOfType(const std::string& typeName, int line);
        /** @brief Compute class byte size (with cycle-safe recursion). */
        long sizeOfClass(const std::string& className, std::unordered_set<std::string>& visiting, int line);
        /** @brief Build/cached class field layout. */
        bool buildClassLayout(const std::string& className, std::unordered_set<std::string>& visiting, int line);
        /** @brief Lookup resolved field layout metadata in class layout. */
        bool lookupFieldLayout(const std::string& className, const std::string& fieldName, FieldLayoutInfo& out, int line);
        /** @brief Build frame layout for one function/method. */
        bool buildFunctionLayout(const std::shared_ptr<FuncDefNode>& functionNode);
        /** @brief Lookup prebuilt function layout by owner/name. */
        const FunctionLayoutInfo* findFunctionLayout(const std::string& className, const std::string& functionName) const;
        /** @brief Assign stack offsets for variable declarations. */
        void assignOffsets(const std::vector<std::shared_ptr<VarDeclNode>>& vars);
        /** @brief Compute storage bytes for variable declaration. */
        long sizeOfVar(const std::shared_ptr<VarDeclNode>& decl);
        /** @brief Infer type and dimensions for any expression node. */
        bool resolveNodeType(const std::shared_ptr<ASTNode>& node, std::string& typeName, std::vector<int>& dimensions);
        /** @brief Infer type info specifically for data-member access node. */
        bool resolveDataMemberType(const std::shared_ptr<DataMemberNode>& node, std::string& typeName, std::vector<int>& dimensions);
        /** @brief Emit array-index linearized offset into address register. */
        bool emitIndexOffsetIntoAddress(int addrReg,
                        const std::vector<std::shared_ptr<ASTNode>>& indices,
                        const std::vector<int>& declaredDimensions,
                        long elementSize,
                        int line);

        /** @brief Check whether active frame has named stack slot. */
        bool hasOffset(const std::string& name) const;
        /** @brief Lookup named stack offset in active frame. */
        long lookupOffset(const std::string& name) const;

        /** @brief Evaluate expression and return result register. */
        int evalExpr(const std::shared_ptr<ASTNode>& node);
        /** @brief Load implicit receiver pointer into target register. */
        bool loadThisPointerInto(int targetReg, int line);
        /** @brief Emit address computation for assignable l-value. */
        int emitAddressForLValue(const std::shared_ptr<ASTNode>& node, int line);
        /** @brief Emit address computation for data member chain/access. */
        int emitAddressForDataMember(DataMemberNode& node);
        /** @brief Emit address for object expression used by object copy/call paths. */
        int emitAddressForObjectExpression(const std::shared_ptr<ASTNode>& node, int line);
        /** @brief Emit word-wise memory copy loop for aggregate transfer. */
        bool emitCopyWords(int dstAddrReg, int srcAddrReg, long byteCount, int line);
        /** @brief Emit scalar store into assignment target. */
        bool emitStoreTarget(const std::shared_ptr<ASTNode>& target, int valueReg);
        /** @brief Emit full function body/prologue/epilogue from layout info. */
        void emitFunctionBody(const std::shared_ptr<FuncDefNode>& functionNode, const FunctionLayoutInfo& layout, bool isMainBody);
        /** @brief Emit runtime helper routines for integer/float input-output. */
        void emitRuntimeIntegerIO();
};

/**
 * @brief Convenience API: generate Moon assembly directly to a file.
 * @param root Program root node.
 * @param outputPath Destination assembly file path.
 * @param errors Optional output vector for codegen diagnostics.
 * @return True on successful generation and file write.
 */
bool generateMoonAssembly(const std::shared_ptr<ProgNode>& root, const std::string& outputPath, std::vector<std::string>* errors = nullptr);

#endif
