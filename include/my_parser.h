#ifndef MY_PARSER_H
#define MY_PARSER_H

#include "Token.h"
#include <initializer_list>
#include <stdexcept>
#include <algorithm>

class SyntaxError : public std::runtime_error {
public:
    explicit SyntaxError(const std::string& message) : std::runtime_error(message) {}
};

class Parser {
    private:
        
        // ============================================================================
        // FOLLOW SETS (For Error Recovery & Epsilon Transitions)
        // ============================================================================

        inline static const std::vector<Token::Type> FOLLOW_APARAMS = {
            Token::Type::CLOSE_PAREN_
        };

        inline static const std::vector<Token::Type> FOLLOW_APARAMS_TAIL = {
            Token::Type::CLOSE_PAREN_
        };

        inline static const std::vector<Token::Type> FOLLOW_ADD_OP = {
            Token::Type::FLOAT_TYPE_, Token::Type::ID_, Token::Type::INTEGER_TYPE_, 
            Token::Type::MINUS_, Token::Type::NOT_, Token::Type::OPEN_PAREN_, Token::Type::PLUS_
        };

        inline static const std::vector<Token::Type> FOLLOW_ADD_OP_TAIL = {
            Token::Type::CLOSE_BRACKET_, Token::Type::CLOSE_PAREN_, Token::Type::COMMA_, 
            Token::Type::EQUAL_, Token::Type::GREATER_EQUAL_, Token::Type::GREATER_THAN_, 
            Token::Type::LESS_EQUAL_, Token::Type::LESS_THAN_, Token::Type::NOT_EQUAL_, 
            Token::Type::SEMICOLON_
        };

        inline static const std::vector<Token::Type> FOLLOW_ARITH_EXPR = {
            Token::Type::CLOSE_BRACKET_, Token::Type::CLOSE_PAREN_, Token::Type::COMMA_, 
            Token::Type::EQUAL_, Token::Type::GREATER_EQUAL_, Token::Type::GREATER_THAN_, 
            Token::Type::LESS_EQUAL_, Token::Type::LESS_THAN_, Token::Type::NOT_EQUAL_, 
            Token::Type::SEMICOLON_
        };

        inline static const std::vector<Token::Type> FOLLOW_ARRAY_SIZE = {
            Token::Type::OPEN_BRACKET_
        };

        inline static const std::vector<Token::Type> FOLLOW_ARRAY_SIZE_LIST = {
            Token::Type::COMMA_, Token::Type::SEMICOLON_
        };

        inline static const std::vector<Token::Type> FOLLOW_ARRAY_SIZE_TAIL = {
            Token::Type::OPEN_BRACKET_
        };

        inline static const std::vector<Token::Type> FOLLOW_ASSIGN_OP = {
            Token::Type::FLOAT_TYPE_, Token::Type::ID_, Token::Type::INTEGER_TYPE_, 
            Token::Type::MINUS_, Token::Type::NOT_, Token::Type::OPEN_PAREN_, Token::Type::PLUS_
        };

        inline static const std::vector<Token::Type> FOLLOW_CLASS_BODY = {
            Token::Type::CLOSE_BRACE_
        };

        inline static const std::vector<Token::Type> FOLLOW_CLASS_DECL = {
            Token::Type::CLASS_KEYWORD_
        };

        inline static const std::vector<Token::Type> FOLLOW_CLASS_DECL_LIST = {
            Token::Type::ID_
        };

        inline static const std::vector<Token::Type> FOLLOW_CLASS_MEMBER_DECL = {
            Token::Type::PRIVATE_KEYWORD_, Token::Type::PUBLIC_KEYWORD_
        };

        inline static const std::vector<Token::Type> FOLLOW_EXPR = {
            Token::Type::CLOSE_PAREN_, Token::Type::COMMA_, Token::Type::SEMICOLON_
        };

        inline static const std::vector<Token::Type> FOLLOW_EXPR_TAIL = {
            Token::Type::CLOSE_PAREN_, Token::Type::COMMA_, Token::Type::SEMICOLON_
        };

        inline static const std::vector<Token::Type> FOLLOW_F_PARAMS = {
            Token::Type::CLOSE_PAREN_
        };

        inline static const std::vector<Token::Type> FOLLOW_F_PARAMS_TAIL = {
            Token::Type::CLOSE_PAREN_
        };

        inline static const std::vector<Token::Type> FOLLOW_FACTOR = {
            Token::Type::AND_, Token::Type::DIVIDE_, Token::Type::MULTIPLY_
        };

        inline static const std::vector<Token::Type> FOLLOW_FACTOR_CALL_TAIL = {
            Token::Type::AND_, Token::Type::CLOSE_PAREN_, Token::Type::DIVIDE_, Token::Type::MULTIPLY_
        };

        inline static const std::vector<Token::Type> FOLLOW_FACTOR_ID_TAIL = {
            Token::Type::AND_, Token::Type::CLOSE_PAREN_, Token::Type::DIVIDE_, Token::Type::MULTIPLY_
        };

        inline static const std::vector<Token::Type> FOLLOW_FACTOR_REST = {
            Token::Type::AND_, Token::Type::CLOSE_PAREN_, Token::Type::DIVIDE_, Token::Type::MULTIPLY_
        };

        inline static const std::vector<Token::Type> FOLLOW_FUNC_BODY = {
            Token::Type::END_OF_FILE_, Token::Type::ID_  // Note: Replaced $ with END_OF_FILE_
        };

        inline static const std::vector<Token::Type> FOLLOW_FUNC_DEF = {
            Token::Type::ID_
        };

        inline static const std::vector<Token::Type> FOLLOW_FUNC_DEF_LIST = {
            Token::Type::MAIN_
        };

        inline static const std::vector<Token::Type> FOLLOW_FUNC_HEAD = {
            Token::Type::LOCAL_
        };

        inline static const std::vector<Token::Type> FOLLOW_FUNC_HEAD_TAIL = {
            Token::Type::LOCAL_
        };

        inline static const std::vector<Token::Type> FOLLOW_INDICE = {
            Token::Type::OPEN_BRACKET_
        };

        inline static const std::vector<Token::Type> FOLLOW_INDICE_LIST = {
            Token::Type::ASSIGNMENT_, Token::Type::DOT_, Token::Type::OPEN_PAREN_
        };

        inline static const std::vector<Token::Type> FOLLOW_INHERITANCE_OPT = {
            Token::Type::OPEN_BRACE_
        };

        inline static const std::vector<Token::Type> FOLLOW_INHERITS_LIST = {
            Token::Type::OPEN_BRACE_
        };

        inline static const std::vector<Token::Type> FOLLOW_LOCAL_VAR_DECL_LIST = {
            Token::Type::DO_KEYWORD_
        };

        inline static const std::vector<Token::Type> FOLLOW_MEMBER_DECL = {
            Token::Type::PRIVATE_KEYWORD_, Token::Type::PUBLIC_KEYWORD_
        };

        inline static const std::vector<Token::Type> FOLLOW_MEMBER_DECL_ID_TAIL = {
            Token::Type::PRIVATE_KEYWORD_, Token::Type::PUBLIC_KEYWORD_
        };

        inline static const std::vector<Token::Type> FOLLOW_MULT_OP = {
            Token::Type::FLOAT_TYPE_, Token::Type::ID_, Token::Type::INTEGER_TYPE_, 
            Token::Type::MINUS_, Token::Type::NOT_, Token::Type::OPEN_PAREN_, Token::Type::PLUS_
        };

        inline static const std::vector<Token::Type> FOLLOW_MULT_OP_TAIL = {
            Token::Type::MINUS_, Token::Type::OR_, Token::Type::PLUS_
        };

        inline static const std::vector<Token::Type> FOLLOW_PROGRAM = {
            Token::Type::END_OF_FILE_
        };

        inline static const std::vector<Token::Type> FOLLOW_REL_EXPR = {
            Token::Type::CLOSE_PAREN_
        };

        inline static const std::vector<Token::Type> FOLLOW_REL_OP = {
            Token::Type::FLOAT_TYPE_, Token::Type::ID_, Token::Type::INTEGER_TYPE_, 
            Token::Type::MINUS_, Token::Type::NOT_, Token::Type::OPEN_PAREN_, Token::Type::PLUS_
        };

        inline static const std::vector<Token::Type> FOLLOW_RETURN_TYPE = {
            Token::Type::LOCAL_
        };

        inline static const std::vector<Token::Type> FOLLOW_START = {
            Token::Type::END_OF_FILE_
        };

        inline static const std::vector<Token::Type> FOLLOW_SIGN = {
            Token::Type::FLOAT_TYPE_, Token::Type::ID_, Token::Type::INTEGER_TYPE_, 
            Token::Type::MINUS_, Token::Type::NOT_, Token::Type::OPEN_PAREN_, Token::Type::PLUS_
        };

        inline static const std::vector<Token::Type> FOLLOW_STAT_BLOCK = {
            Token::Type::ELSE_KEYWORD_, Token::Type::SEMICOLON_
        };

        inline static const std::vector<Token::Type> FOLLOW_STATEMENT = {
            Token::Type::ELSE_KEYWORD_, Token::Type::ID_, Token::Type::IF_KEYWORD_, 
            Token::Type::READ_KEYWORD_, Token::Type::RETURN_KEYWORD_, Token::Type::SEMICOLON_, 
            Token::Type::WHILE_KEYWORD_, Token::Type::WRITE_KEYWORD_
        };

        inline static const std::vector<Token::Type> FOLLOW_STATEMENT_CALL_TAIL = {
            Token::Type::ELSE_KEYWORD_, Token::Type::ID_, Token::Type::IF_KEYWORD_, 
            Token::Type::READ_KEYWORD_, Token::Type::RETURN_KEYWORD_, Token::Type::SEMICOLON_, 
            Token::Type::WHILE_KEYWORD_, Token::Type::WRITE_KEYWORD_
        };

        inline static const std::vector<Token::Type> FOLLOW_STATEMENT_ID_TAIL = {
            Token::Type::ELSE_KEYWORD_, Token::Type::ID_, Token::Type::IF_KEYWORD_, 
            Token::Type::READ_KEYWORD_, Token::Type::RETURN_KEYWORD_, Token::Type::SEMICOLON_, 
            Token::Type::WHILE_KEYWORD_, Token::Type::WRITE_KEYWORD_
        };

        inline static const std::vector<Token::Type> FOLLOW_STATEMENT_LIST = {
            Token::Type::END_KEYWORD_
        };

        inline static const std::vector<Token::Type> FOLLOW_STATEMENT_REST = {
            Token::Type::ELSE_KEYWORD_, Token::Type::ID_, Token::Type::IF_KEYWORD_, 
            Token::Type::READ_KEYWORD_, Token::Type::RETURN_KEYWORD_, Token::Type::SEMICOLON_, 
            Token::Type::WHILE_KEYWORD_, Token::Type::WRITE_KEYWORD_
        };

        inline static const std::vector<Token::Type> FOLLOW_TERM = {
            Token::Type::MINUS_, Token::Type::OR_, Token::Type::PLUS_
        };

        inline static const std::vector<Token::Type> FOLLOW_TYPE = {
            Token::Type::ID_, Token::Type::LOCAL_
        };

        inline static const std::vector<Token::Type> FOLLOW_VAR_DECL = {
            Token::Type::FLOAT_TYPE_, Token::Type::ID_, Token::Type::INTEGER_TYPE_
        };

        inline static const std::vector<Token::Type> FOLLOW_VAR_DECL_LIST = {
            Token::Type::DO_KEYWORD_
        };

        inline static const std::vector<Token::Type> FOLLOW_VARIABLE = {
            Token::Type::CLOSE_PAREN_
        };

        inline static const std::vector<Token::Type> FOLLOW_VISIBILITY = {
            Token::Type::FLOAT_TYPE_, Token::Type::ID_, Token::Type::INTEGER_TYPE_
        };

        // ============================================================================
        // ERROR RECOVERY SETS (For Panic Mode Recovery)

        static void _skipUntil(const std::vector<Token::Type>& followSet);

        static std::string _formatError(const std::string& message, const Token& token, const std::vector<Token::Type>& expectedTokens);

        [[noreturn]] static void _reportError(const std::string& message, const std::vector<Token::Type>& expectedTokens);

        static std::vector<std::string> _errorMessages;

        static std::vector<std::string> _derivationSteps;

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
        static void _parseProgram();
        static void _parseClassDeclList();
        static void _parseFuncDefList();
        static void _parseClassDecl();
        static void _parseInheritanceOpt();
        static void _parseInheritsList();
        static void _parseClassBody();
        static void _parseClassMemberDecl();
        
        // Class visibilit
        // TODO Make it return the actual visibility modifier for Semantic Analysis instead of just true/false
        static bool _parseVisibility(); 

        static void _parseMemberDecl();
        static void _parseMemberDeclIdTail();
        static void _parseMemberDeclTypeTail();

        // ========================================================================
        // FUNCTIONS
        // ========================================================================
        static void _parseFuncDef();
        static void _parseFuncHead();
        static void _parseFuncHeadTail();
        static void _parseReturnType();
        
        // Type is critical for Semantic Analysis, so return the token
        static bool _parseType();

        static void _parseFParams();
        static void _parseFParamsTail();
        static void _parseFuncBody();
        static void _parseLocalVarDeclList();
        static void _parseVarDeclList();
        static bool _parseVarDecl();

        // ========================================================================
        // STATEMENTS
        // ========================================================================
        static void _parseStatementList();
        static bool _parseStatement();
        static void _parseStatBlock();
        static void _parseStatementIdTail();
        static void _parseStatementRest();
        static void _parseStatementCallTail();
        
        static bool _parseAssignOp(); // Returns ASSIGNMENT_

        // ========================================================================
        // EXPRESSIONS
        // ========================================================================
        static void _parseExpr();
        static void _parseExprTail();
        static void _parseRelExpr();
        static void _parseArithExpr();
        static void _parseAddOpTail();
        static void _parseTerm();
        static void _parseMultOpTail();
        static void _parseFactor();
        static void _parseFactorIdTail();
        static void _parseFactorRest();
        static void _parseFactorCallTail();
        static void _parseVariable();

        // ========================================================================
        // PARAMETERS & ARRAYS
        // ========================================================================
        static void _parseAParams();
        static void _parseAParamsTail();
        static void _parseIndiceList();
        static void _parseIndice();
        static void _parseArraySizeList();
        static void _parseArraySize();
        static void _parseArraySizeTail();

        // ========================================================================
        // OPERATORS (Return Token::Type for Semantics)
        // ========================================================================
        static bool _parseRelOp();  // Returns EQUAL_, LESS_THAN_, etc.
        static bool _parseAddOp();  // Returns PLUS_, MINUS_, OR_
        static bool _parseMultOp(); // Returns MULTIPLY_, DIVIDE_, AND_
        static bool _parseSign();   // Returns PLUS_, MINUS_


    public:
        Parser();
        ~Parser();

        bool parseProgram(std::string token_filepath);

        static bool parseTokens(const std::vector<std::vector<Token>>& tokens); // instead of reading a file with text representation of the tokens, take the tokens directly from the lexer

        static const std::vector<std::string>& getErrorMessages();

        static const std::vector<std::string>& getDerivationSteps();

};

#endif // MY_PARSER_H