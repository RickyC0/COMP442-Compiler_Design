#include <iostream>
#include <vector>
#include <string>
#include <iomanip> // Required for std::setw

#include "../include/io.h"
#include "../include/token.h"
#include "../include/my_parser.h"

// Helper for printing section headers
void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << std::endl;
}

// Helper for printing file paths nicely
void printPath(const std::string& label, const std::string& path) {
    std::cout << std::left << std::setw(20) << label << ": " << path << std::endl;
}

int main(int argc, char* argv[]) {
    // Check if the user provided a file argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <source_file>" << std::endl;
        return 1;
    }

    std::string sourceFile = argv[1];

    // SETUP PHASE
    printHeader("COMPILER DRIVER STARTING");
    std::cout << "[INFO] Processing file: " << sourceFile << std::endl;

    // Create output directory: output/<baseName>/
    std::string baseName = getBaseName(sourceFile);
    std::string outputDir = createOutputDir(sourceFile);
    std::cout << "[INFO] Output directory: " << outputDir << std::endl;

    // Build output file paths
    std::string valid_output_file = buildOutputPath(outputDir, baseName, ".outlextokens");
    std::string invalid_output_file = buildOutputPath(outputDir, baseName, ".outlexerrors");
    std::string derivation_file = buildOutputPath(outputDir, baseName, ".outderivation");
    std::string syntax_errors_file = buildOutputPath(outputDir, baseName, ".outsyntaxerrors");

    std::vector<std::vector<Token>> valid_tokens;

    // LEXER PHASE
    printHeader("PHASE 1: LEXICAL ANALYSIS");
    try {
        valid_tokens = lex_file(sourceFile, valid_output_file, invalid_output_file);

        std::cout << "[SUCCESS] Tokenization completed." << std::endl;
        printPath("   -> Tokens", valid_output_file);
        printPath("   -> Lex Errors", invalid_output_file);

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Lexer Error: " << e.what() << std::endl;
        return 1;
    }

    // PARSER PHASE
    printHeader("PHASE 2: SYNTACTIC ANALYSIS");
    try {
        bool success = Parser::parseTokens(valid_tokens);

        writeSyntaxErrorsToFile(syntax_errors_file, Parser::getErrorMessages());
        writeDerivationToFile(derivation_file, Parser::getDerivationSteps());

        if (success) {
            std::cout << "[SUCCESS] Parsing completed. No syntax errors found." << std::endl;
        } else {
            std::cout << "[FAIL] Parsing completed with syntax errors." << std::endl;
            printPath("   -> Syntax Errors", syntax_errors_file);
        }
        printPath("   -> Derivation", derivation_file);

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Parser crashed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n" << std::string(60, '-') << std::endl;
    std::cout << "Done." << std::endl;

    return 0;
}