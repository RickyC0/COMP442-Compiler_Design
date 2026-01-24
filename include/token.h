#include <string>
#include <vector>
#include <map>
#include <cctype>

#ifndef TOKEN_H
#define TOKEN_H

class Token {
    public:
        enum class Type {
            ID_, // Identifier
            INTEGER_LITERAL_, // Integer literal
            FLOAT_LITERAL_, // Float literal
            INVALID_NUMBER_, // Invalid number literal
            EQUAL_, // Equal operator ==
            KEYWORD_, // Keyword
            PLUS_, // Plus operator +
            MINUS_, // Minus operator -
            MULTIPLY_, // Multiply operator *
            DIVIDE_, // Divide operator /
            OPEN_PAREN_, // Open parenthesis (
            CLOSE_PAREN_, // Close parenthesis )
            OPEN_BRACE_, // Open brace {
            CLOSE_BRACE_, // Close brace }
            OPEN_BRACKET_, // Open bracket [
            CLOSE_BRACKET_, // Close bracket ]
            COMMA_, // Comma ,
            DOT_, // Dot .
            COLON_, // Colon :
            COLON_COLON_, // Double colon operator ::
            SEMICOLON_, // Semicolon ;
            NOT_EQUAL_, // Not equal operator <>
            LESS_THAN_, // Less than operator <
            GREATER_THAN_, // Greater than operator >
            LESS_EQUAL_, // Less than or equal operator <=
            GREATER_EQUAL_, // Greater than or equal operator >=
            ASSIGNMENT_, // Assignment operator =
            IF_KEYWORD_, // if keyword
            DO_KEYWORD_, // do keyword
            THEN_KEYWORD_, // then keyword
            END_KEYWORD_, // end keyword
            ELSE_KEYWORD_, // else keyword
            WHILE_KEYWORD_, // while keyword
            READ_KEYWORD_, // read keyword
            WRITE_KEYWORD_, // write keyword
            PUBLIC_KEYWORD_, // public keyword
            PRIVATE_KEYWORD_, // private keyword
            CLASS_KEYWORD_, // class keyword
            RETURN_KEYWORD_, // return keyword
            INHERITS_, // inherits keyword
            LOCAL_, // local keyword
            INTEGER_TYPE_, // integer type
            FLOAT_TYPE_, // float type
            AND_, // and operator
            OR_, // or operator
            NOT_, // not operator
            VOID_TYPE_, // void type
            MAIN_, // main function

            INLINE_COMMENT_, // Inline comment
            BLOCK_COMMENT_, // Block comment
            INVALID_CHAR_, // Invalid char
            INVALID_ID_, // Invalid identifier
            INVALID_, // Generic invalid token

            END_OF_FILE_ // End of file
        };

        inline const static std::map<std::string, Token::Type> keywords = {
            {"if",       Token::Type::IF_KEYWORD_},
            {"do",       Token::Type::DO_KEYWORD_},
            {"then",     Token::Type::THEN_KEYWORD_},
            {"end",      Token::Type::END_KEYWORD_},
            {"else",     Token::Type::ELSE_KEYWORD_},
            {"while",    Token::Type::WHILE_KEYWORD_},
            {"read",     Token::Type::READ_KEYWORD_},
            {"write",    Token::Type::WRITE_KEYWORD_},
            {"public",   Token::Type::PUBLIC_KEYWORD_},
            {"private",  Token::Type::PRIVATE_KEYWORD_},
            {"class",    Token::Type::CLASS_KEYWORD_},
            {"return",   Token::Type::RETURN_KEYWORD_},
            {"inherits", Token::Type::INHERITS_},
            {"local",    Token::Type::LOCAL_},
            {"integer",  Token::Type::INTEGER_TYPE_},
            {"float",    Token::Type::FLOAT_TYPE_},
            {"void",     Token::Type::VOID_TYPE_},
            {"main",     Token::Type::MAIN_},
            {"and",      Token::Type::AND_},
            {"or",       Token::Type::OR_},
            {"not",      Token::Type::NOT_}
        };

        std::string getTypeString() const {
            switch (type_) {
                // --- Keywords ---
                case Type::IF_KEYWORD_:      return "IF";
                case Type::DO_KEYWORD_:      return "DO";
                case Type::THEN_KEYWORD_:    return "THEN";
                case Type::END_KEYWORD_:     return "END";
                case Type::ELSE_KEYWORD_:    return "ELSE";
                case Type::WHILE_KEYWORD_:   return "WHILE";
                case Type::READ_KEYWORD_:    return "READ";
                case Type::WRITE_KEYWORD_:   return "WRITE";
                case Type::PUBLIC_KEYWORD_:  return "PUBLIC";
                case Type::PRIVATE_KEYWORD_: return "PRIVATE";
                case Type::CLASS_KEYWORD_:   return "CLASS";
                case Type::RETURN_KEYWORD_:  return "RETURN";
                case Type::INHERITS_:        return "INHERITS";
                case Type::LOCAL_:           return "LOCAL";
                case Type::MAIN_:            return "MAIN";

                    // --- Types ---
                case Type::INTEGER_TYPE_:    return "INTEGER";
                case Type::FLOAT_TYPE_:      return "FLOAT";
                case Type::VOID_TYPE_:       return "VOID";

                    // --- Logic Operators ---
                case Type::AND_:             return "AND";
                case Type::OR_:              return "OR";
                case Type::NOT_:             return "NOT";

                    // --- Categories ---
                case Type::ID_:              return "ID";
                case Type::INTEGER_LITERAL_: return "INTEGER_LITERAL";
                case Type::FLOAT_LITERAL_:   return "FLOAT_LITERAL"; // If you implemented this

                    // --- Symbols ---
                case Type::PLUS_:            return "PLUS";
                case Type::MINUS_:           return "MINUS";
                case Type::MULTIPLY_:        return "MULTIPLY";
                case Type::DIVIDE_:          return "DIVIDE";
                case Type::ASSIGNMENT_:      return "ASSIGN";
                case Type::EQUAL_:           return "EQ";
                case Type::NOT_EQUAL_:       return "NEQ";
                case Type::LESS_THAN_:       return "LT";
                case Type::GREATER_THAN_:    return "GT";
                case Type::LESS_EQUAL_:      return "LEQ";
                case Type::GREATER_EQUAL_:   return "GEQ";

                case Type::OPEN_PAREN_:      return "OPEN_PAREN";
                case Type::CLOSE_PAREN_:     return "CLOSE_PAREN";
                case Type::OPEN_BRACE_:      return "OPEN_BRACE";
                case Type::CLOSE_BRACE_:     return "CLOSE_BRACE";
                case Type::OPEN_BRACKET_:    return "OPEN_BRACKET";
                case Type::CLOSE_BRACKET_:   return "CLOSE_BRACKET";

                case Type::SEMICOLON_:       return "SEMICOLON";
                case Type::COMMA_:           return "COMMA";
                case Type::DOT_:             return "DOT";
                case Type::COLON_:           return "COLON";
                case Type::COLON_COLON_:     return "COLON_COLON";

                    // --- Errors/EOF ---
                case Type::INVALID_ID_:      return "INVALID_ID";
                case Type::INVALID_NUMBER_:  return "INVALID_NUMBER";
                case Type::INVALID_CHAR_:    return "INVALID_CHAR";
                case Type::END_OF_FILE_:     return "EOF";

                default:                     return "UNKNOWN_TYPE";
            }
        }

        Token(Type type, const std::string& value, int lineNumber) {
                type_ = type;
                value_ = value;
                lineNumber_ = lineNumber;
        }

        Type getType() const {
            return type_;
        }

        const std::string& getValue() const {
            return value_;
        }

        int getLineNumber() const {
            return lineNumber_;
        }

        // Copy constructor
        Token(const Token& other) {
            type_ = other.type_;
            value_ = other.value_;
            lineNumber_ = other.lineNumber_;
        }

        // Copy assignment operator
        Token& operator=(const Token& other) {
            if (this != &other) {
                type_ = other.type_;
                value_ = other.value_;
                lineNumber_ = other.lineNumber_;
            }
            return *this;
        }

        static char getNextChar(const std::string& input, size_t& index);

        static std::vector<Token> tokenize(const std::string& line, int lineNumber);

        static Type isValidKeywordOrId(const std::string& value);
        static Type isValidId(char startChar, const std::string& line, size_t& index);
        static Type isValidMultiCharOperator(const std::string& value);
        static Type isValidLiteral(const std::string& value);
        static std::string scanIdentifier(const std::string& startChar, const std::string& line, size_t& index);
        static bool isAlphaNumeric(char c);

        static Type isValidIntegerLiteral(const std::string& value);
        static Type isValidFloatLiteral(const std::string& value);

        static void skipToken(size_t &index, std::string line);


    private:
        Type type_;
        std::string value_;
        int lineNumber_;

};

#endif // TOKEN_H