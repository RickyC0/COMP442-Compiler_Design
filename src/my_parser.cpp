#include "my_parser.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>

/**
 * @file my_parser.cpp
 * @brief LL(1) recursive-descent parser implementation.
 *
 * @details
 * This source maps grammar productions to parsing functions and builds AST nodes
 * while recording derivation traces and recoverable syntax errors.
 *
 * @par Why this implementation style?
 * Direct one-function-per-production code makes grammar debugging and assignment
 * verification straightforward.
 *
 * @par What comes next?
 * Outputs from this module (AST + syntax diagnostics) feed semantic analysis.
 */

#define TTYPE Token::Type
#define LTTYPE _lookaheadToken.getType()

/** @brief Static lookahead token state for predictive parsing. */
Token Parser::_lookaheadToken(Token::Type::END_OF_FILE_, "", 0);
/** @brief Current index into flattened token stream. */
int Parser::_currentTokenIndex = 0;
/** @brief Panic-mode guard to suppress cascading duplicate errors. */
bool Parser::_inErrorRecoveryMode = false;
/** @brief Original per-line token matrix from lexer. */
std::vector<std::vector<Token>> Parser::_tokens;
/** @brief Flattened token stream consumed by recursive-descent routines. */
std::vector<Token> Parser::_flatTokens;
/** @brief Collected parser error messages. */
std::vector<std::string> Parser::_errorMessages;
/** @brief Derivation trace used for parser debugging/output artifacts. */
std::vector<std::string> Parser::_derivationSteps;
/** @brief AST root produced by latest parse run. */
std::shared_ptr<ProgNode> Parser::_astRoot = nullptr;

/**
 * @brief Advance lookahead to next non-comment token.
 * @return True when a next token was loaded, false at end of stream.
 *
 * @details
 * Comment tokens are skipped here so production logic only sees grammar-relevant
 * terminals.
 */
bool Parser::_nextToken() {
    if (_currentTokenIndex < _flatTokens.size()) {
        Token token = _flatTokens[_currentTokenIndex++];
        _lookaheadToken = token;

        if(LTTYPE == TTYPE::BLOCK_COMMENT_ || LTTYPE == TTYPE::INLINE_COMMENT_) {
            return _nextToken(); // Skip comment tokens
        }

        return true;
    } else {
        // Return an EOF token if we've reached the end of the token list
        return false;
    }
}

/**
 * @brief Synchronize parser cursor to the next token in a follow set.
 * @param followSet Recovery token set for current grammar context.
 *
 * @details
 * Used by panic-mode recovery after syntax errors to resume parsing from a stable
 * boundary and continue collecting diagnostics.
 */
void Parser::_skipUntil(const std::vector<Token::Type>& followSet) {
    // If current lookahead is already in the recovery set, don't skip
    if (std::find(followSet.begin(), followSet.end(), LTTYPE) != followSet.end()) {
        return;
    }

    // Skip tokens until we find one in the follow/recovery set
    while (_currentTokenIndex < _flatTokens.size()) {
        _lookaheadToken = _flatTokens[_currentTokenIndex++];

        // Skip comment tokens
        if (LTTYPE == TTYPE::BLOCK_COMMENT_ || LTTYPE == TTYPE::INLINE_COMMENT_) {
            continue;
        }

        if (std::find(followSet.begin(), followSet.end(), LTTYPE) != followSet.end()) {
            return; // Found a token in the follow set, stop skipping
        }
    }
}

/**
 * @brief Parse lexer tokens provided in memory and build AST.
 * @param tokens Token stream grouped by line.
 * @return True if parsing completed without syntax errors.
 */
bool Parser::parseTokens(const std::vector<std::vector<Token>>& tokens) {
    _tokens = tokens;
    // Flatten vector<vector<Token>> into a single vector<Token>
    _flatTokens.clear();
    for (const auto& line : tokens) {
        for (const auto& token : line) {
            _flatTokens.push_back(token);
        }
    }
    _currentTokenIndex = 0;
    _errorMessages.clear();
    _derivationSteps.clear();
    _inErrorRecoveryMode = false;
    _astRoot = nullptr;

    // Prime the lookahead token
    if (!_nextToken()) {
        _errorMessages.push_back("[ERROR][SYNTAX] Empty token stream");
        return false;
    }

    try {
        _astRoot = _parseProgram();
    } catch (const SyntaxError& e) {
        // Error already logged in _reportError
    }

    return _errorMessages.empty();
}

/**
 * @brief Return AST root produced by most recent parse.
 * @return Program AST root or nullptr when unavailable.
 */
std::shared_ptr<ProgNode> Parser::getASTRoot() {
    return _astRoot;
}

/**
 * @brief Return collected syntax diagnostics.
 * @return Read-only syntax error message list.
 */
const std::vector<std::string>& Parser::getErrorMessages() {
    return _errorMessages;
}

/**
 * @brief Return derivation trace for grammar debugging/output.
 * @return Read-only derivation step list.
 */
const std::vector<std::string>& Parser::getDerivationSteps() {
    return _derivationSteps;
}

/**
 * @brief Format parser error message with lookahead and expected set.
 * @param message High-level error description.
 * @param token Current lookahead token.
 * @param expectedTokens Expected token classes.
 * @return Fully formatted diagnostic string.
 */
std::string Parser::_formatError(const std::string& message, const Token& token, const std::vector<Token::Type>& expectedTokens) {
    std::string errorMsg = "[ERROR][SYNTAX] " 
                            + message 
                            + " at line " 
                            + std::to_string(token.getLineNumber()) 
                            + ". Found token: " 
                            + token.toString() 
                            + ". Expected one of type: ";

    for (const auto& expected : expectedTokens) {
        errorMsg += Token(expected, "", 0).getTypeString() + " ";
    }
    return errorMsg;
}

/**
 * @brief Record syntax error then throw control-flow exception.
 * @param message Error summary.
 * @param expectedTokens Expected token classes at this point.
 * @throws SyntaxError Always thrown after message registration.
 */
void Parser::_reportError(const std::string& message, const std::vector<Token::Type>& expectedTokens) {
    std::string msg = _formatError(message, _lookaheadToken, expectedTokens);
    _errorMessages.push_back(msg);
    throw SyntaxError(msg);
}

/**
 * @brief Consume expected token or trigger panic-mode error path.
 * @param expectedType Token class required by current production.
 */
void Parser::_match(Token::Type expectedType) {
    // The token matches what we expect
    if (LTTYPE == expectedType) {
        
        Parser::_inErrorRecoveryMode = false;
        _nextToken(); 
    }
    
    // ERROR PATH
    else {
        // ERROR SUPPRESSION: Panic Mode -> If we're already in error recovery mode, we don't want to report more errors until we find a token in the follow set. This prevents cascading errors.
        if (Parser::_inErrorRecoveryMode) {
            return; 
        }

        // REPORT ERROR and enter panic mode
        _inErrorRecoveryMode = true; // Turn on panic mode

        // TRIGGER RECOVERY
        // We throw an exception to jump out of the current function and land in the 'catch' block of the nearest high-level rule (like parseStatement)
        _reportError("Unexpected token", {expectedType});
    }
}

// ============================================================================
// ============================================================================
// OPERATOR PARSING FUNCTIONS
// ============================================================================
// ============================================================================

// ============================================================================
// OPERATOR PARSING FUNCTIONS
// ============================================================================

bool Parser::_parseRelOp(std::string* opLexeme) {
    /**
     * @brief Parse relational operator terminal.
     * @param opLexeme Optional output of source lexeme.
     * @return True if a relational operator was consumed.
     */
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::EQUAL_ :
            _derivationSteps.push_back("RelOp -> 'eq'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;
        case TTYPE::NOT_EQUAL_:
            _derivationSteps.push_back("RelOp -> 'neq'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;
        case TTYPE::LESS_THAN_:
            _derivationSteps.push_back("RelOp -> 'lt'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;
        case TTYPE::GREATER_THAN_:
            _derivationSteps.push_back("RelOp -> 'gt'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;
        case TTYPE::LESS_EQUAL_:
            _derivationSteps.push_back("RelOp -> 'leq'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;
        case TTYPE::GREATER_EQUAL_:
            _derivationSteps.push_back("RelOp -> 'geq'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;

        default:
            return false;
    }
}

bool Parser::_parseAddOp(std::string* opLexeme) {
    /**
     * @brief Parse additive-level operator terminal.
     * @param opLexeme Optional output of source lexeme.
     * @return True if additive operator was consumed.
     */
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::PLUS_:
            _derivationSteps.push_back("AddOp -> '+'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;
        case TTYPE::MINUS_:
            _derivationSteps.push_back("AddOp -> '-'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;
        case TTYPE::OR_:
            _derivationSteps.push_back("AddOp -> 'or'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;

        default:
            return false;
    }
}

bool Parser::_parseMultOp(std::string* opLexeme) {
    /**
     * @brief Parse multiplicative-level operator terminal.
     * @param opLexeme Optional output of source lexeme.
     * @return True if multiplicative operator was consumed.
     */
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::MULTIPLY_:
            _derivationSteps.push_back("MultOp -> '*'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;
        case TTYPE::DIVIDE_:
            _derivationSteps.push_back("MultOp -> '/'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;
        case TTYPE::AND_:
            _derivationSteps.push_back("MultOp -> 'and'");
            if (opLexeme != nullptr) {
                *opLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;

        default:
            return false;
    }

}

bool Parser::_parseSign(std::string* signLexeme) {
    /**
     * @brief Parse unary sign operator.
     * @param signLexeme Optional output of sign lexeme.
     * @return True if unary sign was consumed.
     */
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::PLUS_:
            _derivationSteps.push_back("Sign -> '+'");
            if (signLexeme != nullptr) {
                *signLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;
        case TTYPE::MINUS_:
            _derivationSteps.push_back("Sign -> '-'");
            if (signLexeme != nullptr) {
                *signLexeme = _lookaheadToken.getValue();
            }
            _match(opType);
            return true;

        default:
            return false;
    }
}

bool Parser::_parseAssignOp() {
    /**
     * @brief Parse assignment operator token.
     * @return True when assignment token was consumed.
     */
    if (LTTYPE == TTYPE::ASSIGNMENT_) {
        _derivationSteps.push_back("AssignOp -> '='");
        _match(TTYPE::ASSIGNMENT_);
        return true;
    } else {
        return false;
    }
}

std::shared_ptr<TypeNode> Parser::_parseType(){
    /**
     * @brief Parse type non-terminal (built-ins or class identifier).
     * @return Parsed TypeNode or nullptr on epsilon/non-type lookahead.
     */
    Token typeToken = _lookaheadToken;
    switch (LTTYPE)
    {
        case TTYPE::INTEGER_TYPE_:
            _derivationSteps.push_back("Type -> 'integer'");
            _match(LTTYPE);
            return std::make_shared<TypeNode>(typeToken.getLineNumber(), "integer");
        case TTYPE::FLOAT_TYPE_:
            _derivationSteps.push_back("Type -> 'float'");
            _match(LTTYPE);
            return std::make_shared<TypeNode>(typeToken.getLineNumber(), "float");
        case TTYPE::ID_:
            _derivationSteps.push_back("Type -> 'id'");
            _match(LTTYPE);
            return std::make_shared<TypeNode>(typeToken.getLineNumber(), typeToken.getValue());

        default:
            return nullptr;
    }
}

// ============================================================================
// ARRAY PARSING FUNCTIONS
// ============================================================================

/**
 * @brief Parse array-size tail after opening bracket.
 * @return Declared dimension value, or -1 for unsized dimension.
 */
int Parser::_parseArraySizeTail() {
    // Grammar: ArraySizeTail -> intNum ] | ]

    // Case 1: intNum ]
    if (LTTYPE == TTYPE::INTEGER_LITERAL_) {
        Token sizeToken = _lookaheadToken;
        _derivationSteps.push_back("ArraySizeTail -> 'intNum' ']'");
        _match(TTYPE::INTEGER_LITERAL_);
        _match(TTYPE::CLOSE_BRACKET_);
        return std::atoi(sizeToken.getValue().c_str());
    }
    // Case 2: ]
    else if (LTTYPE == TTYPE::CLOSE_BRACKET_) {
        _derivationSteps.push_back("ArraySizeTail -> ']'");
        _match(TTYPE::CLOSE_BRACKET_);
        return -1;
    }
    // Error
    else {
        _reportError("Expected INTEGER_LITERAL or ']'", {TTYPE::INTEGER_LITERAL_, TTYPE::CLOSE_BRACKET_});
    }
}

/**
 * @brief Parse one array-size production.
 * @return Parsed dimension size (or sentinel for unsized).
 */
int Parser::_parseArraySize() {
    // Grammar: ArraySize -> [ ArraySizeTail
    _derivationSteps.push_back("ArraySize -> '[' ArraySizeTail");
    _match(TTYPE::OPEN_BRACKET_);
    
    return _parseArraySizeTail();
}

/**
 * @brief Parse zero-or-more array dimensions.
 * @return Ordered list of parsed dimensions.
 */
std::vector<int> Parser::_parseArraySizeList() {
    std::vector<int> dimensions;
    // Grammar: ArraySizeList -> ArraySize ArraySizeList | EPSILON

    // Check FIRST set of ArraySize -> { [ }
    if (LTTYPE == TTYPE::OPEN_BRACKET_) {
        _derivationSteps.push_back("ArraySizeList -> ArraySize ArraySizeList");
        dimensions.push_back(_parseArraySize());
        std::vector<int> tail = _parseArraySizeList();
        dimensions.insert(dimensions.end(), tail.begin(), tail.end());
    }
    // EPSILON CASE:
    else {
        _derivationSteps.push_back("ArraySizeList -> EPSILON");
    }

    return dimensions;
}

/**
 * @brief Parse one indexing expression inside brackets.
 * @return AST node for index expression.
 */
std::shared_ptr<ASTNode> Parser::_parseIndice() {
    // Grammar: Indice -> [ Expr ]
    _derivationSteps.push_back("Indice -> '[' Expr ']'");
    _match(TTYPE::OPEN_BRACKET_);
    std::shared_ptr<ASTNode> indexExpr = _parseExpr();
    _match(TTYPE::CLOSE_BRACKET_);
    return indexExpr;
}

/**
 * @brief Parse zero-or-more index operations for member/array access.
 * @return List of index expression nodes.
 */
std::vector<std::shared_ptr<ASTNode>> Parser::_parseIndiceList(){
    std::vector<std::shared_ptr<ASTNode>> indices;
    // Grammar: IndiceList -> Indice IndiceList | EPSILON
    if (LTTYPE == TTYPE::OPEN_BRACKET_) {
        _derivationSteps.push_back("IndiceList -> Indice IndiceList");
        indices.push_back(_parseIndice());
        std::vector<std::shared_ptr<ASTNode>> tail = _parseIndiceList();
        indices.insert(indices.end(), tail.begin(), tail.end());
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("IndiceList -> EPSILON");
    }

    return indices;
}

/**
 * @brief Parse remaining actual arguments after first expression.
 * @param params In/out argument list accumulator.
 */
void Parser::_parseAParamsTail(std::vector<std::shared_ptr<ASTNode>>& params) {
    // Grammar: AParamsTail -> , Expr AParamsTail | EPSILON

    // Case 1: , Expr AParamsTail
    if (LTTYPE == TTYPE::COMMA_) {
        _derivationSteps.push_back("AParamsTail -> ',' Expr AParamsTail");
        _match(TTYPE::COMMA_);
        params.push_back(_parseExpr());
        _parseAParamsTail(params);
    }
    // Case 2: EPSILON
    else {
        _derivationSteps.push_back("AParamsTail -> EPSILON");
    }
}

/**
 * @brief Parse actual argument list for function calls.
 * @return Vector of argument expression nodes.
 */
std::vector<std::shared_ptr<ASTNode>> Parser::_parseAParams() {
    std::vector<std::shared_ptr<ASTNode>> params;
    // Grammar: AParams -> Expr AParamsTail | EPSILON
    // Check FIRST set of Expr (starts with Factor's FIRST set)
    if (LTTYPE == TTYPE::ID_ || LTTYPE == TTYPE::INTEGER_LITERAL_ ||
        LTTYPE == TTYPE::FLOAT_LITERAL_ || LTTYPE == TTYPE::OPEN_PAREN_ ||
        LTTYPE == TTYPE::MINUS_ || LTTYPE == TTYPE::PLUS_ || LTTYPE == TTYPE::NOT_) {
        _derivationSteps.push_back("AParams -> Expr AParamsTail");
        params.push_back(_parseExpr());
        _parseAParamsTail(params);
    }
    else {
        _derivationSteps.push_back("AParams -> EPSILON");
    }

    return params;
}

/**
 * @brief Parse variable non-terminal (id plus optional tails).
 * @return Variable/member access AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseVariable(){
    _derivationSteps.push_back("Variable -> 'id' FactorIdTail");
    Token idToken = _lookaheadToken;
    _match(TTYPE::ID_);
    std::shared_ptr<ASTNode> base = std::make_shared<IdNode>(idToken.getLineNumber(), idToken.getValue());
    return _parseFactorIdTail(base);
}

/**
 * @brief Parse chained member access after a call/factor base.
 * @param base Current left-side AST node.
 * @return Extended AST chain.
 */
std::shared_ptr<ASTNode> Parser::_parseFactorCallTail(const std::shared_ptr<ASTNode>& base){
    if(LTTYPE == TTYPE::DOT_){
        _derivationSteps.push_back("FactorCallTail -> '.' 'id' FactorIdTail");
        _match(TTYPE::DOT_);
        Token memberToken = _lookaheadToken;
        _match(TTYPE::ID_);
        std::shared_ptr<ASTNode> memberId = std::make_shared<IdNode>(memberToken.getLineNumber(), memberToken.getValue());
        std::shared_ptr<ASTNode> member = _parseFactorIdTail(memberId);
        member->setLeft(base);
        return member;
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("FactorCallTail -> EPSILON");
        return base;
    }
}

/**
 * @brief Parse continuation after factor base (dot/call/epsilon).
 * @param base Current base node.
 * @return Final factor AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseFactorRest(const std::shared_ptr<ASTNode>& base){
    switch (LTTYPE)
    {
    case TTYPE::DOT_:
        _derivationSteps.push_back("FactorRest -> '.' 'id' FactorIdTail");
        _match(TTYPE::DOT_);
        {
        Token memberToken = _lookaheadToken;
        _match(TTYPE::ID_);
        
        // Immediately create a DataMemberNode
        std::shared_ptr<ASTNode> memberId = std::make_shared<DataMemberNode>(memberToken.getLineNumber(), memberToken.getValue());
        memberId->setLeft(base); // Set the owner (e.g. 'p') right away
        
        return _parseFactorIdTail(memberId);
        }

    case TTYPE::OPEN_PAREN_:
        _derivationSteps.push_back("FactorRest -> '(' AParams ')' FactorCallTail");
        {
        Token openToken = _lookaheadToken;
        _match(TTYPE::OPEN_PAREN_);
        std::vector<std::shared_ptr<ASTNode>> args = _parseAParams();
        _match(TTYPE::CLOSE_PAREN_);
        std::shared_ptr<FuncCallNode> call = std::make_shared<FuncCallNode>(openToken.getLineNumber(), base->getValue());
        for (const auto& arg : args) {
            call->addArgument(arg);
        }
        call->setLeft(base);
        return _parseFactorCallTail(call);
        }

    default:
        _derivationSteps.push_back("FactorRest -> EPSILON");
        return base;
    }
}

/**
 * @brief Parse factor identifier tail: indices followed by rest chain.
 * @param baseId Identifier-origin base node.
 * @return Final factor AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseFactorIdTail(const std::shared_ptr<ASTNode>& baseId){
    _derivationSteps.push_back("FactorIdTail -> IndiceList FactorRest");
    std::vector<std::shared_ptr<ASTNode>> indices = _parseIndiceList();

    std::shared_ptr<ASTNode> base = baseId;
    if (!indices.empty()) {
        // Safely upgrade to DataMemberNode without losing the owner
        auto dataMember = std::dynamic_pointer_cast<DataMemberNode>(baseId);
        if (!dataMember) {
            dataMember = std::make_shared<DataMemberNode>(baseId->getLineNumber(), baseId->getValue());
            dataMember->setLeft(baseId->getLeft());
        }
        for (const auto& idx : indices) {
            dataMember->addIndex(idx);
        }
        base = dataMember;
    }

    return _parseFactorRest(base);
}

/**
 * @brief Parse factor non-terminal.
 * @return Factor AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseFactor() {
    switch (LTTYPE)
    {
    case TTYPE::ID_:
        {
        _derivationSteps.push_back("Factor -> 'id' FactorIdTail");
        Token idToken = _lookaheadToken;
        _match(TTYPE::ID_);
        std::shared_ptr<ASTNode> base = std::make_shared<IdNode>(idToken.getLineNumber(), idToken.getValue());
        return _parseFactorIdTail(base);
        }

    case TTYPE::INTEGER_LITERAL_:
        {
        _derivationSteps.push_back("Factor -> 'intLit'");
        Token lit = _lookaheadToken;
        _match(LTTYPE); // Match the literal
        return std::make_shared<IntLitNode>(lit.getLineNumber(), std::atoi(lit.getValue().c_str()));
        }

    case TTYPE::FLOAT_LITERAL_:
        {
        _derivationSteps.push_back("Factor -> 'floatLit'");
        Token lit = _lookaheadToken;
        _match(LTTYPE); // Match the literal
        return std::make_shared<FloatLitNode>(lit.getLineNumber(), std::strtof(lit.getValue().c_str(), nullptr));
        }

    case TTYPE::OPEN_PAREN_:
        _derivationSteps.push_back("Factor -> '(' ArithExpr ')'");
        _match(TTYPE::OPEN_PAREN_);
        {
        std::shared_ptr<ASTNode> expr = _parseArithExpr();
        _match(TTYPE::CLOSE_PAREN_);
        return expr;
        }

    // Need to check the first set of sign because we are in a switch statement and need to check something
    case TTYPE::MINUS_:
    case TTYPE::PLUS_:
        {
        _derivationSteps.push_back("Factor -> Sign Factor");
        std::string sign;
        Token signToken = _lookaheadToken;
        _parseSign(&sign);
        return std::make_shared<UnaryOpNode>(signToken.getLineNumber(), sign, _parseFactor());
        }

    case TTYPE::NOT_:
        {
        _derivationSteps.push_back("Factor -> '!' Factor");
        Token notToken = _lookaheadToken;
        _match(TTYPE::NOT_);
        return std::make_shared<UnaryOpNode>(notToken.getLineNumber(), "!", _parseFactor());
        }

    default:
        _reportError("Expected factor (identifier, literal, '(', sign, or '!')", {TTYPE::ID_, TTYPE::INTEGER_LITERAL_, TTYPE::FLOAT_LITERAL_, TTYPE::OPEN_PAREN_, TTYPE::MINUS_, TTYPE::PLUS_, TTYPE::NOT_});
    }
}

/**
 * @brief Parse multiplicative tail with left-associative folding.
 * @param left Left operand AST accumulated so far.
 * @return Folded AST subtree.
 */
std::shared_ptr<ASTNode> Parser::_parseMultOpTail(std::shared_ptr<ASTNode> left){
    while (LTTYPE == TTYPE::MULTIPLY_ || LTTYPE == TTYPE::DIVIDE_ || LTTYPE == TTYPE::AND_) {
        _derivationSteps.push_back("MultOpTail -> MultOp Factor MultOpTail");
        std::string op;
        Token opToken = _lookaheadToken;
        _parseMultOp(&op);
        std::shared_ptr<ASTNode> right = _parseFactor();
        left = std::make_shared<BinaryOpNode>(opToken.getLineNumber(), op, left, right);
    }
    _derivationSteps.push_back("MultOpTail -> EPSILON");
    return left;
}


/**
 * @brief Parse term non-terminal.
 * @return Term AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseTerm(){
    _derivationSteps.push_back("Term -> Factor MultOpTail");
    std::shared_ptr<ASTNode> left = _parseFactor();
    return _parseMultOpTail(left);
}

/**
 * @brief Parse additive tail with left-associative folding.
 * @param left Left operand AST accumulated so far.
 * @return Folded AST subtree.
 */
std::shared_ptr<ASTNode> Parser::_parseAddOpTail(std::shared_ptr<ASTNode> left){
    while (LTTYPE == TTYPE::PLUS_ || LTTYPE == TTYPE::MINUS_ || LTTYPE == TTYPE::OR_) {
        _derivationSteps.push_back("AddOpTail -> AddOp Term AddOpTail");
        std::string op;
        Token opToken = _lookaheadToken;
        _parseAddOp(&op);
        std::shared_ptr<ASTNode> right = _parseTerm();
        left = std::make_shared<BinaryOpNode>(opToken.getLineNumber(), op, left, right);
    }
    _derivationSteps.push_back("AddOpTail -> EPSILON");
    return left;
}

/**
 * @brief Parse arithmetic expression tier.
 * @return Arithmetic AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseArithExpr(){
    _derivationSteps.push_back("ArithExpr -> Term AddOpTail");
    std::shared_ptr<ASTNode> left = _parseTerm();
    return _parseAddOpTail(left);
}

/**
 * @brief Parse relational expression with explicit operator.
 * @return Relational AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseRelExpr(){
    _derivationSteps.push_back("RelExpr -> ArithExpr RelOp ArithExpr");
    std::shared_ptr<ASTNode> left = _parseArithExpr();
    std::string op;
    Token opToken = _lookaheadToken;
    if(!_parseRelOp(&op)){
        _reportError("Expected relational operator", {TTYPE::EQUAL_, TTYPE::NOT_EQUAL_, TTYPE::LESS_THAN_, TTYPE::GREATER_THAN_, TTYPE::LESS_EQUAL_, TTYPE::GREATER_EQUAL_});
    }
    std::shared_ptr<ASTNode> right = _parseArithExpr();
    return std::make_shared<BinaryOpNode>(opToken.getLineNumber(), op, left, right);
}

/**
 * @brief Parse optional relational tail after arithmetic expression.
 * @param left Left arithmetic expression.
 * @return Relational node or original left node.
 */
std::shared_ptr<ASTNode> Parser::_parseExprTail(const std::shared_ptr<ASTNode>& left){
    std::string op;
    Token opToken = _lookaheadToken;
    if(_parseRelOp(&op)){
        _derivationSteps.push_back("ExprTail -> RelOp ArithExpr");
        std::shared_ptr<ASTNode> right = _parseArithExpr();
        return std::make_shared<BinaryOpNode>(opToken.getLineNumber(), op, left, right);
    }
    else {
        _derivationSteps.push_back("ExprTail -> EPSILON");
        return left;
    }
}

/**
 * @brief Parse expression non-terminal.
 * @return Expression AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseExpr(){
    _derivationSteps.push_back("Expr -> ArithExpr ExprTail");
    std::shared_ptr<ASTNode> left = _parseArithExpr();
    return _parseExprTail(left);
}

/**
 * @brief Parse statement call tail for chained member call statements.
 * @param callOrMember Existing call/member base.
 * @return Completed statement/call AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseStatementCallTail(const std::shared_ptr<ASTNode>& callOrMember){
    if(LTTYPE == TTYPE::DOT_){
        _derivationSteps.push_back("StatementCallTail -> '.' 'id' StatementIdTail");
        _match(TTYPE::DOT_);
        Token memberToken = _lookaheadToken;
        _match(TTYPE::ID_);
        std::shared_ptr<ASTNode> memberBase = std::make_shared<IdNode>(memberToken.getLineNumber(), memberToken.getValue());
        StatementIdTailResult tail = _parseStatementIdTail(memberBase);
        if (tail.base != nullptr) {
            tail.base->setLeft(callOrMember);
        }
        if (tail.statementNode != nullptr) {
            return tail.statementNode;
        }
        return tail.base;
    }
    else if(LTTYPE == TTYPE::SEMICOLON_){
        _derivationSteps.push_back("StatementCallTail -> ';'");
        _match(TTYPE::SEMICOLON_);
        return callOrMember;
    }
    else{
        _reportError("Expected '.' for member access or ';' to end statement", {TTYPE::DOT_, TTYPE::SEMICOLON_});
    }
}

/**
 * @brief Parse continuation after identifier-led statement start.
 * @param lhsBase Left-hand base expression.
 * @return Statement AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseStatementRest(const std::shared_ptr<ASTNode>& lhsBase){
    if(LTTYPE == TTYPE::DOT_){
        _derivationSteps.push_back("StatementRest -> '.' 'id' StatementIdTail");
        _match(TTYPE::DOT_);
        Token memberToken = _lookaheadToken;
        _match(TTYPE::ID_);
        
        // Immediately create a DataMemberNode, not an IdNode
        std::shared_ptr<ASTNode> memberBase = std::make_shared<DataMemberNode>(memberToken.getLineNumber(), memberToken.getValue());
        memberBase->setLeft(lhsBase); // Set the owner (e.g. 'p') right away
        
        StatementIdTailResult tail = _parseStatementIdTail(memberBase);
        if (tail.statementNode != nullptr) {
            return tail.statementNode;
        }
        return tail.base;
    }
    else if(LTTYPE == TTYPE::OPEN_PAREN_){
        _derivationSteps.push_back("StatementRest -> '(' AParams ')' StatementCallTail");
        Token openToken = _lookaheadToken;
        _match(TTYPE::OPEN_PAREN_);
        std::vector<std::shared_ptr<ASTNode>> args = _parseAParams();
        _match(TTYPE::CLOSE_PAREN_);
        std::shared_ptr<FuncCallNode> call = std::make_shared<FuncCallNode>(openToken.getLineNumber(), lhsBase->getValue());
        for (const auto& arg : args) {
            call->addArgument(arg);
        }
        call->setLeft(lhsBase);
        return _parseStatementCallTail(call);
    }

    else if(LTTYPE == TTYPE::ASSIGNMENT_){
        _derivationSteps.push_back("StatementRest -> AssignOp Expr ';'");
        _parseAssignOp();
        std::shared_ptr<ASTNode> rhs = _parseExpr();
        _match(TTYPE::SEMICOLON_);
        return std::make_shared<AssignStmtNode>(lhsBase->getLineNumber(), lhsBase, rhs);
    }

    else{
        _reportError("Expected '.', '(', or '=' for function call or member access", {TTYPE::DOT_, TTYPE::OPEN_PAREN_, TTYPE::ASSIGNMENT_});
    }
}

/**
 * @brief Parse statement identifier tail (indices + rest disambiguation).
 * @param lhsBase Base identifier/member node.
 * @return Pair of evolved base node and optional statement node.
 */
Parser::StatementIdTailResult Parser::_parseStatementIdTail(const std::shared_ptr<ASTNode>& lhsBase){
    _derivationSteps.push_back("StatementIdTail -> IndiceList StatementRest");
    std::vector<std::shared_ptr<ASTNode>> indices = _parseIndiceList();

    std::shared_ptr<ASTNode> base = lhsBase;
    if (!indices.empty()) {
        // Safely upgrade to DataMemberNode without losing the owner
        auto dataMember = std::dynamic_pointer_cast<DataMemberNode>(lhsBase);
        if (!dataMember) {
            dataMember = std::make_shared<DataMemberNode>(lhsBase->getLineNumber(), lhsBase->getValue());
            dataMember->setLeft(lhsBase->getLeft());
        }
        for (const auto& idx : indices) {
            dataMember->addIndex(idx);
        }
        base = dataMember;
    }

    std::shared_ptr<ASTNode> stmt = _parseStatementRest(base);
    return {base, stmt};
}

/**
 * @brief Parse statement block (single statement or do-end block).
 * @return Block/statement AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseStatBlock(){
    if(LTTYPE == TTYPE::DO_KEYWORD_){
        _derivationSteps.push_back("StatBlock -> '{' StatementList '}'");
        int blockLine = _lookaheadToken.getLineNumber();
        _match(TTYPE::DO_KEYWORD_);
        std::vector<std::shared_ptr<ASTNode>> statements = _parseStatementList();
        _match(TTYPE::END_KEYWORD_);
        std::shared_ptr<BlockNode> block = std::make_shared<BlockNode>(blockLine);
        for (const auto& stmt : statements) {
            if (stmt != nullptr) {
                block->addStatement(stmt);
            }
        }
        return block;
    }
    else {
        std::shared_ptr<ASTNode> stmt = _parseStatement();
        if (stmt != nullptr) {
            return stmt;
        }
        _derivationSteps.push_back("StatBlock -> EPSILON");
        return nullptr;
    }
}

/**
 * @brief Parse one executable statement.
 * @return Statement AST node or nullptr for epsilon context.
 */
std::shared_ptr<ASTNode> Parser::_parseStatement() {
    switch(LTTYPE) {
        case TTYPE::IF_KEYWORD_:
            {
            _derivationSteps.push_back("Statement -> 'if' '(' RelExpr ')' 'then' StatBlock 'else' StatBlock ';'");
            Token ifToken = _lookaheadToken;
            _match(TTYPE::IF_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            std::shared_ptr<ASTNode> cond = _parseRelExpr();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::THEN_KEYWORD_);
            std::shared_ptr<ASTNode> thenBlock = _parseStatBlock();
            _match(TTYPE::ELSE_KEYWORD_);
            std::shared_ptr<ASTNode> elseBlock = _parseStatBlock();
            _match(TTYPE::SEMICOLON_);
            return std::make_shared<IfStmtNode>(ifToken.getLineNumber(), cond, thenBlock, elseBlock);
            }

        case TTYPE::WHILE_KEYWORD_:
            {
            _derivationSteps.push_back("Statement -> 'while' '(' RelExpr ')' StatBlock ';'");
            Token whileToken = _lookaheadToken;
            _match(TTYPE::WHILE_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            std::shared_ptr<ASTNode> cond = _parseRelExpr();
            _match(TTYPE::CLOSE_PAREN_);
            std::shared_ptr<ASTNode> body = _parseStatBlock();
            _match(TTYPE::SEMICOLON_);
            return std::make_shared<WhileStmtNode>(whileToken.getLineNumber(), cond, body);
            }

        case TTYPE::READ_KEYWORD_:
            {
            _derivationSteps.push_back("Statement -> 'read' '(' Variable ')' ';'");
            Token readToken = _lookaheadToken;
            _match(TTYPE::READ_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            std::shared_ptr<ASTNode> variable = _parseVariable();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::SEMICOLON_);
            return std::make_shared<IOStmtNode>(readToken.getLineNumber(), "read", variable);
            }

        case TTYPE::WRITE_KEYWORD_:
            {
            _derivationSteps.push_back("Statement -> 'write' '(' Expr ')' ';'");
            Token writeToken = _lookaheadToken;
            _match(TTYPE::WRITE_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            std::shared_ptr<ASTNode> expr = _parseExpr();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::SEMICOLON_);
            return std::make_shared<IOStmtNode>(writeToken.getLineNumber(), "write", expr);
            }

        case TTYPE::RETURN_KEYWORD_:
            {
            _derivationSteps.push_back("Statement -> 'return' '(' Expr ')' ';'");
            Token retToken = _lookaheadToken;
            _match(TTYPE::RETURN_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            std::shared_ptr<ASTNode> expr = _parseExpr();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::SEMICOLON_);
            return std::make_shared<ReturnStmtNode>(retToken.getLineNumber(), expr);
            }

        case TTYPE::ID_:
            {
            _derivationSteps.push_back("Statement -> 'id' StatementIdTail");
            Token idToken = _lookaheadToken;
            _match(TTYPE::ID_);
            std::shared_ptr<ASTNode> base = std::make_shared<IdNode>(idToken.getLineNumber(), idToken.getValue());
            StatementIdTailResult tail = _parseStatementIdTail(base);
            if (tail.statementNode != nullptr) {
                return tail.statementNode;
            }
            return tail.base;
            }

        default:
            return nullptr;
    }
}
/**
 * @brief Parse statement list with panic-mode recovery.
 * @return Vector of parsed statement nodes.
 */
std::vector<std::shared_ptr<ASTNode>> Parser::_parseStatementList() {
    std::vector<std::shared_ptr<ASTNode>> statements;
    // Grammar: StatementList -> Statement StatementList | EPSILON
    // Iterative with panic mode error recovery
    while (true) {
        try {
            std::shared_ptr<ASTNode> stmt = _parseStatement();
            if (stmt != nullptr) {
                _derivationSteps.push_back("StatementList -> Statement StatementList");
                statements.push_back(stmt);
            } else {
                _derivationSteps.push_back("StatementList -> EPSILON");
                return statements;
            }
        } catch (const SyntaxError& e) {
            // Error already logged. Skip to next statement start or list end.
            _skipUntil({TTYPE::IF_KEYWORD_, TTYPE::WHILE_KEYWORD_, TTYPE::READ_KEYWORD_,
                       TTYPE::WRITE_KEYWORD_, TTYPE::RETURN_KEYWORD_, TTYPE::ID_,
                       TTYPE::END_KEYWORD_, TTYPE::ELSE_KEYWORD_, TTYPE::END_OF_FILE_});
            _inErrorRecoveryMode = false;
        }
    }
}
/**
 * @brief Parse variable declaration production.
 * @param visibility Visibility/locality tag to attach to declaration.
 * @return VarDeclNode or nullptr when no declaration is present.
 */
std::shared_ptr<VarDeclNode> Parser::_parseVarDecl(const std::string& visibility) {
    std::shared_ptr<TypeNode> typeNode = _parseType();
    if(typeNode == nullptr){
        return nullptr;
    }

    Token idToken = _lookaheadToken;
    _derivationSteps.push_back("VarDecl -> Type 'id' ArraySizeList ';'");
    _match(TTYPE::ID_);
    std::vector<int> dims = _parseArraySizeList();
    _match(TTYPE::SEMICOLON_);

    auto var = std::make_shared<VarDeclNode>(idToken.getLineNumber(), typeNode->getValue(), idToken.getValue(), visibility);
    for (int dim : dims) {
        var->addDimension(dim);
    }
    return var;
}

/**
 * @brief Parse zero-or-more variable declarations with recovery.
 * @return Vector of variable declaration nodes.
 */
std::vector<std::shared_ptr<VarDeclNode>> Parser::_parseVarDeclList() {
    std::vector<std::shared_ptr<VarDeclNode>> decls;
    // Grammar: VarDeclList -> VarDecl VarDeclList | EPSILON
    // Iterative with panic mode error recovery
    while (true) {
        try {
            std::shared_ptr<VarDeclNode> decl = _parseVarDecl();
            if (decl != nullptr) {
                _derivationSteps.push_back("VarDeclList -> VarDecl VarDeclList");
                decls.push_back(decl);
            } else {
                _derivationSteps.push_back("VarDeclList -> EPSILON");
                return decls;
            }
        } catch (const SyntaxError& e) {
            _skipUntil({TTYPE::INTEGER_TYPE_, TTYPE::FLOAT_TYPE_, TTYPE::ID_,
                       TTYPE::DO_KEYWORD_, TTYPE::END_OF_FILE_});
            _inErrorRecoveryMode = false;
        }
    }
}

/**
 * @brief Parse optional local declaration region inside function body.
 * @return Vector of local declarations (possibly empty).
 */
std::vector<std::shared_ptr<VarDeclNode>> Parser::_parseLocalVarDeclList(){
    if(LTTYPE == TTYPE::LOCAL_){
        _derivationSteps.push_back("LocalVarDeclList -> 'local' VarDeclList");
        _match(TTYPE::LOCAL_);
        return _parseVarDeclList();
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("LocalVarDeclList -> EPSILON");
        return {};
    }
}

/**
 * @brief Parse function body including local declarations and statements.
 * @param localVars Optional output pointer for parsed local declarations.
 * @return Block node containing locals and statements.
 */
std::shared_ptr<BlockNode> Parser::_parseFuncBody(std::vector<std::shared_ptr<VarDeclNode>>* localVars) {
    _derivationSteps.push_back("FuncBody -> LocalVarDeclList 'do' StatementList 'end'");
    int line = _lookaheadToken.getLineNumber();
    std::vector<std::shared_ptr<VarDeclNode>> locals = _parseLocalVarDeclList();
    if (localVars != nullptr) {
        *localVars = locals;
    }
    _match(TTYPE::DO_KEYWORD_);
    std::vector<std::shared_ptr<ASTNode>> statements = _parseStatementList();
    _match(TTYPE::END_KEYWORD_);

    std::shared_ptr<BlockNode> body = std::make_shared<BlockNode>(line);
    for (const auto& localVar : locals) {
        body->addStatement(localVar);
    }
    for (const auto& stmt : statements) {
        if (stmt != nullptr) {
            body->addStatement(stmt);
        }
    }
    return body;
}

/**
 * @brief Parse remaining formal parameters after the first one.
 * @param params In/out parameter declaration accumulator.
 */
void Parser::_parseFParamsTail(std::vector<std::shared_ptr<VarDeclNode>>& params) {
    // Grammar: FParamsTail -> , Type id FParamsTail | EPSILON

    // Case 1: , Type id FParamsTail
    if (LTTYPE == TTYPE::COMMA_) {
        _derivationSteps.push_back("FParamsTail -> ',' Type 'id' ArraySizeList FParamsTail");
        _match(TTYPE::COMMA_);
        std::shared_ptr<TypeNode> typeNode = _parseType();
        if(typeNode == nullptr){
            _reportError("Expected type (INTEGER_TYPE_, FLOAT_TYPE_, or identifier)", {TTYPE::INTEGER_TYPE_, TTYPE::FLOAT_TYPE_, TTYPE::ID_});
        }
        Token idToken = _lookaheadToken;
        _match(TTYPE::ID_);
        std::vector<int> dims = _parseArraySizeList();
        auto param = std::make_shared<VarDeclNode>(idToken.getLineNumber(), typeNode->getValue(), idToken.getValue(), "param");
        for (int dim : dims) {
            param->addDimension(dim);
        }
        params.push_back(param);
        _parseFParamsTail(params);
    }
    // Case 2: EPSILON
    else {
        _derivationSteps.push_back("FParamsTail -> EPSILON");
    }
}


/**
 * @brief Parse formal parameter list in function/method signatures.
 * @return Vector of formal parameter declaration nodes.
 */
std::vector<std::shared_ptr<VarDeclNode>> Parser::_parseFParams() {
    std::vector<std::shared_ptr<VarDeclNode>> params;
    std::shared_ptr<TypeNode> typeNode = _parseType();
    if(typeNode != nullptr){
        _derivationSteps.push_back("FParams -> Type 'id' ArraySizeList FParamsTail");
        Token idToken = _lookaheadToken;
        _match(TTYPE::ID_);
        std::vector<int> dims = _parseArraySizeList();
        auto param = std::make_shared<VarDeclNode>(idToken.getLineNumber(), typeNode->getValue(), idToken.getValue(), "param");
        for (int dim : dims) {
            param->addDimension(dim);
        }
        params.push_back(param);
        _parseFParamsTail(params);
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("FParams -> EPSILON");
    }

    return params;
}


/**
 * @brief Parse function return type non-terminal.
 * @return Canonical return type string.
 */
std::string Parser::_parseReturnType(){
    if(LTTYPE == TTYPE::VOID_TYPE_){
        _derivationSteps.push_back("ReturnType -> 'void'");
        _match(TTYPE::VOID_TYPE_);
        return "void";
    }
    else {
        std::shared_ptr<TypeNode> typeNode = _parseType();
        if(typeNode != nullptr){
        _derivationSteps.push_back("ReturnType -> Type");
        return typeNode->getValue();
        }
    }

    _reportError("Expected return type (VOID_TYPE_, INTEGER_TYPE_, FLOAT_TYPE_, or ID_)", {TTYPE::VOID_TYPE_, TTYPE::INTEGER_TYPE_, TTYPE::FLOAT_TYPE_, TTYPE::ID_});
}

/**
 * @brief Parse function-head tail for free function or class-qualified method.
 * @param info In/out function-head aggregate information.
 * @param headLine Source line where head started.
 */
void Parser::_parseFuncHeadTail(FuncHeadInfo& info, int headLine) {
    if(LTTYPE == TTYPE::COLON_COLON_){
        _derivationSteps.push_back("FuncHeadTail -> '::' 'id' '(' FParams ')' ':' ReturnType");
        info.className = info.name;
        _match(TTYPE::COLON_COLON_);
        Token fnToken = _lookaheadToken;
        _match(TTYPE::ID_);
        info.name = fnToken.getValue();
        _match(TTYPE::OPEN_PAREN_);
        info.params = _parseFParams();
        _match(TTYPE::CLOSE_PAREN_);
        _match(TTYPE::COLON_);
        info.returnType = _parseReturnType();
    }
    else if(LTTYPE == TTYPE::OPEN_PAREN_){
        _derivationSteps.push_back("FuncHeadTail -> '(' FParams ')' ':' ReturnType");
        _match(TTYPE::OPEN_PAREN_);
        info.params = _parseFParams();
        _match(TTYPE::CLOSE_PAREN_);
        _match(TTYPE::COLON_);
        info.returnType = _parseReturnType();

    }
    else{
        _reportError("Expected '(' for parameters or ';' for function declaration", {TTYPE::OPEN_PAREN_, TTYPE::SEMICOLON_});
    }
}

/**
 * @brief Parse function head (name, params, optional class qualifier, return type).
 * @return Aggregated function-head information.
 */
Parser::FuncHeadInfo Parser::_parseFuncHead() {
    _derivationSteps.push_back("FuncHead -> 'id' FuncHeadTail");
    Token idToken = _lookaheadToken;
    FuncHeadInfo info;
    info.name = idToken.getValue();
    _match(TTYPE::ID_); // Function name
    _parseFuncHeadTail(info, idToken.getLineNumber());
    return info;
}

/**
 * @brief Parse complete function definition.
 * @return Function definition AST node.
 */
std::shared_ptr<FuncDefNode> Parser::_parseFuncDef() {
    if(LTTYPE == TTYPE::ID_){ // First set of FuncHead
        _derivationSteps.push_back("FuncDef -> FuncHead FuncBody");
        int line = _lookaheadToken.getLineNumber();
        FuncHeadInfo head = _parseFuncHead();
        std::vector<std::shared_ptr<VarDeclNode>> locals;
        std::shared_ptr<BlockNode> body = _parseFuncBody(&locals);

        auto func = std::make_shared<FuncDefNode>(line, head.returnType, head.name, head.className);
        for (const auto& param : head.params) {
            func->addParam(param);
        }
        for (const auto& local : locals) {
            func->addLocalVar(local);
        }
        func->setRight(body);
        return func;
    }
    else{
        _reportError("Expected function definition (identifier)", {TTYPE::ID_});
    }
}

/**
 * @brief Parse member declaration tail when first token was identifier.
 * @param memberName First identifier lexeme (type or member name context).
 * @param visibility Visibility modifier.
 * @param line Source line.
 * @return Member declaration node.
 */
std::shared_ptr<ASTNode> Parser::_parseMemberDeclIdTail(const std::string& memberName, const std::string& visibility, int line){
    // Grammar: MemberDeclIdTail -> ID_ ArraySizeList ';'
    //                            | '(' FParams ')' ':' ReturnType ';'
    if(LTTYPE == TTYPE::OPEN_PAREN_){
        // Function prototype
        _derivationSteps.push_back("MemberDeclIdTail -> '(' FParams ')' ':' ReturnType ';'");
        _match(TTYPE::OPEN_PAREN_);
        std::vector<std::shared_ptr<VarDeclNode>> params = _parseFParams();
        _match(TTYPE::CLOSE_PAREN_);
        _match(TTYPE::COLON_);
        std::string returnType = _parseReturnType();
        _match(TTYPE::SEMICOLON_);

        auto funcProto = std::make_shared<FuncDefNode>(line, returnType, memberName, "");
        for (const auto& param : params) {
            funcProto->addParam(param);
        }
        return funcProto;
    }
    else if (LTTYPE == TTYPE::ID_) {
        // Variable declaration (e.g., "public Point p;")
        // Here, 'memberName' is the type ("Point").
        _derivationSteps.push_back("MemberDeclIdTail -> 'id' ArraySizeList ';'");
        
        // We MUST consume the variable name! (e.g., "p")
        std::string varName = _lookaheadToken.getValue();
        _match(TTYPE::ID_); 
        
        std::vector<int> dims = _parseArraySizeList();
        _match(TTYPE::SEMICOLON_);

        // Now we pass ALL 4 arguments: line, type (firstId), name (varName), visibility
        auto var = std::make_shared<VarDeclNode>(line, memberName, varName, visibility);
        for (int dim : dims) {
            var->addDimension(dim);
        }
        return var;
    }

    else {
        // Variable declaration
        _derivationSteps.push_back("MemberDeclIdTail -> ArraySizeList ';'");
        std::vector<int> dims = _parseArraySizeList();
        _match(TTYPE::SEMICOLON_);

        auto var = std::make_shared<VarDeclNode>(line, memberName, visibility);
        for (int dim : dims) {
            var->addDimension(dim);
        }
        return var;
    }
}

/**
 * @brief Parse member declaration tail when explicit type token started declaration.
 * @param typeName Parsed member type.
 * @param memberName Parsed member/function name.
 * @param visibility Visibility modifier.
 * @param line Source line.
 * @return Member declaration node.
 */
std::shared_ptr<ASTNode> Parser::_parseMemberDeclTypeTail(const std::string& typeName, const std::string& memberName, const std::string& visibility, int line){
    // Grammar: MemberDeclTypeTail -> ArraySizeList ';'
    //                              | '(' FParams ')' ':' ReturnType ';'
    if(LTTYPE == TTYPE::OPEN_PAREN_){
        // Function prototype
        _derivationSteps.push_back("MemberDeclTypeTail -> '(' FParams ')' ':' ReturnType ';'");
        _match(TTYPE::OPEN_PAREN_);
        std::vector<std::shared_ptr<VarDeclNode>> params = _parseFParams();
        _match(TTYPE::CLOSE_PAREN_);
        _match(TTYPE::COLON_);
        std::string returnType = _parseReturnType();
        _match(TTYPE::SEMICOLON_);

        auto funcProto = std::make_shared<FuncDefNode>(line, returnType, memberName, "");
        for (const auto& param : params) {
            funcProto->addParam(param);
        }
        return funcProto;
    }
    else {
        // Variable declaration
        _derivationSteps.push_back("MemberDeclTypeTail -> ArraySizeList ';'");
        std::vector<int> dims = _parseArraySizeList();
        _match(TTYPE::SEMICOLON_);

        auto var = std::make_shared<VarDeclNode>(line, typeName, memberName, visibility);
        for (int dim : dims) {
            var->addDimension(dim);
        }
        return var;
    }
}

/**
 * @brief Parse class member declaration entry point.
 * @param visibility Visibility modifier from preceding production.
 * @return Member declaration AST node.
 */
std::shared_ptr<ASTNode> Parser::_parseMemberDecl(const std::string& visibility) {
    // Grammar: MemberDecl -> 'ID_' 'ID_' MemberDeclIdTail
    //                      | 'INTEGER_TYPE_' 'ID_' MemberDeclTypeTail
    //                      | 'FLOAT_TYPE_' 'ID_' MemberDeclTypeTail
    switch(LTTYPE){
        case TTYPE::ID_:
        {
            _derivationSteps.push_back("MemberDecl -> 'id' MemberDeclIdTail");
            Token nameToken = _lookaheadToken;
            _match(TTYPE::ID_);
            return _parseMemberDeclIdTail(nameToken.getValue(), visibility, nameToken.getLineNumber());
        }

        case TTYPE::INTEGER_TYPE_:
        case TTYPE::FLOAT_TYPE_:
        {
            _derivationSteps.push_back("MemberDecl -> Type 'id' MemberDeclTypeTail");
            std::shared_ptr<TypeNode> typeNode = _parseType();
            Token nameToken = _lookaheadToken;
            _match(TTYPE::ID_);
            return _parseMemberDeclTypeTail(typeNode->getValue(), nameToken.getValue(), visibility, nameToken.getLineNumber());
        }

        default:
            _reportError("Expected member declaration (type or identifier)", {TTYPE::ID_, TTYPE::INTEGER_TYPE_, TTYPE::FLOAT_TYPE_});
    }
}

/**
 * @brief Parse visibility modifier.
 * @return Visibility string (public/private).
 */
std::string Parser::_parseVisibility(){
    switch(LTTYPE){
        case TTYPE::PUBLIC_KEYWORD_:
            _derivationSteps.push_back("Visibility -> 'public'");
            _match(TTYPE::PUBLIC_KEYWORD_);
            return "public";

        case TTYPE::PRIVATE_KEYWORD_:
            _derivationSteps.push_back("Visibility -> 'private'");
            _match(TTYPE::PRIVATE_KEYWORD_);
            return "private";

        default:
            _reportError("Expected visibility modifier (public or private)", {TTYPE::PUBLIC_KEYWORD_, TTYPE::PRIVATE_KEYWORD_});
    }
}

/**
 * @brief Parse one class member declaration with leading visibility.
 * @return Member declaration node.
 */
std::shared_ptr<ASTNode> Parser::_parseClassMemberDecl() {
    _derivationSteps.push_back("ClassMemberDecl -> Visibility MemberDecl");
    std::string visibility = _parseVisibility();
    return _parseMemberDecl(visibility);
}

/**
 * @brief Parse class body sequence.
 * @return Vector of member declarations.
 */
std::vector<std::shared_ptr<ASTNode>> Parser::_parseClassBody(){
    std::vector<std::shared_ptr<ASTNode>> members;
    // Grammar: ClassBody -> ClassMemberDecl ClassBody | EPSILON
    // Iterative with panic mode error recovery
    while (LTTYPE == TTYPE::PUBLIC_KEYWORD_ || LTTYPE == TTYPE::PRIVATE_KEYWORD_) {
        try {
            _derivationSteps.push_back("ClassBody -> ClassMemberDecl ClassBody");
            std::shared_ptr<ASTNode> member = _parseClassMemberDecl();
            if (member != nullptr) {
                members.push_back(member);
            }
        } catch (const SyntaxError& e) {
            _skipUntil({TTYPE::PUBLIC_KEYWORD_, TTYPE::PRIVATE_KEYWORD_,
                       TTYPE::CLOSE_BRACE_, TTYPE::END_OF_FILE_});
            _inErrorRecoveryMode = false;
        }
    }
    _derivationSteps.push_back("ClassBody -> EPSILON");
    return members;
}

/**
 * @brief Parse trailing inherited-class list.
 * @return Additional parent class names.
 */
std::vector<std::string> Parser::_parseInheritsList(){
    std::vector<std::string> parents;
    // Grammar: InheritsList -> , id InheritsList | EPSILON
    if(LTTYPE == TTYPE::COMMA_){
        _derivationSteps.push_back("InheritsList -> ',' 'id' InheritsList");
        _match(TTYPE::COMMA_);
        Token parentToken = _lookaheadToken;
        _match(TTYPE::ID_);
        parents.push_back(parentToken.getValue());
        std::vector<std::string> tail = _parseInheritsList();
        parents.insert(parents.end(), tail.begin(), tail.end());
    }
    // Case 2: EPSILON
    else {
        _derivationSteps.push_back("InheritsList -> EPSILON");
    }

    return parents;
}

/**
 * @brief Parse optional inheritance clause.
 * @return Parent class names (possibly empty).
 */
std::vector<std::string> Parser::_parseInheritanceOpt(){
    if(LTTYPE == TTYPE::INHERITS_){
        _derivationSteps.push_back("InheritanceOpt -> 'inherits' 'id' InheritsList");
        _match(TTYPE::INHERITS_);
        Token parentToken = _lookaheadToken;
        _match(TTYPE::ID_);
        std::vector<std::string> parents = {parentToken.getValue()};
        std::vector<std::string> tail = _parseInheritsList();
        parents.insert(parents.end(), tail.begin(), tail.end());
        return parents;
    }
    // Case 2: EPSILON
    else {
        _derivationSteps.push_back("InheritanceOpt -> EPSILON");
        return {};
    }
}


/**
 * @brief Parse class declaration.
 * @return Class declaration node.
 */
std::shared_ptr<ClassDeclNode> Parser::_parseClassDecl() {
    _derivationSteps.push_back("ClassDecl -> 'class' 'id' InheritanceOpt '{' ClassBody '}' ';'");
    _match(TTYPE::CLASS_KEYWORD_);
    Token classToken = _lookaheadToken;
    _match(TTYPE::ID_); // Class name
    std::vector<std::string> parents = _parseInheritanceOpt();
    _match(TTYPE::OPEN_BRACE_);
    std::vector<std::shared_ptr<ASTNode>> members = _parseClassBody();
    _match(TTYPE::CLOSE_BRACE_);
    _match(TTYPE::SEMICOLON_);

    auto classNode = std::make_shared<ClassDeclNode>(classToken.getLineNumber(), classToken.getValue());
    for (const auto& parent : parents) {
        classNode->addParentClass(parent);
    }
    for (const auto& member : members) {
        classNode->addMember(member);
    }
    return classNode;
}

/**
 * @brief Parse top-level function definition list with recovery.
 * @return Vector of function definitions.
 */
std::vector<std::shared_ptr<FuncDefNode>> Parser::_parseFuncDefList() {
    std::vector<std::shared_ptr<FuncDefNode>> funcs;
    // Grammar: FuncDefList -> FuncDef FuncDefList | EPSILON
    // Iterative with panic mode error recovery
    while (LTTYPE == TTYPE::ID_) {
        try {
            _derivationSteps.push_back("FuncDefList -> FuncDef FuncDefList");
            std::shared_ptr<FuncDefNode> func = _parseFuncDef();
            if (func != nullptr) {
                funcs.push_back(func);
            }
        } catch (const SyntaxError& e) {
            _skipUntil({TTYPE::ID_, TTYPE::MAIN_, TTYPE::END_OF_FILE_});
            _inErrorRecoveryMode = false;
        }
    }
    _derivationSteps.push_back("FuncDefList -> EPSILON");
    return funcs;
}

/**
 * @brief Parse top-level class declaration list with recovery.
 * @return Vector of class declarations.
 */
std::vector<std::shared_ptr<ClassDeclNode>> Parser::_parseClassDeclList() {
    std::vector<std::shared_ptr<ClassDeclNode>> classes;
    // Grammar: ClassDeclList -> ClassDecl ClassDeclList | EPSILON
    // Iterative with panic mode error recovery
    while (LTTYPE == TTYPE::CLASS_KEYWORD_) {
        try {
            _derivationSteps.push_back("ClassDeclList -> ClassDecl ClassDeclList");
            std::shared_ptr<ClassDeclNode> cls = _parseClassDecl();
            if (cls != nullptr) {
                classes.push_back(cls);
            }
        } catch (const SyntaxError& e) {
            _skipUntil({TTYPE::CLASS_KEYWORD_, TTYPE::ID_, TTYPE::MAIN_, TTYPE::END_OF_FILE_});
            _inErrorRecoveryMode = false;
        }
    }
    _derivationSteps.push_back("ClassDeclList -> EPSILON");
    return classes;
}

/**
 * @brief Parse complete program grammar entry point.
 * @return Program AST root node.
 *
 * @details
 * Program parse proceeds in phases: class list, function list, then mandatory
 * main body. Each phase is recoverable to maximize diagnostics in one run.
 */
std::shared_ptr<ProgNode> Parser::_parseProgram() {
    _derivationSteps.push_back("Program -> ClassDeclList FuncDefList 'main' FuncBody");

    auto program = std::make_shared<ProgNode>(_lookaheadToken.getLineNumber());

    try {
        std::vector<std::shared_ptr<ClassDeclNode>> classes = _parseClassDeclList();
        for (const auto& cls : classes) {
            program->addClass(cls);
        }
    } catch (const SyntaxError& e) {
        _skipUntil({TTYPE::CLASS_KEYWORD_, TTYPE::ID_, TTYPE::MAIN_, TTYPE::END_OF_FILE_});
        _inErrorRecoveryMode = false;
    }

    try {
        std::vector<std::shared_ptr<FuncDefNode>> funcs = _parseFuncDefList();
        for (const auto& fn : funcs) {
            program->addFunction(fn);
        }
    } catch (const SyntaxError& e) {
        _skipUntil({TTYPE::MAIN_, TTYPE::END_OF_FILE_});
        _inErrorRecoveryMode = false;
    }

    try {
        Token mainToken = _lookaheadToken;
        _match(TTYPE::MAIN_);
        std::vector<std::shared_ptr<VarDeclNode>> locals;
        std::shared_ptr<BlockNode> mainBody = _parseFuncBody(&locals);

        auto mainFunc = std::make_shared<FuncDefNode>(mainToken.getLineNumber(), "void", "main");
        for (const auto& local : locals) {
            mainFunc->addLocalVar(local);
        }
        mainFunc->setRight(mainBody);
        program->addFunction(mainFunc);
    } catch (const SyntaxError& e) {
        _skipUntil({TTYPE::END_OF_FILE_});
        _inErrorRecoveryMode = false;
    }

    return program;
}
