#include "my_parser.h"
#include <fstream>
#include <iostream>
#include <algorithm>

#define TTYPE Token::Type
#define LTTYPE _lookaheadToken.getType()

// Static member definitions
Token Parser::_lookaheadToken(Token::Type::END_OF_FILE_, "", 0);
int Parser::_currentTokenIndex = 0;
bool Parser::_inErrorRecoveryMode = false;
std::vector<std::vector<Token>> Parser::_tokens;
std::vector<Token> Parser::_flatTokens;
std::vector<std::string> Parser::_errorMessages;
std::vector<std::string> Parser::_derivationSteps;

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

void Parser::_skipUntil(const std::vector<Token::Type>& followSet) {
    while (_currentTokenIndex < _flatTokens.size()) {
        _lookaheadToken = _flatTokens[_currentTokenIndex];

        if (std::find(followSet.begin(), followSet.end(), LTTYPE) != followSet.end()) {
            return; // Found a token in the follow set, stop skipping
        }
        _currentTokenIndex++; // Skip the token
    }
}

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

    // Prime the lookahead token
    if (!_nextToken()) {
        _errorMessages.push_back("[ERROR][SYNTAX] Empty token stream");
        return false;
    }

    try {
        _parseProgram();
    } catch (const SyntaxError& e) {
        // Error already logged in _reportError
    }

    return _errorMessages.empty();
}

const std::vector<std::string>& Parser::getErrorMessages() {
    return _errorMessages;
}

const std::vector<std::string>& Parser::getDerivationSteps() {
    return _derivationSteps;
}

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

void Parser::_reportError(const std::string& message, const std::vector<Token::Type>& expectedTokens) {
    std::string msg = _formatError(message, _lookaheadToken, expectedTokens);
    _errorMessages.push_back(msg);
    throw SyntaxError(msg);
}

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

bool Parser::_parseRelOp() {
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::EQUAL_ :
            _derivationSteps.push_back("RelOp -> 'eq'");
            _match(opType);
            return true;
        case TTYPE::NOT_EQUAL_:
            _derivationSteps.push_back("RelOp -> 'neq'");
            _match(opType);
            return true;
        case TTYPE::LESS_THAN_:
            _derivationSteps.push_back("RelOp -> 'lt'");
            _match(opType);
            return true;
        case TTYPE::GREATER_THAN_:
            _derivationSteps.push_back("RelOp -> 'gt'");
            _match(opType);
            return true;
        case TTYPE::LESS_EQUAL_:
            _derivationSteps.push_back("RelOp -> 'leq'");
            _match(opType);
            return true;
        case TTYPE::GREATER_EQUAL_:
            _derivationSteps.push_back("RelOp -> 'geq'");
            _match(opType);
            return true;

        default:
            _derivationSteps.push_back("RelOp -> FALSE");
            return false;
    }
}

bool Parser::_parseAddOp() {
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::PLUS_:
            _derivationSteps.push_back("AddOp -> '+'");
            _match(opType);
            return true;
        case TTYPE::MINUS_:
            _derivationSteps.push_back("AddOp -> '-'");
            _match(opType);
            return true;
        case TTYPE::OR_:
            _derivationSteps.push_back("AddOp -> 'or'");
            _match(opType);
            return true;

        default:
            _derivationSteps.push_back("AddOp -> FALSE");
            return false;
    }
}

bool Parser::_parseMultOp() {
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::MULTIPLY_:
            _derivationSteps.push_back("MultOp -> '*'");
            _match(opType);
            return true;
        case TTYPE::DIVIDE_:
            _derivationSteps.push_back("MultOp -> '/'");
            _match(opType);
            return true;
        case TTYPE::AND_:
            _derivationSteps.push_back("MultOp -> 'and'");
            _match(opType);
            return true;

        default:
            _derivationSteps.push_back("MultOp -> FALSE");
            return false;
    }

}

bool Parser::_parseSign() {
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::PLUS_:
            _derivationSteps.push_back("Sign -> '+'");
            _match(opType);
            return true;
        case TTYPE::MINUS_:
            _derivationSteps.push_back("Sign -> '-'");
            _match(opType);
            return true;

        default:
            _derivationSteps.push_back("Sign -> FALSE");
            return false;
    }
}

bool Parser::_parseAssignOp() {
    if (LTTYPE == TTYPE::ASSIGNMENT_) {
        _derivationSteps.push_back("AssignOp -> '='");
        _match(TTYPE::ASSIGNMENT_);
        return true;
    } else {
        _derivationSteps.push_back("AssignOp -> FALSE");
        return false;
    }
}

bool Parser::_parseType(){
    switch (LTTYPE)
    {
        case TTYPE::INTEGER_TYPE_:
            _derivationSteps.push_back("Type -> 'integer'");
            _match(LTTYPE);
            return true;
        case TTYPE::FLOAT_TYPE_:
            _derivationSteps.push_back("Type -> 'float'");
            _match(LTTYPE);
            return true;
        case TTYPE::ID_:
            _derivationSteps.push_back("Type -> 'id'");
            _match(LTTYPE);
            return true;

        default:
            _derivationSteps.push_back("Type -> FALSE");
            return false;
    }
}

// ============================================================================
// ARRAY PARSING FUNCTIONS
// ============================================================================

void Parser::_parseArraySizeTail() {
    // Grammar: ArraySizeTail -> intNum ] | ]

    // Case 1: intNum ]
    if (LTTYPE == TTYPE::INTEGER_LITERAL_) {
        _derivationSteps.push_back("ArraySizeTail -> 'intNum' ']'");
        _match(TTYPE::INTEGER_LITERAL_);
        _match(TTYPE::CLOSE_BRACKET_);
    }
    // Case 2: ]
    else if (LTTYPE == TTYPE::CLOSE_BRACKET_) {
        _derivationSteps.push_back("ArraySizeTail -> ']'");
        _match(TTYPE::CLOSE_BRACKET_);
    }
    // Error
    else {
        _reportError("Expected INTEGER_LITERAL or ']'", {TTYPE::INTEGER_LITERAL_, TTYPE::CLOSE_BRACKET_});
    }
}

void Parser::_parseArraySize() {
    // Grammar: ArraySize -> [ ArraySizeTail
    _derivationSteps.push_back("ArraySize -> '[' ArraySizeTail");
    _match(TTYPE::OPEN_BRACKET_);
    
    // we parse the tail (size or empty)
    _parseArraySizeTail();
}

void Parser::_parseArraySizeList() {
    // Grammar: ArraySizeList -> ArraySize ArraySizeList | EPSILON

    // Check FIRST set of ArraySize -> { [ }
    if (LTTYPE == TTYPE::OPEN_BRACKET_) {
        _derivationSteps.push_back("ArraySizeList -> ArraySize ArraySizeList");
        _parseArraySize();
        _parseArraySizeList(); // Recursive step
    }
    // EPSILON CASE:
    else {
        _derivationSteps.push_back("ArraySizeList -> EPSILON");
    }
}

void Parser::_parseIndice() {
    // Grammar: Indice -> [ Expr ]
    _derivationSteps.push_back("Indice -> '[' Expr ']'");
    _match(TTYPE::OPEN_BRACKET_);
    _parseExpr();
    _match(TTYPE::CLOSE_BRACKET_);
}

void Parser::_parseIndiceList(){
    // Grammar: IndiceList -> Indice IndiceList | EPSILON
    if (LTTYPE == TTYPE::OPEN_BRACKET_) {
        _derivationSteps.push_back("IndiceList -> Indice IndiceList");
        _parseIndice();
        _parseIndiceList(); // Recursive step for multiple indices
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("IndiceList -> EPSILON");
    }
}

void Parser::_parseAParamsTail() {
    // Grammar: AParamsTail -> , Expr AParamsTail | EPSILON

    // Case 1: , Expr AParamsTail
    if (LTTYPE == TTYPE::COMMA_) {
        _derivationSteps.push_back("AParamsTail -> ',' Expr AParamsTail");
        _match(TTYPE::COMMA_);
        _parseExpr();
        _parseAParamsTail(); // Recursive step for more parameters
    }
    // Case 2: EPSILON
    else {
        _derivationSteps.push_back("AParamsTail -> EPSILON");
    }
}

void Parser::_parseAParams() {
    // Grammar: AParams -> Expr AParamsTail
    _derivationSteps.push_back("AParams -> Expr AParamsTail");
    _parseExpr();
    _parseAParamsTail();
}

void Parser::_parseVariable(){
    _derivationSteps.push_back("Variable -> 'id' FactorIdTail");
    _match(TTYPE::ID_);
    _parseFactorIdTail(); // Check for array access or function call
}

void Parser::_parseFactorCallTail(){
    if(LTTYPE == TTYPE::DOT_){
        _derivationSteps.push_back("FactorCallTail -> '.' 'id' FactorIdTail");
        _match(TTYPE::DOT_);
        _match(TTYPE::ID_);
        _parseFactorIdTail(); // Check for more accesses or calls
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("FactorCallTail -> EPSILON");
    }
}

void Parser::_parseFactorRest(){
    switch (LTTYPE)
    {
    case TTYPE::DOT_:
        _derivationSteps.push_back("FactorRest -> '.' 'id' FactorIdTail");
        _match(TTYPE::DOT_);
        _match(TTYPE::ID_);
        _parseFactorIdTail(); // Check for more accesses or calls
        break;

    case TTYPE::OPEN_PAREN_:
        _derivationSteps.push_back("FactorRest -> '(' AParams ')' FactorCallTail");
        _match(TTYPE::OPEN_PAREN_);
        _parseAParams(); // Parse arguments
        _match(TTYPE::CLOSE_PAREN_);
        _parseFactorCallTail(); // Check for method calls on the result
        break;

    default:
        _derivationSteps.push_back("FactorRest -> EPSILON");
        break;
    }
}

void Parser::_parseFactorIdTail(){
    _derivationSteps.push_back("FactorIdTail -> IndiceList FactorRest");
    _parseIndiceList(); // Check for array access
    _parseFactorRest(); // Check for function call or member access
}

void Parser::_parseFactor() {
    switch (LTTYPE)
    {
    case TTYPE::ID_:
        _derivationSteps.push_back("Factor -> 'id' FactorIdTail");
        _match(TTYPE::ID_);
        _parseFactorIdTail();
        break;

    case TTYPE::INTEGER_LITERAL_:
        _derivationSteps.push_back("Factor -> 'intLit'");
        _match(LTTYPE); // Match the literal
        break;

    case TTYPE::FLOAT_LITERAL_:
        _derivationSteps.push_back("Factor -> 'floatLit'");
        _match(LTTYPE); // Match the literal
        break;

    case TTYPE::OPEN_PAREN_:
        _derivationSteps.push_back("Factor -> '(' ArithExpr ')'");
        _match(TTYPE::OPEN_PAREN_);
        _parseArithExpr();
        _match(TTYPE::CLOSE_PAREN_);
        break;

    // Need to check the first set of sign because we are in a switch statement and need to check something
    case TTYPE::MINUS_:
    case TTYPE::PLUS_:
        _derivationSteps.push_back("Factor -> Sign Factor");
        _parseSign();
        _parseFactor();
        break;

    case TTYPE::NOT_:
        _derivationSteps.push_back("Factor -> '!' Factor");
        _match(TTYPE::NOT_);
        _parseFactor();
        break;

    default:
        _reportError("Expected factor (identifier, literal, '(', sign, or '!')", {TTYPE::ID_, TTYPE::INTEGER_LITERAL_, TTYPE::FLOAT_LITERAL_, TTYPE::OPEN_PAREN_, TTYPE::MINUS_, TTYPE::PLUS_, TTYPE::NOT_});
    }
}

void Parser::_parseMultOpTail(){
    if(LTTYPE == TTYPE::MULTIPLY_ || LTTYPE == TTYPE::DIVIDE_ || LTTYPE == TTYPE::AND_){ // First set of MultOp
        _derivationSteps.push_back("MultOpTail -> MultOp Factor MultOpTail");
        _parseMultOp(); // Consume the operator
        _parseFactor();
        _parseMultOpTail(); // Recursive step for multiple operations
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("MultOpTail -> EPSILON");
    }
}


void Parser::_parseTerm(){
    _derivationSteps.push_back("Term -> Factor MultOpTail");
    _parseFactor();
    _parseMultOpTail(); // Check for multiplication/division
}

void Parser::_parseAddOpTail(){
    if(LTTYPE == TTYPE::PLUS_ || LTTYPE == TTYPE::MINUS_ || LTTYPE == TTYPE::OR_){ // First set of AddOp
        _derivationSteps.push_back("AddOpTail -> AddOp Term AddOpTail");
        _parseAddOp(); // Consume the operator
        _parseTerm();
        _parseAddOpTail(); // Recursive step for multiple operations
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("AddOpTail -> EPSILON");
    }
}

void Parser::_parseArithExpr(){
    _derivationSteps.push_back("ArithExpr -> Term AddOpTail");
    _parseTerm();
    _parseAddOpTail(); // Check for addition/subtraction
}

void Parser::_parseRelExpr(){
    _derivationSteps.push_back("RelExpr -> ArithExpr RelOp ArithExpr");
    _parseArithExpr();
    if(!_parseRelOp()){
        _reportError("Expected relational operator", {TTYPE::EQUAL_, TTYPE::NOT_EQUAL_, TTYPE::LESS_THAN_, TTYPE::GREATER_THAN_, TTYPE::LESS_EQUAL_, TTYPE::GREATER_EQUAL_});
    }
    _parseArithExpr();
}

void Parser::_parseExprTail(){
    if(_parseRelOp()){
        _derivationSteps.push_back("ExprTail -> RelOp ArithExpr");
        _parseArithExpr();
    }
    else {
        _derivationSteps.push_back("ExprTail -> EPSILON");
    }
}

void Parser::_parseExpr(){
    _derivationSteps.push_back("Expr -> ArithExpr ExprTail");
    _parseArithExpr();
    _parseExprTail(); // Check for relational operator
}

void Parser::_parseStatementCallTail(){
    if(LTTYPE == TTYPE::DOT_){
        _derivationSteps.push_back("StatementCallTail -> '.' 'id' StatementIdTail");
        _match(TTYPE::DOT_);
        _match(TTYPE::ID_);
        _parseStatementIdTail(); // Check for more accesses or calls
    }
    else if(LTTYPE == TTYPE::SEMICOLON_){
        _derivationSteps.push_back("StatementCallTail -> ';'");
        _match(TTYPE::SEMICOLON_);
    }
    else{
        _reportError("Expected '.' for member access or ';' to end statement", {TTYPE::DOT_, TTYPE::SEMICOLON_});
    }
}

void Parser::_parseStatementRest(){
    if(LTTYPE == TTYPE::DOT_){
        _derivationSteps.push_back("StatementRest -> '.' 'id' StatementIdTail");
        _match(TTYPE::DOT_);
        _match(TTYPE::ID_);
        _parseStatementIdTail(); // Check for more accesses or calls
    }
    else if(LTTYPE == TTYPE::OPEN_PAREN_){
        _derivationSteps.push_back("StatementRest -> '(' AParams ')' StatementCallTail");
        _match(TTYPE::OPEN_PAREN_);
        _parseAParams(); // Parse arguments
        _match(TTYPE::CLOSE_PAREN_);
        _parseStatementCallTail(); // Check for method calls on the result
    }

    else if(LTTYPE == TTYPE::ASSIGNMENT_){
        _derivationSteps.push_back("StatementRest -> AssignOp Expr ';'");
        _parseAssignOp();
        _parseExpr();
        _match(TTYPE::SEMICOLON_);
    }

    else{
        _reportError("Expected '.', '(', or '=' for function call or member access", {TTYPE::DOT_, TTYPE::OPEN_PAREN_, TTYPE::ASSIGNMENT_});
    }
}

void Parser::_parseStatementIdTail(){
    _derivationSteps.push_back("StatementIdTail -> IndiceList StatementRest");
    _parseIndiceList(); // Check for array access
    _parseStatementRest(); // Check for function call, member access, or assignment
}

void Parser::_parseStatBlock(){
    if(LTTYPE == TTYPE::DO_KEYWORD_){
        _derivationSteps.push_back("StatBlock -> '{' StatementList '}'");
        _match(TTYPE::DO_KEYWORD_);
        _parseStatementList();
        _match(TTYPE::END_KEYWORD_);
    }
    else if(_parseStatement()){
        return;
    }
    else {
        _derivationSteps.push_back("StatBlock -> EPSILON");
        return; // EPSILON case: do nothing (return)
    }
}

bool Parser::_parseStatement() {
    switch(LTTYPE) {
        case TTYPE::IF_KEYWORD_:
            _derivationSteps.push_back("Statement -> 'if' '(' RelExpr ')' 'then' StatBlock 'else' StatBlock ';'");
            _match(TTYPE::IF_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            _parseRelExpr();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::THEN_KEYWORD_);
            _parseStatBlock();
            _match(TTYPE::ELSE_KEYWORD_);
            _parseStatBlock();
            _match(TTYPE::SEMICOLON_);
            return true;

        case TTYPE::WHILE_KEYWORD_:
            _derivationSteps.push_back("Statement -> 'while' '(' RelExpr ')' StatBlock ';'");
            _match(TTYPE::WHILE_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            _parseRelExpr();
            _match(TTYPE::CLOSE_PAREN_);
            _parseStatBlock();
            _match(TTYPE::SEMICOLON_);
            return true;

        case TTYPE::READ_KEYWORD_:
            _derivationSteps.push_back("Statement -> 'read' '(' Variable ')' ';'");
            _match(TTYPE::READ_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            _parseVariable();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::SEMICOLON_);
            return true;

        case TTYPE::WRITE_KEYWORD_:
            _derivationSteps.push_back("Statement -> 'write' '(' Expr ')' ';'");
            _match(TTYPE::WRITE_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            _parseExpr();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::SEMICOLON_);
            return true;

        case TTYPE::RETURN_KEYWORD_:
            _derivationSteps.push_back("Statement -> 'return' '(' Expr ')' ';'");
            _match(TTYPE::RETURN_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            _parseExpr();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::SEMICOLON_);
            return true;

        case TTYPE::ID_:
            _derivationSteps.push_back("Statement -> 'id' StatementIdTail");
            _match(TTYPE::ID_);
            _parseStatementIdTail(); // Check for function call, member access, or assignment
            return true;

        default:
            return false;
    }
}
void Parser::_parseStatementList() {
    // Grammar: StatementList -> Statement StatementList | EPSILON

    // Case 1: Statement StatementList
    if (_parseStatement()) {
        _derivationSteps.push_back("StatementList -> Statement StatementList");
        _parseStatementList(); // Recursive step for more statements
    }
    // Case 2: EPSILON
    else {
        _derivationSteps.push_back("StatementList -> EPSILON");
    }
}
bool Parser::_parseVarDecl() {
    if(!_parseType()){
        return false; // No type found, not a var declaration
    }
    _derivationSteps.push_back("VarDecl -> Type 'id' ArraySizeList ';'");
    _match(TTYPE::ID_);
    _parseArraySizeList(); // Check for array declaration
    _match(TTYPE::SEMICOLON_);
    return true;
}

void Parser::_parseVarDeclList() {
    if (_parseVarDecl()) {
        _derivationSteps.push_back("VarDeclList -> VarDecl VarDeclList");
        _parseVarDeclList(); // Recursive step for more variable declarations
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("VarDeclList -> EPSILON");
    }
}

void Parser::_parseLocalVarDeclList(){
    if(LTTYPE == TTYPE::LOCAL_){
        _derivationSteps.push_back("LocalVarDeclList -> 'local' VarDeclList");
        _match(TTYPE::LOCAL_);
        _parseVarDeclList(); // Parse variable declarations
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("LocalVarDeclList -> EPSILON");
    }
}

void Parser::_parseFuncBody() {
    _derivationSteps.push_back("FuncBody -> LocalVarDeclList 'do' StatementList 'end'");
    _parseLocalVarDeclList(); // Parse local variable declarations
    _match(TTYPE::DO_KEYWORD_);
    _parseStatementList(); // Parse the statements in the function body
    _match(TTYPE::END_KEYWORD_);
}

void Parser::_parseFParamsTail() {
    // Grammar: FParamsTail -> , Type id FParamsTail | EPSILON

    // Case 1: , Type id FParamsTail
    if (LTTYPE == TTYPE::COMMA_) {
        _derivationSteps.push_back("FParamsTail -> ',' Type 'id' ArraySizeList FParamsTail");
        _match(TTYPE::COMMA_);
        if(!_parseType()){
            _reportError("Expected type (INTEGER_TYPE_, FLOAT_TYPE_, or identifier)", {TTYPE::INTEGER_TYPE_, TTYPE::FLOAT_TYPE_, TTYPE::ID_});
        }
        _match(TTYPE::ID_);
        _parseArraySizeList(); // Check for array parameter
        _parseFParamsTail(); // Recursive step for more parameters
    }
    // Case 2: EPSILON
    else {
        _derivationSteps.push_back("FParamsTail -> EPSILON");
    }
}


void Parser::_parseFParams() {
    if(_parseType()){
        _derivationSteps.push_back("FParams -> Type 'id' ArraySizeList FParamsTail");
        _match(TTYPE::ID_);
        _parseArraySizeList(); // Check for array parameter
        _parseFParamsTail(); // Check for more parameters
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("FParams -> EPSILON");
    }
}


void Parser::_parseReturnType(){
    if(LTTYPE == TTYPE::VOID_TYPE_){
        _derivationSteps.push_back("ReturnType -> 'void'");
        _match(TTYPE::VOID_TYPE_);
    }
    else if(_parseType()){
        _derivationSteps.push_back("ReturnType -> Type");
        return;
    }
    else{
        _reportError("Expected return type (VOID_TYPE_, INTEGER_TYPE_, FLOAT_TYPE_, or ID_)", {TTYPE::VOID_TYPE_, TTYPE::INTEGER_TYPE_, TTYPE::FLOAT_TYPE_, TTYPE::ID_});
    }
}

void Parser::_parseFuncHeadTail() {
    if(LTTYPE == TTYPE::COLON_COLON_){
        _derivationSteps.push_back("FuncHeadTail -> '::' 'id' '(' FParams ')' ':' ReturnType");
        _match(TTYPE::COLON_COLON_);
        _match(TTYPE::ID_);
        _match(TTYPE::OPEN_PAREN_);
        _parseFParams(); // Parse parameters
        _match(TTYPE::CLOSE_PAREN_);
        _match(TTYPE::COLON_);
        _parseReturnType(); // Parse return type
    }
    else if(LTTYPE == TTYPE::OPEN_PAREN_){
        _derivationSteps.push_back("FuncHeadTail -> '(' FParams ')' ':' ReturnType");
        _match(TTYPE::OPEN_PAREN_);
        _parseFParams(); // Parse parameters
        _match(TTYPE::CLOSE_PAREN_);
        _match(TTYPE::COLON_);
        _parseReturnType(); // Parse return type

    }
    else{
        _reportError("Expected '(' for parameters or ';' for function declaration", {TTYPE::OPEN_PAREN_, TTYPE::SEMICOLON_});
    }
}

void Parser::_parseFuncHead() {
    _derivationSteps.push_back("FuncHead -> 'id' FuncHeadTail");
    _match(TTYPE::ID_); // Function name
    _parseFuncHeadTail(); // Check for parameters and return type
}

void Parser::_parseFuncDef() {
    if(LTTYPE == TTYPE::ID_){ // First set of FuncHead
        _derivationSteps.push_back("FuncDef -> FuncHead FuncBody");
        _parseFuncHead(); // Parse function header
        _parseFuncBody(); // Parse function body
    }
    else{
        _reportError("Expected function definition (identifier)", {TTYPE::ID_});
    }
}

void Parser::_parseMemberDeclIdTail(){
    // Grammar: MemberDeclIdTail -> ArraySizeList ';'
    //                            | '(' FParams ')' ':' ReturnType ';'
    if(LTTYPE == TTYPE::OPEN_PAREN_){
        // Function prototype
        _derivationSteps.push_back("MemberDeclIdTail -> '(' FParams ')' ':' ReturnType ';'");
        _match(TTYPE::OPEN_PAREN_);
        _parseFParams();
        _match(TTYPE::CLOSE_PAREN_);
        _match(TTYPE::COLON_);
        _parseReturnType();
        _match(TTYPE::SEMICOLON_);
    }
    else {
        // Variable declaration
        _derivationSteps.push_back("MemberDeclIdTail -> ArraySizeList ';'");
        _parseArraySizeList();
        _match(TTYPE::SEMICOLON_);
    }
}

void Parser::_parseMemberDeclTypeTail(){
    // Grammar: MemberDeclTypeTail -> ArraySizeList ';'
    //                              | '(' FParams ')' ':' ReturnType ';'
    if(LTTYPE == TTYPE::OPEN_PAREN_){
        // Function prototype
        _derivationSteps.push_back("MemberDeclTypeTail -> '(' FParams ')' ':' ReturnType ';'");
        _match(TTYPE::OPEN_PAREN_);
        _parseFParams();
        _match(TTYPE::CLOSE_PAREN_);
        _match(TTYPE::COLON_);
        _parseReturnType();
        _match(TTYPE::SEMICOLON_);
    }
    else {
        // Variable declaration
        _derivationSteps.push_back("MemberDeclTypeTail -> ArraySizeList ';'");
        _parseArraySizeList();
        _match(TTYPE::SEMICOLON_);
    }
}

void Parser::_parseMemberDecl() {
    // Grammar: MemberDecl -> 'ID_' 'ID_' MemberDeclIdTail
    //                      | 'INTEGER_TYPE_' 'ID_' MemberDeclTypeTail
    //                      | 'FLOAT_TYPE_' 'ID_' MemberDeclTypeTail
    switch(LTTYPE){
        case TTYPE::ID_:
            _derivationSteps.push_back("MemberDecl -> 'id' MemberDeclIdTail");
            _match(TTYPE::ID_);  // Match the type (class name)
            _parseMemberDeclIdTail();
            break;

        case TTYPE::INTEGER_TYPE_:
        case TTYPE::FLOAT_TYPE_:
            _derivationSteps.push_back("MemberDecl -> Type 'id' MemberDeclTypeTail");
            _match(LTTYPE); // Match the type (int or float)
            _match(TTYPE::ID_); // Match the member name
            _parseMemberDeclTypeTail();
            break;

        default:
            _reportError("Expected member declaration (type or identifier)", {TTYPE::ID_, TTYPE::INTEGER_TYPE_, TTYPE::FLOAT_TYPE_});
    }
}

bool Parser::_parseVisibility(){
    switch(LTTYPE){
        case TTYPE::PUBLIC_KEYWORD_:
            _derivationSteps.push_back("Visibility -> 'public'");
            _match(TTYPE::PUBLIC_KEYWORD_);
            return true;

        case TTYPE::PRIVATE_KEYWORD_:
            _derivationSteps.push_back("Visibility -> 'private'");
            _match(TTYPE::PRIVATE_KEYWORD_);
            return false;

        default:
            _reportError("Expected visibility modifier (public or private)", {TTYPE::PUBLIC_KEYWORD_, TTYPE::PRIVATE_KEYWORD_});
    }
}

void Parser::_parseClassMemberDecl() {
    _derivationSteps.push_back("ClassMemberDecl -> Visibility MemberDecl");
    _parseVisibility(); // Parse visibility modifier
    _parseMemberDecl(); // Parse the member declaration
}

void Parser::_parseClassBody(){
    if(LTTYPE == TTYPE::PUBLIC_KEYWORD_ || LTTYPE == TTYPE::PRIVATE_KEYWORD_){ // First set of ClassMemberDecl AND visibility
        _derivationSteps.push_back("ClassBody -> ClassMemberDecl ClassBody");
        _parseClassMemberDecl(); // Parse class member declaration
        _parseClassBody(); // Recursive step for more members
    }
    // EPSILON case
    else {
        _derivationSteps.push_back("ClassBody -> EPSILON");
    }
}

void Parser::_parseInheritsList(){
    // Grammar: InheritsList -> , id InheritsList | EPSILON
    if(LTTYPE == TTYPE::COMMA_){
        _derivationSteps.push_back("InheritsList -> ',' 'id' InheritsList");
        _match(TTYPE::COMMA_);
        _match(TTYPE::ID_);
        _parseInheritsList(); // Recursive step for more inherited classes
    }
    // Case 2: EPSILON
    else {
        _derivationSteps.push_back("InheritsList -> EPSILON");
    }
}

void Parser::_parseInheritanceOpt(){
    if(LTTYPE == TTYPE::INHERITS_){
        _derivationSteps.push_back("InheritanceOpt -> 'inherits' 'id' InheritsList");
        _match(TTYPE::INHERITS_);
        _match(TTYPE::ID_);
        _parseInheritsList(); // Parse the list of inherited classes
    }
    // Case 2: EPSILON
    else {
        _derivationSteps.push_back("InheritanceOpt -> EPSILON");
    }
}


void Parser::_parseClassDecl() {
    _derivationSteps.push_back("ClassDecl -> 'class' 'id' InheritanceOpt '{' ClassBody '}' ';'");
    _match(TTYPE::CLASS_KEYWORD_);
    _match(TTYPE::ID_); // Class name
    _parseInheritanceOpt(); // Check for optional inheritance
    _match(TTYPE::OPEN_BRACE_);
    _parseClassBody(); // Parse class body
    _match(TTYPE::CLOSE_BRACE_);
    _match(TTYPE::SEMICOLON_);
}

void Parser::_parseFuncDefList() {
    if(LTTYPE == TTYPE::ID_){ // First set of FuncDef
        _derivationSteps.push_back("FuncDefList -> FuncDef FuncDefList");
        _parseFuncDef(); // Parse function definition
        _parseFuncDefList(); // Recursive step for more function definitions
    }
    // Case 2: EPSILON
    else {
        _derivationSteps.push_back("FuncDefList -> EPSILON");
    }
}

void Parser::_parseClassDeclList() {
    if(LTTYPE == TTYPE::CLASS_KEYWORD_){ // First set of ClassDecl
        _derivationSteps.push_back("ClassDeclList -> ClassDecl ClassDeclList");
        _parseClassDecl(); // Parse class declaration
        _parseClassDeclList(); // Recursive step for more class declarations
    }
    // Case 2: EPSILON
    else {
        _derivationSteps.push_back("ClassDeclList -> EPSILON");
    }
}

void Parser::_parseProgram() {
    _derivationSteps.push_back("Program -> ClassDeclList FuncDefList 'main' FuncBody");
    _parseClassDeclList(); // Parse class declarations
    _parseFuncDefList(); // Parse function definitions
    _match(TTYPE::MAIN_);
    _parseFuncBody(); // Parse main function body
}
