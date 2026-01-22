#include "io.h"


std::vector<std::vector<Token>> tokenizeFile(const std::string& filename) {
    std::vector<std::vector<Token>> tokens;
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::string line;
    int lineNumber = 1;

    while (std::getline(file, line)) {
        Token token = Token::tokenize(line, lineNumber);
        tokens.push_back({token});
        lineNumber++;
    }

    file.close();

    return tokens;
}

void writeTokensToFile(const std::string& filename, const std::vector<std::vector<Token>>& tokens) {
    std::ofstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + filename);
    }

    for (const auto& tokenLine : tokens) {
        for (const auto& token : tokenLine) {
            file << "[ " << static_cast<int>(token.getType()) << "], [" << token.getValue() << "], [" << token.getLineNumber() << "]";
        }
        file << "\n";
    }

    file.close();
}