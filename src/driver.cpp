#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../include/io.h"
#include "../include/token.h"
#include "../include/my_parser.h"
#include "../include/semantic.h"
#include "../include/ui.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    // Check if the user provided a file argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <source_file>" << std::endl;
        return 1;
    }

    std::string sourceFile = argv[1];
    std::vector<PhaseSummary> phases;

    // SETUP PHASE
    UI::printTitle("COMP442 COMPILER DRIVER DASHBOARD");
    UI::printKV("Source", sourceFile);

    // Create output directory: output/<baseName>/
    std::string baseName = getBaseName(sourceFile);
    std::string outputDir = createOutputDir(sourceFile);
    UI::printKV("Output", outputDir);

    fs::path lexerDir = fs::path(outputDir) / "Lexer";
    fs::path parserDir = fs::path(outputDir) / "Parser";
    fs::path astDir = fs::path(outputDir) / "AST";
    fs::path semanticDir = fs::path(outputDir) / "Semantics";
    fs::create_directories(lexerDir);
    fs::create_directories(parserDir);
    fs::create_directories(astDir);
    fs::create_directories(semanticDir);

    // Build output file paths
    std::string valid_output_file = buildOutputPath(lexerDir.string(), baseName, ".outlextokens");
    std::string invalid_output_file = buildOutputPath(lexerDir.string(), baseName, ".outlexerrors");
    std::string derivation_file = buildOutputPath(parserDir.string(), baseName, ".outderivation");
    std::string syntax_errors_file = buildOutputPath(parserDir.string(), baseName, ".outsyntaxerrors");
    std::string ast_file = buildOutputPath(astDir.string(), baseName, ".outast");
    std::string ast_dot_file = buildOutputPath(astDir.string(), baseName, ".outast.dot");
    std::string ast_png_file = buildOutputPath(astDir.string(), baseName, ".outast.png");
    std::string symbol_tables_file = buildOutputPath(semanticDir.string(), baseName, ".outsymboltables");
    std::string semantic_errors_file = buildOutputPath(semanticDir.string(), baseName, ".outsemanticerrors");

    std::vector<std::vector<Token>> valid_tokens;
    bool parseSuccess = false;
    bool dotAvailable = false;
    bool pngGenerated = false;

    auto writeLinesToFile = [](const std::string& path, const std::vector<std::string>& lines) {
        std::ofstream out(path, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }

        for (const auto& line : lines) {
            out << line << "\n";
        }
        return true;
    };

    // LEXER PHASE
    UI::printSection("[1/4] LEXICAL ANALYSIS");
    try {
        auto start = std::chrono::steady_clock::now();
        valid_tokens = lex_file(sourceFile, valid_output_file, invalid_output_file);
        auto end = std::chrono::steady_clock::now();
        long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        UI::printStatusLine(true, "Tokenization completed");
        phases.push_back({"Lexer", true, durationMs, "Generated token and lexical error files"});

    } catch (const std::exception& e) {
        UI::printCrash("Lexer", e.what());
        phases.push_back({"Lexer", false, 0, e.what()});
        UI::printSummaryTable(phases);
        return 1;
    }

    // PARSER PHASE
    UI::printSection("[2/4] SYNTACTIC ANALYSIS");
    try {
        auto start = std::chrono::steady_clock::now();
        parseSuccess = Parser::parseTokens(valid_tokens);

        writeSyntaxErrorsToFile(syntax_errors_file, Parser::getErrorMessages());
        writeDerivationToFile(derivation_file, Parser::getDerivationSteps());
        auto end = std::chrono::steady_clock::now();
        long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (parseSuccess) {
            UI::printStatusLine(true, "Parsing completed (no syntax errors)");
            phases.push_back({"Parser", true, durationMs, "No syntax errors"});
        } else {
            UI::printStatusLine(false, "Parsing completed with syntax errors");
            phases.push_back({"Parser", false, durationMs, "Syntax errors written to file"});
        }

    } catch (const std::exception& e) {
        UI::printCrash("Parser", e.what());
        phases.push_back({"Parser", false, 0, e.what()});
        UI::printSummaryTable(phases);
        return 1;
    }

    // AST PHASE
    UI::printSection("[3/4] AST EXPORT");
    try {
        auto start = std::chrono::steady_clock::now();
        ASTPrinter::writeToFile(Parser::getASTRoot(), ast_file);
        ASTPrinter::writeDotToFile(Parser::getASTRoot(), ast_dot_file);

        UI::PngRenderResult pngResult = UI::renderAstPngFromDot(ast_dot_file, ast_png_file);
        dotAvailable = pngResult.dotAvailable;
        pngGenerated = pngResult.pngGenerated;

        auto end = std::chrono::steady_clock::now();
        long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        bool astSuccess = !dotAvailable || (pngGenerated);
        std::string details = dotAvailable
            ? (astSuccess ? "AST/DOT/PNG generated" : "AST/DOT generated, image render incomplete")
            : "AST/DOT generated, Graphviz unavailable";
        phases.push_back({"AST", astSuccess, durationMs, details});

        if (astSuccess) {
            UI::printStatusLine(true, "AST export completed");
        } else {
            UI::printWarning("AST export completed with warnings");
        }

        UI::printPngGenerationNotes(pngResult);

    } catch (const std::exception& e) {
        UI::printCrash("AST export", e.what());
        phases.push_back({"AST", false, 0, e.what()});
        UI::printSummaryTable(phases);
        return 1;
    }

    // SEMANTIC PHASE
    UI::printSection("[4/4] SEMANTIC ANALYSIS");
    try {
        auto start = std::chrono::steady_clock::now();

        SemanticAnalyzer semanticAnalyzer;
        bool semanticSuccess = semanticAnalyzer.analyze(Parser::getASTRoot());

        std::ofstream symbolOut(symbol_tables_file, std::ios::out | std::ios::trunc);
        if (!symbolOut.is_open()) {
            throw std::runtime_error("Failed to open symbol table output file: " + symbol_tables_file);
        }
        symbolOut << semanticAnalyzer.dumpSymbolTables();

        if (!writeLinesToFile(semantic_errors_file, semanticAnalyzer.getErrors())) {
            throw std::runtime_error("Failed to open semantic errors output file: " + semantic_errors_file);
        }

        auto end = std::chrono::steady_clock::now();
        long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::string details = semanticSuccess
            ? "Symbol tables generated, no semantic errors"
            : "Symbol tables generated, semantic errors reported";
        phases.push_back({"Semantic", semanticSuccess, durationMs, details});

        if (semanticSuccess) {
            UI::printStatusLine(true, "Semantic analysis completed (no semantic errors)");
        } else {
            UI::printWarning("Semantic analysis completed with errors");
        }

    } catch (const std::exception& e) {
        UI::printCrash("Semantic analysis", e.what());
        phases.push_back({"Semantic", false, 0, e.what()});
        UI::printSummaryTable(phases);
        return 1;
    }

    UI::printSummaryTable(phases);

    UI::printArtifactList("Lexer Outputs", {
        {"Tokens", valid_output_file},
        {"Lex Errors", invalid_output_file}
    });

    UI::printArtifactList("Parser Outputs", {
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
    UI::printArtifactList("AST Outputs", astArtifacts);

    UI::printArtifactList("Semantic Outputs", {
        {"Symbol Tables", symbol_tables_file},
        {"Sem Errors", semantic_errors_file}
    });

    UI::printDone();

    return 0;
}