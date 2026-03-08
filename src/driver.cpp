#include <iostream>
#include <vector>
#include <string>
#include <iomanip> // Required for std::setw
#include <cstdlib>
#include <filesystem>
#include <chrono>

#include "../include/io.h"
#include "../include/token.h"
#include "../include/my_parser.h"

namespace fs = std::filesystem;

struct PhaseSummary {
    std::string name;
    bool success;
    long long durationMs;
    std::string details;
};

namespace ANSI {
    const std::string Reset = "\033[0m";
    const std::string Bold = "\033[1m";
    const std::string Cyan = "\033[36m";
    const std::string Green = "\033[32m";
    const std::string Yellow = "\033[33m";
    const std::string Red = "\033[31m";
    const std::string Gray = "\033[90m";
}

bool shouldUseColor() {
    return std::getenv("NO_COLOR") == nullptr;
}

std::string colorize(const std::string& text, const std::string& color) {
    if (!shouldUseColor()) {
        return text;
    }
    return color + text + ANSI::Reset;
}

std::string statusTag(bool success) {
    return success
        ? colorize("[OK]", ANSI::Green)
        : colorize("[FAIL]", ANSI::Red);
}

void printRule(char c = '=', int width = 90) {
    std::cout << colorize(std::string(width, c), ANSI::Gray) << "\n";
}

void printTitle(const std::string& title) {
    std::cout << "\n";
    printRule('=');
    std::cout << colorize(title, ANSI::Bold + ANSI::Cyan) << "\n";
    printRule('=');
}

void printSection(const std::string& title) {
    std::cout << "\n";
    printRule('-');
    std::cout << colorize(title, ANSI::Bold + ANSI::Cyan) << "\n";
    printRule('-');
}

void printKV(const std::string& key, const std::string& value) {
    std::cout << std::left << std::setw(14) << key << value << "\n";
}

void printArtifactList(const std::string& title, const std::vector<std::pair<std::string, std::string>>& artifacts) {
    std::cout << "\n" << colorize(title, ANSI::Bold + ANSI::Cyan) << "\n";
    for (const auto& artifact : artifacts) {
        std::cout << "  " << std::left << std::setw(14) << artifact.first << artifact.second << "\n";
    }
}

void printSummaryTable(const std::vector<PhaseSummary>& phases) {
    std::cout << "\n";
    printRule('=');
    std::cout << colorize("RUN SUMMARY", ANSI::Bold + ANSI::Cyan) << "\n";
    printRule('=');

    std::cout << std::left << std::setw(12) << "Phase"
              << std::setw(10) << "Status"
              << std::setw(10) << "Time"
              << "Details\n";
    printRule('-');

    for (const auto& phase : phases) {
        std::cout << std::left << std::setw(12) << phase.name
                  << std::setw(10) << statusTag(phase.success)
                  << std::setw(10) << (std::to_string(phase.durationMs) + "ms")
                  << phase.details << "\n";
    }

    printRule('=');
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
    std::vector<PhaseSummary> phases;

    // SETUP PHASE
    printTitle("COMP442 COMPILER DRIVER DASHBOARD");
    printKV("Source", sourceFile);

    // Create output directory: output/<baseName>/
    std::string baseName = getBaseName(sourceFile);
    std::string outputDir = createOutputDir(sourceFile);
    printKV("Output", outputDir);

    fs::path lexerDir = fs::path(outputDir) / "Lexer";
    fs::path parserDir = fs::path(outputDir) / "Parser";
    fs::path astDir = fs::path(outputDir) / "AST";
    fs::create_directories(lexerDir);
    fs::create_directories(parserDir);
    fs::create_directories(astDir);

    // Build output file paths
    std::string valid_output_file = buildOutputPath(lexerDir.string(), baseName, ".outlextokens");
    std::string invalid_output_file = buildOutputPath(lexerDir.string(), baseName, ".outlexerrors");
    std::string derivation_file = buildOutputPath(parserDir.string(), baseName, ".outderivation");
    std::string syntax_errors_file = buildOutputPath(parserDir.string(), baseName, ".outsyntaxerrors");
    std::string ast_file = buildOutputPath(astDir.string(), baseName, ".outast");
    std::string ast_dot_file = buildOutputPath(astDir.string(), baseName, ".outast.dot");
    std::string ast_png_file = buildOutputPath(astDir.string(), baseName, ".outast.png");

    std::vector<std::vector<Token>> valid_tokens;
    bool parseSuccess = false;
    bool dotAvailable = false;
    bool pngGenerated = false;

    // LEXER PHASE
    printSection("[1/3] LEXICAL ANALYSIS");
    try {
        auto start = std::chrono::steady_clock::now();
        valid_tokens = lex_file(sourceFile, valid_output_file, invalid_output_file);
        auto end = std::chrono::steady_clock::now();
        long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << statusTag(true) << " Tokenization completed\n";
        phases.push_back({"Lexer", true, durationMs, "Generated token and lexical error files"});

    } catch (const std::exception& e) {
        std::cerr << statusTag(false) << " Lexer crashed: " << e.what() << "\n";
        phases.push_back({"Lexer", false, 0, e.what()});
        printSummaryTable(phases);
        return 1;
    }

    // PARSER PHASE
    printSection("[2/3] SYNTACTIC ANALYSIS");
    try {
        auto start = std::chrono::steady_clock::now();
        parseSuccess = Parser::parseTokens(valid_tokens);

        writeSyntaxErrorsToFile(syntax_errors_file, Parser::getErrorMessages());
        writeDerivationToFile(derivation_file, Parser::getDerivationSteps());
        auto end = std::chrono::steady_clock::now();
        long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (parseSuccess) {
            std::cout << statusTag(true) << " Parsing completed (no syntax errors)\n";
            phases.push_back({"Parser", true, durationMs, "No syntax errors"});
        } else {
            std::cout << statusTag(false) << " Parsing completed with syntax errors\n";
            phases.push_back({"Parser", false, durationMs, "Syntax errors written to file"});
        }

    } catch (const std::exception& e) {
        std::cerr << statusTag(false) << " Parser crashed: " << e.what() << "\n";
        phases.push_back({"Parser", false, 0, e.what()});
        printSummaryTable(phases);
        return 1;
    }

    // AST PHASE
    printSection("[3/3] AST EXPORT");
    try {
        auto start = std::chrono::steady_clock::now();
        ASTPrinter::writeToFile(Parser::getASTRoot(), ast_file);
        ASTPrinter::writeDotToFile(Parser::getASTRoot(), ast_dot_file);

        dotAvailable = isGraphvizDotAvailable();
        if (dotAvailable) {
            pngGenerated = renderDotImage(ast_dot_file, "png", ast_png_file);
        }
        auto end = std::chrono::steady_clock::now();
        long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        bool astSuccess = !dotAvailable || (pngGenerated);
        std::string details = dotAvailable
            ? (astSuccess ? "AST/DOT/PNG generated" : "AST/DOT generated, image render incomplete")
            : "AST/DOT generated, Graphviz unavailable";
        phases.push_back({"AST", astSuccess, durationMs, details});

        if (astSuccess) {
            std::cout << statusTag(true) << " AST export completed\n";
        } else {
            std::cout << colorize("[WARN]", ANSI::Yellow) << " AST export completed with warnings\n";
        }

        if (!dotAvailable) {
            std::cout << colorize("[WARN]", ANSI::Yellow) << " Graphviz 'dot' not found. Skipped PNG generation.\n";
        } else if (!pngGenerated) {
            std::cout << colorize("[WARN]", ANSI::Yellow) << " Failed to generate AST image from DOT.\n";
        }

    } catch (const std::exception& e) {
        std::cerr << statusTag(false) << " AST export crashed: " << e.what() << "\n";
        phases.push_back({"AST", false, 0, e.what()});
        printSummaryTable(phases);
        return 1;
    }

    printSummaryTable(phases);

    printArtifactList("Lexer Outputs", {
        {"Tokens", valid_output_file},
        {"Lex Errors", invalid_output_file}
    });

    printArtifactList("Parser Outputs", {
        {"Derivation", derivation_file},
        {"Syntax Errors", syntax_errors_file}
    });

    std::vector<std::pair<std::string, std::string>> astArtifacts = {
        {"AST", ast_file},
        {"DOT", ast_dot_file}
    };
    if (dotAvailable && pngGenerated) {
        astArtifacts.push_back({"PNG", ast_png_file});
    }
    printArtifactList("AST Outputs", astArtifacts);

    std::cout << "\nDone.\n";

    return 0;
}