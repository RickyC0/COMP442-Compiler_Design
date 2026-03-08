#ifndef MY_PARSER_H
#define MY_PARSER_H

#include "Token.h"
#include "AST.h"
#include <initializer_list>
#include <stdexcept>
#include <algorithm>
#include <memory>

class SyntaxError : public std::runtime_error {
public:
    explicit SyntaxError(const std::string& message) : std::runtime_error(message) {}
};

class Parser {
    private:
        struct FuncHeadInfo {
            std::string name;
            std::string className;
            std::vector<std::shared_ptr<VarDeclNode>> params;
            std::string returnType;
        };

        struct StatementIdTailResult {
            std::shared_ptr<ASTNode> base;
            std::shared_ptr<ASTNode> statementNode;
        };

        // ============================================================================
        // ERROR RECOVERY SETS (For Panic Mode Recovery)

        static void _skipUntil(const std::vector<Token::Type>& followSet);

        static std::string _formatError(const std::string& message, const Token& token, const std::vector<Token::Type>& expectedTokens);

        [[noreturn]] static void _reportError(const std::string& message, const std::vector<Token::Type>& expectedTokens);

        static std::vector<std::string> _errorMessages;

        static std::vector<std::string> _derivationSteps;

        static std::shared_ptr<ProgNode> _astRoot;

        // ============================================================================
        // PARSER STATE
        // ============================================================================

        static Token _lookaheadToken;

        static int _currentTokenIndex;

        static bool _nextToken();

        static bool _inErrorRecoveryMode;

        static std::vector<std::vector<Token>> _tokens;

        static std::vector<Token> _flatTokens;

        static void _match(Token::Type expectedType);  

        // ========================================================================
        // PROGRAM STRUCTURE
        // ========================================================================
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
        static std::shared_ptr<ASTNode> _parseMemberDeclIdTail(const std::string& typeName, const std::string& memberName, const std::string& visibility, int line);
        static std::shared_ptr<ASTNode> _parseMemberDeclTypeTail(const std::string& typeName, const std::string& memberName, const std::string& visibility, int line);

        // ========================================================================
        // FUNCTIONS
        // ========================================================================
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

        // ========================================================================
        // STATEMENTS
        // ========================================================================
        static std::vector<std::shared_ptr<ASTNode>> _parseStatementList();
        static std::shared_ptr<ASTNode> _parseStatement();
        static std::shared_ptr<ASTNode> _parseStatBlock();
        static StatementIdTailResult _parseStatementIdTail(const std::shared_ptr<ASTNode>& lhsBase);
        static std::shared_ptr<ASTNode> _parseStatementRest(const std::shared_ptr<ASTNode>& lhsBase);
        static std::shared_ptr<ASTNode> _parseStatementCallTail(const std::shared_ptr<ASTNode>& callOrMember);
        
        static bool _parseAssignOp();

        // ========================================================================
        // EXPRESSIONS
        // ========================================================================
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

        // ========================================================================
        // PARAMETERS & ARRAYS
        // ========================================================================
        static std::vector<std::shared_ptr<ASTNode>> _parseAParams();
        static void _parseAParamsTail(std::vector<std::shared_ptr<ASTNode>>& params);
        static std::vector<std::shared_ptr<ASTNode>> _parseIndiceList();
        static std::shared_ptr<ASTNode> _parseIndice();
        static std::vector<int> _parseArraySizeList();
        static int _parseArraySize();
        static int _parseArraySizeTail();

        // ========================================================================
        // OPERATORS (Return Token::Type for Semantics)
        // ========================================================================
        static bool _parseRelOp(std::string* opLexeme = nullptr);
        static bool _parseAddOp(std::string* opLexeme = nullptr);
        static bool _parseMultOp(std::string* opLexeme = nullptr);
        static bool _parseSign(std::string* signLexeme = nullptr);


    public:
        Parser();
        ~Parser();

        bool parseProgram(std::string token_filepath);

        static bool parseTokens(const std::vector<std::vector<Token>>& tokens); // instead of reading a file with text representation of the tokens, take the tokens directly from the lexer

        static std::shared_ptr<ProgNode> getASTRoot();

        static const std::vector<std::string>& getErrorMessages();

        static const std::vector<std::string>& getDerivationSteps();

};

#endif // MY_PARSER_H