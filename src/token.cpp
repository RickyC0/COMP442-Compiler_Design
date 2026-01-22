#include "token.h"

char Token::getNextChar(const std::string& line, size_t& index) {
    if (index < line.length()) {
		return line[index++];
	}
    return '\0'; // Indicate end of char_input
}

// TODO make it backtrack when necessary by updating index
std::vector<Token> Token::tokenize(const std::string& line, int lineNumber) {
	size_t index = 0;
	char char_input;

	std::string rest_line = line;

	std::vector<Token> tokens;

	while(char_input = getNextChar(line, index), char_input != '\0') {
		rest_line = line.substr(index - 1); // -1 because getNextChar already advanced index
		Token::Type type;
		if(std::isspace(char_input)) {
			continue; // Skip whitespace
		}

		std::string lexeme(1, char_input);
		switch (char_input) {
			case '+':  type = Type::PLUS_; break;
			case '-': type = Type::MINUS_; break;
			case '*': type = Type::MULTIPLY_; break;
			case '/': type = Type::DIVIDE_; break;
			case '<': type = Type::LESS_THAN_; break;
			case '>': type = Type::GREATER_THAN_; break;
			case '=': type = Type::ASSIGNMENT_; break;
			case '{': type = Type::OPEN_BRACE_; break;
			case '}': type = Type::CLOSE_BRACE_; break;
			case '(': type = Type::OPEN_PAREN_; break;
			case ')': type = Type::CLOSE_PAREN_; break;
			case '[': type = Type::OPEN_BRACKET_; break;
			case ']': type = Type::CLOSE_BRACKET_; break;
			case ',': type = Type::COMMA_; break;
			case ';': type = Type::SEMICOLON_; break;
			case '.' : type = Type::DOT_; break;
			case ':' : type = Type::COLON_; break;

			
			default:
				if(isAlphaNumeric(char_input)){
					type = isValidId(char_input, line, index);
				}
				else if{
					//TODO handle multi-char operators
					//TODO integer and float literals

				}

				else{
					tokens.push_back(Token(Type::INVALID_NUMBER_, lexeme));
				}
		}

		tokens.push_back(Token(type, lexeme, lineNumber));
	}

}

bool Token::isAlphaNumeric(char c) {
    // isalnum checks [a-zA-Z0-9]
    // We add the check for underscore '_' manually
    return std::isalnum(c) || c == '_';
}

Token::Type Token::isValidKeywordOrId(const std::string& char_input) {
    auto keywordIt = keywords.find(char_input);

    if (keywordIt != keywords.end()) {
        return keywordIt->second; // Return the corresponding keyword type
    }

    return Type::ID_;
}
	
Token::Type Token::isValidId(char startChar, const std::string& line, size_t& index) {
	// An identifier must start with a letter or underscore
	if (!std::isalpha(startChar) && startChar != '_') {
		return Type::INVALID_ID_;
	}

	std::string lexeme = scanIdentifier(std::string(1, startChar), line, index);

	return isValidKeywordOrId(lexeme);
}

std::string Token::scanIdentifier(const std::string& startChar, const std::string& line, size_t& index) {

	std::string lexeme(1, startChar[0]); // Start with the first character

    while (index < line.length()) {
        char nextChar = line[index];

        if (isAlphaNumeric(nextChar)) {
            lexeme += nextChar;
            index++; // advance the SHARED index
        } else {
            // We hit a space, +, -, or symbol TODO Assuming that there will be spaces after the identifier i.e. not handling cases like "var1+var2"
            break; 
        }
    }

    return lexeme;
}

Token::Type Token::isValidMultiCharOperator(const std::string& char_input) {
    if (char_input == "==") {
        return Type::EQUAL_;
    } else if (char_input == "<>") {
        return Type::NOT_EQUAL_;
    } else if (char_input == "<=") {
        return Type::LESS_EQUAL_;
    } else if (char_input == ">=") {
        return Type::GREATER_EQUAL_;
    } else if (char_input == "::") {
        return Type::COLON_COLON_;
    }
    return Type::INVALID_;
}

