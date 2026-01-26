#include <iostream>
#include <vector>
#include <string>

#include "../include/io.h"
#include "../include/token.h"

int main(int argc, char* argv[]) {
    // Check if the user provided a file argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <source_file>" << std::endl;
        return 1;
    }

    std::string sourceFile = argv[1];
    std::cout << "Lexical analysis started for file: " << sourceFile << std::endl;

    // Derive output filenames from the input filename
    std::string baseName = sourceFile;
    size_t lastDot = sourceFile.find_last_of('.');
    
    if (lastDot != std::string::npos) {
        baseName = sourceFile.substr(0, lastDot);
    }

    // Create the new filenames
    std::string valid_output_file = baseName + ".outlextokensflaci";
    std::string invalid_output_file = baseName + ".outlexerrors";

    try {
        lex_file(sourceFile, valid_output_file, invalid_output_file);

        std::cout << "Tokenization completed." << std::endl;
        std::cout << "Tokens written to: " << valid_output_file << std::endl;
        std::cout << "Errors written to: " << invalid_output_file << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}