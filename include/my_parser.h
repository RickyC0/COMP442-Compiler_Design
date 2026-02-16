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