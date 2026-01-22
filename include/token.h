#include <string>
#include <vector>
#include <map>
#include <cctype>

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

        inline const static std::map<std::string, Type> keywords = {
            {"if",       Type::IF_KEYWORD_},        
            {"do",       Type::DO_KEYWORD_},
            {"then",     Type::THEN_KEYWORD_},
            {"end",      Type::END_KEYWORD_},
            {"else",     Type::ELSE_KEYWORD_},
            {"while",    Type::WHILE_KEYWORD_},
            {"read",     Type::READ_KEYWORD_},
            {"write",    Type::WRITE_KEYWORD_},
            {"public",   Type::PUBLIC_KEYWORD_},
            {"private",  Type::PRIVATE_KEYWORD_},
            {"class",    Type::CLASS_KEYWORD_},
            {"return",   Type::RETURN_KEYWORD_},
            {"inherits", Type::INHERITS_},
            {"local",    Type::LOCAL_},
            {"integer",  Type::INTEGER_TYPE_},
            {"float",    Type::FLOAT_TYPE_},
            {"void",     Type::VOID_TYPE_},
            {"main",     Type::MAIN_},
            {"and",      Type::AND_},
            {"or",       Type::OR_},
            {"not",      Type::NOT_}
        };

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

        const int getLineNumber() const {
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
        static Token::Type isValidId(char startChar, const std::string& line, size_t& index);
        static Type isValidMultiCharOperator(const std::string& value);
        static Type isValidLiteral(const std::string& value);
        static std::string scanIdentifier(const std::string& startChar, const std::string& line, size_t& index);
        static bool isAlphaNumeric(char c);


    private:
        Type type_;
        std::string value_;
        int lineNumber_;

};