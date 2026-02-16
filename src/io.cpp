#include "../include/io.h"
#include "../include/token.h"
#include <stdexcept>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================================
// Output path helpers
// ============================================================================

std::string getBaseName(const std::string& sourceFile) {
    return fs::path(sourceFile).stem().string();
}

std::string createOutputDir(const std::string& sourceFile) {
    std::string baseName = getBaseName(sourceFile);
    fs::path outputDir = fs::path("../output") / baseName;
    fs::create_directories(outputDir);
    return outputDir.string();
}

std::string buildOutputPath(const std::string& outputDir, const std::string& baseName, const std::string& extension) {
    return (fs::path(outputDir) / (baseName + extension)).string();
}


std::tuple<std::vector<std::vector<Token>>, std::vector<std::vector<Token>>> tokenizeFile(const std::string& filename) {
    std::tuple<std::vector<std::vector<Token>>, std::vector<std::vector<Token>>> valid_and_invalid_tokens;
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::string line;
    int lineNumber = 1;
    bool inBlockComment = false;

    while (std::getline(file, line)) {
        auto token_vectors = Token::tokenize(line, lineNumber, inBlockComment);
        std::vector<Token> valids = std::get<0>(token_vectors);
        std::vector<Token> invalids = std::get<1>(token_vectors);

        std::get<0>(valid_and_invalid_tokens).push_back(valids);
        std::get<1>(valid_and_invalid_tokens).push_back(invalids);

        lineNumber++;
    }

    file.close();

    return valid_and_invalid_tokens;
}

void writeTokensToFile(const std::string& filename, const std::vector<std::vector<Token>>& tokens) {
    std::ofstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + filename);
    }

    for (const auto& tokenLine : tokens) {
        if(tokenLine.empty()){ // to not leave blank lines in the files
            continue;
        }

        for (const auto& token : tokenLine) {
            file << token.toString();
        }

        file << "\n";
    }

    file.close();
}

std::vector<std::vector<Token>> lex_file(const std::string& input_file,  const std::string valid_out_file, const std::string invalid_out_file){
    auto valid_and_invalid_tokens = tokenizeFile(input_file);

    auto valids = std::get<0>(valid_and_invalid_tokens);
    auto invalids = std::get<1>(valid_and_invalid_tokens);

    writeTokensToFile(valid_out_file, valids);
    writeErrorsToFile(invalid_out_file, invalids);

    return valids;
}

void writeErrorsToFile(const std::string& filename, const std::vector<std::vector<Token>>& tokens){
    std::ofstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + filename);
    }

    for (const auto& tokenLine : tokens) {
        if(tokenLine.empty()){ // to not leave blank lines in the files
            continue;
        }
        for (const auto& token : tokenLine) {
            file << token.getErrorString("Lexical Error");
            file << "\n";
        }

    }

    file.close();
}

// ============================================================================
// Parser I/O
// ============================================================================

void writeSyntaxErrorsToFile(const std::string& filename, const std::vector<std::string>& errors) {
    std::ofstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + filename);
    }

    for (const auto& err : errors) {
        file << err << "\n";
    }

    file.close();
}

void writeDerivationToFile(const std::string& filename) {
    std::ofstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + filename);
    }

    // Placeholder for future derivation output
    file.close();
}
