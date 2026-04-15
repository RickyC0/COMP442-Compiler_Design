/**
 * @file token.h
 * @brief Lexer token model and lexical scanning helpers.
 *
 * @details
 * This header defines the token abstraction and the static scanning helpers used by
 * the lexical analysis phase. The scanner is intentionally expressed as a set of
 * small classification routines so grammar-level constraints remain explicit in code
 * and easy to evolve during assignment milestones.
 *
 * @par Why this structure?
 * A single static utility class keeps lexical policy centralized (keywords, numeric
 * constraints, comment behavior, and error classification) while avoiding hidden
 * global state, except for the explicit block-comment carry-over buffer.
 *
 * @par What comes next?
 * The parser consumes these token categories directly, so any token kind additions
 * or lexical rule changes here should be mirrored in parser predictive sets and
 * semantic assumptions.
 */
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <cctype>

#ifndef TOKEN_H
#define TOKEN_H

/**
 * @class Token
 * @brief Immutable lexical token value with scanner utilities.
 *
 * @details
 * Each token stores its categorical type, source lexeme, and source line. The same
 * class also hosts scanner routines because token construction and lexical decisions
 * are tightly coupled in this project architecture.
 *
 * @par Why keep scanner helpers here?
 * The design minimizes indirection for students reading generated diagnostics: token
 * construction, classification, and formatting live in one discoverable location.
 *
 * @par What comes next?
 * If lexer complexity grows (for example, DFA tables or unicode handling), these
 * helpers can be moved into a dedicated lexer class while preserving the Token API.
 */
class Token {
    public:
        /**
         * @enum Type
         * @brief Canonical token categories produced by lexical analysis.
         *
         * @details
         * The parser depends on these categories for production dispatch and error
         * recovery. Error-prefixed kinds are intentionally first-class tokens so the
         * pipeline can continue and report multiple issues per file.
         */
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

        /**
         * @brief Resolve a lexeme as either a reserved keyword or a generic identifier.
         * @param lexeme Candidate identifier text.
         * @return Specific keyword token type, or ID_ if not reserved.
         *
         * @par Why?
         * Keyword resolution after identifier scanning keeps the scanner simple and
         * avoids duplicating character rules for each reserved word.
         *
         * @par What comes next?
         * Any language keyword additions should be reflected in this lookup and in the
         * parser grammar terminals.
         */
        static Type getKeywordType(const std::string& lexeme);

        /**
         * @brief Convert token type to stable printable label.
         * @return Human-readable token type name used by logs and output artifacts.
         */
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
                    return "INTEGER_TYPE";
                case Type::FLOAT_TYPE_:
                    return "FLOAT_TYPE";
                case Type::VOID_TYPE_:
                    return "VOID_TYPE";

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

        /**
         * @brief Construct a token from classification and source metadata.
         * @param type Token category.
         * @param value Lexeme extracted from source.
         * @param lineNumber 1-based source line where token starts.
         */
        Token(Type type, const std::string& value, int lineNumber) {
                type_ = type;
                value_ = value;
                lineNumber_ = lineNumber;
        }

        /**
         * @brief Access token category.
         * @return Token::Type classification.
         */
        Type getType() const {
            return type_;
        }

        /**
         * @brief Access token lexeme.
         * @return Source substring captured for this token.
         */
        const std::string& getValue() const {
            return value_;
        }

        /**
         * @brief Access source line for diagnostics.
         * @return 1-based line number where token starts.
         */
        int getLineNumber() const {
            return lineNumber_;
        }

        /**
         * @brief Copy constructor.
         * @param other Token to copy.
         */
        Token(const Token& other) {
            type_ = other.type_;
            value_ = other.value_;
            lineNumber_ = other.lineNumber_;
        }

        /**
         * @brief Format token as output artifact record.
         * @return String representation used in generated lexer output files.
         */
        std::string toString() const {
            return ("[" + this->getTypeString() + ", " + this->getValue() + ", " +
                    std::to_string(this->getLineNumber()) + "] ");
        }

        /**
         * @brief Copy assignment operator.
         * @param other Token to copy.
         * @return Reference to this token.
         */
        Token& operator=(const Token& other) {
            if (this != &other) {
                type_ = other.type_;
                value_ = other.value_;
                lineNumber_ = other.lineNumber_;
            }
            return *this;
        }

        /**
         * @brief Build formatted lexical error message for this token.
         * @param error_step Pipeline label prefix (for example, "LEXER").
         * @return Structured error string for diagnostics output.
         */
        std::string getErrorString(std::string error_step) const;

        /**
         * @brief Return current character and advance scanner index.
         * @param input Source line.
         * @param index Current scanner position, advanced when in range.
         * @return Character at current position or '\\0' at end-of-line.
         */
        static char getNextChar(const std::string& input, size_t& index);

        /**
         * @brief Tokenize one source line.
         * @param line Source text for current line.
         * @param lineNumber 1-based source line number.
         * @param inBlockComment Cross-line state flag for block comments.
         * @return Tuple(validOrAllTokens, invalidTokens).
         *
         * @details
         * Valid stream includes comment tokens and normal tokens. Invalid stream
         * contains only lexical-error tokens used for error output artifacts.
         *
         * @par Why line-by-line?
         * The project pipeline emits per-line diagnostics and keeps parser-facing
         * handling simple while still supporting multi-line block comments via state.
         *
         * @par What comes next?
         * Driver code should call flushPendingBlockComment() at end-of-file to emit
         * unterminated block-comment diagnostics.
         */
        static std::tuple<std::vector<Token>, std::vector<Token>> tokenize(const std::string& line, int lineNumber, bool& inBlockComment);

        /**
         * @brief Resolve scanned identifier lexeme to keyword-or-id category.
         * @param value Identifier lexeme.
         * @return Specific keyword type or ID_.
         */
        static Type isValidKeywordOrId(const std::string& value);

        /**
         * @brief Validate and classify identifier token starting at current index.
         * @param startChar First character already observed.
         * @param line Source line.
         * @param index Current scanner position, advanced through identifier.
         * @return ID_ / keyword type or INVALID_ID_.
         *
         * @note Language rule: id ::= letter alphanum*.
         */
        static Type isValidId(char startChar, const std::string& line, size_t& index);

        /**
         * @brief Classify single- and multi-character operators.
         * @param line Source line.
         * @param index Current scanner position, advanced according to operator width.
         * @return Operator token type or INVALID_CHAR_.
         */
        static Type isValidCharOperator(const std::string& line, size_t& index);

        /**
         * @brief Scan full identifier body after first character.
         * @param startChar First identifier character as string.
         * @param line Source line.
         * @param index Current scanner index (advanced during scan).
         * @return Complete identifier lexeme.
         */
        static std::string _scanIdentifier(const std::string& startChar, const std::string& line, size_t& index);

        /**
         * @brief Check if character is allowed in identifier body.
         * @param c Character to test.
         * @return True if alphanumeric or underscore.
         */
        static bool _isAlphaNumeric(char c);

        /**
         * @brief Validate numeric literal starting at current index.
         * @param index Current scanner position, advanced across literal.
         * @param line Source line.
         * @return INTEGER_LITERAL_, FLOAT_LITERAL_, or INVALID_NUMBER_.
         */
        static Type isValidIntegerOrFloat(size_t& index, const std::string& line);

        /**
         * @brief Validate integer portion of numeric literal.
         * @param index Current scanner position, advanced through integer part.
         * @param line Source line.
         * @return True if integer form is valid.
         *
         * @note integer ::= nonzero digit* | 0.
         */
        static bool _isValidIntegerLiteral(size_t& index, const std::string& line);

        /**
         * @brief Validate fraction part beginning with '.'
         * @param line Source line.
         * @param index Current scanner position at '.'.
         * @return True if accepted by language float policy.
         */
        static bool _isFractionPart(const std::string& line, size_t& index);

        /**
         * @brief Validate optional exponent part for float literal.
         * @param index Scanner index currently at optional 'e'.
         * @param line Source line.
         * @return True if exponent form is valid.
         *
         * @note float ::= integer fraction [e[+|−] integer]
         */
        static bool _isValidFloatLiteral(size_t& index, const std::string& line);

        /**
         * @brief Advance to token boundary when recovering from lexical error.
         * @param index Scanner index updated in-place.
         * @param line Source line.
         */
        static void skipToken(size_t &index, const std::string& line);

        /**
         * @brief Restore scanner index to known safe position.
         * @param index Scanner index updated in-place.
         * @param line Source line.
         * @param start_index Position to restore.
         *
         * @details
         * Reserved for future recovery workflows; currently declared for interface
         * completeness in case predictive lexical recovery requires rollback.
         */
        static void backtrack(size_t &index, const std::string& line, const size_t& start_index);

        /**
         * @brief Continue or terminate block-comment parsing.
         * @param index Scanner index.
         * @param line Source line.
         * @param inBlockComment Cross-line block-comment state.
         * @return BLOCK_COMMENT_ if closed, UNTERMINATED_COMMENT_ otherwise.
         */
        static Type _isBlockComment(size_t& index, const std::string& line, bool& inBlockComment);

        /**
         * @brief Parse inline comment beginning with "//".
         * @param index Scanner index.
         * @param line Source line.
         * @return INLINE_COMMENT_.
         */
        static Type _isInlineComment(size_t& index, const std::string& line);

        /**
         * @brief Classify '/' as divide, inline comment, or block comment start.
         * @param startChar Current character expected to be '/'.
         * @param index Scanner index.
         * @param line Source line.
         * @param inBlockComment Cross-line block-comment state.
         * @return DIVIDE_, INLINE_COMMENT_, BLOCK_COMMENT_, or UNTERMINATED_COMMENT_.
         */
        static Type isValidComment(char startChar, size_t& index, const std::string& line, bool& inBlockComment);

        /**
         * @brief Accumulated text for currently open multi-line block comment.
         *
         * @details
         * This is explicit shared state across tokenize() calls to preserve full
         * comment lexeme when the opening '/' + '*' and the closing '*' + '/' span different lines.
         */
        static std::string _blockCommentAccum;

        /**
         * @brief Line number where current multi-line block comment started.
         */
        static int _blockCommentStartLine;

        /**
         * @brief Emit pending unterminated block comment as both valid and invalid token.
         * @param valid Output stream containing all produced tokens.
         * @param invalid Output stream containing lexical error tokens.
         *
         * @par Why?
         * Unterminated comments are lexical errors, but preserving the raw lexeme in
         * both streams improves observability for debugging and grading.
         *
         * @par What comes next?
         * Call this once after the last source line is tokenized.
         */
        static void flushPendingBlockComment(std::vector<Token>& valid, std::vector<Token>& invalid);

        



    private:
        /** @brief Token category. */
        Type type_;
        /** @brief Original lexeme text. */
        std::string value_;
        /** @brief 1-based source line number. */
        int lineNumber_;

};

#endif // TOKEN_H