#include <chrono>
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

namespace {
std::string buildBackEndSkipReason(bool parseSuccess, bool semanticHasErrors, const std::shared_ptr<ProgNode>& root) {
    if (!parseSuccess) {
        return "parser errors";
    }
    if (semanticHasErrors) {
        return "semantic errors";
    }
    if (root == nullptr) {
        return "missing AST";
    }
    return "prerequisites not satisfied";
}
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
    UI::printTitle("COMP442 COMPILER DRIVER DASHBOARD");
    UI::printKV("Source", sourceFile);

    CompilerOutputPaths outputs = prepareCompilerOutputPaths(sourceFile, argv[0]);
    UI::printKV("Output", outputs.outputDir);

    std::vector<std::vector<Token>> valid_tokens;
    bool parseSuccess = false;
    bool dotAvailable = false;
    bool pngGenerated = false;
    bool semanticHasErrors = false;
    bool semanticHasWarnings = false;
    bool codegenHasRunnableMoon = false;

    // LEXER PHASE
    UI::printSection("[1/6] LEXICAL ANALYSIS");
    try {
        auto start = std::chrono::steady_clock::now();
        size_t lexicalErrorCount = 0;
        valid_tokens = lex_file(sourceFile, outputs.validTokensFile, outputs.invalidTokensFile, &lexicalErrorCount);
        auto end = std::chrono::steady_clock::now();
        long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (lexicalErrorCount == 0) {
            UI::printStatusLine(true, "Tokenization completed (no lexical errors)");
            phases.push_back({"Lexer", true, durationMs, "Generated token and lexical error files"});
        } else {
            UI::printStatusLine(false, "Tokenization completed with " + std::to_string(lexicalErrorCount) + " lexical error(s)");
            phases.push_back({"Lexer", false, durationMs,
                              std::to_string(lexicalErrorCount) + " lexical error(s) written to file"});
        }

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

        writeSyntaxErrorsToFile(outputs.syntaxErrorsFile, Parser::getErrorMessages());
        writeDerivationToFile(outputs.derivationFile, Parser::getDerivationSteps());
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
        ASTPrinter::writeToFile(Parser::getASTRoot(), outputs.astFile);
        ASTPrinter::writeDotToFile(Parser::getASTRoot(), outputs.astDotFile);

        UI::PngRenderResult pngResult = UI::renderAstPngFromDot(outputs.astDotFile, outputs.astPngFile);
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

        (void)semanticSuccess;

        if (!writeTextToFile(outputs.symbolTablesFile, semanticAnalyzer.dumpSymbolTables())) {
            throw std::runtime_error("Failed to open symbol table output file: " + outputs.symbolTablesFile);
        }

        const auto& errors = semanticAnalyzer.getErrors();
        const auto& warnings = semanticAnalyzer.getWarnings();
        const bool hasErrors = !errors.empty();
        const bool hasWarnings = !warnings.empty();
        semanticHasErrors = hasErrors;
        semanticHasWarnings = hasWarnings;

        std::vector<std::string> semanticDiagnostics = errors;
        semanticDiagnostics.insert(semanticDiagnostics.end(), warnings.begin(), warnings.end());

        if (!writeLinesToFile(outputs.semanticDiagnosticsFile, semanticDiagnostics)) {
            throw std::runtime_error("Failed to open semantic errors output file: " + outputs.semanticDiagnosticsFile);
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

    const std::string backEndSkipReason = buildBackEndSkipReason(parseSuccess, semanticHasErrors, Parser::getASTRoot());

    // CODEGEN PHASE
    UI::printSection("[5/6] CODE GENERATION");
    if (!canAttemptBackEnd) {
        UI::printWarning("Code generation not attempted because " + backEndSkipReason);
        phases.push_back({"CodeGen", false, 0, "Not attempted due to " + backEndSkipReason});
        if (!writeLinesToFile(outputs.codegenDiagnosticsFile, {
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

            codegenSuccess = generateMoonAssembly(Parser::getASTRoot(), outputs.moonOutputFile, &codegenErrors);

            if (!writeLinesToFile(outputs.codegenDiagnosticsFile, codegenErrors)) {
                throw std::runtime_error("Failed to open codegen diagnostics output file: " + outputs.codegenDiagnosticsFile);
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
        if (!writeLinesToFile(outputs.moonRunLogFile, {
            "[ERROR][MOON] Not attempted due to " + backEndSkipReason
        })) {
            UI::printWarning("Failed to write Moon run log for skipped phase");
        }
    } else if (!codegenHasRunnableMoon) {
        UI::printWarning("Moon simulation not attempted because code generation did not produce runnable output");
        phases.push_back({"Moon", false, 0, "Not attempted due to code generation failure"});
        if (!writeLinesToFile(outputs.moonRunLogFile, {
            "[ERROR][MOON] Not attempted due to code generation failure"
        })) {
            UI::printWarning("Failed to write Moon run log for skipped phase");
        }
    } else {
        try {
            auto start = std::chrono::steady_clock::now();

            MoonRunResult moonResult = runMoonSimulator(argv[0], outputs.moonOutputFile, outputs.moonRunLogFile);

            if (moonResult.success) {
                UI::printStatusLine(true, "Moon simulation completed");
            } else if (!moonResult.attempted) {
                if (moonResult.details.find("executable not found") != std::string::npos) {
                    UI::printWarning("Moon simulator executable not found at: " + moonResult.moonExecutablePath);
                } else {
                    UI::printWarning(moonResult.details);
                }
            } else {
                UI::printStatusLine(false, "Moon simulation failed");
            }

            auto end = std::chrono::steady_clock::now();
            long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            phases.push_back({"Moon", moonResult.success, durationMs, moonResult.details});

        } catch (const std::exception& e) {
            UI::printCrash("Moon simulation", e.what());
            phases.push_back({"Moon", false, 0, e.what()});
            UI::printSummaryTable(phases);
            return 1;
        }
    }

    UI::printSummaryTable(phases);

    UI::printArtifactList("Lexer Outputs", {
        {"Tokens", outputs.validTokensFile},
        {"Lex Errors", outputs.invalidTokensFile}
    });

    UI::printArtifactList("Parser Outputs", {
        {"Derivation", outputs.derivationFile},
        {"Syntax Errors", outputs.syntaxErrorsFile}
    });

    std::vector<std::pair<std::string, std::string>> astArtifacts = {
        {"AST", outputs.astFile},
        {"DOT", outputs.astDotFile}
    };
    if (dotAvailable && pngGenerated) {
        astArtifacts.push_back({"PNG", outputs.astPngFile});
    }
    UI::printArtifactList("AST Outputs", astArtifacts);

    UI::printArtifactList("Semantic Outputs", {
        {"Symbol Tables", outputs.symbolTablesFile},
        {"Sem Diagnostics ", outputs.semanticDiagnosticsFile},
    });

    UI::printArtifactList("CodeGen Outputs", {
        {"Moon", outputs.moonOutputFile},
        {"CodeGen Diags", outputs.codegenDiagnosticsFile},
        {"Moon Run Log", outputs.moonRunLogFile},
    });

    UI::printDone();

    return 0;
}