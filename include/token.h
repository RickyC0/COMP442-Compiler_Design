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
            UNTERMINATED_COMMENT_, // Unclosed Block comment
            TERMINATED_COMMENT_,
            INVALID_CHAR_, // Invalid char
            INVALID_ID_, // Invalid identifier
            INVALID_, // Generic invalid token

            END_OF_FILE_ // End of file
        };

        static Type getKeywordType(const std::string& lexeme);

        std::string getTypeString() const {
            switch (type_) {
                // --- Keywords ---
                case Type::IF_KEYWORD_:
                    return "IF";
                case Type::DO_KEYWORD_:
                    return "DO";
                case Type::THEN_KEYWORD_:
                    return "THEN";
                case Type::END_KEYWORD_:
                    return "END";
                case Type::ELSE_KEYWORD_:
                    return "ELSE";
                case Type::WHILE_KEYWORD_:
                    return "WHILE";
                case Type::READ_KEYWORD_:
                    return "READ";
                case Type::WRITE_KEYWORD_:
                    return "WRITE";
                case Type::PUBLIC_KEYWORD_:
                    return "PUBLIC";
                case Type::PRIVATE_KEYWORD_:
                    return "PRIVATE";
                case Type::CLASS_KEYWORD_:
                    return "CLASS";
                case Type::RETURN_KEYWORD_:
                    return "RETURN";
                case Type::INHERITS_:
                    return "INHERITS";
                case Type::LOCAL_:
                    return "LOCAL";
                case Type::MAIN_:
                    return "MAIN";

                    // --- Types ---
                case Type::INTEGER_TYPE_:
                    return "INTEGER";
                case Type::FLOAT_TYPE_:
                    return "FLOAT";
                case Type::VOID_TYPE_:
                    return "VOID";

                    // --- Logic Operators ---
                case Type::AND_:
                    return "AND";
                case Type::OR_:
                    return "OR";
                case Type::NOT_:
                    return "NOT";

                    // --- Categories ---
                case Type::ID_:
                    return "ID";
                case Type::INTEGER_LITERAL_:
                    return "INTEGER_LITERAL";
                case Type::FLOAT_LITERAL_:
                    return "FLOAT_LITERAL"; // If you implemented this

                    // --- Symbols ---
                case Type::PLUS_:
                    return "PLUS";
                case Type::MINUS_:
                    return "MINUS";
                case Type::MULTIPLY_:
                    return "MULTIPLY";
                case Type::DIVIDE_:
                    return "DIVIDE";
                case Type::ASSIGNMENT_:
                    return "ASSIGN";
                case Type::EQUAL_:
                    return "EQ";
                case Type::NOT_EQUAL_:
                    return "NEQ";
                case Type::LESS_THAN_:
                    return "LT";
                case Type::GREATER_THAN_:
                    return "GT";
                case Type::LESS_EQUAL_:
                    return "LEQ";
                case Type::GREATER_EQUAL_:
                    return "GEQ";

                case Type::OPEN_PAREN_:
                    return "OPEN_PAREN";
                case Type::CLOSE_PAREN_:
                    return "CLOSE_PAREN";
                case Type::OPEN_BRACE_:
                    return "OPEN_BRACE";
                case Type::CLOSE_BRACE_:
                    return "CLOSE_BRACE";
                case Type::OPEN_BRACKET_:
                    return "OPEN_BRACKET";
                case Type::CLOSE_BRACKET_:
                    return "CLOSE_BRACKET";

                case Type::SEMICOLON_:
                    return "SEMICOLON";
                case Type::COMMA_:
                    return "COMMA";
                case Type::DOT_:
                    return "DOT";
                case Type::COLON_:
                    return "COLON";
                case Type::COLON_COLON_:
                    return "COLON_COLON";

                    // --- Errors/EOF ---
                case Type::INVALID_ID_:
                    return "INVALID_ID";
                case Type::INVALID_NUMBER_:
                    return "INVALID_NUMBER";
                case Type::INVALID_CHAR_:
                    return "INVALID_CHAR";
                case Type::BLOCK_COMMENT_:
                    return "BLOCK_COMMENT";
                case Type::INLINE_COMMENT_:
                    return "INLINE_COMMENT";
                case Type::UNTERMINATED_COMMENT_:
                    return "UNTERMINATED_COMMENT";
                case Type::TERMINATED_COMMENT_:
                    return "TERMINATED_COMMENT_";
                case Type::END_OF_FILE_:
                    return "EOF";

                default:
                    return "UNKNOWN_TYPE";
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

        std::string toString() const {
            return ("[" + this->getTypeString() + ", " + this->getValue() + ", " +
                    std::to_string(this->getLineNumber()) + "] ");
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

        std::string getErrorString(std::string error_step) const;

        static char getNextChar(const std::string& input, size_t& index);

        static std::tuple<std::vector<Token>, std::vector<Token>> tokenize(const std::string& line, int lineNumber, bool& inBlockComment);

        static Type isValidKeywordOrId(const std::string& value);

        //id ::=letter alphanum*
        static Type isValidId(char startChar, const std::string& line, size_t& index);
        static Type isValidCharOperator(const std::string& line, size_t& index);
        static std::string _scanIdentifier(const std::string& startChar, const std::string& line, size_t& index);
        static bool _isAlphaNumeric(char c);

        static Type isValidIntegerOrFloat(size_t& index, const std::string& line);

        // integer ::= nonzero digit* | 0
        static bool _isValidIntegerLiteral(size_t& index, const std::string& line);

        static bool _isFractionPart(const std::string& line, size_t& index);

        // float ::= integer fraction [e[+|−] integer]
        static bool _isValidFloatLiteral(size_t& index, const std::string& line);

        static void skipToken(size_t &index, const std::string& line);

        static void backtrack(size_t &index, const std::string& line, const size_t& start_index);

        static Type _isBlockComment(size_t& index, const std::string& line, bool& inBlockComment);

        static Type _isInlineComment(size_t& index, const std::string& line);

        static Type isValidComment(char startChar, size_t& index, const std::string& line, bool& inBlockComment);

        



    private:
        Type type_;
        std::string value_;
        int lineNumber_;

};

#endif // TOKEN_H