#include "../include/token.h"

// Static member definitions for block comment accumulation
std::string Token::_blockCommentAccum;
int Token::_blockCommentStartLine = 0;

Token::Type Token::getKeywordType(const std::string& lexeme) {
    // It is created only the first time this function is called, to avoid atartup crash
    static const std::map<std::string, Token::Type> keywords = {
            {"if",      Token::Type::IF_KEYWORD_},
            {"else",    Token::Type::ELSE_KEYWORD_},
            {"integer", Token::Type::INTEGER_TYPE_},
            {"float",   Token::Type::FLOAT_TYPE_},
            {"void",    Token::Type::VOID_TYPE_},
            {"class",   Token::Type::CLASS_KEYWORD_},
            {"while",   Token::Type::WHILE_KEYWORD_},
            {"read",    Token::Type::READ_KEYWORD_},
            {"write",   Token::Type::WRITE_KEYWORD_},
            {"return",  Token::Type::RETURN_KEYWORD_},
            {"main",    Token::Type::MAIN_},    
            {"then",    Token::Type::THEN_KEYWORD_},
            {"do",      Token::Type::DO_KEYWORD_},
            {"end",     Token::Type::END_KEYWORD_},
            {"public",  Token::Type::PUBLIC_KEYWORD_},
            {"private", Token::Type::PRIVATE_KEYWORD_},
            {"local",   Token::Type::LOCAL_},
            {"inherits",Token::Type::INHERITS_},
            {"and",     Token::Type::AND_},
            {"or",      Token::Type::OR_},
            {"not",     Token::Type::NOT_}
    };

    auto it = keywords.find(lexeme);
    if (it != keywords.end()) {
        return it->second;
    }
    return Token::Type::ID_; // It's not a keyword, so it must be an Identifier
}

char Token::getNextChar(const std::string& line, size_t& index) {
    if (index < line.length()) {
		return line[index++];
	}
    return '\0'; // Indicate end of current_char
}

void Token::skipToken(size_t& index, const std::string& line){
    while (index < line.length() && !isspace(line[index])) {
        index++;
    }
}

std::string Token::getErrorString(std::string error_step) const {
    std::string error_type = this -> getTypeString();
    std::string error_token = this -> getValue();
    std::string error_line = "Line "+std::to_string(this->getLineNumber());

    std::string error_message = error_step + ": " + error_type + ": " + error_token + ": " + error_line;

    return error_message;

}

// TODO make it backtrack when necessary by updating index
std::tuple<std::vector<Token>, std::vector<Token>> Token::tokenize(const std::string& line, int lineNumber, bool& inBlockComment) {
	size_t index = 0;
	std::vector<Token> all_tokens;
    std::vector<Token> invalid_tokens;

	while(index < line.length()) {
        if (inBlockComment) {
            size_t closingPos = line.find("*/", index);

            if (closingPos != std::string::npos) {
                // Found closing */ on this line — finalize the accumulated comment
                _blockCommentAccum += line.substr(index, closingPos + 2 - index);
                all_tokens.emplace_back(Type::BLOCK_COMMENT_, _blockCommentAccum, _blockCommentStartLine);
                _blockCommentAccum.clear();

                index = closingPos + 2;
                inBlockComment = false;
            } else {
                // Whole line is part of the comment — accumulate, don't create a token
                _blockCommentAccum += line.substr(index) + "\n";
                index = line.length();
            }
            continue;
        }

		char current_char = line[index];

		Token::Type type = Type::INVALID_;
        size_t start_index = index;
        std::string lexeme;
        

		if(std::isspace(current_char)) {
            index++;
			continue; // Skip whitespace
		}

		switch (current_char) {
			case '+':  type = Type::PLUS_; break;
			case '-': type = Type::MINUS_; break;
			case '*': type = Type::MULTIPLY_; break;
			case '{': type = Type::OPEN_BRACE_; break;
			case '}': type = Type::CLOSE_BRACE_; break;
			case '(': type = Type::OPEN_PAREN_; break;
			case ')': type = Type::CLOSE_PAREN_; break;
			case '[': type = Type::OPEN_BRACKET_; break;
			case ']': type = Type::CLOSE_BRACKET_; break;
			case ',': type = Type::COMMA_; break;
			case ';': type = Type::SEMICOLON_; break;
            case '.': type = Type::DOT_; break;

            //Not a single character Type
			default:
				//Check for Comments
                if (current_char == '/') {
                    type = isValidComment(current_char, index, line, inBlockComment);
                }

                //Checks for INTEGER or FLOAT Types
				else if(isdigit(current_char)) {
                    // handle float and invalid number
                    // If starts with number but has more characters attached -> INVALID_ID_

					type = isValidIntegerOrFloat(index, line);
				}

                //Check for ID type
				else if(_isAlphaNumeric(current_char)){
					type = isValidId(current_char, line, index);
				}

                //Multi character operator
                else{
					type = isValidCharOperator(line, index);
				}

                break;
        }

        if (index > start_index) {//calculate the length base on how far the index moved
            lexeme = line.substr(start_index, index - start_index);
            if (type == Type::UNTERMINATED_COMMENT_) {
                // Multi-line block comment starts here — accumulate, don't create token yet
                _blockCommentAccum = lexeme + "\n";
                _blockCommentStartLine = lineNumber;
            } else {
                all_tokens.emplace_back(type, lexeme, lineNumber);
            }
        }else{ //None of the above cases updated the index
            index++;
        }

        //save invalid tokens to the error file
        if (type == Token::Type::INVALID_ID_ ||
            type == Token::Type::INVALID_NUMBER_ ||
            type == Token::Type::INVALID_CHAR_ ||
            type == Token::Type::INVALID_){

            invalid_tokens.emplace_back(type, lexeme, lineNumber);
        }

	}
    std::tuple<std::vector<Token>, std::vector<Token>> output = {all_tokens, invalid_tokens};
	return output;

}

//TODO FIX 0... DONE
bool Token::_isValidIntegerLiteral(size_t& index, const std::string& line) {
    if (index >= line.length()) return false;

    // Number starts with 0 ---
    if (line[index] == '0') {
        index++; // Consume '0'
        
        // If it's a digit, we have a leading zero error (e.g., 012)
        if (index < line.length() && isdigit(line[index])) {
            skipToken(index, line); 
            return false;
        }
        
        // Note: We do NOT return immediately here We still need to check if the suffix is valid (e.g. 0a is invalid)
    } 
    // Number starts with 1-9 ---
    else {
        // Consume contiguous digits
        while (index < line.length() && isdigit(line[index])) {
            index++;
        }
    }

    // Number suffix check
    // We stopped reading digits. Now we check what stopped us.
    if (index < line.length()) {
        char next = line[index];
        
        // If the next char is a Letter or Underscore, it might be an error.
        // valid:  123 + 5  (next is space)
        // valid:  123;     (next is ;)
        // valid:  123.45   (next is .)
        // valid:  123e5    (next is e)
        // INVALID: 123a    (next is a)
        // INVALID: 123_    (next is _)
        
        if (isalpha(next) || next == '_') {
            // Exception: 'e' or 'E' are valid if we are parsing a float next
            if (next != 'e' && next != 'E') {
                skipToken(index, line);
                return false;
            }
        }
    }

    return true;
}

// float ::= integer fraction [e[+|−] integer]
// Logic: [e[+|−] integer] assuming fraction has been checked
// TODO FIX XXX.245232'0'
bool Token::_isValidFloatLiteral(size_t& index, const std::string& line) {
	if (index >= line.length() || (line[index] != 'e' && line[index] != 'E')) { // No exponent part -> still valid float
        return true; 
    }

	index++; // Consume 'e' or 'E'

	// Optional Sign (+ or -) since if we got here then there was an exponent part
    if (index < line.length() && (line[index] == '+' || line[index] == '-')) {
        index++; 
    }

	// Now we must have an integer
	if (index >= line.length() || !isdigit(line[index])) {
        skipToken(index, line);
        return false;
    }

    // Validate the Exponent's Integer value
    return _isValidIntegerLiteral(index, line);
	
}

bool Token::_isFractionPart(const std::string& line, size_t& index) {
    if (index >= line.length() || line[index] != '.') { // check for '.'
        return false;
    }

    size_t dotIndex = index;
    index++; // Consume '.'

    size_t start_digits_index = index;

    // do not stop at 0. consume everything to see the full number.
    while (index < line.length() && isdigit(line[index])) {
        index++;
    }

    size_t digit_count = index - start_digits_index;

    //Validation: Must have at least one digit
    if (digit_count == 0) {
        // Found "123." with no digits after.
        // This is an invalid fraction.
        skipToken(index, line);
        return false;
    }

    // TODO: "accept 0s first but not as their last digit"
    char last_digit = line[index - 1];

    if (last_digit == '0') {
        // If it ends in 0, it is ONLY valid if the length is exactly 1
        // .0   -> Count 1, Ends in 0 -> VALID
        // .50  -> Count 2, Ends in 0 -> INVALID (Trailing Zero)
        // .00  -> Count 2, Ends in 0 -> INVALID
        // .050 -> Count 3, Ends in 0 -> INVALID
        if (digit_count > 1) {
            skipToken(index, line);
            return false;
        }
    }
    return true;
//
//    // There must be at least one NONZERO digit after the decimal point
//    if (index < line.length() && isdigit(line[index]) && line[index] != '0') { //TODO FIX THIS digit* part to make it accept 0s first but not as ther last digit
//        // Consume digits after the decimal point
//        while (index < line.length() && isdigit(line[index]) && line[index] != '0') {
//            index++;
//        }
//        return true; // Valid fraction part
//    } else { // zero or no digit after decimal point
//
//        if (line[index++] == '0' && isspace(line[index])){ //check for exactly one zero: consume zero and check next is space
//            return true; // Valid fraction part with single zero
//        }
//
//        skipToken(index, line);
//        return false; // Invalid fraction part
//    }
}

//TODO Fix flot missing + or -, since now it considers it as wrong when they are missing
Token::Type Token::isValidIntegerOrFloat(size_t& index, const std::string& line) {
	bool is_integer = _isValidIntegerLiteral(index, line); 

	if (!is_integer) {
		return Type::INVALID_NUMBER_;
	} // If not integer, return invalid number

    bool hasFraction = false;

    if (index < line.length() && line[index] == '.') {

        // We attempt to parse the fraction.
        // If _isFractionPart returns false here, it means we had a dot
        // but the digits following it were invalid (e.g., "12." or "12.50").
        if (!_isFractionPart(line, index)) {
            return Type::INVALID_NUMBER_;
        }

        hasFraction = true;
    }

    if (index < line.length() && (line[index] == 'e' || line[index] == 'E')) {

        // We attempt to parse the exponent.
        if (!_isValidFloatLiteral(index, line)) {
            return Type::INVALID_NUMBER_; // Fail immediately if exponent is bad
        }

        // If we have an exponent, it is automatically a Float
        return Type::FLOAT_LITERAL_;
    }

    // 4. Final Decision
    if (hasFraction) {
        return Type::FLOAT_LITERAL_;
    }

    return Type::INTEGER_LITERAL_;

}

bool Token::_isAlphaNumeric(char c) {
    // isalnum checks [a-zA-Z0-9]
    // We add the check for underscore '_' manually
    return std::isalnum(c) || c == '_';
}

Token::Type Token::isValidKeywordOrId(const std::string& current_char) {
     return getKeywordType(current_char);
}
	
Token::Type Token::isValidId(char startChar, const std::string& line, size_t& index) {

	// An identifier must start with a letter
	if (std::isdigit(startChar) || startChar == '_') { //starts with an underscore (not possible with digit since handled before id)
        skipToken(index, line);

        return Type::INVALID_ID_;
	}

	std::string lexeme = _scanIdentifier(std::string(1, startChar), line, index);

	return isValidKeywordOrId(lexeme);
}

// helper to assess the rest of the identifier
std::string Token::_scanIdentifier(const std::string& startChar, const std::string& line, size_t& index) {
    std::string lexeme(1, startChar[0]); // Start with the first character

    // We must move past it so we don't read it twice.
    index++;

    while (index < line.length()) {
        char nextChar = line[index];

        if (_isAlphaNumeric(nextChar)) {
            lexeme += nextChar;
            index++;
        } else {
            break;
        }
    }

    return lexeme;
}

// Checks for uni and multi-character operators and returns the appropriate Type
Token::Type Token::isValidCharOperator(const std::string& line, size_t& index) {
    char current = line[index++];
	char next = line[index];

	if (current == '='){
		if (next == '='){
			index++; // consume next
			return Type::EQUAL_;
		}

		return Type::ASSIGNMENT_;
	}
	else if (current == '<'){
		if (next == '>'){
			index++; // consume next
			return Type::NOT_EQUAL_;
		}
		else if (next == '='){
			index++; // consume next
			return Type::LESS_EQUAL_;
		}

		return Type::LESS_THAN_;
	}
	else if (current == '>'){
		if (next == '='){
			index++; // consume next
			return Type::GREATER_EQUAL_;
		}

		return Type::GREATER_THAN_;
	}
	else if (current == ':'){
		if (next == ':'){
			index++; // consume next
			return Type::COLON_COLON_;
		}

		return Type::COLON_;
	}

	// If no operator uni or multi character matched, backtrack index
	return Type::INVALID_CHAR_;
}

Token::Type Token::isValidComment(char startChar, size_t& index, const std::string& line, bool& inBlockComment) {

    // 1. Check if it starts with '/'
    if (startChar != '/') {
        return Type::INVALID_;
    }

    index++;

    // 2. Check strict bounds before peeking
    if (index >= line.length()) {
        return Type::DIVIDE_; // It's just a single slash at EOF
    }

    char nextChar = line[index];

    // 3. Dispatch to specific comment handlers
    if (nextChar == '/') {
        return _isInlineComment(index, line);
    }
    else if (nextChar == '*') {
        return _isBlockComment(index, line, inBlockComment);
    }

    // 4. If it's not // or /*, it is just a Division operator
    return Type::DIVIDE_;
}

Token::Type Token::_isInlineComment(size_t& index, const std::string& line) {
    // We already know line[index] is '/'. Consume it.
    index++;
    // An inline comment consumes the rest of the line -> move the index to the end.
    index = line.length();

    return Type::INLINE_COMMENT_;
}

Token::Type Token::_isBlockComment(size_t& index, const std::string& line, bool& inBlockComment) {
    // We already know line[index] is '*'. Consume it.
    index++;

    inBlockComment = true;

    // Search for the closing "*/" sequence on this line
    while (index < line.length()) {

        if (line[index] == '*' && index + 1 < line.length() && line[index + 1] == '/') {
            index += 2; // Consume both '*' and '/'
            inBlockComment = false;
            break;
        }

        index++;
    }

    if(inBlockComment){
        // Reached end of line without finding closing */
        // The next line will continue to be part of this block comment
        index = line.length();
        return Type::UNTERMINATED_COMMENT_;
    }else{
        // Comment closed on this line; index already points past the '/'
        return Type::BLOCK_COMMENT_;
    }
}

void Token::flushPendingBlockComment(std::vector<Token>& valid, std::vector<Token>& invalid) {
    if (!_blockCommentAccum.empty()) {
        Token t(Type::UNTERMINATED_COMMENT_, _blockCommentAccum, _blockCommentStartLine);
        valid.push_back(t);
        invalid.push_back(t);
        _blockCommentAccum.clear();
    }
}

