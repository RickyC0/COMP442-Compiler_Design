#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../include/io.h"
#include "../include/token.h"
#include "../include/my_parser.h"
#include "../include/semantic.h"
#include "../include/codegen.h"
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
    fs::path codegenDir = fs::path(outputDir) / "CodeGen";
    fs::create_directories(lexerDir);
    fs::create_directories(parserDir);
    fs::create_directories(astDir);
    fs::create_directories(semanticDir);
    fs::create_directories(codegenDir);

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
    std::string moon_output_file = buildOutputPath(codegenDir.string(), baseName, ".moon");
    std::string codegen_diagnostics_file = buildOutputPath(codegenDir.string(), baseName, ".outcodegenerrors");
    std::string moon_run_log_file = buildOutputPath(codegenDir.string(), baseName, ".outmoonrun");

    std::vector<std::vector<Token>> valid_tokens;
    bool parseSuccess = false;
    bool dotAvailable = false;
    bool pngGenerated = false;
    bool semanticHasErrors = false;
    bool semanticHasWarnings = false;
    bool codegenHasRunnableMoon = false;

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
    UI::printSection("[1/6] LEXICAL ANALYSIS");
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
    UI::printSection("[2/6] SYNTACTIC ANALYSIS");
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
    UI::printSection("[3/6] AST EXPORT");
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
    UI::printSection("[4/6] SEMANTIC ANALYSIS");
    try {
        auto start = std::chrono::steady_clock::now();

        SemanticAnalyzer semanticAnalyzer;
        bool semanticSuccess = semanticAnalyzer.analyze(Parser::getASTRoot());

        std::ofstream symbolOut(symbol_tables_file, std::ios::out | std::ios::trunc);
        if (!symbolOut.is_open()) {
            throw std::runtime_error("Failed to open symbol table output file: " + symbol_tables_file);
        }
        symbolOut << semanticAnalyzer.dumpSymbolTables();

        const auto& errors = semanticAnalyzer.getErrors();
        const auto& warnings = semanticAnalyzer.getWarnings();
        const bool hasErrors = !errors.empty();
        const bool hasWarnings = !warnings.empty();
        semanticHasErrors = hasErrors;
        semanticHasWarnings = hasWarnings;

        std::vector<std::string> semanticDiagnostics = errors;
        semanticDiagnostics.insert(semanticDiagnostics.end(), warnings.begin(), warnings.end());

        if (!writeLinesToFile(semantic_errors_file, semanticDiagnostics)) {
            throw std::runtime_error("Failed to open semantic errors output file: " + semantic_errors_file);
        }

        auto end = std::chrono::steady_clock::now();
        long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::string details;
        if (hasErrors) {
            details = "Symbol tables generated, semantic errors reported";
        } else if (hasWarnings) {
            details = "Symbol tables generated, semantic warnings reported";
        } else {
            details = "Symbol tables generated, no semantic diagnostics";
        }

        phases.push_back({"Semantic", !hasErrors, durationMs, details});

        if (hasErrors) {
            UI::printStatusLine(false, "Semantic analysis completed with errors");
        } else if (hasWarnings) {
            UI::printWarning("Semantic analysis completed with warnings");
        } else {
            UI::printStatusLine(true, "Semantic analysis completed (no semantic diagnostics)");
        }

    } catch (const std::exception& e) {
        UI::printCrash("Semantic analysis", e.what());
        phases.push_back({"Semantic", false, 0, e.what()});
        UI::printSummaryTable(phases);
        return 1;
    }

    const bool canAttemptBackEnd = parseSuccess && !semanticHasErrors && Parser::getASTRoot() != nullptr;

    std::string backEndSkipReason;
    if (!parseSuccess) {
        backEndSkipReason = "parser errors";
    } else if (semanticHasErrors) {
        backEndSkipReason = "semantic errors";
    } else if (Parser::getASTRoot() == nullptr) {
        backEndSkipReason = "missing AST";
    } else {
        backEndSkipReason = "prerequisites not satisfied";
    }

    // CODEGEN PHASE
    UI::printSection("[5/6] CODE GENERATION");
    if (!canAttemptBackEnd) {
        UI::printWarning("Code generation not attempted because " + backEndSkipReason);
        phases.push_back({"CodeGen", false, 0, "Not attempted due to " + backEndSkipReason});
        if (!writeLinesToFile(codegen_diagnostics_file, {
            "[ERROR][CODEGEN] Not attempted due to " + backEndSkipReason
        })) {
            UI::printWarning("Failed to write codegen diagnostics file for skipped phase");
        }
    } else {
        try {
            auto start = std::chrono::steady_clock::now();

            std::vector<std::string> codegenErrors;
            bool codegenSuccess = false;
            std::string details;

            codegenSuccess = generateMoonAssembly(Parser::getASTRoot(), moon_output_file, &codegenErrors);

            if (!writeLinesToFile(codegen_diagnostics_file, codegenErrors)) {
                throw std::runtime_error("Failed to open codegen diagnostics output file: " + codegen_diagnostics_file);
            }

            if (codegenSuccess && codegenErrors.empty()) {
                details = "Moon assembly generated";
                UI::printStatusLine(true, "Code generation completed");
                codegenHasRunnableMoon = true;
            } else if (codegenSuccess) {
                details = "Moon assembly generated with codegen diagnostics";
                UI::printWarning("Code generation completed with diagnostics");
                codegenHasRunnableMoon = true;
            } else {
                details = "Code generation failed";
                UI::printStatusLine(false, "Code generation failed");
            }

            phases.push_back({"CodeGen", codegenSuccess && codegenErrors.empty(),
                              std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count(),
                              details});

        } catch (const std::exception& e) {
            UI::printCrash("Code generation", e.what());
            phases.push_back({"CodeGen", false, 0, e.what()});
            UI::printSummaryTable(phases);
            return 1;
        }
    }

    // MOON EXECUTION PHASE
    UI::printSection("[6/6] MOON SIMULATION");
    if (!canAttemptBackEnd) {
        UI::printWarning("Moon simulation not attempted because " + backEndSkipReason);
        phases.push_back({"Moon", false, 0, "Not attempted due to " + backEndSkipReason});
        if (!writeLinesToFile(moon_run_log_file, {
            "[ERROR][MOON] Not attempted due to " + backEndSkipReason
        })) {
            UI::printWarning("Failed to write Moon run log for skipped phase");
        }
    } else if (!codegenHasRunnableMoon) {
        UI::printWarning("Moon simulation not attempted because code generation did not produce runnable output");
        phases.push_back({"Moon", false, 0, "Not attempted due to code generation failure"});
        if (!writeLinesToFile(moon_run_log_file, {
            "[ERROR][MOON] Not attempted due to code generation failure"
        })) {
            UI::printWarning("Failed to write Moon run log for skipped phase");
        }
    } else {
        try {
            auto start = std::chrono::steady_clock::now();

            bool moonSuccess = false;
            std::string details;

#ifdef _WIN32
            const fs::path moonExePath = fs::absolute(fs::path("..") / "exe" / "moon.exe");
#else
            const fs::path moonExePath = fs::absolute(fs::path("..") / "exe" / "moon");
#endif
            const fs::path codegenDirAbs = fs::absolute(codegenDir);

            if (!fs::exists(moonExePath)) {
                details = "Skipped: /exe/moon executable not found";
                UI::printWarning("Moon simulator executable not found at: " + moonExePath.string());
                writeLinesToFile(moon_run_log_file, {
                    "[ERROR][MOON] Executable not found: " + moonExePath.string()
                });
            } else if (!fs::exists(moon_output_file)) {
                details = "Skipped: generated .moon file not found";
                UI::printWarning("Generated Moon file not found: " + moon_output_file);
                writeLinesToFile(moon_run_log_file, {
                    "[ERROR][MOON] Generated Moon file not found: " + moon_output_file
                });
            } else {
                const std::string moonInputName = fs::path(moon_output_file).filename().string();
                const std::string runLogName = fs::path(moon_run_log_file).filename().string();

#ifdef _WIN32
                const std::string command =
                    "cd /d \"" + codegenDirAbs.string() + "\" && \"" + moonExePath.string() + "\" \"" + moonInputName + "\" > \"" + runLogName + "\" 2>&1";
#else
                const std::string command =
                    "cd \"" + codegenDirAbs.string() + "\" && \"" + moonExePath.string() + "\" \"" + moonInputName + "\" > \"" + runLogName + "\" 2>&1";
#endif

                const int exitCode = std::system(command.c_str());
                moonSuccess = (exitCode == 0);

                if (moonSuccess) {
                    UI::printStatusLine(true, "Moon simulation completed");
                    details = "Moon executed generated assembly";
                } else {
                    UI::printStatusLine(false, "Moon simulation failed");
                    details = "Moon exited with non-zero status";
                }
            }

            auto end = std::chrono::steady_clock::now();
            long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            phases.push_back({"Moon", moonSuccess, durationMs, details});

        } catch (const std::exception& e) {
            UI::printCrash("Moon simulation", e.what());
            phases.push_back({"Moon", false, 0, e.what()});
            UI::printSummaryTable(phases);
            return 1;
        }
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
        {"Sem Diagnostics ", semantic_errors_file},
    });

    UI::printArtifactList("CodeGen Outputs", {
        {"Moon", moon_output_file},
        {"CodeGen Diags", codegen_diagnostics_file},
        {"Moon Run Log", moon_run_log_file},
    });

    UI::printDone();

    return 0;
}