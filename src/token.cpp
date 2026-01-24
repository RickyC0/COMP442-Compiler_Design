#include "../include/token.h"



char Token::getNextChar(const std::string& line, size_t& index) {
    if (index < line.length()) {
		return line[index++];
	}
    return '\0'; // Indicate end of char_input
}

void Token::skipToken(size_t& index, std::string line){
    while (index < line.length() && !isspace(line[index])) {
        index++;
    }
}

// TODO make it backtrack when necessary by updating index
std::vector<Token> Token::tokenize(const std::string& line, int lineNumber) {
	size_t index = 0;
	char char_input;

	std::string rest_line = line;

	std::vector<Token> tokens;

	while((char_input = getNextChar(line, index)) != '\0') {
		size_t start_index = index - 1; // -1 because getNextChar already advanced index

		Token::Type type = Type::INVALID_;

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

            //Not a single character Type
			default:
                char current;
                char next;

                //Checks for INTEGER or FLOAT Types
				if(isdigit(char_input)) {
                    // TODO handle float and invalid number
                    // TODO If starts with number but has more characters attached -> INVALID_ID_
                    type = Type::INTEGER_LITERAL_;

                    while (index < line.length() && isdigit(current = line[index])) {
                        index++;
                    }

                    current = line[index-1];
                    next = line[index];


                    if (!isdigit(current) || !(isdigit(next) || isspace(next) || next == '\0')) { //contains a character that is not a number either in the last read one or the one after
                        skipToken(index, line);
                        type = Type::INVALID_NUMBER_;
                    }
                }

                //Check for ID type
				else if(isAlphaNumeric(char_input)){
					type = isValidId(char_input, line, index);
				}

                //TODO Multicharacter operator
                else if(false){}

				else{
					type = Type::INVALID_CHAR_;
				}
        }

        if (index > start_index + 1) {
            lexeme = line.substr(start_index, index - start_index);
        }

        tokens.emplace_back(type, lexeme, lineNumber);
	}

	return tokens;

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

	// An identifier must start with a letter
	if (std::isdigit(startChar) || startChar == '_') { //starts with an underscore (not possible with digit since handled before id)
        skipToken(index, line);

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

