#ifndef MY_PARSER_H
#define MY_PARSER_H

/**
 * @file my_parser.h
 * @brief Predictive recursive-descent parser interface and parser state contract.
 *
 * @details
 * This header defines the parser used by the compiler pipeline to transform a
 * token stream into an AST while collecting derivation traces and syntax errors.
 *
 * @par Why this shape?
 * The parser is implemented as static methods plus static state to keep grammar
 * procedures directly callable as production functions without object plumbing.
 * This matches course-style LL(1) implementations and makes derivation output
 * deterministic and easy to inspect.
 *
 * @par What comes next?
 * The AST produced here is consumed by semantic analysis and then code generation.
 * Any grammar or node-shape changes in this parser should be mirrored in semantic
 * checks and backend lowering rules.
 */

#include "Token.h"
#include "AST.h"
#include <initializer_list>
#include <stdexcept>
#include <algorithm>
#include <memory>

/**
 * @class SyntaxError
 * @brief Internal exception used for parser-local control flow during recovery.
 *
 * @details
 * This exception is thrown by low-level parse routines after registering an error.
 * Higher-level routines catch it to trigger panic-mode synchronization.
 */
class SyntaxError : public std::runtime_error {
public:
    /**
     * @brief Construct syntax error exception.
     * @param message Human-readable syntax error message.
     */
    explicit SyntaxError(const std::string& message) : std::runtime_error(message) {}
};

/**
 * @class Parser
 * @brief LL(1) recursive-descent parser with panic-mode recovery and AST construction.
 *
 * @details
 * The parser consumes a flattened token stream and applies grammar productions as
 * dedicated methods. It records derivation steps and syntax errors while continuing
 * parsing when possible.
 *
 * @par Why static state?
 * Static parser state simplifies mapping from grammar productions to methods and
 * supports existing assignment tooling that expects global parser outputs.
 *
 * @par What comes next?
 * For larger projects, these static members can be migrated into instance state so
 * multiple parser contexts can run concurrently in tests.
 */
class Parser {
    private:
        /**
         * @struct FuncHeadInfo
         * @brief Temporary container for parsed function-head components.
         *
         * @details
         * This structure decouples function-head parsing from body parsing and keeps
         * method/free-function disambiguation explicit before node construction.
         */
        struct FuncHeadInfo {
            std::string name;
            std::string className;
            std::vector<std::shared_ptr<VarDeclNode>> params;
            std::string returnType;
        };

        /**
         * @struct StatementIdTailResult
         * @brief Result bundle for identifier-led statement disambiguation.
         *
         * @details
         * Identifier-leading statements can become assignments, calls, or member
         * chains. This struct carries both updated base expression and final
         * statement node (if one is materialized).
         */
        struct StatementIdTailResult {
            std::shared_ptr<ASTNode> base;
            std::shared_ptr<ASTNode> statementNode;
        };

        // ============================================================================
        // ERROR RECOVERY SETS (For Panic Mode Recovery)

        /**
         * @name Error Recovery and Diagnostics
         * @brief Helpers for panic-mode synchronization and syntax diagnostics.
         *
         * @details
         * These methods implement a fail-fast local strategy with recovery at
         * higher grammar boundaries.
         *
         * @par Why?
         * Without recovery, recursive-descent parsers often stop at the first error;
         * panic-mode improves usability by surfacing multiple syntax issues.
         *
         * @par What comes next?
         * Follow sets can be refined as grammar evolves to reduce skipped regions.
         */
        //@{

        static void _skipUntil(const std::vector<Token::Type>& followSet);

        static std::string _formatError(const std::string& message, const Token& token, const std::vector<Token::Type>& expectedTokens);

        [[noreturn]] static void _reportError(const std::string& message, const std::vector<Token::Type>& expectedTokens);

        static std::vector<std::string> _errorMessages;

        static std::vector<std::string> _derivationSteps;

        static std::shared_ptr<ProgNode> _astRoot;

        //@}

        // ============================================================================
        // PARSER STATE
        // ============================================================================

        /**
         * @name Parser Runtime State
         * @brief Token cursor, lookahead, and shared parser context.
         *
         * @par Why?
         * Predictive parsing requires one-token lookahead and explicit cursor control.
         *
         * @par What comes next?
         * This state can be encapsulated in a context object if parser reentrancy
         * becomes a requirement.
         */
        //@{

        static Token _lookaheadToken;

        static int _currentTokenIndex;

        static bool _nextToken();

        static bool _inErrorRecoveryMode;

        static std::vector<std::vector<Token>> _tokens;

        static std::vector<Token> _flatTokens;

        static void _match(Token::Type expectedType);  

        //@}

        // ========================================================================
        // PROGRAM STRUCTURE
        // ========================================================================

        /**
         * @name Program and Declaration Parsing
         * @brief Parse top-level program structure, classes, and declarations.
         *
         * @details
         * These routines parse compilation-unit skeleton first (classes/functions/
         * main) before statement-level details.
         *
         * @par Why?
         * A predictable top-down structure keeps synchronization points clear and
         * aligns with grammar non-terminal hierarchy.
         */
        //@{

        static std::shared_ptr<ProgNode> _parseProgram();
        static std::vector<std::shared_ptr<ClassDeclNode>> _parseClassDeclList();
        static std::vector<std::shared_ptr<FuncDefNode>> _parseFuncDefList();
        static std::shared_ptr<ClassDeclNode> _parseClassDecl();
        static std::vector<std::string> _parseInheritanceOpt();
        static std::vector<std::string> _parseInheritsList();
        static std::vector<std::shared_ptr<ASTNode>> _parseClassBody();
        static std::shared_ptr<ASTNode> _parseClassMemberDecl();
        
        // Class visibilit
        // TODO Make it return the actual visibility modifier for Semantic Analysis instead of just true/false
        static std::string _parseVisibility(); 

        static std::shared_ptr<ASTNode> _parseMemberDecl(const std::string& visibility);
        static std::shared_ptr<ASTNode> _parseMemberDeclIdTail(const std::string& memberName, const std::string& visibility, int line);
        static std::shared_ptr<ASTNode> _parseMemberDeclTypeTail(const std::string& typeName, const std::string& memberName, const std::string& visibility, int line);

        //@}

        // ========================================================================
        // FUNCTIONS
        // ========================================================================

        /**
         * @name Function Parsing
         * @brief Parse function signatures, parameters, locals, and bodies.
         *
         * @par Why?
         * Separating function-head parsing from body parsing allows clear handling of
         * method-qualified names and return-type constraints.
         */
        //@{

        static std::shared_ptr<FuncDefNode> _parseFuncDef();
        static FuncHeadInfo _parseFuncHead();
        static void _parseFuncHeadTail(FuncHeadInfo& info, int headLine);
        static std::string _parseReturnType();
        
        static std::shared_ptr<TypeNode> _parseType();

        static std::vector<std::shared_ptr<VarDeclNode>> _parseFParams();
        static void _parseFParamsTail(std::vector<std::shared_ptr<VarDeclNode>>& params);
        static std::shared_ptr<BlockNode> _parseFuncBody(std::vector<std::shared_ptr<VarDeclNode>>* localVars = nullptr);
        static std::vector<std::shared_ptr<VarDeclNode>> _parseLocalVarDeclList();
        static std::vector<std::shared_ptr<VarDeclNode>> _parseVarDeclList();
        static std::shared_ptr<VarDeclNode> _parseVarDecl(const std::string& visibility = "local");

        //@}

        // ========================================================================
        // STATEMENTS
        // ========================================================================

        /**
         * @name Statement Parsing
         * @brief Parse executable statements and statement blocks.
         *
         * @details
         * Identifier-leading statements are disambiguated through tail routines
         * because they may represent calls, assignments, or member chains.
         */
        //@{

        static std::vector<std::shared_ptr<ASTNode>> _parseStatementList();
        static std::shared_ptr<ASTNode> _parseStatement();
        static std::shared_ptr<ASTNode> _parseStatBlock();
        static StatementIdTailResult _parseStatementIdTail(const std::shared_ptr<ASTNode>& lhsBase);
        static std::shared_ptr<ASTNode> _parseStatementRest(const std::shared_ptr<ASTNode>& lhsBase);
        static std::shared_ptr<ASTNode> _parseStatementCallTail(const std::shared_ptr<ASTNode>& callOrMember);
        
        static bool _parseAssignOp();

        //@}

        // ========================================================================
        // EXPRESSIONS
        // ========================================================================

        /**
         * @name Expression Parsing
         * @brief Parse expressions by precedence tiers.
         *
         * @details
         * Expression parsing is layered (factor -> term -> arith -> relational) to
         * encode precedence and associativity while constructing AST operator nodes.
         */
        //@{

        static std::shared_ptr<ASTNode> _parseExpr();
        static std::shared_ptr<ASTNode> _parseExprTail(const std::shared_ptr<ASTNode>& left);
        static std::shared_ptr<ASTNode> _parseRelExpr();
        static std::shared_ptr<ASTNode> _parseArithExpr();
        static std::shared_ptr<ASTNode> _parseAddOpTail(std::shared_ptr<ASTNode> left);
        static std::shared_ptr<ASTNode> _parseTerm();
        static std::shared_ptr<ASTNode> _parseMultOpTail(std::shared_ptr<ASTNode> left);
        static std::shared_ptr<ASTNode> _parseFactor();
        static std::shared_ptr<ASTNode> _parseFactorIdTail(const std::shared_ptr<ASTNode>& baseId);
        static std::shared_ptr<ASTNode> _parseFactorRest(const std::shared_ptr<ASTNode>& base);
        static std::shared_ptr<ASTNode> _parseFactorCallTail(const std::shared_ptr<ASTNode>& base);
        static std::shared_ptr<ASTNode> _parseVariable();

        //@}

        // ========================================================================
        // PARAMETERS & ARRAYS
        // ========================================================================

        /**
         * @name Parameters and Array Dimensions
         * @brief Parse formal/actual parameter lists and array dimension/index lists.
         *
         * @par Why?
         * Arrays and parameters recur across declarations and expressions, so these
         * helpers centralize grammar behavior and reduce duplicated edge cases.
         */
        //@{

        static std::vector<std::shared_ptr<ASTNode>> _parseAParams();
        static void _parseAParamsTail(std::vector<std::shared_ptr<ASTNode>>& params);
        static std::vector<std::shared_ptr<ASTNode>> _parseIndiceList();
        static std::shared_ptr<ASTNode> _parseIndice();
        static std::vector<int> _parseArraySizeList();
        static int _parseArraySize();
        static int _parseArraySizeTail();

        //@}

        // ========================================================================
        // OPERATORS (Return Token::Type for Semantics)
        // ========================================================================

        /**
         * @name Operator Parsing
         * @brief Parse operator terminals and optionally return concrete lexemes.
         *
         * @details
         * These methods expose lexemes so AST operators preserve source-level symbols
         * for semantic analysis and later diagnostics.
         */
        //@{

        static bool _parseRelOp(std::string* opLexeme = nullptr);
        static bool _parseAddOp(std::string* opLexeme = nullptr);
        static bool _parseMultOp(std::string* opLexeme = nullptr);
        static bool _parseSign(std::string* signLexeme = nullptr);

        //@}


    public:
        /** @brief Construct parser facade object. */
        Parser();
        /** @brief Destroy parser facade object. */
        ~Parser();

        /**
         * @brief Parse tokens from a token artifact file path.
         * @param token_filepath Path to token input artifact.
         * @return True if parsing completed with no syntax errors.
         *
         * @details
         * Kept for compatibility with file-based workflow tooling.
         */
        bool parseProgram(std::string token_filepath);

        /**
         * @brief Parse lexer-produced tokens directly (preferred in-memory path).
         * @param tokens Token stream grouped by source line.
         * @return True if syntax parsing completed without errors.
         *
         * @par Why?
         * This avoids re-parsing text artifacts and preserves exact lexer token data.
         *
         * @par What comes next?
         * On success, call getASTRoot() and forward AST to semantic analysis.
         */
        static bool parseTokens(const std::vector<std::vector<Token>>& tokens);

        /**
         * @brief Retrieve the parser-produced AST root.
         * @return Program AST root or nullptr if parsing failed.
         */
        static std::shared_ptr<ProgNode> getASTRoot();

        /**
         * @brief Retrieve accumulated syntax error messages.
         * @return Read-only list of parser error messages.
         */
        static const std::vector<std::string>& getErrorMessages();

        /**
         * @brief Retrieve grammar derivation trace.
         * @return Read-only list of derivation steps.
         *
         * @details
         * Useful for debugging grammar transitions and assignment deliverables.
         */
        static const std::vector<std::string>& getDerivationSteps();

};

#endif // MY_PARSER_H