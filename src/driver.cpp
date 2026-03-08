#include <iostream>
#include <vector>
#include <string>
#include <iomanip> // Required for std::setw
#include <cstdlib>

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

bool isGraphvizDotAvailable() {
#ifdef _WIN32
    return std::system("dot -V >NUL 2>&1") == 0;
#else
    return std::system("dot -V >/dev/null 2>&1") == 0;
#endif
}

bool renderDotImage(const std::string& inputDot, const std::string& format, const std::string& outputPath) {
    std::string cmd = "dot -T" + format + " \"" + inputDot + "\" -o \"" + outputPath + "\"";
    return std::system(cmd.c_str()) == 0;
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
    std::string ast_file = buildOutputPath(outputDir, baseName, ".outast");
    std::string ast_dot_file = buildOutputPath(outputDir, baseName, ".outast.dot");
    std::string ast_png_file = buildOutputPath(outputDir, baseName, ".outast.png");

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
        ASTPrinter::writeToFile(Parser::getASTRoot(), ast_file);
        ASTPrinter::writeDotToFile(Parser::getASTRoot(), ast_dot_file);

        bool dotAvailable = isGraphvizDotAvailable();
        bool pngGenerated = false;
        if (dotAvailable) {
            pngGenerated = renderDotImage(ast_dot_file, "png", ast_png_file);
        }

        if (success) {
            std::cout << "[SUCCESS] Parsing completed. No syntax errors found." << std::endl;
        } else {
            std::cout << "[FAIL] Parsing completed with syntax errors." << std::endl;
            printPath("   -> Syntax Errors", syntax_errors_file);
        }
        printPath("   -> Derivation", derivation_file);
        printPath("   -> AST", ast_file);
        printPath("   -> AST DOT", ast_dot_file);
        if (pngGenerated) {
            printPath("   -> AST PNG", ast_png_file);
        }
        if (!dotAvailable) {
            std::cout << "[WARN] Graphviz 'dot' not found. Skipped AST image generation." << std::endl;
        } else if (!pngGenerated) {
            std::cout << "[WARN] Failed to generate AST image from DOT." << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Parser crashed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n" << std::string(60, '-') << std::endl;
    std::cout << "Done." << std::endl;

    return 0;
}