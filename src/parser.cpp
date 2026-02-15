#include "parser.h"
#include <fstream>
#include <iostream>

#define TTYPE Token::Type
#define LTTYPE _lookaheadToken.getType()

static bool Parser::_nextToken() {
    if (_currentTokenIndex < _tokens.size()) {
        Token token = _tokens[_currentTokenIndex++];
        _lookaheadToken = token;
        return true;
    } else {
        // Return an EOF token if we've reached the end of the token list
        return false;
    }
}

static void Parser::_skipUntil(const std::vector<Token::Type>& followSet) {
    while (_currentTokenIndex < _tokens.size()) {
        _lookaheadToken = _tokens[_currentTokenIndex];

        if (std::find(followSet.begin(), followSet.end(), LTTYPE) != followSet.end()) {
            return; // Found a token in the follow set, stop skipping
        }
        _currentTokenIndex++; // Skip the token
    }
}

static bool Parser::parseTokens(std::vector<Token>& tokens) {
    _tokens = tokens;
    _currentTokenIndex = 0;


    //TODO
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

static void Parser::_match(Token::Type expectedType) {
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

static bool Parser::_parseRelOp() {
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::EQUAL_ :
        case TTYPE::NOT_EQUAL_:
        case TTYPE::LESS_THAN_:
        case TTYPE::GREATER_THAN_:
        case TTYPE::LESS_EQUAL_:
        case TTYPE::GREATER_EQUAL_:
            _match(opType); 
            return true;
            
        default:
            _reportError("Expected relational operator", {TTYPE::EQUAL_, TTYPE::NOT_EQUAL_, TTYPE::LESS_THAN_, TTYPE::GREATER_THAN_, TTYPE::LESS_EQUAL_, TTYPE::GREATER_EQUAL_});
    }
}

static bool Parser::_parseAddOp() {
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::PLUS_:
        case TTYPE::MINUS_:
        case TTYPE::OR_:
            _match(opType); // Fix: Use match
            return true;
            
        default:
            _reportError("Expected '+', '-', or 'or'", {TTYPE::PLUS_, TTYPE::MINUS_, TTYPE::OR_});
    }
}

static bool Parser::_parseMultOp() {
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::MULTIPLY_:
        case TTYPE::DIVIDE_:
        case TTYPE::AND_:
            _match(opType); // Fix: Use match
            return true;
            
        default:
            _reportError("Expected '*', '/', or '&'", {TTYPE::MULTIPLY_, TTYPE::DIVIDE_, TTYPE::AND_});
    }

}

static bool Parser::_parseSign() {
    Token::Type opType = LTTYPE;

    switch (opType) {
        case TTYPE::PLUS_:
        case TTYPE::MINUS_:
            _match(opType); // Fix: Use match
            return true;
            
        default:
            _reportError("Expected '+' or '-'", {TTYPE::PLUS_, TTYPE::MINUS_});
    }
}

static bool Parser::_parseAssignOp() {
    if (LTTYPE == TTYPE::ASSIGNMENT_) {
        _match(TTYPE::ASSIGNMENT_);
        return true;
    } else {
        _reportError("Expected assignment operator '='", {TTYPE::ASSIGNMENT_});
    }
}

static bool Parser::_parseType(){
    switch (LTTYPE)
    {
        case TTYPE::INTEGER_TYPE_:
        case TTYPE::FLOAT_TYPE_:
        case TTYPE::ID_:
            _match(LTTYPE); // Match the type
            return true;

        default:
            _reportError("Expected type (int, float, or identifier)", {TTYPE::INTEGER_TYPE_, TTYPE::FLOAT_TYPE_, TTYPE::ID_});
    }
}

// ============================================================================
// ARRAY PARSING FUNCTIONS
// ============================================================================

static void Parser::_parseArraySizeTail() {
    // Grammar: ArraySizeTail -> intNum ] | ]
    
    // Case 1: intNum ]
    if (LTTYPE == TTYPE::INTEGER_LITERAL) { // Assuming INTEGER_LITERAL for numbers
        _match(TTYPE::INTEGER_LITERAL);
        _match(TTYPE::CLOSE_BRACKET_);
    }
    // Case 2: ]
    else if (LTTYPE == TTYPE::CLOSE_BRACKET_) {
        _match(TTYPE::CLOSE_BRACKET_);
    }
    // Error
    else {
        _reportError("Expected integer or ']'", {TTYPE::INTEGER_LITERAL, TTYPE::CLOSE_BRACKET_});
    }
}

static void Parser::_parseArraySize() {
    // Grammar: ArraySize -> [ ArraySizeTail
    _match(TTYPE::OPEN_BRACKET_);
    
    // we parse the tail (size or empty)
    _parseArraySizeTail();
}

static void Parser::_parseArraySizeList() {
    // Grammar: ArraySizeList -> ArraySize ArraySizeList | EPSILON
    
    // Check FIRST set of ArraySize -> { [ }
    if (LTTYPE == TTYPE::OPEN_BRACKET_) {
        _parseArraySize();
        _parseArraySizeList(); // Recursive step
    }
    // EPSILON CASE:
    // If we don't see '[', we do nothing (return)
}

static void Parser::_parseIndice() {
    // Grammar: Indice -> [ Expr ]
    _match(TTYPE::OPEN_BRACKET_);
    _parseExpr();
    _match(TTYPE::CLOSE_BRACKET_);
}

static void Parser::_parseIndiceList(){
    _parseIndice();
    _parseIndiceList(); // Recursive step for multiple indices
}

static void Parser::_parseAParamsTail() {
    // Grammar: AParamsTail -> , Expr AParamsTail | EPSILON
    
    // Case 1: , Expr AParamsTail
    if (LTTYPE == TTYPE::COMMA_) {
        _match(TTYPE::COMMA_);
        _parseExpr();
        _parseAParamsTail(); // Recursive step for more parameters
    }
    // Case 2: EPSILON
    // If we don't see a comma, we do nothing (return)
}

static void Parser::_parseAParams() {
    // Grammar: AParams -> Expr AParamsTail
    _parseExpr();
    _parseAParamsTail();
}

static void Parser::_parseVariable(){
    _match(TTYPE::ID_);
    _parseFactorIdTail(); // Check for array access or function call
}

static void Parser::_parseFactorCallTail(){
    if(LTTYPE == TTYPE::DOT_){
        _match(TTYPE::DOT_);
        _match(TTYPE::ID_);
        _parseFactorIdTail(); // Check for more accesses or calls
    }
    // EPSILON case: do nothing (return)
}

static void Parser::_parseFactorRest(){
    switch (LTTYPE)
    {
    case TTYPE::DOT_:
        _match(TTYPE::DOT_);
        _match(TTYPE::ID_);
        _parseFactorIdTail(); // Check for more accesses or calls
        break;

    case TTYPE::OPEN_PAREN_:
        _match(TTYPE::OPEN_PAREN_);
        _parseAParams(); // Parse arguments
        _match(TTYPE::CLOSE_PAREN_);
        _parseFactorCallTail(); // Check for method calls on the result
        break;
    
    default:
        break;
    }
}

static void Parser::_parseFactorIdTail(){
    _parseIndiceList(); // Check for array access
    _parseFactorRest(); // Check for function call or member access
}

static void Parser::_parseFactor() {
    switch (LTTYPE)
    {
    case TTYPE::ID_:
        _match(TTYPE::ID_);
        _parseFactorIdTail();
        break;

    case TTYPE::INTEGER_LITERAL:
    case TTYPE::FLOAT_LITERAL:
    case TTYPE::STRING_LITERAL:
        _match(LTTYPE); // Match the literal
        break;

    case TTYPE::OPEN_PAREN_:
        _match(TTYPE::OPEN_PAREN_);
        _parseArithExpr();
        _match(TTYPE::CLOSE_PAREN_);
        break;

    // Need to check the first set of sign because we are in a switrch statement and need to check something
    case TTYPE::MINUS_:
    case TTYPE::PLUS_:
        _parseSign();
        _parseFactor();
        break;

    case TTYPE::NOT_:
        _match(TTYPE::NOT_);
        _parseFactor();
        break;
    
    default:
        _reportError("Expected factor (identifier, literal, '(', sign, or '!')", {TTYPE::ID_, TTYPE::INTEGER_LITERAL, TTYPE::FLOAT_LITERAL, TTYPE::STRING_LITERAL, TTYPE::OPEN_PAREN_, TTYPE::MINUS_, TTYPE::PLUS_, TTYPE::NOT_});
    }
}

//TODO CHECK FIRST SET INSTEAD OF CALLING _parseMultOp() WHICH THROWS AN ERROR IF IT DOESN'T MATCH
static void Parser::_parseMultOpTail(){
    if(LLTYPE == TTYPE::MULTIPLY_ || LTTYPE == TTYPE::DIVIDE_ || LTTYPE == TTYPE::AND_){ // First set of MultOp
        _parseFactor();
        _parseMultOpTail(); // Recursive step for multiple operations
    }
    // EPSILON case: do nothing (return)
}


static void Parser::_parseTerm(){
    _parseFactor();
    _parseMultOpTail(); // Check for multiplication/division
}

static void Parser::_parseAddOpTail(){
    if(LLTYPE == TTYPE::PLUS_ || LTTYPE == TTYPE::MINUS_ || LTTYPE == TTYPE::OR_){ // First set of AddOp
        _parseTerm();
        _parseAddOpTail(); // Recursive step for multiple operations
    }

    // EPSILON case: do nothing (return)
}

static void Parser::_parseArithExpr(){
    _parseTerm();
    _parseAddOpTail(); // Check for addition/subtraction
}

static void Parser::_parseRelExpr(){
    _parseArithExpr();
    _parseRelOp();
    _parseArithExpr();
}

static void Parser::_parseExprTail(){
    if(_parseRelOp()){
        _parseArithExpr();
    }
}

static void Parser::_parseExpr(){
    _parseArithExpr();
    _parseExprTail(); // Check for relational operator
}

static void Parser::_parseStatementCallTail(){
    if(LTTYPE == TTYPE::DOT_){
        _match(TTYPE::DOT_);
        _match(TTYPE::ID_);
        _parseStatementIdTail(); // Check for more accesses or calls
    }
    else if(LTTYPE == TTYPE::SEMICOLON_){
        _match(TTYPE::SEMICOLON_);
    }
    else{
        _reportError("Expected '.' for member access or ';' to end statement", {TTYPE::DOT_, TTYPE::SEMICOLON_});
    }
}

static void Parser::_parseStatementRest(){
    if(LTTYPE == TTYPE::DOT_){
        _match(TTYPE::DOT_);
        _match(TTYPE::ID_);
        _parseFactorIdTail(); // Check for more accesses or calls
    }
    else if(LTTYPE == TTYPE::OPEN_PAREN_){
        _match(TTYPE::OPEN_PAREN_);
        _parseAParams(); // Parse arguments
        _match(TTYPE::CLOSE_PAREN_);
        _parseStatementCallTail(); // Check for method calls on the result
    }

    else if(LTTYPE == TTYPE::ASSIGNMENT_){
        _parseAssignOp();
        _parseExpr();
        _match(TTYPE::SEMICOLON_);
    }

    else{
        _reportError("Expected '.', '(', or '=' for function call or member access", {TTYPE::DOT_, TTYPE::OPEN_PAREN_, TTYPE::ASSIGNMENT_});
    }
}

static void Parser::_parseStatementIdTail(){
    _parseIndiceList(); // Check for array access
    _parseStatementRest(); // Check for function call, member access, or assignment    
}

static void Parser::_parseStatBlock(){
    if(LTTYPE == TTYPE::OPEN_BRACE_) {
        _match(TTYPE::OPEN_BRACE_);
        _parseStatementList();
        _match(TTYPE::CLOSE_BRACE_);
    } 
    else if(_parseStatement()){
        return;
    }
    else {
        return; // EPSILON case: do nothing (return)
    }
}

static void Parser::_parseStatement() {
    switch(LTTYPE) {
        case TTYPE::IF_KEYWORD_:
            _match(TTYPE::IF_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            _parseRelExpr();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::THEN_KEYWORD_);
            _parseStatBlock();
            _match(TTYPE::ELSE_KEYWORD_);
            _parseStatBlock();
            _match(TTYPE::SEMICOLON_); 
            break;

        case TTYPE::WHILE_KEYWORD_:
            _match(TTYPE::WHILE_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            _parseRelExpr();
            _match(TTYPE::CLOSE_PAREN_);
            _parseStatBlock();
            _match(TTYPE::SEMICOLON_);
            break;

        case TTYPE::READ_KEYWORD_:
            _match(TTYPE::READ_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            _parseVariable();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::SEMICOLON_);
            break;

        case TTYPE::WRITE_KEYWORD_:
            _match(TTYPE::WRITE_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            _parseExpr();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::SEMICOLON_);
            break;

        case TTYPE::RETURN_KEYWORD_:
            _match(TTYPE::RETURN_KEYWORD_);
            _match(TTYPE::OPEN_PAREN_);
            _parseExpr();
            _match(TTYPE::CLOSE_PAREN_);
            _match(TTYPE::SEMICOLON_);
            break;

        case TTYPE::ID_:
            _match(TTYPE::ID_);
            _parseStatementIdTail(); // Check for function call, member access, or assignment
            break;

        default:
            _reportError("Expected statement (if, while, read, write, return, or identifier)", {TTYPE::IF_KEYWORD_, TTYPE::WHILE_KEYWORD_, TTYPE::READ_KEYWORD_, TTYPE::WRITE_KEYWORD_, TTYPE::RETURN_KEYWORD_, TTYPE::ID_});
    }
}
static void Parser::_parseStatementList() {
    // Grammar: StatementList -> Statement StatementList | EPSILON
    
    // Case 1: Statement StatementList
    if (_parseStatement()) {
        _parseStatementList(); // Recursive step for more statements
    }
    // Case 2: EPSILON
}
static void Parser::_parseVarDecl() {
    _parseType();
    _match(TTYPE::ID_);
    _parseArraySizeList(); // Check for array declaration
    _match(TTYPE::SEMICOLON_);
}

static void Parser::_parseVarDeclList() {
    if (_parseVarDecl()) {
        _parseVarDeclList(); // Recursive step for more variable declarations
    }
    // EPSILON case: do nothing (return)
}

static void Parser::_parseLocalVarDeclList(){
    if(LTTYPE == TTYPE::LOCAL_){
        _parseValDeclList(); // Recursive step for more variable declarations
    }
    // EPSILON case: do nothing (return)
}

static void Parser::_parseFuncBody() {
    _parseLocalVarDeclList(); // Parse local variable declarations
    _match(TTYPE::DO_KEYWORD_);
    _parseStatementList(); // Parse the statements in the function body
    _match(TTYPE::END_KEYWORD_);
}

static void Parser::_parseFParamsTail() {
    // Grammar: FParamsTail -> , Type id FParamsTail | EPSILON
    
    // Case 1: , Type id FParamsTail
    if (LTTYPE == TTYPE::COMMA_) {
        _match(TTYPE::COMMA_);
        _parseType();
        _match(TTYPE::ID_);
        _parseArraySizeList(); // Check for array parameter
        _parseFParamsTail(); // Recursive step for more parameters
    }
    // Case 2: EPSILON
    // If we don't see a comma, we do nothing (return)
}

// TODO CHECK FIRST SET INSTEAD OF CALLING _parseType() WHICH THROWS AN ERROR IF IT DOESN'T MATCH
static void Parser::_parseFParams() {
    if(_parseType()){
        _match(TTYPE::ID_);
        _parseArraySizeList(); // Check for array parameter
        _parseFParamsTail(); // Check for more parameters
    }
    // EPSILON case: do nothing (return)
}

// TODO CHECK FIRST SET INSTEAD OF CALLING _parseType() WHICH THROWS AN ERROR IF IT DOESN'T MATCH
static void Parser::_parseReturnType(){
    if(LTTYPE == TTYPE::VOID_TYPE_){
        _match(TTYPE::VOID_TYPE_);
    }
    else if(_parseType()){
        return;
    }
    else{
        _reportError("Expected return type (void, int, float, or identifier)", {TTYPE::VOID_TYPE_, TTYPE::INTEGER_TYPE_, TTYPE::FLOAT_TYPE_, TTYPE::ID_});
    }
}

static void Parser::_parseFuncHeadTail() {
    if(LTTYPE == TTYPE::COLON_COLON_){
        _match(TTYPE::COLON_COLON_);
        _match(TTYPE::ID_);
        _match(TTYPE::OPEN_PAREN_);
        _parseFParams(); // Parse parameters
        _match(TTYPE::CLOSE_PAREN_);
        _match(TTYPE::COLON_);
        _parseReturnType(); // Parse return type
    }
    else if(LTTYPE == TTYPE::OPEN_PAREN_){
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

static void Parser::_parseFuncHead() {
    _match(TTYPE::ID_); // Function name
    _parseFuncHeadTail(); // Check for parameters and return type
}

static void Parser::_parseFuncDef() {
    if(LTTYPE == TTYPE::ID_){ // First set of FuncHead
        _parseFuncHead(); // Parse function header
        _parseFuncBody(); // Parse function body
    }
    else{
        _reportError("Expected function definition (identifier)", {TTYPE::ID_});
    }
}

static void Parser::_parseMemberDeclIdTail(){
    if(LTTYPE == TTYPE::ID_){
        _parseArraySizeList(); // Check for array declaration
        _match(TTYPE::SEMICOLON_);
    }
}

static void Parser::_parseMemberDecl() {
    switch(LTTYPE){
        case TTYPE::ID_:
            _match(TTYPE::ID_);
            _parseMemberDeclIdTail(); // Check for array declaration
            break;

        case TTYPE::INTEGER_TYPE_:
        case TTYPE::FLOAT_TYPE_:
            _match(TTYPE::ID_);
            _parseArraySizeList(); // Check for array declaration
            _match(TTYPE::SEMICOLON_);
    }
}

static bool Parser::_parseVisibility(){
    switch(LTTYPE){
        case TTYPE::PUBLIC_:
            _match(TTYPE::PUBLIC_);
            return true;

        case TTYPE::PRIVATE_:
            _match(TTYPE::PRIVATE_);
            return false;

        default:
            _reportError("Expected visibility modifier (public, private, or protected)", {TTYPE::PUBLIC_, TTYPE::PRIVATE_});
    }
}

static void Parser::_parseClassMemberDecl() {
    _parseVisibility(); // Parse visibility modifier
    _parseMemberDecl(); // Parse the member declaration
}

static void Parser::_parseClassBody(){
    if(LTTYPE == TTYPE::PUBLIC_KEYWORD_ || LTTYPE == TTYPE::PRIVATE_KEYWORD_){ // First set of ClassMemberDecl AND visibility
        _parseClassMemberDecl(); // Parse class member declaration
        _parseClassBody(); // Recursive step for more members
    }

    // EPSILON case: do nothing (return)
}

static void Parser::_parseInheritsList(){
    if(LTTYPE == TTYPE::ID_){
        _match(TTYPE::ID_);
        _match(TTYPE::COMMA_);
        _parseInheritsList(); // Recursive step for more inherited classes
    }
    // Case 2: EPSILON
}

static void Parser::_parseInheritanceOpt(){
    if(LTTPE == TTYPE::INHERITS_KEYWORD_){
        _match(TTYPE::INHERITS_KEYWORD_);
        _match(TTYPE::ID_);
        _parseInheritsList(); // Parse the list of inherited classes
    }

    // Case 2: EPSILON
}


static void Parser::_parseClassDecl() {
    _match(TTYPE::CLASS_KEYWORD_);
    _match(TTYPE::ID_); // Class name
    _parseInheritanceOpt(); // Check for optional inheritance
    _match(TTYPE::OPEN_BRACE_);
    _parseClassBody(); // Parse class body
    _match(TTYPE::CLOSE_BRACE_);
    _match(TTYPE::SEMICOLON_);
}

static void Parser::_parseFuncDefList() {
    if(LTTYPE == TTYPE::ID_){ // First set of FuncDef
        _parseFuncDef(); // Parse function definition
        _parseFuncDefList(); // Recursive step for more function definitions
    }
    // Case 2: EPSILON
}

static void Parser::_parseClassDeclList() {
    if(LTTYPE == TTYPE::CLASS_KEYWORD_){ // First set of ClassDecl
        _parseClassDecl(); // Parse class declaration
        _parseClassDeclList(); // Recursive step for more class declarations
    }
    // Case 2: EPSILON
}

static void Parser::_parseProgram() {
    _parseClassDeclList(); // Parse class declarations
    _parseFuncDefList(); // Parse function definitions
    _match(TTYPE::MAIN_);
    _parseFuncBody(); // Parse main function body
}
