#include "../include/io.h"
#include "../include/token.h"
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace {
bool isRepoRoot(const fs::path& candidate) {
    return fs::exists(candidate / "CMakeLists.txt") &&
           fs::exists(candidate / "include") &&
           fs::exists(candidate / "src");
}

fs::path findRepoRootFrom(fs::path start) {
    while (!start.empty()) {
        if (isRepoRoot(start)) {
            return start;
        }

        const fs::path parent = start.parent_path();
        if (parent == start) {
            break;
        }
        start = parent;
    }

    return {};
}

fs::path resolveRepoRoot(const std::string& sourceFile, const std::string& driverExecutablePath) {
    if (!driverExecutablePath.empty()) {
        const fs::path exePath = fs::absolute(fs::path(driverExecutablePath));
        const fs::path fromExe = findRepoRootFrom(exePath.parent_path());
        if (!fromExe.empty()) {
            return fromExe;
        }
    }

    if (!sourceFile.empty()) {
        const fs::path sourcePath = fs::absolute(fs::path(sourceFile));
        const fs::path fromSource = findRepoRootFrom(sourcePath.parent_path());
        if (!fromSource.empty()) {
            return fromSource;
        }
    }

    const fs::path fromCwd = findRepoRootFrom(fs::current_path());
    if (!fromCwd.empty()) {
        return fromCwd;
    }

    return fs::current_path();
}
}

// ============================================================================
// Output path helpers
// ============================================================================

std::string getBaseName(const std::string& sourceFile) {
    return fs::path(sourceFile).stem().string();
}

std::string createOutputDir(const std::string& sourceFile, const std::string& driverExecutablePath) {
    std::string baseName = getBaseName(sourceFile);
    fs::path outputDir = resolveRepoRoot(sourceFile, driverExecutablePath) / "output" / baseName;
    fs::create_directories(outputDir);
    return outputDir.string();
}

std::string buildOutputPath(const std::string& outputDir, const std::string& baseName, const std::string& extension) {
    return (fs::path(outputDir) / (baseName + extension)).string();
}

CompilerOutputPaths prepareCompilerOutputPaths(const std::string& sourceFile, const std::string& driverExecutablePath) {
    CompilerOutputPaths paths;

    paths.baseName = getBaseName(sourceFile);
    paths.outputDir = createOutputDir(sourceFile, driverExecutablePath);

    fs::path lexerDir = fs::path(paths.outputDir) / "Lexer";
    fs::path parserDir = fs::path(paths.outputDir) / "Parser";
    fs::path astDir = fs::path(paths.outputDir) / "AST";
    fs::path semanticDir = fs::path(paths.outputDir) / "Semantics";
    fs::path codegenDir = fs::path(paths.outputDir) / "CodeGen";

    fs::create_directories(lexerDir);
    fs::create_directories(parserDir);
    fs::create_directories(astDir);
    fs::create_directories(semanticDir);
    fs::create_directories(codegenDir);

    paths.lexerDir = lexerDir.string();
    paths.parserDir = parserDir.string();
    paths.astDir = astDir.string();
    paths.semanticDir = semanticDir.string();
    paths.codegenDir = codegenDir.string();

    paths.validTokensFile = buildOutputPath(paths.lexerDir, paths.baseName, ".outlextokens");
    paths.invalidTokensFile = buildOutputPath(paths.lexerDir, paths.baseName, ".outlexerrors");
    paths.derivationFile = buildOutputPath(paths.parserDir, paths.baseName, ".outderivation");
    paths.syntaxErrorsFile = buildOutputPath(paths.parserDir, paths.baseName, ".outsyntaxerrors");
    paths.astFile = buildOutputPath(paths.astDir, paths.baseName, ".outast");
    paths.astDotFile = buildOutputPath(paths.astDir, paths.baseName, ".outast.dot");
    paths.astPngFile = buildOutputPath(paths.astDir, paths.baseName, ".outast.png");
    paths.symbolTablesFile = buildOutputPath(paths.semanticDir, paths.baseName, ".outsymboltables");
    paths.semanticDiagnosticsFile = buildOutputPath(paths.semanticDir, paths.baseName, ".outsemanticerrors");
    paths.moonOutputFile = buildOutputPath(paths.codegenDir, paths.baseName, ".moon");
    paths.codegenDiagnosticsFile = buildOutputPath(paths.codegenDir, paths.baseName, ".outcodegenerrors");
    paths.moonRunLogFile = buildOutputPath(paths.codegenDir, paths.baseName, ".outmoonrun");

    return paths;
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

    // If we reached EOF while still inside a block comment, flush it as an error
    if (inBlockComment) {
        std::vector<Token> pendingValid;
        std::vector<Token> pendingInvalid;
        Token::flushPendingBlockComment(pendingValid, pendingInvalid);
        if (!pendingValid.empty()) {
            std::get<0>(valid_and_invalid_tokens).push_back(pendingValid);
        }
        if (!pendingInvalid.empty()) {
            std::get<1>(valid_and_invalid_tokens).push_back(pendingInvalid);
        }
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

std::vector<std::vector<Token>> lex_file(const std::string& input_file,
                                         const std::string valid_out_file,
                                         const std::string invalid_out_file,
                                         size_t* invalidTokenCount) {
    auto valid_and_invalid_tokens = tokenizeFile(input_file);

    auto valids = std::get<0>(valid_and_invalid_tokens);
    auto invalids = std::get<1>(valid_and_invalid_tokens);

    size_t invalidCount = 0;
    for (const auto& lineTokens : invalids) {
        invalidCount += lineTokens.size();
    }
    if (invalidTokenCount != nullptr) {
        *invalidTokenCount = invalidCount;
    }

    writeTokensToFile(valid_out_file, valids);
    writeErrorsToFile(invalid_out_file, invalids);

    return valids;
}

bool writeLinesToFile(const std::string& filename, const std::vector<std::string>& lines) {
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& line : lines) {
        file << line << "\n";
    }

    return true;
}

bool writeTextToFile(const std::string& filename, const std::string& text) {
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file << text;
    return true;
}

MoonRunResult runMoonSimulator(const std::string& driverExecutablePath,
                               const std::string& moonInputFile,
                               const std::string& moonRunLogFile) {
    MoonRunResult result;

#ifdef _WIN32
    const std::string moonExecutableName = "moon.exe";
#else
    const std::string moonExecutableName = "moon";
#endif

    const fs::path driverExePath = fs::absolute(fs::path(driverExecutablePath));
    const fs::path driverDir = driverExePath.parent_path();
    const fs::path moonExeSameDir = driverDir / moonExecutableName;
    const fs::path moonExeWorkspace = fs::absolute(fs::path("exe") / moonExecutableName);
    const fs::path moonExeLegacyParent = fs::absolute(fs::path("..") / "exe" / moonExecutableName);

    const fs::path moonExePath =
        fs::exists(moonExeSameDir) ? moonExeSameDir :
        (fs::exists(moonExeWorkspace) ? moonExeWorkspace : moonExeLegacyParent);

    result.moonExecutablePath = moonExePath.string();

    if (!fs::exists(moonExePath)) {
        result.details = "Skipped: /exe/moon executable not found";
        writeLinesToFile(moonRunLogFile, {
            "[ERROR][MOON] Executable not found: " + moonExePath.string()
        });
        return result;
    }

    const fs::path moonInputPath = fs::path(moonInputFile);
    if (!fs::exists(moonInputPath)) {
        result.details = "Skipped: generated .moon file not found";
        writeLinesToFile(moonRunLogFile, {
            "[ERROR][MOON] Generated Moon file not found: " + moonInputFile
        });
        return result;
    }

    fs::path inputDir = moonInputPath.parent_path();
    if (inputDir.empty()) {
        inputDir = fs::path(".");
    }

    const fs::path codegenDirAbs = fs::absolute(inputDir);
    const std::string moonInputName = moonInputPath.filename().string();
    const std::string runLogName = fs::path(moonRunLogFile).filename().string();

#ifdef _WIN32
    const std::string command =
        "cd /d \"" + codegenDirAbs.string() + "\" && \"" + moonExePath.string() + "\" \"" + moonInputName + "\" > \"" + runLogName + "\" 2>&1";
#else
    const std::string command =
        "cd \"" + codegenDirAbs.string() + "\" && \"" + moonExePath.string() + "\" \"" + moonInputName + "\" > \"" + runLogName + "\" 2>&1";
#endif

    result.attempted = true;
    const int exitCode = std::system(command.c_str());
    result.success = (exitCode == 0);
    result.details = result.success
        ? "Moon executed generated assembly"
        : "Moon exited with non-zero status";

    return result;
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

void writeDerivationToFile(const std::string& filename, const std::vector<std::string>& derivationSteps) {
    std::ofstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + filename);
    }

    for (const auto& step : derivationSteps) {
        file << step << "\n";
    }

    file.close();
}
